// Helldivers 2 tacpad — UI event handler declarations (hand-maintained).

#ifndef _UI_EVENTS_H
#define _UI_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

extern void assignStratagems();
void updateStratagemSelection();
void _executeStdStratagem(uint8_t *sequence, char *path);
void _executeUserStratagem(uint8_t index);
void updatePresets();
void showMsgBox(char *msg);
void hideMsgBox(lv_timer_t *timer);
void enableImageMode();
void disableImageMode();
void resolvePresetImages();
char *resolvePresetKey(char presetIndex, int8_t itemIndex);
void resetPresets();
void finalizeManualExecution();
void manualExecuteDirection(int direction);
void initManualViewToggle();
void initManualArmButton();
void manualForceDisarm(void);
void initCallInScreen();
void showCallIn(const lv_img_dsc_t *icon, const char *name, const char *voice);
void initHellbombButton();
void updateManualSequence();
void matchManualSequence();
void mapManualSequence();
void resetAllManSeqs();
void getManualCmdSeq(uint8_t index, lv_obj_t *targetCmdSeq[]);
void getManualImages(lv_obj_t *manualImages[]);
void setManualSequence(lv_obj_t *manualCmdSeq[], uint8_t opacity);
void resetAllCooldowns();
uint32_t getNow();

#define MAX_USER_STRATAGEMS 6
#define MAX_USER_PRESETS 6

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
