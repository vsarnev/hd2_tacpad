#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"
#include "ui/styles.h"
#include "main.h"
#include "configuration.h"

static const char *TAG_BAT = "Battery";

// --- Wiring ---------------------------------------------------------------------------------------
// battery+  --[ Rtop ]--+--[ Rbottom ]-- GND      (tap = the junction -> GPIO7)
//                       |
//                     GPIO7  (ADC1_CH6)
//
// Set BATT_DIVIDER_RATIO to (Rtop + Rbottom) / Rbottom. Equal resistors (e.g. 10k/10k) = 2.0.
#define BATT_DIVIDER_RATIO 2.0f

// Coarse gain trim (compile-time). Runtime fine-tuning is done on-screen with the +/- buttons, which
// adjust an additive offset stored in NVS — no reflash needed.
#define BATT_CAL_TRIM 1.0253f

#define BATT_ADC_CHANNEL ADC_CHANNEL_6 // GPIO7 = ADC1_CH6 on the ESP32-S3
#define BATT_ADC_ATTEN ADC_ATTEN_DB_12 // ~0..3.1V input range
#define BATT_SAMPLES 96                // averaged per reading (heavy, since there's no smoothing cap)
#define BATT_UPDATE_MS 15000           // battery moves slowly; poll gently
#define BATT_CAL_KEY "battcal"         // NVS key for the runtime calibration offset (mV)
#define BATT_CAL_STEP_MV 10            // each +/- tap nudges by 0.01 V
#define BATT_CAL_LIMIT_MV 500          // clamp the offset to a sane range

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_cali_handle_t s_cali = NULL;
static bool s_caliValid = false;
static int s_calOffsetMv = 0;
static int s_lastMv = 0;

static lv_obj_t *s_topLabel = NULL;    // battery icon + % at the top of the config screen
static lv_obj_t *s_voltLabel = NULL;   // voltage readout in the Misc tab
static lv_obj_t *s_offsetLabel = NULL; // current calibration offset, between the -/+ buttons

// Resting-voltage(mV) -> %% for a 1S LiPo. Coarse but far better than a straight line. Interpolated.
static int lipoPercent(int mv)
{
    static const int curve[][2] = {
        {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80},
        {3980, 75}, {3950, 70}, {3910, 65}, {3870, 60}, {3850, 55},
        {3840, 50}, {3820, 45}, {3800, 40}, {3790, 35}, {3770, 30},
        {3750, 25}, {3730, 20}, {3710, 15}, {3690, 10}, {3610, 5}, {3300, 0}};
    const int n = sizeof(curve) / sizeof(curve[0]);

    if (mv >= curve[0][0])
    {
        return 100;
    }
    if (mv <= curve[n - 1][0])
    {
        return 0;
    }
    for (int i = 0; i < n - 1; i++)
    {
        const int vHi = curve[i][0], pHi = curve[i][1];
        const int vLo = curve[i + 1][0], pLo = curve[i + 1][1];
        if (mv <= vHi && mv >= vLo)
        {
            return pLo + (mv - vLo) * (pHi - pLo) / (vHi - vLo);
        }
    }
    return 0;
}

static const char *battSymbol(int pct)
{
    if (pct >= 85)
    {
        return LV_SYMBOL_BATTERY_FULL;
    }
    if (pct >= 60)
    {
        return LV_SYMBOL_BATTERY_3;
    }
    if (pct >= 35)
    {
        return LV_SYMBOL_BATTERY_2;
    }
    if (pct >= 10)
    {
        return LV_SYMBOL_BATTERY_1;
    }
    return LV_SYMBOL_BATTERY_EMPTY;
}

int batteryMillivolts(void)
{
    if (s_adc == NULL)
    {
        return 0;
    }

    int64_t sum = 0;
    int valid = 0;
    for (int i = 0; i < BATT_SAMPLES; i++)
    {
        int raw;
        if (adc_oneshot_read(s_adc, BATT_ADC_CHANNEL, &raw) == ESP_OK)
        {
            sum += raw;
            valid++;
        }
    }
    if (valid == 0)
    {
        return 0;
    }

    const int rawAvg = (int)(sum / valid);
    int pinMv = 0;
    if (s_caliValid)
    {
        adc_cali_raw_to_voltage(s_cali, rawAvg, &pinMv);
    }
    else
    {
        pinMv = rawAvg * 3100 / 4095; // rough fallback if hardware calibration is unavailable
    }

    s_lastMv = (int)(pinMv * BATT_DIVIDER_RATIO * BATT_CAL_TRIM) + s_calOffsetMv;
    return s_lastMv;
}

int batteryPercent(void)
{
    return lipoPercent(batteryMillivolts());
}

// Refresh both readouts from a single fresh reading.
static void batt_refresh(void)
{
    const int mv = batteryMillivolts();
    const int pct = lipoPercent(mv);

    if (s_topLabel != NULL)
    {
        lv_label_set_text_fmt(s_topLabel, "%s %d%%", battSymbol(pct), pct);
    }
    if (s_voltLabel != NULL)
    {
        lv_label_set_text_fmt(s_voltLabel, "%d.%02d V", mv / 1000, (mv % 1000) / 10);
    }
    if (s_offsetLabel != NULL)
    {
        lv_label_set_text_fmt(s_offsetLabel, "%+d mV", s_calOffsetMv);
    }
}

static void batt_update_timer(lv_timer_t *t)
{
    (void)t;
    batt_refresh();
}

// Runtime one-point calibration: nudge the reading by +/- 0.01 V per tap, persisted to NVS.
static void batt_cal_nudge(int deltaMv)
{
    s_calOffsetMv += deltaMv;
    if (s_calOffsetMv > BATT_CAL_LIMIT_MV)
    {
        s_calOffsetMv = BATT_CAL_LIMIT_MV;
    }
    if (s_calOffsetMv < -BATT_CAL_LIMIT_MV)
    {
        s_calOffsetMv = -BATT_CAL_LIMIT_MV;
    }
    setConfigBig(BATT_CAL_KEY, (uint16_t)s_calOffsetMv);
    batt_refresh();
}

static void batt_cal_minus_cb(lv_event_t *e)
{
    (void)e;
    batt_cal_nudge(-BATT_CAL_STEP_MV);
}

static void batt_cal_plus_cb(lv_event_t *e)
{
    (void)e;
    batt_cal_nudge(+BATT_CAL_STEP_MV);
}

// Load the saved calibration offset from NVS. Must be called after the config NVS handle is open,
// i.e. from loadConfig() — NOT from initBatteryMonitor(), which runs in ui_post() before loadConfig
// opens the handle (reading it there always returned the default 0, so the offset never persisted).
void batteryLoadCalibration(void)
{
    int mv = getConfigBig(BATT_CAL_KEY, 0);
    if (mv > BATT_CAL_LIMIT_MV)
    {
        mv = BATT_CAL_LIMIT_MV;
    }
    if (mv < -BATT_CAL_LIMIT_MV)
    {
        mv = -BATT_CAL_LIMIT_MV;
    }
    s_calOffsetMv = mv;
    batt_refresh();
}

void initBatteryMonitor(void)
{
    // ADC1 one-shot on GPIO7.
    adc_oneshot_unit_init_cfg_t unitCfg = {.unit_id = ADC_UNIT_1};
    if (adc_oneshot_new_unit(&unitCfg, &s_adc) != ESP_OK)
    {
        ESP_LOGE(TAG_BAT, "ADC unit init failed");
        return;
    }
    adc_oneshot_chan_cfg_t chanCfg = {.atten = BATT_ADC_ATTEN, .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc_oneshot_config_channel(s_adc, BATT_ADC_CHANNEL, &chanCfg);

    adc_cali_curve_fitting_config_t caliCfg = {
        .unit_id = ADC_UNIT_1, .atten = BATT_ADC_ATTEN, .bitwidth = ADC_BITWIDTH_DEFAULT};
    if (adc_cali_create_scheme_curve_fitting(&caliCfg, &s_cali) == ESP_OK)
    {
        s_caliValid = true;
    }

    // Narrow the config tab bar (and shrink its label font so "Display / Audio" still fits) to free
    // the right end for a glanceable battery icon + % in line with the tabs.
    lv_obj_t *tabBtns = lv_tabview_get_tab_btns(objects.tab_view_config);
    lv_obj_set_style_text_font(tabBtns, &lv_font_montserrat_12, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_width(tabBtns, 410);
    lv_obj_set_style_pad_column(tabBtns, 2, LV_PART_MAIN | LV_STATE_DEFAULT); // tighter gaps between tabs
    lv_btnmatrix_set_btn_width(tabBtns, 0, 2);                                // give "Display / Audio" 2x width

    s_topLabel = lv_label_create(objects.config);
    lv_obj_set_style_text_font(s_topLabel, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_topLabel, lv_color_hex(colorActive), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(s_topLabel, LV_SYMBOL_BATTERY_FULL " --%");
    lv_obj_align_to(s_topLabel, tabBtns, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    // Voltage readout + calibration (-/+) row on the config "Misc." tab (located via the
    // autocomplete switch that lives there).
    lv_obj_t *tab = lv_obj_get_parent(lv_obj_get_parent(objects.chb_auto_complete));

    lv_obj_t *row = lv_obj_create(tab);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(row, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(row, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(row, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(row, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_layout(row, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_flex_main_place(row, LV_FLEX_ALIGN_START, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(row);
    lv_label_set_text_static(title, "Battery");

    s_voltLabel = lv_label_create(row);
    lv_obj_set_flex_grow(s_voltLabel, 1);
    lv_obj_set_style_text_align(s_voltLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_voltLabel, lv_color_hex(colorActive), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_static(s_voltLabel, "-- V");

    lv_obj_t *minus = lv_btn_create(row);
    lv_obj_set_size(minus, 62, 46);
    add_style_button_std(minus);
    lv_obj_add_event_cb(minus, batt_cal_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *minusLbl = lv_label_create(minus);
    lv_obj_set_style_text_font(minusLbl, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_static(minusLbl, "-");
    lv_obj_center(minusLbl);

    s_offsetLabel = lv_label_create(row);
    lv_obj_set_style_text_align(s_offsetLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_static(s_offsetLabel, "+0 mV");

    lv_obj_t *plus = lv_btn_create(row);
    lv_obj_set_size(plus, 62, 46);
    add_style_button_std(plus);
    lv_obj_add_event_cb(plus, batt_cal_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *plusLbl = lv_label_create(plus);
    lv_obj_set_style_text_font(plusLbl, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_static(plusLbl, "+");
    lv_obj_center(plusLbl);

    lv_timer_create(batt_update_timer, BATT_UPDATE_MS, NULL);
    batt_refresh(); // first reading immediately
}
