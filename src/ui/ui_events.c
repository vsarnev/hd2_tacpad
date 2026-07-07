// Helldivers 2 tacpad — UI event handlers, manual stratagem input, voice callouts, arm mode.
// Originally scaffolded by SquareLine Studio (LVGL 8.3.11); now substantially hand-written.

#include "ui.h"
#include "ui_events.h"
#include "screens.h"
#include "styles.h"
#include "hid_dev.h"
#include "esp_log.h"
#include "stratagems.h"
#include "i2s_player.h"
#include "main.h"
#include "keymaps.h"
#include "stratagem_names.h"
#include "esp_random.h"
#include <string.h>
#include "ui_post.h"
#include "configuration.h"
#include "actions.h"
#include "ui_assignment.h"
#include "esp_timer.h"

// User button list
lv_obj_t *gameButtons[MAX_USER_STRATAGEMS];
// Stratagem list index of user buttons
int indices[MAX_USER_STRATAGEMS];
// Stratagem list index of user buttons
int types[MAX_USER_STRATAGEMS];
// Amount of user assigned stratagems
uint8_t strategemsAmount = 0;

lv_obj_t *cooldownLabels[MAX_USER_STRATAGEMS];
uint32_t cooldownValues[MAX_USER_STRATAGEMS];
extern lv_timer_t *cooldownTimer;
int cooldownResetIndex = -1;

int manualIndex = 0;
int manualList = 0;
int manualSequence[MAX_CMD_LENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int manualMatch = -1;
// True after a match pass if the entered sequence is still the start (prefix) of at least one valid
// code. When it goes false, the user has entered something that matches nothing — an error.
bool manualPrefixValid = false;
extern bool manualAutoComplete;
extern bool showCooldowns;
bool userSGautoComplete = false;
lv_timer_t *timerManual = NULL;

// Manual-screen view toggle: false = show the loadout codes, true = show the utility codes. This
// only changes what's displayed/highlighted; both loadout and utilities stay callable either way.
bool manualUtilityView = false;
// The always-available utilities are the first entries of strategemBaseList: Reinforce, Resupply,
// SOS, Eagle Rearm.
#define MANUAL_UTILITY_COUNT 4
// Segmented view-toggle buttons (created on the manual screen in initManualViewToggle). Positioned
// in the bottom strip just left of the 76px back arrow (which sits at BOTTOM_RIGHT, -2 offset).
static lv_obj_t *manualViewBtnLoadout = NULL;
static lv_obj_t *manualViewBtnUtility = NULL;
#define MANUAL_VIEW_BTN_W 74
#define MANUAL_VIEW_BTN_H 38
#define MANUAL_VIEW_UTIL_X (-82)     // right edge 4px left of the back button's left edge
#define MANUAL_VIEW_LOADOUT_X (-160) // right edge 4px left of the utility button's left edge
static void manualSetViewToggleVisible(bool visible);
static void manualUpdateViewToggle();
static const char *pickVoiceLine(const char *name, const char *soundPath, int list, int matchIndex);

// Manual "arm" toggle, in the empty centre of the d-pad. Arming holds Ctrl down (opening the in-game
// stratagem menu) and unlocks the 4 arrows; a completed code fires and auto-disarms (its own Ctrl-up
// throws), and disarming by hand releases Ctrl. Arrows are inert until armed. See main.c
// hidHoldModifier / hidReleaseModifier.
bool manualArmed = false;
static lv_obj_t *manualArmBtn = NULL;
static lv_obj_t *manualArmLabel = NULL;
static void manualSetArrowsEnabled(bool enabled);
static void manualUpdateArmButton(void);
static void manualSetArmed(bool armed, bool sendHid, bool playSound);

lv_timer_t *timerMsg = NULL;
bool presetImageMode = false;
char presetKey[3] = "p0i";
extern bool gameAfterPreset;

const lv_img_dsc_t *presetImageList[] = {
	&img_icon1,
	&img_icon2,
	&img_icon3,
	&img_icon4,
	&img_icon5,
	&img_icon6,
	&img_mission_conduct_geological_survey,
	&img_mission_emergency_evacuation,
	&img_mission_enable_e_710_extraction,
	&img_mission_eradicate_automaton_forces,
	&img_mission_eradicate_terminid_swarm,
	&img_mission_evacuate_high_value_assets,
	&img_mission_launch_icbm,
	&img_mission_retrieve_essential_personnel,
	&img_mission_retrieve_valuable_data,
	&img_mission_spread_democracy,
	&img_mission_terminate_illegal_broadcast,
	&img_mission_upload_escape_pod_data,
	&img_faction_terminid,
	&img_faction_automaton,
	&img_faction_illuminate};

const lv_img_dsc_t *manualArrowList[] = {
	&img_stratagem_arrow_up1,
	&img_stratagem_arrow_down1,
	&img_stratagem_arrow_left1,
	&img_stratagem_arrow_right1};

// HID input mask for special keys
#define INPUT_CTRL_MASK 1 // 1 CTRL left

void initGame()
{
	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		const bool configured = gameButtons[c] != NULL;
		lv_obj_t *targetButton = objects.custom_stratagem1;

		bool useHiResIcon = true;

		switch (c)
		{
		case 1:
			targetButton = objects.custom_stratagem2;
			break;
		case 2:
			targetButton = objects.custom_stratagem3;
			break;
		case 3:
			targetButton = objects.custom_stratagem4;
			break;
		case 4:
			targetButton = objects.custom_stratagem5;
			useHiResIcon = false;
			break;
		case 5:
			targetButton = objects.custom_stratagem6;
			useHiResIcon = false;
			break;
		}

		uint8_t itemIndex = indices[c];
		stratagemItem item = strategemItemList[itemIndex];
		const lv_img_dsc_t *imgSrc = useHiResIcon ? item.imgHiRes : item.imgMeRes;

		if (configured)
		{
			lv_obj_set_style_border_color(targetButton, lv_color_hex(item.color), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_bg_img_src(targetButton, imgSrc, LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_clear_flag(targetButton, LV_OBJ_FLAG_HIDDEN);
		}
		else
		{
			lv_obj_add_flag(targetButton, LV_OBJ_FLAG_HIDDEN);
		}
	}
}

void getManualCmdSeq(uint8_t index, lv_obj_t *targetCmdSeq[])
{
	size_t arrSize = sizeof(lv_obj_t *) * MAX_CMD_LENGTH;

	switch (index)
	{
	case 0:
		memcpy(targetCmdSeq, (lv_obj_t *[]){objects.seq_cmd1_1, objects.seq_cmd1_2, objects.seq_cmd1_3, objects.seq_cmd1_4, objects.seq_cmd1_5, objects.seq_cmd1_6, objects.seq_cmd1_7, objects.seq_cmd1_8, objects.seq_cmd1_9}, arrSize);
		break;
	case 1:
		memcpy(targetCmdSeq, (lv_obj_t *[]){objects.seq_cmd2_1, objects.seq_cmd2_2, objects.seq_cmd2_3, objects.seq_cmd2_4, objects.seq_cmd2_5, objects.seq_cmd2_6, objects.seq_cmd2_7, objects.seq_cmd2_8, objects.seq_cmd2_9}, arrSize);
		break;
	case 2:
		memcpy(targetCmdSeq, (lv_obj_t *[]){objects.seq_cmd3_1, objects.seq_cmd3_2, objects.seq_cmd3_3, objects.seq_cmd3_4, objects.seq_cmd3_5, objects.seq_cmd3_6, objects.seq_cmd3_7, objects.seq_cmd3_8, objects.seq_cmd3_9}, arrSize);
		break;
	case 3:
		memcpy(targetCmdSeq, (lv_obj_t *[]){objects.seq_cmd4_1, objects.seq_cmd4_2, objects.seq_cmd4_3, objects.seq_cmd4_4, objects.seq_cmd4_5, objects.seq_cmd4_6, objects.seq_cmd4_7, objects.seq_cmd4_8, objects.seq_cmd4_9}, arrSize);
		break;
	case 4:
		memcpy(targetCmdSeq, (lv_obj_t *[]){objects.seq_cmd5_1, objects.seq_cmd5_2, objects.seq_cmd5_3, objects.seq_cmd5_4, objects.seq_cmd5_5, objects.seq_cmd5_6, objects.seq_cmd5_7, objects.seq_cmd5_8, objects.seq_cmd5_9}, arrSize);
		break;
	case 5:
		memcpy(targetCmdSeq, (lv_obj_t *[]){objects.seq_cmd6_1, objects.seq_cmd6_2, objects.seq_cmd6_3, objects.seq_cmd6_4, objects.seq_cmd6_5, objects.seq_cmd6_6, objects.seq_cmd6_7, objects.seq_cmd6_8, objects.seq_cmd6_9}, arrSize);
		break;
	}
}

void getManualImages(lv_obj_t *manualImages[])
{
	size_t arrSize = sizeof(lv_obj_t *) * MAX_USER_STRATAGEMS;

	memcpy(manualImages, (lv_obj_t *[]){objects.seq_ico1, objects.seq_ico2, objects.seq_ico3, objects.seq_ico4, objects.seq_ico5, objects.seq_ico6}, arrSize);
}

// Goto game screen
void action_goto_game(lv_event_t *e)
{
	// Leaving the manual screen for the game screen is the manual "back" (closing); every other route
	// to the game screen (from setup, a preset, or auto-nav on a full loadout) is opening it.
	playbackSound(lv_scr_act() == objects.manual ? SND_LOADOUT_CLOSE : SND_LOADOUT_OPEN);
	initGame();
}

// Fill the six on-screen code slots from either the loadout (utility=false) or the always-available
// utilities (utility=true). Reused by the manual-screen setup and the loadout/utility view toggle.
void populateManualDisplay(bool utility)
{
	const lv_obj_t *manualSeqs[] = {
		objects.cnt_usr_stg_seq1,
		objects.cnt_usr_stg_seq2,
		objects.cnt_usr_stg_seq3,
		objects.cnt_usr_stg_seq4,
		objects.cnt_usr_stg_seq5,
		objects.cnt_usr_stg_seq6};

	const lv_obj_t *manualImages[MAX_USER_STRATAGEMS];
	getManualImages(manualImages);

	for (uint8_t c1 = 0; c1 < MAX_USER_STRATAGEMS; c1++)
	{
		lv_obj_t *cntSeq = manualSeqs[c1];
		lv_obj_t *imgSeq = manualImages[c1];

		const uint8_t *seq = NULL;
		const lv_img_dsc_t *icon = NULL;
		bool populated;

		if (utility)
		{
			// strategemBaseList stores the 96px hi-res icon; the manual slots are 48px and EEZ renders
			// at native scale, so use the matching 48px lo-res variant (same as the loadout items' _3
			// icons) — no zoom needed.
			static const lv_img_dsc_t *const utilityIcons[MANUAL_UTILITY_COUNT] = {
				&img_rf3, &img_res3, &img_sos3, &img_er3};

			populated = c1 < MANUAL_UTILITY_COUNT;
			if (populated)
			{
				seq = strategemBaseList[c1].sequence;
				icon = utilityIcons[c1];
			}
		}
		else
		{
			populated = gameButtons[c1] != NULL;
			if (populated)
			{
				const uint8_t itemIndex = indices[c1];
				seq = strategemItemList[itemIndex].sequence;
				icon = strategemItemList[itemIndex].imgLoRes;
			}
		}

		if (!populated)
		{
			lv_obj_add_flag(cntSeq, LV_OBJ_FLAG_HIDDEN);
			continue;
		}

		lv_img_set_src(imgSeq, icon);
		lv_obj_clear_flag(cntSeq, LV_OBJ_FLAG_HIDDEN);

		lv_obj_t *targetCmdSeq[MAX_CMD_LENGTH];
		getManualCmdSeq(c1, targetCmdSeq);

		for (uint8_t c2 = 0; c2 < MAX_CMD_LENGTH; c2++)
		{
			const uint8_t dirCmd = seq[c2];
			lv_obj_t *seqArrowImg = targetCmdSeq[c2];

			if (dirCmd == 0)
			{
				lv_obj_add_flag(seqArrowImg, LV_OBJ_FLAG_HIDDEN);
			}
			else
			{
				const lv_img_dsc_t *arrowImg = &img_stratagem_arrow_up3;

				switch (dirCmd)
				{
				case INPUT_DOWN:
					arrowImg = &img_stratagem_arrow_down3;
					break;
				case INPUT_LEFT:
					arrowImg = &img_stratagem_arrow_left3;
					break;
				case INPUT_RIGHT:
					arrowImg = &img_stratagem_arrow_right3;
					break;
				}

				lv_img_set_src(seqArrowImg, arrowImg);
				lv_obj_clear_flag(seqArrowImg, LV_OBJ_FLAG_HIDDEN);
			}
		}
	}
}

void action_goto_manual(lv_event_t *e)
{
	playbackSound(SND_LOADOUT_OPEN);

	// Always land on the loadout view when opening the manual screen.
	manualUtilityView = false;

	uint8_t configureCount = 0;
	for (uint8_t c1 = 0; c1 < MAX_USER_STRATAGEMS; c1++)
	{
		if (gameButtons[c1] != NULL)
		{
			configureCount++;
		}
	}

	populateManualDisplay(false);

	if (configureCount > 0)
	{
		userSGautoComplete = true;

		lv_obj_add_flag(objects.manual_preview_item, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(objects.input_seq, LV_OBJ_FLAG_HIDDEN);

		lv_obj_clear_flag(objects.cnt_usr_stg_lst, LV_OBJ_FLAG_HIDDEN);

		// The loadout/utility toggle only makes sense with a loadout to switch away from.
		manualSetViewToggleVisible(true);
		manualUpdateViewToggle();
	}
	else
	{
		userSGautoComplete = false;

		lv_obj_clear_flag(objects.manual_preview_item, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(objects.input_seq, LV_OBJ_FLAG_HIDDEN);

		lv_obj_add_flag(objects.cnt_usr_stg_lst, LV_OBJ_FLAG_HIDDEN);

		manualSetViewToggleVisible(false);
	}
}

// --- Manual-screen loadout/utility view toggle ---

static void manualUpdateViewToggle()
{
	if (manualViewBtnLoadout == NULL)
	{
		return;
	}

	// Active view: filled Helldivers yellow with black text. Inactive: black with white text (the
	// white outline from add_style_button_std stays, matching the back button's look).
	const lv_color_t activeBg = lv_color_hex(colorActive);
	const lv_color_t inactiveBg = lv_color_hex(0x000000);
	const lv_color_t activeText = lv_color_hex(0x000000);
	const lv_color_t inactiveText = lv_color_hex(0xffffff);

	lv_obj_set_style_bg_color(manualViewBtnLoadout, manualUtilityView ? inactiveBg : activeBg, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(manualViewBtnLoadout, manualUtilityView ? inactiveText : activeText, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(manualViewBtnUtility, manualUtilityView ? activeBg : inactiveBg, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(manualViewBtnUtility, manualUtilityView ? activeText : inactiveText, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void manualSetViewToggleVisible(bool visible)
{
	if (manualViewBtnLoadout == NULL)
	{
		return;
	}

	if (visible)
	{
		lv_obj_clear_flag(manualViewBtnLoadout, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(manualViewBtnUtility, LV_OBJ_FLAG_HIDDEN);
	}
	else
	{
		lv_obj_add_flag(manualViewBtnLoadout, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(manualViewBtnUtility, LV_OBJ_FLAG_HIDDEN);
	}
}

static void manualSetView(bool utility)
{
	manualUtilityView = utility;

	// Drop any half-entered code when switching views.
	if (timerManual != NULL)
	{
		lv_timer_del(timerManual);
		timerManual = NULL;
	}
	manualIndex = 0;
	manualMatch = -1;
	for (uint8_t c = 0; c < MAX_CMD_LENGTH; c++)
	{
		manualSequence[c] = 0;
	}

	populateManualDisplay(utility);
	resetAllManSeqs();
	manualUpdateViewToggle();
}

static void action_manual_view_loadout(lv_event_t *e)
{
	playbackSound(SND_CLICK);
	manualSetView(false);
}

static void action_manual_view_utility(lv_event_t *e)
{
	playbackSound(SND_CLICK);
	manualSetView(true);
}

void initManualViewToggle()
{
	// Two small segmented buttons in the bottom strip, just left of the back arrow, switching the
	// manual screen between the loadout codes and the utility codes. Hidden until a loadout exists.
	manualViewBtnUtility = lv_btn_create(objects.manual);
	lv_obj_set_size(manualViewBtnUtility, MANUAL_VIEW_BTN_W, MANUAL_VIEW_BTN_H);
	lv_obj_align(manualViewBtnUtility, LV_ALIGN_BOTTOM_RIGHT, MANUAL_VIEW_UTIL_X, -2);
	add_style_button_std(manualViewBtnUtility); // black bg, white outline, radius 8 (like the back arrow)
	lv_obj_set_style_pad_all(manualViewBtnUtility, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_add_event_cb(manualViewBtnUtility, action_manual_view_utility, LV_EVENT_CLICKED, NULL);
	lv_obj_t *lblU = lv_label_create(manualViewBtnUtility);
	lv_label_set_text(lblU, "UTILITY");
	lv_obj_set_style_text_font(lblU, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_center(lblU);

	manualViewBtnLoadout = lv_btn_create(objects.manual);
	lv_obj_set_size(manualViewBtnLoadout, MANUAL_VIEW_BTN_W, MANUAL_VIEW_BTN_H);
	lv_obj_align(manualViewBtnLoadout, LV_ALIGN_BOTTOM_RIGHT, MANUAL_VIEW_LOADOUT_X, -2);
	add_style_button_std(manualViewBtnLoadout);
	lv_obj_set_style_pad_all(manualViewBtnLoadout, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_add_event_cb(manualViewBtnLoadout, action_manual_view_loadout, LV_EVENT_CLICKED, NULL);
	lv_obj_t *lblL = lv_label_create(manualViewBtnLoadout);
	lv_label_set_text(lblL, "LOADOUT");
	lv_obj_set_style_text_font(lblL, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_center(lblL);

	lv_obj_add_flag(manualViewBtnUtility, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(manualViewBtnLoadout, LV_OBJ_FLAG_HIDDEN);

	manualUpdateViewToggle();
}

// Dim the 4 arrows when locked, full opacity when live.
static void manualSetArrowsEnabled(bool enabled)
{
	lv_opa_t opa = enabled ? LV_OPA_COVER : LV_OPA_40;
	lv_obj_set_style_opa(objects.manual_arrow_up, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_opa(objects.manual_arrow_down, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_opa(objects.manual_arrow_left, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_opa(objects.manual_arrow_right, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
}

// Reflect the arm state on the centre button: the up-chevron stays, its colour + the button fill
// flip (white-on-dark when locked, black-on-gold when live).
static void manualUpdateArmButton(void)
{
	if (manualArmBtn == NULL)
	{
		return;
	}

	lv_obj_set_style_bg_color(manualArmBtn, lv_color_hex(manualArmed ? colorActive : 0x000000),
							  LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(manualArmLabel, lv_color_hex(manualArmed ? 0x000000 : colorTheme),
								LV_PART_MAIN | LV_STATE_DEFAULT);
}

// Enter/leave the armed state. sendHid=false when a firing sequence will release Ctrl itself (the
// completion burst ends in a Ctrl-up), so we neither double-release nor race that burst.
static void manualSetArmed(bool armed, bool sendHid, bool playSound)
{
	manualArmed = armed;

	if (sendHid)
	{
		if (armed)
		{
			hidHoldModifier(INPUT_CTRL_MASK);
		}
		else
		{
			hidReleaseModifier();
		}
	}

	manualSetArrowsEnabled(armed);
	manualUpdateArmButton();

	if (playSound)
	{
		playbackSound(armed ? SND_STRAT_START : SND_STRAT_CLOSE);
	}

	// Leaving armed: drop any half-entered sequence so the next arm starts clean.
	if (!armed)
	{
		if (timerManual != NULL)
		{
			lv_timer_del(timerManual);
			timerManual = NULL;
		}

		manualMatch = -1;
		manualIndex = 0;

		for (uint8_t c = 0; c < MAX_CMD_LENGTH; c++)
		{
			manualSequence[c] = 0;
		}

		if (!userSGautoComplete)
		{
			updateManualSequence();
		}
		else
		{
			resetAllManSeqs();
		}
	}
}

// Centre-of-d-pad toggle handler.
static void action_manual_arm_toggle(lv_event_t *e)
{
	(void)e;
	manualSetArmed(!manualArmed, true, true);
}

// Force the pad disarmed (Ctrl released) when leaving the manual screen, so Ctrl never sticks held.
void manualForceDisarm(void)
{
	if (manualArmed)
	{
		manualSetArmed(false, true, false);
	}
}

// Build the arm/disarm toggle in the empty centre cell of the d-pad cross (the RIGHT_MID arrow flex
// centres near x362/y160; a 60px round button fits the gap without touching the 76px arrows).
void initManualArmButton(void)
{
	manualArmBtn = lv_btn_create(objects.manual);
	lv_obj_set_size(manualArmBtn, 60, 60);
	lv_obj_align(manualArmBtn, LV_ALIGN_RIGHT_MID, -88, 0);
	add_style_button_std(manualArmBtn);
	lv_obj_set_style_radius(manualArmBtn, 30, LV_PART_MAIN | LV_STATE_DEFAULT); // round — clearly not an arrow
	lv_obj_set_style_pad_all(manualArmBtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_add_event_cb(manualArmBtn, action_manual_arm_toggle, LV_EVENT_CLICKED, NULL);

	manualArmLabel = lv_label_create(manualArmBtn);
	lv_label_set_text(manualArmLabel, LV_SYMBOL_UP); // stylised up-chevron; colour reflects arm state
	lv_obj_set_style_text_font(manualArmLabel, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_center(manualArmLabel);

	manualArmed = false;
	manualUpdateArmButton();
	manualSetArrowsEnabled(false);
}

void action_goto_setup(lv_event_t *e)
{
	playbackSound(SND_LOADOUT_CLOSE); // returning to setup reads as "closing" the game/manual screen

	resetAllCooldowns();

	lv_timer_pause(cooldownTimer);
}

// Update selection in UI (text and bar)
void updateStratagemSelection()
{
	strategemsAmount = 0;

	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS - 1; c++)
	{
		if (gameButtons[c] == NULL)
		{
			gameButtons[c] = gameButtons[c + 1];
			indices[c] = indices[c + 1];
			types[c] = types[c + 1];

			gameButtons[c + 1] = NULL;
			indices[c + 1] = -1;
			types[c + 1] = -1;
		}
	}

	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		if (gameButtons[c] != NULL)
		{
			strategemsAmount++;
		}
	}

	lv_bar_set_value(objects.bar_amount, strategemsAmount, LV_ANIM_OFF);

	int barColor = colorTheme;

	if (strategemsAmount > 0 && strategemsAmount < 4)
	{
		barColor = sgRed;
	}
	else if (strategemsAmount == 4)
	{
		barColor = sgGreen;
	}
	else if (strategemsAmount > 4)
	{
		barColor = sgBlue;
	}

	lv_obj_set_style_bg_color(objects.bar_amount, lv_color_hex(barColor), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(objects.bar_amount, lv_color_hex(barColor), LV_PART_INDICATOR | LV_STATE_DEFAULT);

	char textAmount[] = "0 / 0";
	textAmount[0] = (char)(strategemsAmount + '0');
	textAmount[4] = (char)(MAX_USER_STRATAGEMS + '0');

	lv_label_set_text(objects.label_amount, (void *)textAmount);

	if (strategemsAmount == MAX_USER_STRATAGEMS)
	{
		action_goto_game(NULL);
	}
}

// Unassign stratagem item from button
void action_deselect_stratagem(lv_event_t *e)
{
	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		if (gameButtons[c] == e->current_target)
		{
			gameButtons[c] = NULL;
			indices[c] = -1;
			types[c] = -1;
		}
	}

	updateStratagemSelection();

	playbackSound(SND_DESELECT);
}

// Select/deselect handler for the code-added Hellbomb button (mirrors the generated tab buttons).
static void event_cb_hellbomb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
	{
		return;
	}
	if (lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED))
	{
		action_select_stratagem(e);
	}
	else
	{
		action_deselect_stratagem(e);
	}
}

// Hellbomb isn't in the EEZ selection UI (it lived only in the base/mission list). Add a selectable
// button for it to the Ground tab so it can go into a loadout like any other stratagem. The tab is a
// flex row-wrap, so the button just appends into the grid.
void initHellbombButton()
{
	lv_obj_t *btn = lv_btn_create(objects.tab_ground);
	lv_obj_set_size(btn, 76, 76);
	lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
	add_style_button_std(btn);
	lv_obj_set_style_bg_img_src(btn, &img_hb1, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_user_data(btn, (void *)SG_HB);
	lv_obj_add_event_cb(btn, event_cb_hellbomb, LV_EVENT_ALL, NULL);
}

// Assign stratagem item from button
void action_select_stratagem(lv_event_t *e)
{
	if (strategemsAmount < MAX_USER_STRATAGEMS)
	{
		for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
		{
			if (gameButtons[c] == NULL)
			{
				enum stratagemType type = (enum stratagemType)lv_obj_get_user_data(e->current_target);
				int index = -1;

				for (uint8_t c = 0; c < SG_ITEM_AMOUNT; c++)
				{
					stratagemItem item = strategemItemList[c];

					if (item.type == type)
					{
						index = c;
						break;
					}
				}

				gameButtons[c] = e->current_target;
				indices[c] = index;
				types[c] = type;
				break;
			}
		}

		updateStratagemSelection();

		playbackSound(SND_SELECT);
	}
	else
	{
		lv_obj_clear_state(e->current_target, LV_STATE_CHECKED);

		playbackSound(SND_DESELECT);
	}
}

// Reset all selected stratagems from user
void action_reset_stratagems(lv_event_t *e)
{
	if (strategemsAmount == 0)
	{
		return;
	}

	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		if (gameButtons[c] != NULL)
		{
			lv_obj_clear_state(gameButtons[c], LV_STATE_CHECKED);
			gameButtons[c] = NULL;
			indices[c] = -1;
			types[c] = -1;
		}
	}

	updateStratagemSelection();

	playbackSound(SND_RESET);
}

// Change connectivity (Bluetooth/USB)
void action_change_connectivity(lv_event_t *e)
{
	uint8_t connectionType = lv_dropdown_get_selected(objects.dd_connectivity);

	setConnectivity(connectionType + 1, false);
}

// Change assigned keymap
void action_change_keymap(lv_event_t *e)
{
	uint8_t keymapIndex = lv_dropdown_get_selected(objects.dd_keymap);

	setKeymap(keymapIndex, false);
}

void _executeStdStratagem(uint8_t *sequence, char *path)
{
	(void)path; // The per-stratagem category sound (SND_WEAPON, SND_EAGLE, ...) is intentionally NOT
	            // played: every fire is overridden by SND_PRIME + a voice callout, and those category
	            // WAVs are the original repo sounds that have been removed. The soundPath macros stay
	            // only as category identifiers for pickVoiceLine's strcmp — their files are never opened.
	setStratagemCode(sequence, INPUT_CTRL_MASK, false);
}

void _executeUserStratagem(uint8_t index)
{
	uint8_t itemIndex = indices[index];
	stratagemItem item = strategemItemList[itemIndex];

	setStratagemCode(item.sequence, INPUT_CTRL_MASK, false);

	if (showCooldowns)
	{
		const shipModuleDetails shipModuleList[MAX_SHIP_MODULES] = {
			{SHIP_LVC, objects.chb_ship_mod_lvc, 0, 0.5, 0.0},
			{SHIP_ZBL, objects.chb_ship_mod_zbl, 1, 0.1, 0.0},
			{SHIP_HC, objects.chb_ship_mod_hc, 2, 0.1, 0.0},
			{SHIP_MA, objects.chb_ship_mod_ma, 3, 0.05, 0.0},
			{SHIP_SRP, objects.chb_ship_mod_srp, 4, 0.1, 0.0},
			{SHIP_SS, objects.chb_ship_mod_ss, 5, 0.1, 0.0},
			{SHIP_TSU, objects.chb_ship_mod_tsu, 6, 0.0, 1.0},
			{SHIP_RLS, objects.chb_ship_mod_rls, 7, 0.0, 3.0},
			{SHIP_DT, objects.chb_ship_mod_dt, 8, 0.0, 3.0}};

		double cooldown = item.cooldown;
		double callin = item.callIn;
		double factor = 1.0;

		for (uint8_t c = 0; c < MAX_SHIP_MODULES; c++)
		{
			shipModuleDetails shipModuleItem = shipModuleList[c];

			if (lv_obj_has_state(shipModuleItem.checkbox, LV_STATE_CHECKED) && item.shipModules & (1 << shipModuleItem.shift))
			{
				factor -= shipModuleItem.cooldown;
				callin += shipModuleItem.callin;
			}
		}

		cooldown *= factor;
		cooldownValues[index] = getNow() + cooldown + callin;

		if (cooldownTimer->paused)
		{
			lv_timer_resume(cooldownTimer);
		}

		ui_update_task();
	}

	// Same feedback as manual input: prime cue, full-screen reveal, and the category voice line.
	const char *name = strategemItemNames[itemIndex];
	const char *voice = pickVoiceLine(name, item.soundPath, 0, itemIndex);

	playbackSound(SND_PRIME);
	showCallIn(item.imgHiRes, name, voice);
}

void action_reset_cooldown(lv_event_t *e)
{
	playbackSound(SND_STRAT_CLOSE); // generic menu-press cue

	cooldownResetIndex = (int)e->user_data;

	lv_obj_add_flag(cooldownLabels[cooldownResetIndex], LV_OBJ_FLAG_HIDDEN);

	cooldownValues[cooldownResetIndex] = 0;

	ui_update_task();
}

// Trigger standard stratagem
void action_trigger_stratagem_base(lv_event_t *e)
{
	int index = (int)e->user_data;
	uint8_t *sequence = (uint8_t *)strategemBaseList[index].sequence;
	char *path = strategemBaseList[index].soundPath;

	_executeStdStratagem(sequence, path);

	// Same feedback as manual input: prime cue, full-screen reveal, and the category voice line.
	const char *name = strategemBaseNames[index];
	const char *voice = pickVoiceLine(name, path, 1, index);

	playbackSound(SND_PRIME);
	showCallIn(strategemBaseList[index].imgHiRes, name, voice);

	if (index >= MAX_USER_STRATAGEMS) // Mission stratagems
	{
		lv_scr_load_anim(objects.game, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
	}
}

// Trigger user stratagem
void action_trigger_stratagem_user(lv_event_t *e)
{
	int index = (int)e->user_data;

	if (index == cooldownResetIndex)
	{
		cooldownResetIndex = -1;
		return;
	}

	_executeUserStratagem(index);
}

// Trigger keyboard demo (send "hello" via bluetooth connection to host)
void action_keyboard_demo(lv_event_t *e)
{
	uint8_t sequence[MAX_CMD_LENGTH] = {HID_KEY_H,
										HID_KEY_E,
										HID_KEY_L,
										HID_KEY_L,
										HID_KEY_O,
										0,
										0,
										0,
										0};

	setStratagemCode(sequence, 0, true);
}

char *resolvePresetKey(char presetIndex, int8_t itemIndex)
{
	static char key[3] = "p00";

	char buffer[1];

	if (itemIndex >= 0)
	{
		itoa(itemIndex, buffer, 10);
	}
	else
	{
		buffer[1] = 'i';
	}

	key[1] = presetIndex;
	key[2] = buffer[0];

	return key;
}

void updatePresets()
{
	if (openConfig() != ESP_OK)
	{
		return;
	}

	bool hasPresetList[MAX_USER_PRESETS] = {};

	for (uint8_t c = 0; c < MAX_USER_PRESETS; c++)
	{
		char buffer[1];
		itoa(c + 1, buffer, 10);

		presetKey[1] = buffer[0];
		presetKey[2] = '0';

		hasPresetList[c] = getConfig(presetKey, -1) != -1;
	}

	closeConfig();

	resolvePresetImages();

	lv_obj_t *presetButtons[] = {
		objects.btn_preset1,
		objects.btn_preset2,
		objects.btn_preset3,
		objects.btn_preset4,
		objects.btn_preset5,
		objects.btn_preset6};

	for (uint8_t c = 0; c < MAX_USER_PRESETS; c++)
	{
		lv_obj_set_style_border_color(presetButtons[c], lv_color_hex(hasPresetList[c] ? sgGreen : sgRed), LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

void action_get_preset(lv_event_t *e)
{
	playbackSound(SND_STRAT_CLOSE); // generic menu-press cue

	if (openConfig() != ESP_OK)
	{
		return;
	}

	int userData = (int)e->user_data;

	char buffer[1];
	itoa(userData, buffer, 10);

	presetKey[1] = buffer[0];

	if (presetImageMode)
	{
		presetImageMode = false;

		disableImageMode();

		lv_scr_load_anim(objects.image, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

		return;
	}

	action_reset_stratagems(NULL);

	lv_obj_t *lists[8] = {
		objects.tab_rifle,
		objects.tab_special,
		objects.tab_backpack,
		objects.tab_supply,
		objects.tab_sentry,
		objects.tab_ground,
		objects.tab_strike,
		objects.tab_eagle};

	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		char *key = resolvePresetKey(presetKey[1], c);

		int8_t presetIndex = getConfig(key, -1);
		types[c] = presetIndex;

		if (presetIndex == -1)
		{
			continue;
		}

		uint8_t listIndex = 0;
		uint16_t childIndex = 0;

		while (1)
		{
			lv_obj_t *child = lv_obj_get_child(lists[listIndex], childIndex);

			if (child == NULL)
			{
				uint8_t listsLength = (uint8_t)sizeof(lists);

				if (listIndex < listsLength)
				{
					listIndex++;
					childIndex = 0;
					continue;
				}

				break;
			}

			enum stratagemType type = (enum stratagemType)lv_obj_get_user_data(child);

			if (type == presetIndex)
			{
				int index = -1;

				for (int c = 0; c < SG_ITEM_AMOUNT; c++)
				{
					stratagemItem item = strategemItemList[c];

					if (item.type == type)
					{
						index = c;
						break;
					}
				}

				gameButtons[c] = child;
				indices[c] = index;

				lv_obj_add_state(child, LV_STATE_CHECKED);
				break;
			}

			childIndex++;
		}
	}

	closeConfig();

	updateStratagemSelection();

	if (strategemsAmount == 0)
	{
		showMsgBox("Preset\nempty");
	}
	else
	{
		if (gameAfterPreset)
		{
			initGame();

			lv_scr_load_anim(objects.game, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

			return;
		}
		else
		{
			showMsgBox("Preset\nloaded");
		}
	}

	lv_scr_load_anim(objects.setup, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

void action_set_preset(lv_event_t *e)
{
	playbackSound(SND_STRAT_CLOSE); // generic menu-press cue

	if (presetImageMode)
	{
		return;
	}

	int userData = (int)e->user_data;

	char buffer[1];
	itoa(userData, buffer, 10);

	presetKey[1] = buffer[0];

	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		char *key = resolvePresetKey(presetKey[1], c);

		setConfig(key, (uint8_t)types[c]);
	}

	updatePresets();

	playbackSound(SND_DESELECT);

	if (strategemsAmount == 0)
	{
		showMsgBox("Preset\ncleared");
	}
	else
	{
		showMsgBox("Preset\nsaved");
	}

	lv_scr_load_anim(objects.setup, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

void showMsgBox(char *msg)
{
	lv_label_set_text(objects.msg_label, msg);
	lv_obj_clear_flag(objects.msg_box, LV_OBJ_FLAG_HIDDEN);

	if (timerMsg != NULL)
	{
		lv_timer_del(timerMsg);
		timerMsg = NULL;
	}

	timerMsg = lv_timer_create(hideMsgBox, 1500, NULL);
}

void hideMsgBox(lv_timer_t *timer)
{
	if (timerMsg != NULL)
	{
		lv_timer_del(timerMsg);
		timerMsg = NULL;
	}

	lv_obj_add_flag(objects.msg_box, LV_OBJ_FLAG_HIDDEN);
}

void enableImageMode()
{
	presetImageMode = lv_obj_has_state(objects.btn_preset_image, LV_STATE_CHECKED);
}

void disableImageMode()
{
	lv_obj_clear_state(objects.btn_preset_image, LV_STATE_CHECKED);
	presetImageMode = false;
}

void action_assign_preset_image(lv_event_t *e)
{
	playbackSound(SND_STRAT_CLOSE); // generic menu-press cue

	int userData = (int)e->user_data;

	setConfig(presetKey, (uint8_t)userData);
	resolvePresetImages();

	lv_scr_load_anim(objects.preset, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

void action_clear_preset_image(lv_event_t *e)
{
	playbackSound(SND_STRAT_CLOSE); // generic menu-press cue

	setConfig(presetKey, 0);
	resolvePresetImages();

	lv_scr_load_anim(objects.preset, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

void resolvePresetImages()
{
	if (openConfig() != ESP_OK)
	{
		return;
	}

	lv_obj_t *presetButtons[] = {
		objects.btn_preset1,
		objects.btn_preset2,
		objects.btn_preset3,
		objects.btn_preset4,
		objects.btn_preset5,
		objects.btn_preset6};

	for (uint8_t c = 0; c < MAX_USER_PRESETS; c++)
	{
		char buffer[1];
		itoa(c + 1, buffer, 10);

		presetKey[1] = buffer[0];
		presetKey[2] = 'i';

		uint8_t imageIndex = getConfig(presetKey, 0);
		uint8_t offset = imageIndex == 0 ? 0 : 5;

		if (imageIndex == 0)
		{
			imageIndex = c;
		}

		const lv_img_dsc_t *presetImage = presetImageList[imageIndex + offset];
		lv_obj_t *button = presetButtons[c];

		lv_obj_set_style_bg_img_src(button, presetImage, LV_PART_MAIN | LV_STATE_DEFAULT);
	}

	closeConfig();
}

void resetPresets()
{
	if (openConfig() != ESP_OK)
	{
		return;
	}

	for (uint8_t c = 0; c < MAX_USER_PRESETS; c++)
	{
		char buffer[1];
		itoa(c + 1, buffer, 10);

		// Define preset index
		presetKey[1] = buffer[0];

		// Delete image
		presetKey[2] = 'i';
		setConfig(presetKey, 0);

		// Delete stratagems
		presetKey[2] = '0';
		setConfig(presetKey, -1);
	}

	closeConfig();

	updatePresets();
}

// Core manual-input step: register one arrow direction (1=up, 2=down, 3=left, 4=right),
// update/resolve the sequence, and (re)arm the auto-finalize timer. Called by the four tap arrows.
void manualExecuteDirection(int arrowDirection)
{
	// The arrows are inert until the pad is armed (Ctrl held / in-game stratagem menu open). Arming
	// is done from the centre d-pad toggle.
	if (!manualArmed)
	{
		return;
	}

	// Touch-bounce double-clicks are handled at the source — the release-bridge in lv_port.c merges
	// the panel's contact flicker into one clean press — so no debounce is needed here.

	// Helldivers directional SFX on each arrow input
	switch (arrowDirection)
	{
	case 1: playbackSound(SND_ARR_UP); break;
	case 2: playbackSound(SND_ARR_DOWN); break;
	case 3: playbackSound(SND_ARR_LEFT); break;
	case 4: playbackSound(SND_ARR_RIGHT); break;
	default: break;
	}

	manualSequence[manualIndex] = arrowDirection;

	// Clear any pending idle-reset timer; we'll re-arm it below only if nothing fired.
	if (timerManual != NULL)
	{
		lv_timer_del(timerManual);
		timerManual = NULL;
	}

	if (manualIndex < MAX_CMD_LENGTH)
	{
		manualIndex++;

		// Match + progress feedback, mode-aware. Either path costs one full-frame flush per input
		// (this panel can't push a partial region), which briefly holds the next touch read; the
		// sound already fired above on its own task, so audio stays instant.
		if (userSGautoComplete)
		{
			// Loadout active: match ONLY your selected stratagems (plus the always-available
			// utilities) and light up their on-screen codes as you type — filter + progress in one.
			mapManualSequence();
		}
		else
		{
			// No loadout: match against every stratagem.
			matchManualSequence();
		}

		// Wrong input: the entered sequence is no longer the start of ANY valid code. Reset instantly
		// and buzz, like the game — no reason to wait out the idle timeout on nonsense.
		if (!manualPrefixValid)
		{
			playbackSound(SND_STRAT_ERROR);
			finalizeManualExecution(); // manualMatch is -1 here, so this only resets (+ pulses the menu)
			return;
		}

		// Valid prefix — send THIS keystroke to the host now (Ctrl is held by arm), so the in-game
		// stratagem menu builds live as you tap. Wrong inputs are caught above and never reach it.
		hidSendLiveKey(INPUT_CTRL_MASK, LookupKeycode(arrowDirection));

		// Valid prefix — in no-loadout mode show the entered arrows now (loadout mode already
		// highlighted the matching codes inside mapManualSequence).
		if (!userSGautoComplete)
		{
			updateManualSequence();
		}

		if (manualMatch >= 0)
		{
			// Fire the INSTANT the entered code completes a stratagem's full pattern (the element
			// after manualIndex is the 0 terminator, or the code fills all MAX_CMD_LENGTH slots with
			// no terminator — e.g. the 9-input Bastion) — no timeout wait.
			const uint8_t *seq;
			if (manualList == 0)
			{
				seq = strategemItemList[manualMatch].sequence;
			}
			else if (manualList == 1)
			{
				seq = strategemBaseList[manualMatch].sequence;
			}
			else // manualList == 2: manualMatch is a user loadout slot, not a list index
			{
				seq = strategemItemList[indices[manualMatch]].sequence;
			}

			if (manualIndex >= MAX_CMD_LENGTH || seq[manualIndex] == 0)
			{
				finalizeManualExecution();
				return;
			}
		}
	}
	else
	{
		finalizeManualExecution();
		return;
	}

	// No complete match yet — arm an idle timeout that just resets the sequence if abandoned.
	timerManual = lv_timer_create(finalizeManualExecution, 2500, NULL);
}

void action_manual_execute(lv_event_t *e)
{
	manualExecuteDirection((int)e->user_data);
}


// --- Stratagem voice callouts ---
// A voice line (Super Destroyer / Eagle 1 confirmation) plays when the call-in screen appears, just
// after the stratagem's own cue. Lines are grouped into pools by category; specific stratagems
// (gas, smoke, laser, barrages, emplacements) are detected by name and get their own pool. One line
// is chosen at random from the pool each time.
#define V(f) "S:assets/sound/" f
static const char *const voiceEagleAtk[] = {V("eag_att1.wav"), V("eag_att2.wav"), V("eag_att3.wav"), V("eag_att4.wav"), V("eag_att5.wav"), V("eag_att6.wav"), V("eag_att7.wav"), V("eag_att8.wav")};
static const char *const voiceEagleRrm[] = {V("eag_rrm1.wav"), V("eag_rrm2.wav"), V("eag_rrm3.wav"), V("eag_rrm4.wav")};
static const char *const voiceOrbStr[] = {V("orb_str1.wav"), V("orb_str2.wav")};
static const char *const voiceOrbBar[] = {V("orb_bar1.wav"), V("orb_bar2.wav")};
static const char *const voiceGas[] = {V("gas1.wav"), V("gas2.wav")};
static const char *const voiceLaser[] = {V("laser1.wav")};
static const char *const voiceSmoke[] = {V("smoke1.wav")};
static const char *const voiceSentry[] = {V("sentry1.wav")};
static const char *const voiceSupW[] = {V("supw1.wav"), V("supw2.wav")};
static const char *const voiceEquip[] = {V("equip1.wav"), V("equip2.wav")};
static const char *const voiceEmpl[] = {V("empl1.wav")};
static const char *const voiceMines[] = {V("mines1.wav")};
static const char *const voiceReinf[] = {V("reinf1.wav"), V("reinf2.wav")};
static const char *const voiceSOS[] = {V("sos1.wav")}; // now the "Allied destroyer has joined" team line
static const char *const voiceHellb[] = {V("hellb1.wav"), V("hellb2.wav")};
#undef V

static const char *pickFrom(const char *const *pool, size_t n)
{
	return n == 0 ? NULL : pool[esp_random() % n];
}
#define PICK(p) pickFrom((p), sizeof(p) / sizeof((p)[0]))

// Pick a voice line for a fired stratagem. list==1 is a base entry (utility/mission, keyed by index);
// otherwise it's an item, matched by name keyword for specials then by sound category.
static const char *pickVoiceLine(const char *name, const char *soundPath, int list, int matchIndex)
{
	if (list == 1)
	{
		switch (matchIndex)
		{
		case 0: return PICK(voiceReinf);    // Reinforce
		case 1: return PICK(voiceEquip);    // Resupply — no ship resupply line, use "equipment package"
		case 2: return PICK(voiceSOS);      // SOS — "Allied destroyer has joined squadron"
		case 3: return PICK(voiceEagleRrm); // Eagle Rearm
		case 4: return PICK(voiceHellb);    // Hellbomb
		default: return NULL;               // mission-specific: no voice line yet
		}
	}

	if (name == NULL)
	{
		return NULL;
	}

	const bool orbital = soundPath != NULL && strcmp(soundPath, SND_ORBITAL) == 0;
	const bool sentry = soundPath != NULL && (strcmp(soundPath, SND_SENTRY) == 0 || strcmp(soundPath, SND_MORTAR) == 0);

	// Specials by name, checked before the broad category fallback. Order matters: emplacements and
	// sentries come first so the gas/smoke keywords don't steal e.g. a Gas Mortar Sentry or a
	// Grenadier Battlement.
	if (strstr(name, "Emplacement") || strstr(name, "Tesla") || strstr(name, "Relay") || strstr(name, "Battlement")) return PICK(voiceEmpl);
	if (sentry || strstr(name, "Sentry")) return PICK(voiceSentry);
	if (strcmp(name, "Hellbomb") == 0) return PICK(voiceHellb); // the item-list Hellbomb (not "Portable Hellbomb")
	if (strstr(name, "Exosuit")) return PICK(voiceEquip); // mechs deploy as a package, not a support weapon
	if (strstr(name, "Minefield") || strstr(name, "Mines")) return PICK(voiceMines);
	if (strstr(name, "Gas")) return PICK(voiceGas);
	if (strstr(name, "Smoke")) return PICK(voiceSmoke);
	if (orbital && strstr(name, "Laser")) return PICK(voiceLaser);
	if (strstr(name, "Barrage")) return PICK(voiceOrbBar);

	// Category fallback by the stratagem's sound path.
	if (soundPath == NULL) return NULL;
	if (strcmp(soundPath, SND_EAGLE) == 0) return PICK(voiceEagleAtk);
	if (strcmp(soundPath, SND_EAGLE_RELOAD) == 0) return PICK(voiceEagleRrm);
	if (strcmp(soundPath, SND_ORBITAL) == 0) return PICK(voiceOrbStr);
	if (strcmp(soundPath, SND_WEAPON) == 0 || strcmp(soundPath, SND_SUPPLY) == 0) return PICK(voiceSupW);
	if (strcmp(soundPath, SND_BACKPACK) == 0 || strcmp(soundPath, SND_SHIELD) == 0 || strcmp(soundPath, SND_BOT) == 0) return PICK(voiceEquip);
	if (strcmp(soundPath, SND_MINES) == 0) return PICK(voiceMines);

	return NULL;
}

// --- Stratagem call-in ("REQUEST RECEIVED") reveal ---
// A full-screen overlay on the top layer (above every screen), shown when a stratagem fires: its
// hi-res icon, its name, and a REQUEST RECEIVED banner. Auto-dismisses. When it appears it also
// schedules the category voice line, so the callout lands just after the stratagem's own cue.

// How long after the reveal to play the voice line, leaving room for the "primed" cue first.
#define VOICE_DELAY_MS 1200
static lv_timer_t *voiceTimer = NULL;
static const char *pendingVoice = NULL;

static void voicePlay(lv_timer_t *t)
{
	lv_timer_del(t);
	voiceTimer = NULL;
	if (pendingVoice != NULL)
	{
		playbackSound((char *)pendingVoice);
	}
}

static lv_obj_t *callInScreen = NULL;
static lv_obj_t *callInIcon = NULL;
static lv_obj_t *callInName = NULL;
static lv_timer_t *callInTimer = NULL;

static void callInDismiss(lv_timer_t *t)
{
	lv_timer_del(t);
	callInTimer = NULL;
	if (callInScreen != NULL)
	{
		lv_obj_add_flag(callInScreen, LV_OBJ_FLAG_HIDDEN);
	}
}

// Tap the reveal to dismiss it early — lets you fire off Game-screen stratagems back to back without
// waiting out the 5s each time.
static void callInTapDismiss(lv_event_t *e)
{
	if (callInScreen != NULL)
	{
		lv_obj_add_flag(callInScreen, LV_OBJ_FLAG_HIDDEN);
	}
	if (callInTimer != NULL)
	{
		lv_timer_del(callInTimer);
		callInTimer = NULL;
	}
}

void showCallIn(const lv_img_dsc_t *icon, const char *name, const char *voice)
{
	if (callInScreen == NULL)
	{
		return;
	}

	if (icon != NULL)
	{
		lv_img_set_src(callInIcon, icon);
		lv_obj_clear_flag(callInIcon, LV_OBJ_FLAG_HIDDEN);
	}
	else
	{
		lv_obj_add_flag(callInIcon, LV_OBJ_FLAG_HIDDEN);
	}

	lv_label_set_text(callInName, name != NULL ? name : "");

	lv_obj_clear_flag(callInScreen, LV_OBJ_FLAG_HIDDEN);
	lv_obj_move_foreground(callInScreen);

	if (callInTimer != NULL)
	{
		lv_timer_del(callInTimer);
	}
	callInTimer = lv_timer_create(callInDismiss, 5000, NULL);

	// Queue the voice line to play a beat after the reveal, so it follows the stratagem's own cue.
	pendingVoice = voice;
	if (voiceTimer != NULL)
	{
		lv_timer_del(voiceTimer);
		voiceTimer = NULL;
	}
	if (voice != NULL)
	{
		voiceTimer = lv_timer_create(voicePlay, VOICE_DELAY_MS, NULL);
	}
}

void initCallInScreen()
{
	callInScreen = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(callInScreen);
	lv_obj_set_size(callInScreen, LV_PCT(100), LV_PCT(100));
	lv_obj_set_style_bg_color(callInScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(callInScreen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_clear_flag(callInScreen, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(callInScreen, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(callInScreen, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(callInScreen, callInTapDismiss, LV_EVENT_CLICKED, NULL);

	lv_obj_set_flex_flow(callInScreen, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(callInScreen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_row(callInScreen, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

	callInIcon = lv_img_create(callInScreen);

	callInName = lv_label_create(callInScreen);
	lv_label_set_text(callInName, "");
	lv_obj_set_style_text_font(callInName, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(callInName, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_label_set_long_mode(callInName, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(callInName, LV_PCT(90));
	lv_obj_set_style_text_align(callInName, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

	lv_obj_t *banner = lv_label_create(callInScreen);
	lv_label_set_text(banner, "REQUEST RECEIVED");
	lv_obj_set_style_text_font(banner, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(banner, lv_color_hex(colorActive), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void finalizeManualExecution()
{
	if (manualMatch >= 0)
	{
		char *soundPath;
		const lv_img_dsc_t *icon = NULL;
		const char *name = NULL;

		if (manualList == 0)
		{
			stratagemItem item = strategemItemList[manualMatch];

			soundPath = item.soundPath;
			icon = item.imgHiRes;
			name = strategemItemNames[manualMatch];
		}
		else if (manualList == 1)
		{
			stratagemBase item = strategemBaseList[manualMatch];

			soundPath = item.soundPath;
			icon = item.imgHiRes;
			name = strategemBaseNames[manualMatch];
		}
		else if (manualList == 2)
		{
			const uint8_t itemIndex = indices[manualMatch];
			const stratagemItem item = strategemItemList[itemIndex];

			soundPath = item.soundPath;
			icon = item.imgHiRes;
			name = strategemItemNames[itemIndex];
		}

		const char *voice = pickVoiceLine(name, soundPath, manualList, manualMatch);

		// The code's keystrokes were already sent to the host live as they were tapped, so here we
		// only give feedback and release Ctrl — that Ctrl-up throws the stratagem and auto-disarms.
		playbackSound(SND_PRIME);      // Helldivers "primed" cue
		showCallIn(icon, name, voice); // full-screen reveal + the queued voice callout

		if (manualArmed)
		{
			hidReleaseModifier();
			manualArmed = false;
			manualSetArrowsEnabled(false);
			manualUpdateArmButton();
		}
	}
	else if (manualArmed)
	{
		// Reset without a completed code (wrong input or idle timeout): the valid partial already
		// sent live is still in the in-game menu, so pulse Ctrl to clear it and keep the pad in sync.
		hidPulseModifier(INPUT_CTRL_MASK);
	}

	manualMatch = -1;
	manualIndex = 0;

	for (uint8_t c = 0; c < MAX_CMD_LENGTH; c++)
	{
		manualSequence[c] = 0;
	}

	lv_obj_set_style_bg_img_src(objects.manual_preview_item, "", LV_PART_MAIN | LV_STATE_DEFAULT);

	if (!userSGautoComplete)
	{
		updateManualSequence();
	}
	else
	{
		resetAllManSeqs();
	}
}

void updateManualSequence()
{
	lv_obj_t *cmdImages[MAX_CMD_LENGTH] = {
		objects.manual_cmd1,
		objects.manual_cmd2,
		objects.manual_cmd3,
		objects.manual_cmd4,
		objects.manual_cmd5,
		objects.manual_cmd6,
		objects.manual_cmd7,
		objects.manual_cmd8,
		objects.manual_cmd9};

	for (uint8_t c = 0; c < MAX_CMD_LENGTH; c++)
	{
		const uint8_t currentCmd = manualSequence[c];
		lv_obj_t *target = cmdImages[c];

		if (currentCmd > 0)
		{
			const uint8_t imageIndex = currentCmd - 1;
			const lv_img_dsc_t *arrowImage = manualArrowList[imageIndex];

			lv_img_set_src(target, arrowImage);
			lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
		}
		else
		{
			lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
		}
	}
}

// Pure match computation — NO LVGL updates, so entering a code triggers zero screen redraws
// (each arrow is just a sound + this cheap byte-compare). Sets manualMatch/manualList for the
// entered sequence; manualMatch stays -1 unless exactly one stratagem matches.
void matchManualSequence()
{
	int matchCount = 0;
	manualMatch = -1;
	manualPrefixValid = false;

	for (uint8_t c1 = 0; c1 < SG_ITEM_AMOUNT; c1++)
	{
		stratagemItem item = strategemItemList[c1];
		bool match = true;
		bool matchComplete = false;

		for (uint8_t c2 = 0; c2 < manualIndex; c2++)
		{
			matchComplete = (c2 >= MAX_CMD_LENGTH - 1) || (item.sequence[c2 + 1] == 0);
			if (item.sequence[c2] != manualSequence[c2])
			{
				match = false;
				break;
			}
		}

		// Still true here → the entered sequence is a valid prefix of this code.
		if (match)
		{
			manualPrefixValid = true;
		}

		if (!manualAutoComplete && !matchComplete)
		{
			match = false;
		}

		if (match)
		{
			manualList = 0;
			matchCount++;
			manualMatch = c1;
		}
	}

	if (matchCount == 0)
	{
		for (uint8_t c1 = 0; c1 < SG_BASE_AMOUNT; c1++)
		{
			stratagemBase item = strategemBaseList[c1];
			bool match = true;
			bool matchComplete = false;

			for (uint8_t c2 = 0; c2 < manualIndex; c2++)
			{
				matchComplete = (c2 >= MAX_CMD_LENGTH - 1) || (item.sequence[c2 + 1] == 0);
				if (item.sequence[c2] != manualSequence[c2])
				{
					match = false;
					break;
				}
			}

			// Still true here → the entered sequence is a valid prefix of this base code.
			if (match)
			{
				manualPrefixValid = true;
			}

			if (!manualAutoComplete && !matchComplete)
			{
				match = false;
			}

			if (match)
			{
				manualList = 1;
				matchCount++;
				manualMatch = c1;
			}
		}
	}

	if (matchCount != 1)
	{
		manualMatch = -1;
	}
}

// Silently match the set NOT currently displayed (no highlight), so both your loadout and the
// utilities stay callable whichever view is shown. Feeds manualPrefixValid too, so entering the
// hidden set's code isn't treated as an error. Sets the fire target only if the displayed set
// didn't already match (displayed set keeps priority).
static void matchManualHiddenSet(bool utilityShown)
{
	if (utilityShown)
	{
		// Utilities are shown → the hidden set is the loadout items.
		for (uint8_t c1 = 0; c1 < MAX_USER_STRATAGEMS; c1++)
		{
			if (gameButtons[c1] == NULL)
			{
				break;
			}

			const uint8_t *seq = strategemItemList[indices[c1]].sequence;
			bool match = true;
			bool matchComplete = false;

			for (uint8_t c2 = 0; c2 < manualIndex; c2++)
			{
				matchComplete = (c2 >= MAX_CMD_LENGTH - 1) || (seq[c2 + 1] == 0);
				if (seq[c2] != manualSequence[c2])
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				manualPrefixValid = true;
			}
			if (!manualAutoComplete && !matchComplete)
			{
				match = false;
			}
			if (match && manualMatch < 0)
			{
				manualList = 2;
				manualMatch = c1;
			}
		}
	}
	else
	{
		// Loadout is shown → the hidden set is the utilities.
		for (uint8_t u = 0; u < MANUAL_UTILITY_COUNT; u++)
		{
			const uint8_t *seq = strategemBaseList[u].sequence;
			bool match = true;
			bool matchComplete = false;

			for (uint8_t c2 = 0; c2 < manualIndex; c2++)
			{
				matchComplete = (c2 >= MAX_CMD_LENGTH - 1) || (seq[c2 + 1] == 0);
				if (seq[c2] != manualSequence[c2])
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				manualPrefixValid = true;
			}
			if (!manualAutoComplete && !matchComplete)
			{
				match = false;
			}
			if (match && manualMatch < 0)
			{
				manualList = 1;
				manualMatch = u;
			}
		}
	}
}

void mapManualSequence()
{
	const lv_obj_t *manualImages[MAX_USER_STRATAGEMS];
	getManualImages(manualImages);

	resetAllManSeqs();

	// Clear any stale match; set below only if this input still matches something.
	manualMatch = -1;
	manualPrefixValid = false;

	// Highlight-match the DISPLAYED set: your loadout items, or the utilities in utility view.
	for (uint8_t c1 = 0; c1 < MAX_USER_STRATAGEMS; c1++)
	{
		const uint8_t *seq;
		int slotList;

		if (manualUtilityView)
		{
			if (c1 >= MANUAL_UTILITY_COUNT)
			{
				break;
			}
			seq = strategemBaseList[c1].sequence;
			slotList = 1;
		}
		else
		{
			if (gameButtons[c1] == NULL)
			{
				break; // slots are compacted, so every remaining slot is empty too
			}
			seq = strategemItemList[indices[c1]].sequence;
			slotList = 2;
		}

		lv_obj_t *targetCmdSeq[MAX_CMD_LENGTH];
		getManualCmdSeq(c1, targetCmdSeq);

		bool match = true;
		bool matchComplete = false;

		setManualSequence(targetCmdSeq, 91);

		for (uint8_t c2 = 0; c2 < manualIndex; c2++)
		{
			matchComplete = (c2 >= MAX_CMD_LENGTH - 1) || (seq[c2 + 1] == 0);

			if (seq[c2] != manualSequence[c2])
			{
				match = false;

				lv_obj_set_style_opa(manualImages[c1], 91, LV_PART_MAIN | LV_STATE_DEFAULT);
				setManualSequence(targetCmdSeq, 91);
				break;
			}

			lv_obj_set_style_opa(targetCmdSeq[c2], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
		}

		// Still true here → the entered sequence is a valid prefix of this displayed stratagem.
		if (match)
		{
			manualPrefixValid = true;
		}

		if (!manualAutoComplete && !matchComplete)
		{
			match = false;
		}

		if (match)
		{
			manualList = slotList;
			manualMatch = c1;
		}
	}

	// Both sets stay callable regardless of view — silently match the hidden one for the fire and
	// prefix (error) logic.
	matchManualHiddenSet(manualUtilityView);
}

void resetAllManSeqs()
{
	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		lv_obj_t *targetCmdSeq[MAX_CMD_LENGTH];
		getManualCmdSeq(c, targetCmdSeq);
		setManualSequence(targetCmdSeq, 255);
	}

	const lv_obj_t *manualImages[MAX_USER_STRATAGEMS];
	getManualImages(manualImages);

	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		lv_obj_t *imgIco = manualImages[c];

		lv_obj_set_style_opa(imgIco, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

void setManualSequence(lv_obj_t *manualCmdSeq[], uint8_t opacity)
{
	for (uint8_t c = 0; c < MAX_CMD_LENGTH; c++)
	{
		lv_obj_t *imgArrow = manualCmdSeq[c];

		lv_obj_set_style_opa(imgArrow, opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

void resetAllCooldowns()
{
	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		lv_obj_add_flag(cooldownLabels[c], LV_OBJ_FLAG_HIDDEN);

		cooldownValues[c] = 0;
	}

	ui_update_task();
}

uint32_t getNow()
{
	return esp_timer_get_time() / 1000000;
}