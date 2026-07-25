# ESP32 C3 Nano TX
<img width="579" height="616" alt="image" src="https://github.com/user-attachments/assets/e5a0a2bb-08a6-49ff-bc16-0ad2317f77ac" />

ESP32 C3 Nano sized ExpressLRS transmitter originally based on the Simple TX Arduino Transmitter project, with a few extra features.

Simple TX is an Arduino based ELRS RC transmitter TX by Kkbin505. He designed an RC transmitter (soft and hardware) that is simple to build, and use.

I have rewritten it to run on an ESP32 C3 module with an OLED screen that you can buy on Aliexpress for a few dollars. This is small enough to make my earlier TinyTX project even smaller, and it has a built in OLED screen as well! I am now reading telemetry from the ELRS module and implemented the full ELRS Lua script for configuring ELRS as well as the handset.

# THIS IS STILL BEING CODED - Check back for new updates
https://youtube.com/shorts/-rPwNI36CTY?feature=share

The radio works, I'm happy with it, but i am sure there will be things that need fixing or improving. Version 3 of the 3D print finally fits everything and can be screwed together using m1.2x6 whoop canopy screws. From here on in its mainly going to be code bug fixes, and maybe i'll come up with another 3d print design.

Currently wishlist/code tasks:
* Bluetooth joystick using ESP32-BLE-Gamepad library (RX as a TX does not suport BLE Joystick so need to implement it myself)
* Multitask the OLED updates to free up more time for the ELRS code

Programmed in Arduino IDE with love and a some AI suggestions (found myself rewriting everything it did). 
1. Install the ESP32 Boards via board manager
2. Install the U8g2 library by Oliver
3. then select ESP32C3 Dev Module board, and select the COM port your ESP32 is connected to
3. Set USB CDC On Boot "Enabled"
4. Build and upload.

Alternatively, go to https://espressif.github.io/esptool-js/
Click Connect (select the ESP32's COM port)
Change Flash Address to 0x0000, and press Choose File and selet ESP32C3-TX.bin from the Assets folder
Leave other options default, then click Program
Once the programming completes, you will need to reboot the ESP32 yourself (unplug and replug, as it doesnt seem to reset like it states in the programmer output window...)

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
* M1.2 x 6mm self tapping screws https://ja.aliexpress.com/item/1005008724193768.html
* Two resistors for a voltage divider circuit https://ja.aliexpress.com/item/1005009412772665.html (10k and 20k OR 15k and 30k OR 16k and 33k)
* AWG 32 or 30 Silicone wires in lots of colors (recommend 10 different colors) https://ja.aliexpress.com/item/4000485605478.html
* Battery pigtail with BT2.0 plug (10cm long), and a 3D printed case (joystick covers can be printed or bought)
Total cost is around $15-20

Based on work by kkbin505 with his SimpleTX project, but is now almost completely rewritten, 90% by hand (LLMs only used for code suggestions)
 * This project is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY. 
