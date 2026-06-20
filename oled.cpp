/*
 * This file is part of ESP32C3-TX
 *
 * ESP32C3-TX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ESP32C3-TX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include "oled.h"
displayState currentScreen = MAIN_PAGE;
displayState lastScreen;
// Instantiate the U8g2 object. (Rotation R0, No reset pin, SCL pin 6, SDA pin 5)
U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);


void initDisplay() {
    // Force I2C initialization to use the board's dedicated hardware pins (SDA=5, SCL=6)
    Wire.begin(5, 6);
    //Wire.setClock(800000); // 800kHz ultra-fast clock rate minimizes I2C bus lockup time
    // Start U8g2
    display.begin();
    display.setContrast(255); // Maximize visibility on the tiny 0.42" screen
    display.setBusClock(800000); // 400kHz fast I2C mode matching your ELRS telemetry update speed
    
    // Choose a small, scannable font fitting the 40px height limit
    display.setFont(u8g2_font_04b_03_tr); 
}

void showBootLogo() {
    display.clearBuffer();          // Clear the display internal ram memory
    
    // Draw NanoTX logo
    // Parameters: drawXBM(x_start, y_start, width, height, bitmap_array)
    display.drawXBM(0, 0, 72, 40, nanotx_logo_72x40);
    
    // 2. Add your dynamic runtime status text on the bottom area
    // Micro fonts like u8g2_font_u8glib_4_tf (4px height) fit perfectly in tight 72x40 panels
    display.setFont(u8g2_font_tiny5duo_tf); 
    
    // Print ELRS version or connection updates dynamically 
    display.setCursor(2, 36);       // X=2, Y=38 bottom-left
    display.print("ELRS");
    
    display.setCursor(50, 36);      // X=50, Y=38 bottom-right
    display.print("v1.0");
    
    display.sendBuffer();     
}

void clearScreen() {
    display.clearBuffer();          // Clear the display internal ram memory
    display.sendBuffer(); 
}

void nextScreen() {
    switch (currentScreen) {
        case MAIN_PAGE:       currentScreen = ELRS_STATS_PAGE; break;
        case ELRS_STATS_PAGE: currentScreen = MAIN_PAGE;       break; // Wrap around
    }
}


void updateHomeScreen(float voltage, int telemetryState, uint8_t uplinkLQ, int16_t channels[],int16_t minChannelValue,int16_t maxChannelValue) {
    static int lastVoltageX10 = 0;
    static int lastTelemetryState = -1;
    static int lastBars = 0;
    static int16_t lastAux1 = -1;
    static int16_t lastAux2 = -1;
    static int16_t lastAux3 = -1;

#ifdef TIMINGDEBUG
    uint32_t processTime;
    uint32_t transmitTime;
    uint32_t startTime = micros();
#endif
    // Clear only the top half (this is all that will be sent)
    display.setDrawColor(0);
    display.drawBox(OLED_X_OFFSET, OLED_Y_OFFSET, 72, 17);

    // Ensure draw color is set to white/on
    display.setDrawColor(1);

    // ==========================================
    // 1. TOP LEFT QUADRANT: LARGE VOLTAGE TEXT
    // ==========================================
    // Tall numerical font (~16px high)
    display.setFont(u8g2_font_logisoso16_tf); 
    display.setCursor(6 + OLED_X_OFFSET, 16 + OLED_Y_OFFSET);
    display.print(voltage, 1);
    
    // Smaller 'v' 
    display.setFont(u8g2_font_tiny5duo_tf); 
    display.setCursor(display.getCursorX()+2, 10 + OLED_Y_OFFSET);
    display.print("v");

    // ==========================================
    // 2. TOP RIGHT QUADRANT: LARGE SIGNAL BARS
    // ==========================================
    uint8_t bars = 0;
    if (telemetryState == 1 || telemetryState == 3) {
        if (uplinkLQ > 75) bars = 4;
        else if (uplinkLQ > 50) bars = 3;
        else if (uplinkLQ > 25) bars = 2;
        else if (uplinkLQ > 0)  bars = 1;
        
        // Format: drawBox(X, Y, Width, Height)
        if (bars >= 1) display.drawBox(45 + OLED_X_OFFSET, 11 + OLED_Y_OFFSET, 3, 3);   // Bar 1 (Shortest)
        if (bars >= 2) display.drawBox(52 + OLED_X_OFFSET, 8 + OLED_Y_OFFSET,  3, 6);   // Bar 2
        if (bars >= 3) display.drawBox(59 + OLED_X_OFFSET, 4 + OLED_Y_OFFSET,  3, 10);  // Bar 3
        if (bars >= 4) display.drawBox(66 + OLED_X_OFFSET, 0 + OLED_Y_OFFSET,  3, 14);  // Bar 4 (Tallest)
    } else if (telemetryState == 0 || telemetryState == 2) {
        display.drawBox(47 + OLED_X_OFFSET, 6 + OLED_Y_OFFSET, 8, 2); // display -- to indicate not connected
        display.drawBox(59 + OLED_X_OFFSET, 6 + OLED_Y_OFFSET, 8, 2);
        // tbd display NC when ELRS module not detected
    } else if (telemetryState == 5) { // Bluetooth Joystick mode
        // Draw bluetooth logo
    }
    if (telemetryState == 2 || telemetryState == 3) { // Bluetooth transmitter mode
        // Draw small bluetooth logo in top-left of ELRS signal area
        display.drawXBM(45 + OLED_X_OFFSET, 0 + OLED_Y_OFFSET, 8, 10, bt_logo_8x10);
    }
    uint8_t tileX = 0;
    uint8_t tileY = 0;
    uint8_t tileW = 9;
    uint8_t tileH = 2;

    if ((int)(voltage*10)==lastVoltageX10 && currentScreen == lastScreen) {
        // Dont write the voltage section if it hasnt changed
        tileX = 4;
        tileW = 5;
    }
    if (lastBars == bars && telemetryState == lastTelemetryState && currentScreen == lastScreen){
        // Dont write the bars/connection state if it hasnt changed
        tileW -= 5;
    } 
#ifdef TIMINGDEBUG
    processTime = micros() - startTime;
#endif
    if (tileW > 0) display.updateDisplayArea(tileX,tileY,tileW,tileH);
#ifdef TIMINGDEBUG
    transmitTime = micros() - (startTime + processTime);
    Serial.print("Top Half Process Time: "); Serial.print(processTime); Serial.print("uS. Send time: "); Serial.print(transmitTime); Serial.println("uS.");
#endif
    lastVoltageX10 = voltage*10;
    lastBars = bars;
    lastTelemetryState = telemetryState;

    // ==========================================
    // 2. CENTER ROW: GIMBAL STICK CROSSHAIR BOXES
    // ==========================================
    const uint8_t boxSize = 22;
    const uint8_t leftBoxX = 12;
    const uint8_t rightBoxX = 40;
    const uint8_t boxesY = 18;

    // Fast erase background box area
    display.setDrawColor(0);
    display.drawBox(0+OLED_X_OFFSET, 17+OLED_Y_OFFSET, 72, 22); 
    // Ensure draw color is set to white/on
    display.setDrawColor(1);

    // Format: drawFrame(X, Y, Width, Height)
    display.drawFrame(leftBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    display.drawFrame(rightBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    
    // Map channel widths (Assuming 1000 to 2000us)
    uint8_t leftX = map(channels[3], minChannelValue, maxChannelValue, leftBoxX + 1, leftBoxX + boxSize - 2); // Rudder
    uint8_t leftY = map(channels[2], minChannelValue, maxChannelValue, boxesY + boxSize - 2, boxesY + 1);      // Throttle (Inverted Y)
    
    uint8_t rightX = map(channels[0], minChannelValue, maxChannelValue, rightBoxX + 1, rightBoxX + boxSize - 2); // Aileron
    uint8_t rightY = map(channels[1], minChannelValue, maxChannelValue, boxesY + boxSize - 2, boxesY + 1);       // Elevator (Inverted Y)
    
    // Render crosshairs as small indicator boxes
    display.drawBox(leftX - 1 + OLED_X_OFFSET, leftY - 1 + OLED_Y_OFFSET, 2, 2);
    display.drawBox(rightX - 1 + OLED_X_OFFSET, rightY - 1 + OLED_Y_OFFSET, 2, 2);

    // ==========================================
    // 3. FLANKS: AUX SLIDERS
    // ==========================================
    // Format: drawLine(X1, Y1, X2, Y2)
    // Left AUX Slider (Far Left)
    display.drawLine(4 + OLED_X_OFFSET, 18 + OLED_Y_OFFSET, 4 + OLED_X_OFFSET, 38 + OLED_Y_OFFSET);
    uint8_t aux1Y = map(channels[4], minChannelValue, maxChannelValue, 36, 16); // AUX 1
    display.drawBox(2 + OLED_X_OFFSET, aux1Y + OLED_Y_OFFSET, 5, 3);

    // Right AUX Sliders (Far Right)
    display.drawLine(68 + OLED_X_OFFSET, 18 + OLED_Y_OFFSET, 68 + OLED_X_OFFSET, 26 + OLED_Y_OFFSET);
    uint8_t aux2Y = map(channels[5], minChannelValue, maxChannelValue, 24, 16); // AUX 2
    display.drawBox(66 + OLED_X_OFFSET, aux2Y + OLED_Y_OFFSET, 5, 3);

    display.drawLine(68 + OLED_X_OFFSET, 30 + OLED_Y_OFFSET, 68 + OLED_X_OFFSET, 38 + OLED_Y_OFFSET);
    uint8_t aux3Y = map(channels[6], minChannelValue, maxChannelValue, 36, 28); // AUX 2
    display.drawBox(66 + OLED_X_OFFSET, aux3Y + OLED_Y_OFFSET, 5, 3);

    // Send the partial display buffer to the physical screen hardware
    tileX = 0;
    tileY = 2;
    tileW = 9;
    tileH = 3;
    if (channels[4]==lastAux1 && currentScreen == lastScreen) { // dont send the left most tiles if aux1 didnt change
        tileX += 1;
        tileW -= 1;
    }
    if (channels[5]==lastAux2 && channels[6]==lastAux3 && currentScreen == lastScreen) tileW -= 1; // dont send the right most tiles if aux2 or 3 didnt change

#ifdef TIMINGDEBUG
    processTime = micros() - startTime;
#endif
    display.updateDisplayArea(tileX,tileY,tileW,tileH);
#ifdef TIMINGDEBUG
    transmitTime = micros() - (startTime + processTime);

    Serial.print("Stick Pos Process Time: "); Serial.print(processTime); Serial.print("uS. Send time: "); Serial.print(transmitTime); Serial.println("uS.");
#endif

    lastAux1 = channels[4];
    lastAux2 = channels[5];
    lastAux3 = channels[6];
    lastScreen = currentScreen;
}

void updateElrsStatsScreen(const ELRSLua& luaInstance) {
      // U8g2 full frame buffer drawing process
      display.clearBuffer();
      
      // NOTE: All drawing coordinates must add the OLED_X_OFFSET and OLED_Y_OFFSET 
      // to map correctly onto the visible 72x40 matrix array.
      
      // Example Status Display Layout:
    display.setFont(u8g2_font_nokiafc22_tf); 
    display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 8);
    display.print(luaInstance.txModule.name);
    //u8g2_font_squeezed_b7_tr

    display.setFont(u8g2_font_artossans8_8r); 
    display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 22);
    display.print(luaInstance.txModule.params[1].choices[luaInstance.txModule.params[1].currentVal].text); 

    display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 36);
    display.print(luaInstance.txModule.params[7].choices[luaInstance.txModule.params[7].currentVal].text); 
    display.print(luaInstance.txModule.params[7].units);
    if (luaInstance.txModule.params[8].currentVal == 1) display.print(" (Dynamic)");
      
    // Optional: Draw a tiny visual frame bounding the visible screen region
    //display.drawFrame(OLED_X_OFFSET, OLED_Y_OFFSET, OLED_WIDTH, OLED_HEIGHT);
      
    // Send local RAM buffer to the physical display
    display.sendBuffer();
    lastScreen = currentScreen;

}

void displayMessage(char title[], char line1[], char line2[]) {
    // U8g2 full frame buffer drawing process
    display.clearBuffer();
    // Ensure draw color is set to white/on
    display.setDrawColor(1);
    // ==========================================
    // 1. Title
    // ==========================================
    // 7 pixel high Nokia font for title
    display.setFont(u8g2_font_nokiafc22_tf); 
    
    int xOffset = (12 - strlen(title))*7;
    display.setCursor(xOffset + OLED_X_OFFSET, 8 + OLED_Y_OFFSET);
    display.print(title);
    
    display.setFont(u8g2_font_squeezed_b6_tr); // Back to a tiny font (~5px baseline)

    if(strlen(line2) > 0) {
        display.setCursor(2 + OLED_X_OFFSET, 22 + OLED_Y_OFFSET);
        display.print(line1);
        display.setCursor(2 + OLED_X_OFFSET, 36 + OLED_Y_OFFSET);
        display.print(line2);
    } else {
        display.setCursor(2 + OLED_X_OFFSET, 30 + OLED_Y_OFFSET);      
        display.print(line1);  
    }
}


void showCalibrationScreen(int stage, int secondsLeft) {
    display.clearBuffer();
    // Ensure draw color is set to white/on
    display.setDrawColor(1);

    // ==========================================
    // 1. Title
    // ==========================================
    // 7 pixel high Nokia font for title
    display.setFont(u8g2_font_nokiafc22_tf); 
    
    // U8g2 positions strings from the bottom baseline. Y coordinate adjusted down +14.
    display.setCursor(4 + OLED_X_OFFSET, 8 + OLED_Y_OFFSET);
    display.print("CALIBRATION");
    
    display.setFont(u8g2_font_squeezed_b6_tr); // Back to a tiny font (~5px baseline)
    display.setCursor(2 + OLED_X_OFFSET, 17 + OLED_Y_OFFSET); // Maintain horizontal progression
    switch (stage) {
        case 1:
            display.print("Starting...");
            break;
        case 2:
            display.print("Center Sticks "); display.print(secondsLeft);
            break;
        case 3:
            display.print("Move Sticks "); display.print(secondsLeft);
            break;
        case 4:
            display.print("Complete");
            break;
    }

    // ==========================================
    // 2. GIMBAL STICK CROSSHAIR BOXES
    // ==========================================
    const uint8_t boxSize = 22;
    const uint8_t leftBoxX = 12;
    const uint8_t rightBoxX = 40;
    const uint8_t boxesY = 18;
    
    // Format: drawFrame(X, Y, Width, Height) maps directly to old drawRect
    display.drawFrame(leftBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    display.drawFrame(rightBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    
    static bool direction = true;
    static float currXPos = 2;
    static float currYPos = 2;
    switch (stage) {
        case 1:
        case 2:
        case 4:
            // Render crosshairs in the center
            display.drawBox(leftBoxX + ((boxSize-2)/2) + OLED_X_OFFSET, boxesY + ((boxSize-2)/2) + OLED_Y_OFFSET, 2, 2);
            display.drawBox(rightBoxX + ((boxSize-2)/2) + OLED_X_OFFSET, boxesY + ((boxSize-2)/2) + OLED_Y_OFFSET, 2, 2);
            break;
        case 3:
            // Render crosshairs in the center
            display.drawBox(leftBoxX + (int)currXPos + OLED_X_OFFSET, boxesY + (int)currYPos + OLED_Y_OFFSET, 2, 2);
            display.drawBox(rightBoxX + boxSize + OLED_X_OFFSET - (int)currXPos, boxesY + (int)currYPos + OLED_Y_OFFSET, 2, 2);
            if (direction) {
                currXPos+=0.3;
                currYPos+=0.3;
                if (currXPos > boxSize - 2) {
                    currXPos-=0.6;
                    currYPos-=0.6;
                    direction = false;
                }
            } else {
                currXPos-=0.3;
                currYPos-=0.3;
                if (currXPos < 2) {
                    currXPos+=0.6;
                    currYPos+=0.6;
                    direction = true;
                }
            }
            break;
    }
    // Send the frame buffer to the physical screen hardware
    display.sendBuffer();
}

