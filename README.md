# ESP32 C3 Nano TX
<img width="579" height="616" alt="image" src="https://github.com/user-attachments/assets/e5a0a2bb-08a6-49ff-bc16-0ad2317f77ac" />

ESP32 C3 Nano sized ExpressLRS transmitter originally based on the Simple TX Arduino Transmitter project, with a few extra features.

Simple TX is an Arduino based ELRS RC transmitter TX by Kkbin505. He designed an RC transmitter (soft and hardware) that is simple to build, and use.

I am rewriting it to run on an ESP32 C3. This has an OLED screen and is small enough to make my TinyTX project even smaller. I am reading telemetry from the ELRS module and implemented the full ELRS Lua script for configuring ELRS as well as the handset.

# Features
https://www.youtube.com/shorts/WRp26U7txlU

While i would consider this still a WIP, the current version is working perfectly and from here on i will mostly be doing maintenance, and maybe some new features for the firmware. ESP32 builtin button cycles screens (home, ELRS stats, channel outputs, model finder), and long press initiates functions. From the home screen, long press starts gimbal calibration. From ELRS stats screen it opens the ELRS config menu. From the Channel outputs screen it opens the channel menu (radio settings). Within menus, the builtin button functions like a back button. The joystick button is "enter" to start editing, save, and execute commands. The right joystick navigates the menu, similar to the OSD menu in Betaflight.

While in the main screen, the left joystick button toggles Arm (AUX1). The right joystick button toggles AUX2 (typically angle mode), and long press of the right joystick toggles AUX3 (typically turtle mode). 

ELRS menu lets you configure everything the LUA script usually does on EdgeTX. Yes you can select 1000hz packet rate. It seems to do around 860 packets/s while in the menu, probably more when armed and the OLED updates are slowed.

Channel menu lets you select ADC filtering, expo, channel reversing, limiting and the long press duration for AUX3.

Model finder is a simple RSSI DBM readout with strength bar and LED flashing in relation to how strong RSSI is.

Note that screen updates are slowed to 2hz while armed, because sending data to the OLED takes a long time! More than 2 frame packets (4ms) at 500hz packet rate... Normally i am sending at 10hz, so this eats up 20 frames of data. Worth keeping in mind anyway.
Version 3 of the 3D print finally fits everything and can be screwed together using m1.2x6 whoop canopy screws. 

Currently outstanding code tasks (wishlist):
* Bluetooth joystick using ESP32-BLE-Gamepad library (RX as a TX does not suport BLE Joystick so need to implement it myself)
* Multitask the OLED updates to free up more time for the ELRS code
* ~~Bluetooth transmitter?? Connect bluetooth joysticks to this device and forward controls to the ELRS transmitter~~ Unfortunately most game controllers use full bluetooth which is only supported on the original ESP32, not the C3

Programmed in Arduino IDE with love and a some AI suggestions (found myself rewriting everything it did). 
1. Install the ESP32 Boards via board manager
2. then select ESP32C3 Dev Module board
3. USB CDC On Boot "Enabled"
4. You may need to load some libraries such as U8G2
5. Build and upload.

# Features:
- Fits in your pocket. Not like the Radio Master Pocket which will get you funny looks... Actually pocketable.
- Using PS5 hall sensor joysticks so you dont miss out on that super magnetic accuracy even at this size.
- 0.42" OLED screen with easy to read, simple display
- Runs on a 1s lipo battery - the same as you fly your whoops with. Have not tested but expect more than 1hr runtime with a  1s 300mah
- Designed to use a 100mw telemetry Nano receiver as a TX module (RX as a TX). Also support ExpressLRS 2.4G external TX modules, but that would probably be bigger than the radio itself. External modules would need a separate power supply as most are designed to work on 7v+
- Up to 500Hz packet rate. Possibly even FLRC 1000hz? Who even uses that...
- 4 analog channels
- 3 AUX channels selectable via joystick button presses (left joystick button toggles Arm, right joystick button toggles channel 6, long press for channel 7)

Current STL is basic, and a tight fit. Make sure all wires are as short as needed, trim as necessary. I will be designing a PCB board you can order from PCBWay or whatever that will make building this a 15 minute job. But for now its manual wiring. 

# Parts list:
* ESP32 C3 with Oled https://ja.aliexpress.com/item/1005009157602185.html
* PS5 Joysticks https://ja.aliexpress.com/item/1005006282848536.html
* Nano receiver to be used as TX module https://ja.aliexpress.com/item/1005010228737836.html?
* 5V boost converter: https://ja.aliexpress.com/item/1005011803028198.html
* Also need two resistors for a voltage divider circuit (16k and 33k), silicone wires, battery pigtail, and a 3D printed case (joystick covers can be printed or bought)
Total cost is around $15-20

Based on work by kkbin505 with his SimpleTX project, but is now almost completely rewritten, 90% by hand (LLMs only used for code suggestions)
 * This project is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY. 
