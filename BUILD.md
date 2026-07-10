# HD2 Tacpad Build Guide

How to build the physical Tacpad: print the shell, wire the electronics, flash the firmware, and close it up. For the firmware feature list and credits, see the [README](README.md).

> [!NOTE]
> This is a working draft. The **Bill of Materials** (exact parts/links) and **3D printing settings** are being finalized, so the sections below are marked _TODO_ where your specifics go.

![The finished Tacpad](screens/tacpad_1.jpg)

---

## 1. Bill of Materials

> _TODO: finalize quantities, part links, and prices._

**Electronics**
- **JC3248W535** - 3.5" ESP32-S3 QSPI touchscreen (the brains + display).
- **microSD card** - for the audio assets.
- **2x 3.7 V LiPo cells** (this build uses 1100 mAh each, wired in **parallel** for ~2200 mAh).
- **TP4056 charging module** - LiPo charge management.
- **DC-DC step-up (boost) converter** - battery voltage up to a stable 5 V.
- **Latching power switch** - external on/off.
- **2x 10 kΩ resistors** - battery-level voltage divider _(optional; see [Battery monitor](README.md#battery-monitor-optional))_.
- Hookup wire, solder, **Kapton tape** (insulation).

**Printed parts:** see [Section 2](#2-3d-printed-parts).

---

## 2. 3D printed parts

The shell is based on [Senpaijeffa's Helldivers 2 Tacpad](https://makerworld.com/en/models/997088-helldivers-2-tacpad-with-touchscreen-for-cosplay#profileId-1437063). The STLs for this build live in [`3d-print/`](3d-print/):

- [`3d-print/tacpad-body.stl`](3d-print/tacpad-body.stl) is the main body / enclosure.
- [`3d-print/tacpad-lid.stl`](3d-print/tacpad-lid.stl) is the lid.

### Print Settings
Use your standard print settings for both parts. I recommend at least 2 walls, 15% infill and 4 top/bottom layers.
Supports are required on both parts, but I've taken steps to minimize where they're needed to only the internal structure of the main body, and the LiPo pouch on the lid.

---

## 3. Electronics & wiring

> [!IMPORTANT]
> The **wiring diagram below is the source of truth**; the written steps are a walkthrough of it. If anything conflicts, trust the diagram.

![Wiring diagram](screens/wiring_diagram.jpg)

### Power flow at a glance
```
2× LiPo (parallel) ──► TP4056 (charge) ──► Step-up 5V ──► Power switch ──► JC3248W535 VIN
                          ▲
USB-C breakout (VBUS) ────┘   (accessible charge-in)
```
Charging and running are **decoupled**: the TP4056 charges the cells independently of the switch, so you can charge with the unit off. The board's own USB-C is enclosed, so **the assembled Tacpad runs over Bluetooth**.

### 3a. Battery, charging & boost (TP4056 + step-up)
Wire the two LiPo cells in **parallel** (both `+` together, both `−` together) into the TP4056's battery pads, then feed the TP4056 output into the step-up converter's input. Set the step-up's output to **5 V** before connecting it to the display.

![TP4056 + step-up wiring](screens/4056_chip_wiring.jpg)

> [!WARNING]
> **Insulate the step-up converter** (Kapton tape over the pads/underside) so it can't short against other components or the shell.

![Step-up insulated with Kapton tape](screens/kapton_tape.jpg)

### 3b. Power switch
The latching switch goes in the power path so flipping it cuts power to the display entirely (true off: no standby drain, no idle hiss).

![Power switch wiring](screens/switch_wiring.jpg)

### 3c. Charging port (USB-C breakout)
The SparkFun USB-C breakout feeds charge power into the TP4056's input. It already has the **5.1 kΩ CC pull-downs**, so a USB-C charger will supply 5 V. Wire only **VBUS → charge-in (5 V)** and **GND → GND**; leave CC/D±/SBU unconnected. It mounts on the **rear** for accessible charging.

![Rear USB-C charging port](screens/charge_port.jpg)

### 3d. Display connections
Bring the switched 5 V and ground into the display's power input, and (optionally) the battery-sense line.

![Display wiring](screens/display_wiring.jpg)

### 3e. Battery level monitor (optional)
For the on-screen battery %, add a **10 kΩ / 10 kΩ voltage divider** from the battery positive to **GPIO7** (`ADC1_CH6`, on the `P2` header). Full wiring + the never-wire-raw-battery warning are in the [README](README.md#battery-monitor-optional). Calibrate on-device with the −/+ buttons in settings.

---

## 4. Assembly

With everything wired and tested, tuck the electronics into the printed body and route the battery/charge port. These pre-assembly shots show the layout before closing the lid:

![Pre-assembly layout](screens/tacpad_wiring_1.jpg)
![Pre-assembly layout, alternate](screens/tacpad_wiring_2.jpg)

> _TODO: step-by-step fit/mounting notes (screen seating, battery placement, screw/clip points, closing the lid)._

---

## 5. Firmware

Flash the firmware and copy the audio assets to the SD card. Full steps are in the [README → Build & flash](README.md#build--flash). On first boot, pair over **Bluetooth** and set your stratagem keybinding in settings.

---

## 6. Finished

![Finished Tacpad](screens/tacpad_2.jpg)
![Tacpad worn on the arm](screens/tacpad_on_arm.jpg)

For Super Earth. 🫡
