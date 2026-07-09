#ifndef BATTERY_H
#define BATTERY_H

#ifdef __cplusplus
extern "C"
{
#endif

    // Set up the ADC on GPIO7, build the battery-% indicator on the config screen, and start the
    // periodic update timer. Call once from ui_post() after the UI exists.
    void initBatteryMonitor(void);
    void batteryLoadCalibration(void); // load the saved calibration offset (call after NVS is open)

    int batteryMillivolts(void); // battery voltage in mV (after the divider), averaged
    int batteryPercent(void);    // 0..100, mapped from a 1S LiPo curve

#ifdef __cplusplus
}
#endif

#endif
