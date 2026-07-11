# HD2 Tacpad Build Guide

How to build the physical Tacpad: print the shell, wire the electronics, flash the firmware, and close it up. For the firmware feature list and credits, see the [README](README.md).

![The finished Tacpad](screens/tacpad_1.jpg)

---

## 1. Bill of Materials

Any part links are examples, not direct recommendations. You may find cheaper sources at local hobby stores, Amazon, etc.

**Electronics**
- **[JC3248W535](https://www.aliexpress.com/item/1005007566332450.html?)** - 3.5" ESP32-S3 QSPI touchscreen (the brains + display).
- **microSD card** - for the audio assets. Any size works; the contents are only ~10 MB.
- **[2x 3.7V 1100mAh LiPo cells](https://www.aliexpress.com/item/1005004304618610.html?)** - you can use different sizes to suit your goal, as long as they fit in the battery tray.
- **[TP4056 charging module](https://www.aliexpress.com/item/1005006904523567.html?)** - LiPo charge management. If you can find one with the CC1/CC2 pins correctly wired for USB-C power negotiation, let me know! Otherwise the Tacpad will only charge from a USB-A-to-USB-C cable. A Micro-USB module works too, if you prefer.
- **[0.9-5V To 5V DC-DC Step-Up Boost Converter Board](https://www.aliexpress.com/item/1005003479999072.html?)** - boosts the 3.7V LiPo to a stable 5V.
- **[KCD1 Latching power switch](https://www.aliexpress.com/item/32873386670.html?)** - external on/off.
- **[2x 10 kΩ resistors](https://www.aliexpress.com/item/1005007539842999.html?)** - battery-level voltage divider _(optional; see [Battery monitor](README.md#battery-monitor-optional))_.

**Connectors**

I recommend a generic 1.25 mm JST connector kit plus the solder/crimping hardware to go with it, which makes disassembly much easier. [I'm a big fan of these types of connector sets.](https://www.aliexpress.com/item/1005008691681233.html?) The bare-minimum connectors you'll need:
- **4P1.25 JST male connector** - connects power to the ESP32's power port.
- **2P1.25 JST male connector** - connects the speaker to the ESP32's speaker port.
- **8P1.25 JST male connector** - (optional) connects the battery-sense wire to GPIO7 for the battery monitor.
- **2x male + female JST connector (any size)** - connect the LiPo cells to the TP4056. Use whatever size you like (I used 2.5 mm for mine).

**Hardware**
- **[4x M3x4x5mm Heat Set Inserts](https://www.aliexpress.com/item/1005006071488810.html?)** - Voron-standard heat-set inserts, for screwing the lid to the body.
- **4x M3x8 FCHS** - to screw the lid onto the main body.
- **3x generic Velcro straps** - the Tacpad has space for 2 straps on the bottom and 1 on top for affixing it to your cosplay/arm/etc.
- Hookup wire, solder, **Kapton tape** (insulation).

**Printed parts:** see [Section 2](#2-3d-printed-parts).

---

## 2. 3D printed parts

The shell is based on [Senpaijeffa's Helldivers 2 Tacpad](https://makerworld.com/en/models/997088-helldivers-2-tacpad-with-touchscreen-for-cosplay#profileId-1437063). The STLs for this build live in [`3d-print/`](3d-print/):

- [`3d-print/tacpad-body.stl`](3d-print/tacpad-body.stl) is the main body / enclosure.
- [`3d-print/tacpad-lid.stl`](3d-print/tacpad-lid.stl) is the lid.

### Print Settings
Standard settings work for both parts. I recommend at least 2 walls, 15% infill, and 4 top/bottom layers. Supports are required on both, but I've kept them to a minimum: only the internal structure of the main body and the battery tray on the lid need them.

---

## 3. Electronics & wiring

The **wiring diagram below is the source of truth**; the written steps are a walkthrough of it. If anything conflicts, trust the diagram.

![Wiring diagram](screens/wiring_diagram.jpg)

### Power flow at a glance
The unit runs off two 3.7V 1100 mAh LiPo cells, which give it an estimated 6-7 hours of runtime. They feed the TP4056 charging module, so the unit can charge over USB whether it's switched on or not. Note that the Display's own USB-C port is concealed once assembled, so **flash the unit before final assembly**.

The TP4056 then feeds the DC-DC step-up converter, which supplies a stable 5V to the Display. The power switch sits in that 5V line so you can turn the unit on and off.

The speaker wires directly to the speaker port (mind the polarity).

Finally, a voltage divider (two 10 kΩ resistors in series) is wired across OUT+ and OUT-, with the midpoint tapped to GPIO7 on the Display for the battery monitor.

### 3a. Battery, charging & boost (TP4056 + step-up)
> #### **[WARNING]**
> Make sure both batteries are at a reasonably similar charge before connecting them. Once wired together they'll balance their voltages, so a large difference between them can be hazardous.

Solder two 2-pin JST connectors in parallel onto the TP4056's BAT+ and BAT- pads. Mind your polarity and be careful not to short anything. Then plug a battery into each.

Connect a USB cable to the TP4056 to test charging, and use a multimeter on the OUT+/OUT- pads to confirm power is flowing.

Next, wire OUT+ to the DC-DC converter's V0 pad, and OUT- to its GND pad. If you're adding the battery monitor, it's easiest to solder the two 10 kΩ resistors onto the OUT+/OUT- pads now, at the same time as the wires, though that's optional.

Check the wiring with your multimeter: the converter should read ~5V between its GND and V1 pads. (Mine reads closer to 5.4V, which is fine.)

Now run your 5V feed: a wire from the converter's V1 pad (5V out) and another from GND (either the converter's GND pad or the TP4056's OUT-).

Optionally, terminate those two wires in a 2P1.25 mm or 2P2.5 mm JST connector so the step-up is easy to connect and disconnect from the power switch, as shown below:

![TP4056 + step-up wiring](screens/4056_chip_wiring.jpg)

Then insulate the converter with Kapton tape (or similar) so it can't short against anything.

![Step-up insulated with Kapton tape](screens/kapton_tape.jpg)

### 3b. Rocker switch and power connector

The converter's V1 pad is your VIN, and the GND (or OUT-) pad is your GND. Wire both to the 4-pin power connector (4P1.25 mm), with the rocker switch interrupting the VIN line so you can cut power to the Display.

Use the photo below for reference, and mind the labeling on the Display so your wires reach the correct pins (the wiring diagram above shows them).

With the power connector plugged into the Display (again, please double-check polarity!), flip the switch and test that the unit powers on.

![Power switch wiring](screens/switch_wiring.jpg)

### 3c. Speaker wiring

Wire the speaker to the 2P1.25 mm connector, minding the polarity per the wiring diagram. The unit should play audio now.

### 3d. Battery level monitor (optional)

For the on-screen battery display, wire the two 10 kΩ resistors in series across OUT+ and OUT- (see section 3a), then take a tap off the midpoint between them. Route that tap to the 3rd pin of the 8P1.25 mm connector, and double-check it lands on GPIO7 on the back of the Display.

### 3e. Final wiring

Your connectors going into the Display should look like this:

![Display wiring](screens/display_wiring.jpg)

---

## 4. Firmware

> #### **[WARNING]**
> Never feed battery power and USB power into the Display at the same time. This can cause all sorts of issues, so switch the unit off before plugging in USB for flashing.

Flash the firmware and copy the audio assets to the SD card. Full steps are in the [README → Build & flash](README.md#build--flash). Then confirm everything works: sound plays on the speaker, the display is responsive, and the battery % reads about right.

If you're using the battery monitor, you can fine-tune its accuracy: measure the real voltage across OUT+/OUT- with a multimeter, then offset the battery voltage in the **settings → Misc** tab until the two match.

---

## 5. Assembly

> This is a good moment to double-check the ESP32 is flashed with the Tacpad firmware. Once assembly is complete, the Display's USB-C connector is inaccessible.

With everything wired and tested, start fitting the components into the bottom lid. The TP4056 should slot cleanly into its space (a dab of glue helps if it's a bit loose).

Next, slot the speaker into its cutout, routing the wires out through the channel and inward, then fix it with a little glue.

Lastly, install the two LiPo cells in the battery slot. I used a little double-sided adhesive to stop them sliding around; it's non-permanent, so I can still pull them out if needed.

For the main body, first melt the 4 heat-set inserts into the four corners a soldering iron. The lid will screw into these. Then use the 4 provided with the Display screws to it to the main body, keeping its USB-C side next to the speaker hole so the screen is oriented correctly (no worries if it isn't, you can always flip it in the settings menu). Finally, drop the rocker switch into its slot.

> Note: in the assembly photos below the DC-DC converter has no Kapton tape yet; I insulated it after taking these.

Wire the whole unit up, aiming for something like the images below:

![Pre-assembly layout](screens/tacpad_wiring_1.jpg)
![Pre-assembly layout, alternate](screens/tacpad_wiring_2.jpg)

Give everything one more test to confirm the electronics still work.

Once you're happy, carefully tuck the wires away (make sure nothing gets pinched or crushed) and fit the lid onto the back of the main body, keeping the USB charging port lined up with the charging slot.

![Charging port aligned in its slot](screens/charge_port.jpg)

Use the 4 M3x8 FHCS to screw the lid to the body, again checking everything seats without much resistance. You don't want to pinch a wire and fry the whole thing this close to the finish line!

---

## 6. Finished

Congratulations on your finished Tacpad, Helldiver! May it serve you well. Use the 3 Velcro strap points to attach it to your cosplay (or wherever you like), or leave it as-is as a display piece.

To charge, plug a USB-C cable into the back: you should see the inside glow red while charging, then switch to blue/green once full (check your charging module's exact colour scheme). If the port doesn't quite fit a cable, sand the opening a little to widen it.

![Finished Tacpad](screens/tacpad_2.jpg)
![Tacpad worn on the arm](screens/tacpad_on_arm.jpg)

For Super Earth!
