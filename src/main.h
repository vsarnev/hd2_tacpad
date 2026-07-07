#ifndef MAIN
#define MAIN

#include <lvgl.h>
#include <esp_system.h>

#ifdef __cplusplus
extern "C"
{
#endif

    enum // ConnectionType
    {
        CT_NONE = 0,
        CT_BLUETOOTH = 1,
        CT_USB = 2,
    };

#ifdef __cplusplus
} /*extern "C"*/
#endif

// UI Sounds — repointed from the original Macropad sounds to Helldivers SFX.
#define SND_SWITCH "S:assets/sound/close.wav"   // general menu / config interaction (generic press)
#define SND_INTRO "S:assets/sound/init.wav"
#define SND_SELECT "S:assets/sound/lsel.wav"    // stratagem selected into loadout
#define SND_DESELECT "S:assets/sound/close.wav" // stratagem removed from loadout
#define SND_RESET "S:assets/sound/lclose.wav"   // loadout reset / cleared
#define SND_TAB "S:assets/sound/tabsw.wav"       // category tab switch (distinct from the generic click)

// Helldivers SFX (Gromlon Props pack) — space-free names; must match the files on the SD card
#define SND_ARR_UP "S:assets/sound/arru.wav"
#define SND_ARR_DOWN "S:assets/sound/arrd.wav"
#define SND_ARR_LEFT "S:assets/sound/arrl.wav"
#define SND_ARR_RIGHT "S:assets/sound/arrr.wav"
#define SND_PRIME "S:assets/sound/prime.wav"
#define SND_STRAT_START "S:assets/sound/start.wav"
#define SND_STRAT_CLOSE "S:assets/sound/close.wav"
#define SND_CLICK "S:assets/sound/close.wav" // generic press (was click.wav — close.wav is the generic cue now)
#define SND_LOADOUT_OPEN "S:assets/sound/lopen.wav"
#define SND_LOADOUT_CLOSE "S:assets/sound/lclose.wav"
#define SND_STRAT_ERROR "S:assets/sound/serror.wav"

// Default stratagems
#define SND_REINFORCE "S:assets/sound/reinf.wav"
#define SND_SUPPLY "S:assets/sound/supp.wav"
#define SND_SOS "S:assets/sound/sos.wav"
#define SND_EAGLE_RELOAD "S:assets/sound/eagrel.wav"

// User selected stratagems
#define SND_BACKPACK "S:assets/sound/bkpk.wav"
#define SND_BOT "S:assets/sound/bot.wav"
#define SND_EAGLE "S:assets/sound/eagstk.wav"
#define SND_MINES "S:assets/sound/min.wav"
#define SND_MORTAR "S:assets/sound/mrt.wav"
#define SND_ORBITAL "S:assets/sound/orbstk.wav"
#define SND_SHIELD "S:assets/sound/shd.wav"
#define SND_SENTRY "S:assets/sound/snt.wav"
#define SND_WEAPON "S:assets/sound/weap.wav"

enum styleColors
{
    colorTheme = 0xFFFFFF,
    colorActive = 0xFFDF00,
    sgRed = 0xDE7B6C,
    sgGreen = 0x679552,
    sgBlue = 0x49ADC9
};

#define MAX_CMD_LENGTH 9
#define SG_BASE_AMOUNT 17
#define SG_ITEM_AMOUNT 91

void app_main();
void playbackSound(char *path);
void setStratagemCode(uint8_t sequence[MAX_CMD_LENGTH], uint8_t mask, bool plain);
void hidHoldModifier(uint8_t mask); // hold a modifier (Ctrl) down — opens the in-game stratagem menu
void hidReleaseModifier(void);      // release the held modifier — closes/throws
void dimScreen(int brightness);
void updateConnection();
void ui_update_task();

#endif
