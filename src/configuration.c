#include <esp_system.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "i2s_sdcard.h"
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"
#include "ui/styles.h"
#include "main.h"
#include "configuration.h"

const char *TAG_CFG = "Configuration";

// Handle for NVS config
nvs_handle_t nvsConfig;

extern bool playerMuted;
extern int inputDelay;
extern int screenRotation;
extern uint8_t keymapIndex;
extern uint8_t connectionType;
extern bool manualAutoComplete;
extern bool gameAfterPreset;
extern bool showCooldowns;

extern esp_err_t ble_controller_init();
extern esp_err_t ble_controller_deinit();
extern esp_err_t usb_controller_init();
extern esp_err_t usb_controller_deinit();

#define CFG_KEY_DELAY "delay"
#define CFG_KEY_ROTATION "rotation"
#define CFG_KEY_BRIGHTNESS "brightness"
#define CFG_KEY_MUTED "muted"
#define CFG_KEY_VOLUME "volume"
#define CFG_KEY_CONNECTIVITY "connectivity"

// Software master volume (0..256), defined in main.c and applied in the audio player.
extern volatile uint16_t audioVolume;
// The code-created volume slider + value label (built in initVolumeControl).
static lv_obj_t *sldVolume = NULL;
static lv_obj_t *lblVolume = NULL;
#define CFG_KEY_KEYMAP "keymap"
#define CFG_KEY_AUTOCOMPLETE "autoComplete"
#define CFG_KEY_COOLDOWN "showCooldown"
#define CFG_KEY_SHIPMODULES "shipModules"
#define CFG_KEY_GAMEAFTERPRESET "gamePreset"

// Init configuration from NVS
esp_err_t initConfig()
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(init_sdcard());
    lv_fs_fatfs_init();

    return ret;
}

// Read configuration value by key
// key - Identifier for configuration value
// defaultValue - Default value which will be returned if the key/value does not exist
int8_t getConfig(char *key, int8_t defaultValue)
{
    uint8_t value;

    esp_err_t ret = nvs_get_u8(nvsConfig, key, &value);

    switch (ret)
    {
    case ESP_OK:
        return value;
    case ESP_ERR_NVS_NOT_FOUND:
        break;
    default:
        ESP_LOGE(TAG_CFG, "Error (%s) reading!\n", esp_err_to_name(ret));
    }

    return defaultValue;
}

int16_t getConfigBig(char *key, int16_t defaultValue)
{
    uint16_t value;

    esp_err_t ret = nvs_get_u16(nvsConfig, key, &value);

    switch (ret)
    {
    case ESP_OK:
        return value;
    case ESP_ERR_NVS_NOT_FOUND:
        break;
    default:
        ESP_LOGE(TAG_CFG, "Error (%s) reading!\n", esp_err_to_name(ret));
    }

    return defaultValue;
}

// Write configuration value by key
// key - Identifier for configuration value
// value - Value which should be stored
void setConfig(char *key, uint8_t value)
{
    esp_err_t ret;

    ret = nvs_open("config", NVS_READWRITE, &nvsConfig);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_CFG, "Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
        return;
    }

    ret = nvs_set_u8(nvsConfig, key, value);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_CFG, "Write (%s) failed!\n", key);
    }

    nvs_close(nvsConfig);
}

void setConfigBig(char *key, uint16_t value)
{
    esp_err_t ret;

    ret = nvs_open("config", NVS_READWRITE, &nvsConfig);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_CFG, "Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
        return;
    }

    ret = nvs_set_u16(nvsConfig, key, value);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_CFG, "Write (%s) failed!\n", key);
    }

    nvs_close(nvsConfig);
}

// Write the HID input delay to configuration
void setDelay(int delay, bool restore)
{
    inputDelay = delay;

    char *textDelay = (char *)malloc(7 * sizeof(char));
    sprintf(textDelay, "%d %s", delay, " ms");

    lv_label_set_text(objects.lbl_delay, (void *)textDelay);

    if (restore)
    {
        lv_slider_set_value(objects.sld_delay, (int)(delay / 10), LV_ANIM_OFF);
    }
    else
    {
        setConfig(CFG_KEY_DELAY, delay);
    }
}

// Write the display rotation to configuration
void setRotation(int rotation, bool restore)
{
    bool restart = !restore && screenRotation != rotation;

    screenRotation = rotation;

    if (restore)
    {
        if (screenRotation == LV_DISP_ROT_270)
        {
            lv_obj_add_state(objects.chb_flip, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_flip, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig(CFG_KEY_ROTATION, screenRotation);
    }

    playbackSound(SND_SWITCH);

    if (restart)
    {
        esp_restart();
    }
}

// Write the display brightness to configuration
void setBrightness(int brightness, bool restore)
{
    dimScreen(brightness);

    char *textBrightness = (char *)malloc(5 * sizeof(char));
    sprintf(textBrightness, "%d %s", brightness, " %");

    lv_label_set_text(objects.lbl_brightness, (void *)textBrightness);

    if (restore)
    {
        lv_slider_set_value(objects.sld_brightness, (int)(brightness / 10), LV_ANIM_OFF);
    }
    else
    {
        setConfig(CFG_KEY_BRIGHTNESS, brightness);
    }
}

// Write the sound mute state to configuration
void setMuted(bool muted, bool restore)
{
    playerMuted = muted;

    if (restore)
    {
        if (muted)
        {
            lv_obj_add_state(objects.chb_mute, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_mute, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig(CFG_KEY_MUTED, playerMuted ? 1 : 0);
    }

    playbackSound(SND_SWITCH);
}

// Write the sound volume (0..100 %) to configuration and apply it to the player.
void setVolume(int volume, bool restore)
{
    if (volume < 0)
    {
        volume = 0;
    }
    if (volume > 100)
    {
        volume = 100;
    }

    audioVolume = (uint16_t)((volume * 256) / 100); // 0..256 for the player

    if (lblVolume != NULL)
    {
        char textVolume[8];
        sprintf(textVolume, "%d %%", volume);
        lv_label_set_text(lblVolume, textVolume);
    }

    if (restore)
    {
        if (sldVolume != NULL)
        {
            lv_slider_set_value(sldVolume, volume, LV_ANIM_OFF);
        }
    }
    else
    {
        setConfig(CFG_KEY_VOLUME, volume);
    }
}

static void volume_slider_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
    {
        setVolume(lv_slider_get_value(lv_event_get_target(e)), false);
    }
}

// Build the volume control (title + value + slider) as a new row on the config "Output" tab, next
// to the mute toggle. Not part of the generated UI, so it's created in code and called from ui_post.
void initVolumeControl()
{
    // The config tab that holds the brightness row (brightness slider -> its row container -> tab).
    lv_obj_t *tab = lv_obj_get_parent(lv_obj_get_parent(objects.sld_brightness));

    lv_obj_t *row = lv_obj_create(tab);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(row, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(row, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(row, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(row, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(row, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_layout(row, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(row);
    lv_obj_set_size(title, LV_PCT(50), LV_SIZE_CONTENT);
    lv_label_set_text_static(title, "Sound volume");

    lblVolume = lv_label_create(row);
    lv_obj_set_size(lblVolume, LV_PCT(50), LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(lblVolume, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lblVolume, lv_color_hex(colorActive), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_static(lblVolume, "100 %");

    sldVolume = lv_slider_create(row);
    lv_obj_set_size(sldVolume, LV_PCT(100), 12);
    lv_slider_set_range(sldVolume, 0, 100);
    lv_slider_set_value(sldVolume, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(sldVolume, volume_slider_cb, LV_EVENT_ALL, NULL);
    add_style_slider_config(sldVolume);
}

// Write the keymap assignment to configuration
void setConnectivity(uint8_t index, bool restore)
{
    if (restore)
    {
        lv_dropdown_set_selected(objects.dd_connectivity, index - 1);
    }
    else
    {
        setConfig(CFG_KEY_CONNECTIVITY, index);
    }

    playbackSound(SND_SWITCH);

    deinitConnection();

    connectionType = index;

    switch (connectionType)
    {
    case CT_BLUETOOTH:
        // Init Bluetooth controller
        ble_controller_init();
        break;
    case CT_USB:
        // Init USB controller
        usb_controller_init();
        break;
    default:
        break;
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);

    updateConnection();
}

// Write the keymap assignment to configuration
void setKeymap(uint8_t index, bool restore)
{
    keymapIndex = index;

    if (restore)
    {
        lv_dropdown_set_selected(objects.dd_keymap, index);
    }
    else
    {
        setConfig(CFG_KEY_KEYMAP, index);
    }

    playbackSound(SND_SWITCH);
}

void setAutoComplete(bool enable, bool restore)
{
    manualAutoComplete = enable;

    if (restore)
    {
        if (enable)
        {
            lv_obj_add_state(objects.chb_auto_complete, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_auto_complete, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig(CFG_KEY_AUTOCOMPLETE, manualAutoComplete ? 1 : 0);
    }

    playbackSound(SND_SWITCH);
}

void setGameAfterPreset(bool enable, bool restore)
{
    gameAfterPreset = enable;

    if (restore)
    {
        if (enable)
        {
            lv_obj_add_state(objects.chb_game_after_preset, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_game_after_preset, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig(CFG_KEY_GAMEAFTERPRESET, gameAfterPreset ? 1 : 0);
    }

    playbackSound(SND_SWITCH);
}

void setCooldown(bool enable, bool restore)
{
    showCooldowns = enable;

    if (restore)
    {
        if (enable)
        {
            lv_obj_add_state(objects.chb_cooldowns, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_cooldowns, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig(CFG_KEY_COOLDOWN, showCooldowns ? 1 : 0);
    }

    playbackSound(SND_SWITCH);
}

void setShipModules(bool restore)
{
    shipModule list[MAX_SHIP_MODULES] = {
        {SHIP_LVC, objects.chb_ship_mod_lvc},
        {SHIP_ZBL, objects.chb_ship_mod_zbl},
        {SHIP_HC, objects.chb_ship_mod_hc},
        {SHIP_MA, objects.chb_ship_mod_ma},
        {SHIP_SRP, objects.chb_ship_mod_srp},
        {SHIP_SS, objects.chb_ship_mod_ss},
        {SHIP_TSU, objects.chb_ship_mod_tsu},
        {SHIP_RLS, objects.chb_ship_mod_rls},
        {SHIP_DT, objects.chb_ship_mod_dt}};

    if (restore)
    {
        int16_t shipModules = getConfigBig(CFG_KEY_SHIPMODULES, 0);

        for (uint8_t c = 0; c < MAX_SHIP_MODULES; c++)
        {
            shipModule item = list[c];

            if (shipModules >= item.value)
            {
                shipModules -= item.value;

                lv_obj_add_state(item.checkbox, LV_STATE_CHECKED);
            }
            else
            {
                lv_obj_clear_state(item.checkbox, LV_STATE_CHECKED);
            }
        }
    }
    else
    {
        uint16_t shipModules = 0;

        for (uint8_t c = 0; c < MAX_SHIP_MODULES; c++)
        {
            shipModule item = list[c];

            if (lv_obj_has_state(item.checkbox, LV_STATE_CHECKED))
            {
                shipModules += item.value;
            }
        }

        setConfigBig(CFG_KEY_SHIPMODULES, shipModules);
    }
}

esp_err_t openConfig()
{
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &nvsConfig);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_CFG, "Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
        return ret;
    }

    return ret;
}

void closeConfig()
{
    nvs_close(nvsConfig);
}

// Load complete configuration from NVS
void loadConfig()
{
    if (openConfig() != ESP_OK)
    {
        return;
    }

    uint8_t delay = getConfig(CFG_KEY_DELAY, 100);
    setDelay(delay, true);

    uint8_t rotation = getConfig(CFG_KEY_ROTATION, 100);
    setRotation(rotation, true);

    uint8_t screen_brightness = getConfig(CFG_KEY_BRIGHTNESS, 50);
    setBrightness(screen_brightness, true);

    uint8_t sound_muted = getConfig(CFG_KEY_MUTED, 0);
    setMuted(sound_muted == 1, true);

    uint8_t sound_volume = getConfig(CFG_KEY_VOLUME, 100);
    setVolume(sound_volume, true);

    uint8_t keymap_index = getConfig(CFG_KEY_KEYMAP, 0);
    setKeymap(keymap_index, true);

    uint8_t auto_complete = getConfig(CFG_KEY_AUTOCOMPLETE, 0);
    setAutoComplete(auto_complete == 1, true);

    uint8_t game_after_preset = getConfig(CFG_KEY_GAMEAFTERPRESET, 0);
    setGameAfterPreset(game_after_preset == 1, true);

    uint8_t show_cooldown = getConfig(CFG_KEY_COOLDOWN, 0);
    setCooldown(show_cooldown == 1, true);

    setShipModules(true);

    closeConfig();
}

void initConnection()
{
    if (openConfig() != ESP_OK)
    {
        return;
    }

    uint8_t connectivity_index = getConfig(CFG_KEY_CONNECTIVITY, 0);
    setConnectivity(connectivity_index, true);

    closeConfig();
}

void deinitConnection()
{
    switch (connectionType)
    {
    case CT_BLUETOOTH:
        // Deinit Bluetooth controller
        ble_controller_deinit();
        break;
    case CT_USB:
        // Deinit USB controller
        usb_controller_deinit();
        break;
    default:
        break;
    }
}

// Load single configuration of a key/value from NVS
int8_t peekConfig(char *key, int8_t defaultValue)
{
    if (openConfig() != ESP_OK)
    {
        return defaultValue;
    }

    int8_t value = getConfig(key, defaultValue);

    closeConfig();

    return value;
}

// Clear all stored configuration in NVS and write it to default values
void resetConfig()
{
    if (openConfig() != ESP_OK)
    {
        return;
    }

    nvs_erase_all(nvsConfig);

    closeConfig();

    setDelay(100, true);
    setBrightness(50, true);
    setMuted(0, true);
    setKeymap(0, true);
    setAutoComplete(1, true);
    setGameAfterPreset(1, true);
    setCooldown(0, true);
    setShipModules(true);

    setRotation(LV_DISP_ROT_90, true);
}