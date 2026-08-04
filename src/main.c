#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include <esp_system.h>
#include <ui/ui.h>
#include <ui/vars.h>
#include <ui/screens.h>
#include <ui/actions.h>
#include <ui/images.h>
#include <ui/ui_events.h>
#include <ui/ui_post.h>
#include "hid_dev.h"
#include "i2s_player.h"
#include "ble/ble_controller.h"
#include "usb/usb_controller.h"
#include "configuration.h"
#include "keymaps.h"
#include "main.h"

// Connection type
uint8_t connectionType = CT_NONE;

// Flag for ready state of LVGL after init
bool lvglReady = false;

// Stratagem code sequence (buffer) for execution
uint8_t stratagemCode[MAX_CMD_LENGTH];
// Stratagem mask for modifier key combination (ctrl, alt, etc.)
uint8_t stratagemMask;

// Flag for sound playback state
bool soundPlayback = false;
// Path to sound file which should be played
char *soundFile;
// Flag for muting sound playback
bool playerMuted;
// Set true to abort the currently-playing sound (so a new press's sound starts immediately)
volatile bool soundInterrupt = false;
// Software master volume, 0..256 (256 = full). Set from the config slider, applied in the player.
volatile uint16_t audioVolume = 256;

// Delay for HID input execution in milliseconds (default: 100)
int inputDelay = 100;

bool manualAutoComplete = true;
bool showCooldowns = false;
bool gameAfterPreset = false;

// Rotation of screen (default: 90)
int screenRotation = LV_DISP_ROT_90;

extern lv_obj_t *cooldownLabels[MAX_USER_STRATAGEMS];
extern uint32_t cooldownValues[MAX_USER_STRATAGEMS];
uint16_t lastCooldownDiffs[MAX_USER_STRATAGEMS];

lv_timer_t *cooldownTimer;

// Set stratagem code sequence which should be executed
// sequence - keycode buffer
// mask - modifier keys
// plain - resolve via keymap or send directly (raw)
void setStratagemCode(uint8_t sequence[MAX_CMD_LENGTH], uint8_t mask, bool plain)
{
  switch (connectionType)
  {
  case CT_BLUETOOTH:
    if (!ble_connected())
    {
      return;
    }
    break;
  case CT_USB:
    if (!usb_connected())
    {
      return;
    }
    break;
  default:
    return;
  }

  uint8_t sequenceLength = 0;

  for (uint8_t c = 0; c < MAX_CMD_LENGTH; c++)
  {
    if (sequence[c] > 0)
    {
      uint8_t rawCode = sequence[c];
      stratagemCode[c] = plain ? rawCode : LookupKeycode(rawCode);
      sequenceLength++;
    }
    else
    {
      break;
    }
  }

  stratagemMask = mask;
}

// Playback sound file from specified path
// path - path to the sound file
void playbackSound(char *path)
{
  soundFile = path;
  soundInterrupt = true; // cut any sound currently playing so this one starts right away
  soundPlayback = true;
}

// Dim the screen to a specific value
// brightness - A brightness value between 0 and 100 (percent)
void dimScreen(int brightness)
{
  bsp_display_brightness_set(brightness);
}

// Update UI relating to connection state
void updateConnection()
{
  if (!lvglReady)
  {
    return;
  }

  lv_img_dsc_t *imgConnection;

  switch (connectionType)
  {
  case CT_BLUETOOTH:
    // Check bluetooth connection state
    if (ble_connected())
    {
      imgConnection = (lv_img_dsc_t *)&img_btcon;
    }
    else
    {
      imgConnection = (lv_img_dsc_t *)&img_btdis;
    }
    break;
  case CT_USB:
    // Check USB connection state
    if (usb_connected())
    {
      imgConnection = (lv_img_dsc_t *)&img_us_bcon;
    }
    else
    {
      imgConnection = (lv_img_dsc_t *)&img_us_bdis;
    }
    break;
  default:
    return;
  }

  lv_obj_set_style_bg_img_src(objects.img_connection1, imgConnection, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_img_src(objects.img_connection2, imgConnection, LV_PART_MAIN | LV_STATE_DEFAULT);
}

// Delay for checking if a the stratagem execution buffer is filled
#define INPUT_CHECK_DELAY 5

// Manual "arm" mode: the UI task asks the HID task to hold the stratagem modifier (Ctrl) down —
// opening the in-game stratagem menu — or to release it (closing/throwing). Routing it through the
// HID task keeps every HID report on one task, so it can never race the sequence burst below.
// PULSE = release then re-hold, i.e. reset the in-game menu (used when a wrong input clears a
// partially-entered code so the pad and the game stay in sync).
enum { HID_ARM_NONE = 0, HID_ARM_HOLD, HID_ARM_RELEASE, HID_ARM_PULSE };
static volatile int hidArmRequest = HID_ARM_NONE;
static volatile uint8_t hidArmMask = 0;

// Live-key ring buffer: individual keystrokes sent while armed (modifier held) so the in-game
// stratagem menu builds as the user taps, instead of one burst at the very end.
#define HID_LIVE_QUEUE_LEN 16
static volatile uint8_t hidLiveQueue[HID_LIVE_QUEUE_LEN];
static volatile uint8_t hidLiveHead = 0;
static volatile uint8_t hidLiveTail = 0;

void hidHoldModifier(uint8_t mask)
{
  hidArmMask = mask;
  hidArmRequest = HID_ARM_HOLD;
}

void hidReleaseModifier(void)
{
  hidArmRequest = HID_ARM_RELEASE;
}

void hidPulseModifier(uint8_t mask)
{
  hidArmMask = mask;
  hidArmRequest = HID_ARM_PULSE;
}

void hidSendLiveKey(uint8_t mask, uint8_t keycode)
{
  hidArmMask = mask; // the modifier is held across live keys
  uint8_t next = (hidLiveTail + 1) % HID_LIVE_QUEUE_LEN;
  if (next == hidLiveHead)
  {
    return; // queue full (never at human tap speed) — drop
  }
  hidLiveQueue[hidLiveTail] = keycode;
  hidLiveTail = next;
}

// Task for exeuction of HID inputs
void hid_input_task(void *pvParameters)
{
  while (1)
  {
    vTaskDelay(INPUT_CHECK_DELAY / portTICK_PERIOD_MS);

    // Resolve the active HID sender once per tick; skip the tick if nothing is connected (do NOT
    // return — that would kill the task permanently and no input would ever send again).
    void (*fptr)(unsigned char, unsigned char, unsigned char);

    switch (connectionType)
    {
    case CT_BLUETOOTH:
      fptr = &ble_keyboard_send;
      break;
    case CT_USB:
      fptr = &usb_keyboard_send;
      break;
    default:
      // Nothing connected: drop pending requests so nothing fires stale on reconnect.
      hidArmRequest = HID_ARM_NONE;
      hidLiveHead = hidLiveTail;
      continue;
    }

    double inputDelayPeriod = inputDelay / portTICK_PERIOD_MS;

    // Drain live keystrokes first (modifier stays held via hidArmMask), so a key tapped just before
    // an arm request is delivered ahead of the hold/release/pulse below.
    while (hidLiveHead != hidLiveTail)
    {
      uint8_t key = hidLiveQueue[hidLiveHead];
      hidLiveHead = (hidLiveHead + 1) % HID_LIVE_QUEUE_LEN;

      fptr(hidArmMask, key, 1);
      vTaskDelay(inputDelayPeriod);
      fptr(hidArmMask, key, 0); // mask kept — the modifier stays held
      vTaskDelay(inputDelayPeriod);
    }

    // Service a pending arm request: hold the modifier (open menu), release it (close/throw), or
    // pulse it (release then re-hold to reset the menu after a wrong input).
    if (hidArmRequest != HID_ARM_NONE)
    {
      if (hidArmRequest == HID_ARM_PULSE)
      {
        fptr(0, 0, 0);
        vTaskDelay(inputDelayPeriod);
        fptr(hidArmMask, 0, 0);
      }
      else
      {
        fptr(hidArmRequest == HID_ARM_HOLD ? hidArmMask : 0, 0, 0);
      }
      hidArmRequest = HID_ARM_NONE;
    }

    if (stratagemCode[0] > 0)
    {
      uint8_t cmdIndex = 0;

      // Press the modifier (Ctrl) first, then walk the sequence pressing/releasing each key.
      fptr(stratagemMask, 0, 0);
      vTaskDelay(inputDelayPeriod);

      while (stratagemCode[cmdIndex] > 0 && cmdIndex < MAX_CMD_LENGTH)
      {
        fptr(stratagemMask, stratagemCode[cmdIndex], 1);
        vTaskDelay(inputDelayPeriod);
        fptr(stratagemMask, stratagemCode[cmdIndex], 0);
        vTaskDelay(inputDelayPeriod);

        stratagemCode[cmdIndex] = 0;
        cmdIndex++;
      }

      // Release everything (this final Ctrl-up is what throws the stratagem — and, in arm mode,
      // is the auto-disarm after a completed code).
      fptr(0, 0, 0);
    }
  }
}

// Dedicated audio task: plays queued sounds immediately (low latency), and because playbackSound
// sets soundInterrupt, a new press cuts the current sound so rapid inputs produce rapid beeps.
void audio_task(void *pvParameters)
{
  while (1)
  {
    if (soundPlayback && lvglReady)
    {
      soundPlayback = false;
      soundInterrupt = false;

      if (!playerMuted)
      {
        play_wav(soundFile);
      }
    }
    else
    {
      vTaskDelay(2 / portTICK_PERIOD_MS);
    }
  }
}

// Task for exeuction of EEZ Flows
void flow_tick_task()
{
  ui_tick();
}

void ui_update_task()
{
  if (!lvglReady)
  {
    return;
  }

  bool activeTimer = false;

  for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
  {
    uint32_t cooldownValue = cooldownValues[c];
    lv_obj_t *cooldownLabel = cooldownLabels[c];
    int16_t diff = cooldownValue - getNow();

    if (cooldownValue > 0 && diff > 0)
    {
      activeTimer = true;

      if (lastCooldownDiffs[c] == diff)
      {
        continue;
      }

      lastCooldownDiffs[c] = diff;

      uint8_t min = 0;
      uint16_t sec = diff;

      while (sec >= 60)
      {
        min++;
        sec -= 60;
      }

      char *textCooldown = (char *)malloc(8 * sizeof(char));
      sprintf(textCooldown, "%d:%02d", min, sec);

      lv_label_set_text(cooldownLabel, (void *)textCooldown);

      if (lv_obj_has_flag(cooldownLabel, LV_OBJ_FLAG_HIDDEN))
      {
        lv_obj_clear_flag(cooldownLabel, LV_OBJ_FLAG_HIDDEN);
      }
    }
    else
    {
      cooldownValues[c] = 0;

      if (!lv_obj_has_flag(cooldownLabel, LV_OBJ_FLAG_HIDDEN))
      {
        lv_obj_add_flag(cooldownLabel, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  if(!activeTimer && !cooldownTimer->paused){
    lv_timer_pause(cooldownTimer);
  }
}

// App main function
void app_main()
{
  // Init and load config from NVS storage
  initConfig();

  // Setup HID input task (async)
  xTaskCreatePinnedToCore(&hid_input_task, "hid_input_task", 2048, NULL, 5, NULL, 0);

  // Dedicated audio task so sound playback never blocks HID sends and fires with minimal latency
  xTaskCreatePinnedToCore(&audio_task, "audio_task", 4096, NULL, 5, NULL, 1);

  // Resolve screen rotation from config
  screenRotation = peekConfig("rotation", LV_DISP_ROT_90);

  // Display configuration
  bsp_display_cfg_t cfg = {
      .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
      .buffer_size = HD2MP_LCD_QSPI_H_RES * HD2MP_LCD_QSPI_V_RES,
      .rotate = screenRotation,
  };

  // Init display
  bsp_display_start_with_config(&cfg);
  // Turn of display backlight
  bsp_display_backlight_off();

  // Lock the mutex due to the LVGL APIs are not thread-safe
  bsp_display_lock(0);

  // Start LVGL
  ui_init();
  // UI post processing
  ui_post();

  // Release the mutex
  bsp_display_unlock();

  vTaskDelay(200 / portTICK_PERIOD_MS);

  // Turn on display backlight
  bsp_display_backlight_on();

  // Setup cooldown timer
  cooldownTimer = lv_timer_create(ui_update_task, 1000, NULL);
  cooldownTimer->repeat_count = -1;
  lv_timer_pause(cooldownTimer);

  // Read config
  loadConfig();

  lvglReady = true;

  // Preload the rapid-tap arrow SFX into PSRAM so each press plays instantly (no per-sound SD open,
  // which was the startup latency that made fast presses feel like they queued up).
  preload_wav(SND_ARR_UP);
  preload_wav(SND_ARR_DOWN);
  preload_wav(SND_ARR_LEFT);
  preload_wav(SND_ARR_RIGHT);

  // Playback intro sound
  playbackSound(SND_INTRO);

  updateConnection();
  updateStratagemSelection();
  updatePresets();

  // Setup timer for EEZ Flow ui tick
  lv_timer_t *flowTickTimer = lv_timer_create(flow_tick_task, 10, NULL);
  flowTickTimer->repeat_count = -1;
}