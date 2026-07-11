# HD2 Tacpad

> ***"Stratagems primed. For Super Earth. For Managed Democracy."***

A working **HELLDIVERS™ 2 Stratagem Tacpad**: a cosplay prop that's also a real stratagem input device. Punch your codes into the touchscreen and the Tacpad transmits them to your PC as keyboard input, complete with the sound effects, team voice callouts, and a **"REQUEST RECEIVED"** call-in reveal straight off the front lines.

Built on an affordable ESP32-S3 touchscreen, it pairs over **Bluetooth** as a wireless keyboard. It's an actual stratagem macropad that *also* looks the part strapped to your wrist on the drop.

![The Tacpad](screens/tacpad_1.jpg)

> [!NOTE]
> This is a **fork** of [unic8s/hd2_macropad](https://github.com/unic8s/hd2_macropad); see [Lineage & Credits](#lineage--credits). It runs on the specific device [JC3248W535](https://www.aliexpress.com/item/1005007566332450.html?) (a 3.5" ESP32-S3 QSPI touchscreen).
>
> **The assembled Tacpad connects over Bluetooth only**, since the enclosure covers the board's USB-C port. The firmware keeps USB support (inherited from the base project) for use on the bare board, e.g. for flashing.

## Contents

- [What this fork adds](#what-this-fork-adds)
- [Lineage & Credits](#lineage--credits)
- **Build Guide**
  - [1. Bill of Materials](#1-bill-of-materials)
  - [2. 3D Printed Parts](#2-3d-printed-parts)
  - [3. Electronics & Wiring](#3-electronics--wiring)
  - [4. Firmware](#4-firmware)
  - [5. Assembly](#5-assembly)
  - [6. Finished](#6-finished)
- [Get the game](#get-the-game) · [Disclaimer](#disclaimer)

---

## What this fork adds

Everything from the base firmware, plus a full front-line immersion layer:

- 🔊 **Helldivers SFX throughout.** Directional arrow tones, stratagem-prime cue, menu open/close, tab swipes. The original macropad sounds are swapped for in-universe audio.
- 📡 **Team voice callouts.** Your Super Destroyer and Eagle-1 announce stratagems by category with randomized lines, triggered on the call-in, exactly like in the field.
- 🖥️ **"REQUEST RECEIVED" call-in screen.** A full-screen reveal (icon + name) every time a stratagem is dispatched.
- 🎯 **Manual arm-mode input.** Hold the centre d-pad toggle to *open the stratagem menu* (it holds Ctrl on the host), then tap your code on the arrows. Each valid input is sent **live**, so the in-game menu builds as you type; complete a valid code and it throws + auto-disarms.
- ⚡ **Crisp, reliable touch input.** The panel's phantom double-taps are fixed at the source, so codes register cleanly even when spammed fast.
- 🔈 **Volume control** in settings and loudness-normalized audio across every cue and voice line.
- 🔋 **Battery level indicator.** For a LiPo-powered build, a battery icon + % up top and a voltage readout with on-device calibration, read through the ESP32's ADC.

Loadout selection, presets, user icons, cooldown tracking, ship-module modifiers, and BLE/USB switching all carry over from the base firmware.

---

## Lineage & Credits

This project stands on the shoulders of prior democratic contributions, so please support the originals:

- **Firmware base** is [unic8s/hd2_macropad](https://github.com/unic8s/hd2_macropad). All the core stratagem-input, loadout, preset, cooldown, and configuration machinery is theirs. This fork is an immersion + input-feel layer on top. Detailed device setup & configuration lives in their [Wiki](https://github.com/unic8s/hd2_macropad/wiki).
- **Physical Tacpad design** is [Senpaijeffa's *Helldivers 2 Tacpad*](https://makerworld.com/en/models/997088-helldivers-2-tacpad-with-touchscreen-for-cosplay#profileId-1437063) on MakerWorld, the 3D-printed prop this build is based on.
- **Stratagem icons** are [@nvigneux](https://github.com/nvigneux)'s [Helldivers 2 Stratagems SVG set](https://github.com/nvigneux/Helldivers-2-Stratagems-icons-svg).
- **Device demo/reference** is [@NorthernMan54](https://github.com/NorthernMan54)'s [JC3248W535EN project](https://github.com/NorthernMan54/JC3248W535EN).
- **UI & menu SFX** come from [Gromlon Props](https://github.com/gromprops/Helldivers-2-Stratagem-Tacpad); the interface, menu, and stratagem sound effects are from their Helldivers 2 Stratagem Tacpad project.
- **Voice lines** are HELLDIVERS™ 2 in-game audio, from the community clip collection shared on [r/Helldivers](https://www.reddit.com/r/Helldivers/comments/1c348h6/helldivers_2_short_audio_clips_here_for_your/) ([clips folder](https://drive.google.com/drive/folders/1VT6HKNjR-lEG9xjQJB1dwWCI1ufyEFug)).

**HELLDIVERS™ 2** is © Arrowhead Game Studios, published by Sony Interactive Entertainment. This is a non-commercial fan project and is not affiliated with either.

---

# Build Guide

How to build the physical Tacpad: gather the parts, print the shell, wire the electronics, flash the firmware, and close it up.

## 1. Bill of Materials

Part links are examples, not direct recommendations. You may find cheaper sources at local hobby stores, Amazon, etc.

### Electronics

| Qty | Part | Notes |
| :-: | --- | --- |
| 1 | [JC3248W535](https://www.aliexpress.com/item/1005007566332450.html?) | 3.5" ESP32-S3 QSPI touchscreen (the brains + display). |
| 1 | microSD card | For the audio assets. Any size works; the contents are only ~10 MB. |
| 2 | [3.7V 1100mAh LiPo cell](https://www.aliexpress.com/item/1005004304618610.html?) | Different sizes work too, as long as they fit the battery tray. |
| 1 | [TP4056 charging module](https://www.aliexpress.com/item/1005006904523567.html?) | LiPo charge management. If you can find one with CC1/CC2 wired for USB-C negotiation, let me know! Otherwise it charges from a USB-A-to-USB-C cable. Micro-USB modules also work. |
| 1 | [DC-DC step-up (boost) converter, 0.9-5V to 5V](https://www.aliexpress.com/item/1005003479999072.html?) | Boosts the 3.7 V LiPo to a stable 5 V. |
| 1 | [KCD1 latching power switch](https://www.aliexpress.com/item/32873386670.html?) | External on/off. |
| 2 | [10 kΩ resistor](https://www.aliexpress.com/item/1005007539842999.html?) | Battery-level voltage divider (optional; see [3d](#3d-battery-level-monitor-optional)). |

### Connectors

A generic [1.25 mm JST connector kit](https://www.aliexpress.com/item/1005008691681233.html?) plus the solder/crimp hardware makes disassembly much easier. Bare-minimum connectors:

| Qty | Connector | Use |
| :-: | --- | --- |
| 1 | 4P1.25 JST male | Power into the ESP32's power port. |
| 1 | 2P1.25 JST male | Speaker into the ESP32's speaker port. |
| 1 | 8P1.25 JST male | *(optional)* Battery-sense wire to GPIO7 for the battery monitor. |
| 2 | Male + female JST (any size) | Connect the LiPo cells to the TP4056. I used 2.5 mm. |

### Hardware

| Qty | Part | Notes |
| :-: | --- | --- |
| 4 | [M3x4x5mm heat-set insert](https://www.aliexpress.com/item/1005006071488810.html?) | Voron-standard inserts, for fastening the lid to the body. |
| 4 | M3x8 FCHS | Screws the lid onto the main body. |
| 3 | Generic Velcro strap | 2 straps on the bottom, 1 on top, for affixing to your cosplay/arm. |
| As needed | Hookup wire, solder, Kapton tape | For wiring and insulation. |

**Printed parts:** see [Section 2](#2-3d-printed-parts).

---

## 2. 3D Printed Parts

The shell is based on [Senpaijeffa's Helldivers 2 Tacpad](https://makerworld.com/en/models/997088-helldivers-2-tacpad-with-touchscreen-for-cosplay#profileId-1437063). The STLs for this build live in [`3d-print/`](3d-print/):

- [`3d-print/tacpad-body.stl`](3d-print/tacpad-body.stl) is the main body / enclosure.
- [`3d-print/tacpad-lid.stl`](3d-print/tacpad-lid.stl) is the lid.

**Print settings:** standard settings work for both parts. I recommend at least 2 walls, 15% infill, and 4 top/bottom layers. Supports are required on both, but I've kept them to a minimum: only the internal structure of the main body and the battery tray on the lid need them.

---

## 3. Electronics & Wiring

> [!IMPORTANT]
> The **wiring diagram is the source of truth**; the steps below are a walkthrough of it. If anything conflicts, trust the diagram.

![Wiring diagram](screens/wiring_diagram.jpg)

**Power flow at a glance:** the unit runs off two 3.7 V 1100 mAh LiPo cells (~6-7 hours of runtime) that feed the TP4056 charging module, so it can charge over USB whether it's switched on or not. The TP4056 feeds the DC-DC step-up converter, which supplies a stable 5 V to the Display through the power switch. The speaker wires straight to the speaker port, and an optional voltage divider taps the battery to GPIO7 for the on-screen battery monitor.

> [!WARNING]
> Before connecting the two batteries in parallel, make sure they're at a **reasonably similar charge**. Once wired together they'll balance their voltages, so a large difference between them can be hazardous.

| Step | Reference |
| --- | :-: |
| **3a. Battery, charging & boost.** Solder two 2-pin JST connectors in parallel onto the TP4056's **BAT+ / BAT-** pads (mind polarity, don't short anything), then plug a battery into each. Connect a USB cable to the TP4056 and confirm power at **OUT+ / OUT-** with a multimeter. Wire **OUT+ → the converter's V0** pad and **OUT- → GND**. *(If adding the battery monitor, solder the two 10 kΩ resistors to OUT+/OUT- now.)* Confirm ~5 V between the converter's **GND and V1** pads (mine reads ~5.4 V, which is fine). | <img src="screens/4056_chip_wiring.jpg" alt="TP4056 + step-up wiring" width="400"> |
| **3a (cont.) 5 V feed + insulation.** Run a wire from the converter's **V1** pad (5 V out) and another from **GND** (either the converter's GND or the TP4056's OUT-); optionally terminate them in a 2P JST so the step-up is easy to disconnect. Then **insulate the converter with Kapton tape** so it can't short against anything. | <img src="screens/kapton_tape.jpg" alt="Step-up insulated with Kapton tape" width="400"> |
| **3b. Rocker switch & power connector.** The converter's **V1 = VIN** and **GND = GND**. Wire both to the **4P1.25 power connector**, with the rocker switch **interrupting the VIN line**. Mind the Display's pin labeling. Plug it in (double-check polarity!), flip the switch, and confirm the unit powers on. | <img src="screens/switch_wiring.jpg" alt="Power switch wiring" width="400"> |
| **3c. Speaker.** Wire the speaker to the **2P1.25 connector**, minding polarity per the diagram. The unit should play audio now. | |
| **3d. Battery level monitor** *(optional)*. Wire the two 10 kΩ resistors in series across **OUT+ and OUT-**, then take a tap off the midpoint. Route it to the **3rd pin of the 8P1.25 connector**, and double-check it lands on **GPIO7**. Calibrate on-screen later (settings → Misc). | See 3a |
| **3e. Final wiring.** Your connectors going into the Display should look like this. | <img src="screens/display_wiring.jpg" alt="Display wiring" width="400"> |

---

## 4. Firmware

> [!WARNING]
> Never feed battery power and USB power into the Display at the same time. Switch the unit off before plugging in USB for flashing.

Flash the firmware with [PlatformIO](https://platformio.org/) (ESP-IDF / `espressif32`):

```sh
pio run -t upload
```

Then copy the contents of [`sdcard/`](sdcard/) to the root of a microSD card and insert it. The SD card holds **only the audio** (44.1 kHz / 16-bit / mono WAVs); the stratagem images are compiled into the firmware. On first boot, pair over **Bluetooth** and set your stratagem keybinding in settings (defaults to **Ctrl + WASD/arrow keys**).

Confirm everything works: sound plays on the speaker, the display is responsive, and the battery % reads about right. If you're using the battery monitor, fine-tune it by measuring the real voltage across OUT+/OUT- with a multimeter and adjusting the offset in **settings → Misc** until they match.

> [!TIP]
> For the base firmware's full configuration options, unic8s's [Wiki](https://github.com/unic8s/hd2_macropad/wiki) applies to this fork.

---

## 5. Assembly

> [!NOTE]
> Double-check the firmware is flashed before you start. Once assembled, the Display's USB-C connector is inaccessible. *(The converter has no Kapton tape in the photos below; I insulated it afterwards.)*

| Step | Reference |
| --- | :-: |
| **Fit the electronics into the lid.** Slot the TP4056 into its space (a dab of glue helps if loose). Slot the speaker into its cutout, routing the wires out through the channel and inward, and fix it with glue. Install the two LiPo cells in the battery tray (I used a little double-sided adhesive so they don't slide, but can still be removed). | <img src="screens/tacpad_wiring_1.jpg" alt="Pre-assembly layout" width="400"> |
| **Prep the main body.** Melt the 4 heat-set inserts into place with a soldering iron, then screw the Display in with the 4 screws, keeping its USB-C side next to the speaker hole so the screen is oriented correctly *(you can always flip it in settings)*. Drop the rocker switch into its slot, then wire everything up and give it one more test. | <img src="screens/tacpad_wiring_2.jpg" alt="Pre-assembly layout, alternate" width="400"> |
| **Close it up.** Tuck the wires away (make sure nothing gets pinched), then fit the lid onto the back of the body, keeping the USB charging port lined up with its slot. Fasten with the 4 M3x8 FHCS, checking everything seats without much resistance. | <img src="screens/charge_port.jpg" alt="Charging port aligned in its slot" width="400"> |

---

## 6. Finished

Congratulations on your finished Tacpad, Helldiver! Use the 3 Velcro strap points to attach it to your cosplay (or wherever you like), or leave it as-is as a display piece.

To charge, plug a USB-C cable into the back: the inside glows red while charging and switches to blue/green once full (check your charging module's exact colour scheme). If the port doesn't quite fit a cable, sand the opening a little to widen it.

| | |
| :-: | :-: |
| <img src="screens/tacpad_2.jpg" alt="Finished Tacpad" width="400"> | <img src="screens/tacpad_on_arm.jpg" alt="Tacpad worn on the arm" width="400"> |

**For Super Earth!**

---

## Get the game

[HELLDIVERS™ 2 on Steam](https://store.steampowered.com/app/553850/HELLDIVERS_2/) · [PlayStation™](https://www.playstation.com/games/helldivers-2/)

## Disclaimer

> This is a private, open-source, non-commercial fan project and is not associated in any way with Sony Interactive Entertainment LLC or Arrowhead Game Studios. HELLDIVERS™ and PlayStation™ are registered trademarks of Sony Interactive Entertainment LLC. Assets in this project are either produced for free non-commercial use or published by the owners credited above.
