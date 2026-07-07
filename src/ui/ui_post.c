#include "main.h"
#include "screens.h"
#include "ui.h"
#include "ui_events.h"
#include "version.h"
#include "configuration.h"

extern lv_obj_t *cooldownLabels[MAX_USER_STRATAGEMS];

// The manual back button (obj71) navigates via EEZ flow with no sound of its own; give it a close cue.
static void manual_back_sound_cb(lv_event_t *e)
{
  (void)e;
  playbackSound(SND_LOADOUT_CLOSE);
}

// The config/settings screen is entered (gear, obj20) and left (back, obj59) via EEZ flow, which
// plays no sound. Give them the same open/close cues the other screens use.
static void config_open_sound_cb(lv_event_t *e)
{
  (void)e;
  playbackSound(SND_LOADOUT_OPEN);
}

static void config_close_sound_cb(lv_event_t *e)
{
  (void)e;
  playbackSound(SND_LOADOUT_CLOSE);
}

// Leaving the manual screen while armed would leave Ctrl held down on the host — force a disarm.
static void manual_unload_disarm_cb(lv_event_t *e)
{
  (void)e;
  manualForceDisarm();
}

void ui_post()
{
  lv_obj_t *tabsBtnsList[] = {
      lv_tabview_get_tab_btns(objects.tab_view_setup),
      lv_tabview_get_tab_btns(objects.tab_view_config)};

  lv_color_t itemColorActive = lv_color_hex(colorActive);

  for (uint8_t c = 0; c < 2; c++)
  {
    lv_obj_t *tabBtnsItem = tabsBtnsList[c];

    lv_obj_set_style_border_color(tabBtnsItem, itemColorActive, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tabBtnsItem, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tabBtnsItem, itemColorActive, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tabBtnsItem, lv_color_hex(0x999999), LV_PART_ITEMS | LV_STATE_CHECKED);
  }

  lv_obj_t *dropDownList[] = {
      lv_dropdown_get_list(objects.dd_connectivity),
      lv_dropdown_get_list(objects.dd_keymap)};

  for (uint8_t c = 0; c < 2; c++)
  {
    lv_obj_t *dropDownItem = dropDownList[c];

    lv_obj_set_style_text_color(dropDownItem, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(dropDownItem, itemColorActive, LV_PART_SELECTED | LV_STATE_CHECKED);
  }

  cooldownLabels[0] = objects.label_cooldown1;
  cooldownLabels[1] = objects.label_cooldown2;
  cooldownLabels[2] = objects.label_cooldown3;
  cooldownLabels[3] = objects.label_cooldown4;
  cooldownLabels[4] = objects.label_cooldown5;
  cooldownLabels[5] = objects.label_cooldown6;

  resetAllCooldowns();

  // Update software version in UI
  lv_label_set_text(objects.lbl_version, SW_VER);

  // Make the manual arrow buttons render-free on press: strip the pressed-state style (which dims
  // the icon to 50% opacity, forcing a full-frame flush on every press AND release). With it gone,
  // tapping an arrow changes nothing on screen, so rapid stratagem input isn't throttled by the
  // display flush — each tap is just a sound + a cheap match.
  lv_obj_remove_style(objects.manual_arrow_up, NULL, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_remove_style(objects.manual_arrow_down, NULL, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_remove_style(objects.manual_arrow_left, NULL, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_remove_style(objects.manual_arrow_right, NULL, LV_PART_MAIN | LV_STATE_PRESSED);

  // Build the loadout/utility view-toggle buttons on the manual screen (bottom, by the back arrow).
  initManualViewToggle();

  // Build the arm/disarm toggle in the centre of the manual d-pad (holds Ctrl to open the in-game
  // stratagem menu; arrows are locked until armed).
  initManualArmButton();

  // Build the full-screen "REQUEST RECEIVED" call-in reveal (top layer, shown when a stratagem fires).
  initCallInScreen();

  // Add a selectable Hellbomb button to the Ground tab (not part of the generated selection UI).
  initHellbombButton();

  // Add the sound-volume slider to the config screen (not part of the generated UI).
  initVolumeControl();

  // Stop the manual screen (and the arrows' container) from running scroll-detection on every press.
  // Nothing there needs to scroll (it all fits), and the scroll interaction was emitting a stray
  // second click after the tap — the arrow double-click that a time-based debounce couldn't catch.
  lv_obj_clear_flag(objects.manual, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(lv_obj_get_parent(objects.manual_arrow_up), LV_OBJ_FLAG_SCROLLABLE);

  // Give the manual back button its close cue (its flow navigation plays nothing on its own).
  lv_obj_add_event_cb(objects.obj71, manual_back_sound_cb, LV_EVENT_CLICKED, NULL);

  // Same for the settings screen's gear (enter) and back (exit) buttons.
  lv_obj_add_event_cb(objects.obj20, config_open_sound_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(objects.obj59, config_close_sound_cb, LV_EVENT_CLICKED, NULL);

  // Safety: if the manual screen is left while armed, release the held Ctrl so it never sticks.
  lv_obj_add_event_cb(objects.manual, manual_unload_disarm_cb, LV_EVENT_SCREEN_UNLOAD_START, NULL);
}