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

void initDisplay() {
    // Force I2C initialization to use the board's dedicated hardware pins (SDA=5, SCL=6)
    Wire.begin(5, 6);
    Wire.setClock(800000); // 800kHz ultra-fast clock rate minimizes I2C bus lockup time
    
    // Initialize the SSD1306 driver (standard address for this module is 0x3C)
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 initialization failed!"));
        //while(true); // Halt execution if display hardware is missing
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

void nextScreen() {
    switch (currentScreen) {
        case MAIN_PAGE:       currentScreen = ELRS_STATS_PAGE; break;
        case ELRS_STATS_PAGE: currentScreen = MAIN_PAGE;       break; // Wrap around
    }
}

void updateHomeScreen(float voltage, uint8_t uplinkLQ, int16_t channels[]) {
    display.clearDisplay();
    display.setTextSize(1);
    // Global hardware offset variables for 72x40 screens on 128x64 drivers
    const uint8_t offsetX = 30;
    const uint8_t offsetY = 24;
    // ==========================================
    // 1. TOP LEFT QUADRANT: LARGE VOLTAGE TEXT
    // ==========================================
    display.setTextSize(2); // Scale up font size to fill the quadrant vertically
    display.setCursor(0 + offsetX, 0 + offsetY);
    display.print(voltage, 1);
    
    // Smaller 'v' to fit neatly next to the large numbers
    display.setTextSize(1);
    display.print("v");

    // ==========================================
    // 2. TOP RIGHT QUADRANT: LARGE SIGNAL BARS
    // ==========================================
    uint8_t bars = 0;
    if (uplinkLQ > 75) bars = 4;
    else if (uplinkLQ > 50) bars = 3;
    else if (uplinkLQ > 25) bars = 2;
    else if (uplinkLQ > 0)  bars = 1;
    
    // Expanded coordinate sizes so the bar layout occupies the whole top right space
    // Format: fillRect(X, Y, Width, Height, Color)
    if (bars >= 1) display.fillRect(45 + offsetX, 11 + offsetY, 3, 3,  SSD1306_WHITE); // Bar 1 (Shortest)
    if (bars >= 2) display.fillRect(52 + offsetX, 8 + offsetY,  3, 6,  SSD1306_WHITE); // Bar 2
    if (bars >= 3) display.fillRect(59 + offsetX, 4 + offsetY,  3, 10, SSD1306_WHITE); // Bar 3
    if (bars >= 4) display.fillRect(66 + offsetX, 0 + offsetY,  3, 14, SSD1306_WHITE); // Bar 4 (Tallest)


    // ==========================================
    // 2. CENTER ROW: GIMBAL STICK CROSSHAIR BOXES
    // ==========================================
    const uint8_t boxSize = 20;
    const uint8_t leftBoxX = 12;
    const uint8_t rightBoxX = 40;
    const uint8_t boxesY = 18;
    
    // Draw outer frame boxes with offsets applied
    display.drawRect(leftBoxX + offsetX, boxesY + offsetY, boxSize, boxSize, SSD1306_WHITE);
    display.drawRect(rightBoxX + offsetX, boxesY + offsetY, boxSize, boxSize, SSD1306_WHITE);
    
    // Map channel widths (Assuming 1000 to 2000us)
    uint8_t leftX = map(channels[3], 1000, 2000, leftBoxX + 1, leftBoxX + boxSize - 2); // Rudder
    uint8_t leftY = map(channels[2], 1000, 2000, boxesY + boxSize - 2, boxesY + 1);      // Throttle (Inverted Y)
    
    uint8_t rightX = map(channels[0], 1000, 2000, rightBoxX + 1, rightBoxX + boxSize - 2); // Aileron
    uint8_t rightY = map(channels[1], 1000, 2000, boxesY + boxSize - 2, boxesY + 1);       // Elevator (Inverted Y)
    
    // Render crosshairs with offsets applied
    display.fillRect(leftX - 1 + offsetX, leftY - 1 + offsetY, 2, 2, SSD1306_WHITE);
    display.fillRect(rightX - 1 + offsetX, rightY - 1 + offsetY, 2, 2, SSD1306_WHITE);

    // ==========================================
    // 3. FLANKS: AUX SLIDERS
    // ==========================================
    // Left AUX Slider (Far Left)
    display.drawLine(4 + offsetX, 18 + offsetY, 4 + offsetX, 38 + offsetY, SSD1306_WHITE);
    uint8_t aux1Y = map(channels[4], 1000, 2000, 36, 16); // AUX 1
    display.fillRect(2 + offsetX, aux1Y + offsetY, 5, 3, SSD1306_WHITE);
    
    // Right AUX Slider (Far Right)
    display.drawLine(67 + offsetX, 18 + offsetY, 67 + offsetX, 38 + offsetY, SSD1306_WHITE);
    uint8_t aux2Y = map(channels[5], 1000, 2000, 36, 16); // AUX 2
    display.fillRect(65 + offsetX, aux2Y + offsetY, 5, 3, SSD1306_WHITE);

    display.display();
    
}

void updateElrsStatsScreen(const ELRSLua& luaInstance) {
    display.clearDisplay();
    display.setTextSize(1);
    // Global hardware offset variables for 72x40 screens on 128x64 drivers
    const uint8_t offsetX = 30;
    const uint8_t offsetY = 24;
    // ==========================================
    // 1. TOP LEFT QUADRANT: LARGE VOLTAGE TEXT
    // ==========================================
    display.setTextSize(1); // Scale up font size to fill the quadrant vertically
    display.setCursor(0 + offsetX, 0 + offsetY);
    if (luaInstance.ready()){
        display.println(luaInstance.txModule.name);
        display.println(luaInstance.txModule.params[1].choices[luaInstance.txModule.params[1].currentVal].text);
        display.print(luaInstance.txModule.params[7].choices[luaInstance.txModule.params[7].currentVal].text);
        display.print(luaInstance.txModule.params[7].units);
        if (luaInstance.txModule.params[8].currentVal == 1) display.print(" (Dynamic)");
    }
    else {
        display.print("Loading..."); 
    }

    display.display();
}
