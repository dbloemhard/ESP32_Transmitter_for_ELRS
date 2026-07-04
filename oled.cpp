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
menuMode menuState = MENU_BROWSE;

// Instantiate the U8g2 object. (Rotation R0, No reset pin, SCL pin 6, SDA pin 5)
U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);


void initDisplay() {
    // Force I2C initialization to use the board's dedicated hardware pins (SDA=5, SCL=6)
    Wire.begin(5, 6);
    // Start U8g2
    display.begin();
    display.setContrast(255);
    display.setBusClock(800000); // 400kHz fast I2C mode
}

void drawBootLogo() {
    display.clearBuffer();          // Clear the display internal ram memory
    
    // Draw NanoTX logo
    // Parameters: drawXBM(x_start, y_start, width, height, bitmap_array)
    display.drawXBM(0, 0, 72, 40, nanotx_logo_72x40);

    display.setFont(u8g2_font_tiny5duo_tf); 
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
        case MAIN_PAGE:  currentScreen = ELRS_STATS; break;
        case ELRS_STATS: currentScreen = MAIN_PAGE;  break; // Wrap around
    }
}


void drawHomeScreen(float voltage, int telemetryState, uint8_t uplinkLQ, int16_t channels[],int16_t minChannelValue,int16_t maxChannelValue) {
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

void drawElrsStatsScreen(const ELRS& elrs) {
      // U8g2 full frame buffer drawing process
      display.clearBuffer();

//    display.setFont(u8g2_font_nokiafc22_tf); 
    display.setFont(u8g2_font_squeezed_b7_tr); 
    display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 8);
    if (strlen(elrs.txModule.name) > 0) {
        display.print(elrs.txModule.name);

//        display.setFont(u8g2_font_artossans8_8r); 
        display.setFont(u8g2_font_NokiaSmallPlain_tr); 
        display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 22);

        if (elrs.ready()) {
            int index = elrs.txModule.params.findParamByLabel(LABEL_PACKET_RATE);
            if (index >= 0) {
                const crsfParameter &param = elrs.txModule.params.getParam(index);
                display.print(param.choices[param.currentVal].text); 
            } else {
                display.print("Packet rate ???"); 
            }
            display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 36);
            index = elrs.txModule.params.findParamByLabel(LABEL_TX_POWER_STRING);
            if (index >= 0) {
                const crsfParameter &param = elrs.txModule.params.getParam(index);
                display.print(param.label); 
            } else {
                display.print("TX power ???"); 
            }

        } else {
            display.print("Loading Params...");
        }

    } else {
        display.print("No TX Module");
        display.setFont(u8g2_font_streamline_interface_essential_circle_triangle_t);
        display.setCursor(OLED_X_OFFSET + 25, OLED_Y_OFFSET + 38);   
        display.print("3"); // Icon set exclamation mark in triangle     
    }

    display.sendBuffer();
    lastScreen = currentScreen;

}


 //----- Menu text animation timing configuration
const unsigned long STATE_START_PAD  = 1000; 
const unsigned long STATE_SCROLL     = 1000;  
const unsigned long STATE_END_PAD    = 500;  
const unsigned long TOTAL_CYCLE      = STATE_START_PAD + STATE_SCROLL + STATE_END_PAD;
const int MAX_TEXT_LENGTH            = 64; // Theoretically ELRS could have longer strings, but not likely

void drawScrollingMenuValue(const char* text, int textX, int textY, int displayAreaWidth) {
    int textWidth = display.getStrWidth(text);

    unsigned long currentMillis = millis();
    static unsigned long lastResetTime = 0;
    static char lastText[MAX_TEXT_LENGTH] = "";
    enum ScrollState {START,SCROLLING,FINISH};
    static ScrollState currentState = START;

    // If text changes, reset state
    if (strcmp(text, lastText) != 0) {
        strncpy(lastText, text, sizeof(lastText) - 1);
        lastText[sizeof(lastText) - 1] = '\0'; // Ensure null-termination
        currentState = START;
        lastResetTime = currentMillis;
    }
    
    // Handle short text: Center statically without scrolling
    if (textWidth <= displayAreaWidth) {
        int centerX = textX + (displayAreaWidth - textWidth) / 2;
        display.drawStr(centerX, textY, text);
        return;
    }

    // Handle standard loop cycling
    if (currentMillis - lastResetTime >= TOTAL_CYCLE) {
        lastResetTime = currentMillis;
    }
    
    unsigned long progress = currentMillis - lastResetTime;
    int maxScrollDistance = textWidth - displayAreaWidth;
    int currentXOffset = 0;

    // State Machine
    if (progress < STATE_START_PAD) {
        currentXOffset = 0;
    } 
    else if (progress < (STATE_START_PAD + STATE_SCROLL)) {
        unsigned long scrollProgress = progress - STATE_START_PAD;
        currentXOffset = (maxScrollDistance * scrollProgress) / STATE_SCROLL;
    } 
    else {
        currentXOffset = maxScrollDistance;
    }

    // Assume that setClipWindow has already been set appropriately
    display.drawStr(textX - currentXOffset, textY, text);
    // // Hardware clipping render
    // display.setClipWindow(x, y, x + w, y + h);
    // display.drawStr(x - currentXOffset, textY, text);
    // display.setMaxClipWindow(); 
}


// Draw a menu item within a given window (top left coordinates and h/w set with x/y/h/w)
void drawMenuItem(const crsfParameter& menuItem, int offsetX, int offsetY) {
    display.setFont(u8g2_font_squeezed_b7_tr); 
    switch (menuItem.type) {
        case CRSF_FOLDER: // Folder will have a triangle on the right, so it has 5 pixels less width
            // Set clipping window boundary constraints to avoid drawing over the menu direction arrows
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH-7, OLED_Y_OFFSET+OLED_HEIGHT-5);
            drawScrollingMenuValue(menuItem.label, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 23 + offsetY, 62);
            break;
        case CRSF_COMMAND:
        case CRSF_INFO:
            // Set clipping window boundary constraints to avoid drawing over the menu direction arrows
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH, OLED_Y_OFFSET+OLED_HEIGHT-5);
            drawScrollingMenuValue(menuItem.label, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 23 + offsetY, 67);
            break;
        default:      
            // Set clipping window boundary constraints to avoid drawing over the menu direction arrows
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH, OLED_Y_OFFSET+OLED_HEIGHT-5);
            int textWidth = display.getStrWidth(menuItem.label);
            int centerX = (OLED_WIDTH - textWidth) / 2;
            display.drawStr(OLED_X_OFFSET + centerX + offsetX, OLED_Y_OFFSET + 15 + offsetY, menuItem.label);
            display.setFont(u8g2_font_NokiaSmallPlain_tr);                
            drawScrollingMenuValue(menuItem.choices[menuItem.currentVal].text, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 30 + offsetY, 62);
            break;   
    }
    display.setMaxClipWindow(); // Reset clip window limits
}


// Animation configuration
// 0 = No animation
// 1 = Animate Up
// 2 = Animate Down
// 3 = Animate Left
// 4 = Animate Right
static uint8_t activeAnimation = 0;   // Stores the current running animation type (1-4)
static unsigned long animStartTime = 0;
const unsigned long ANIM_DURATION = 500; // Transition duration in milliseconds
// Menu Item tracking
static int prevMenuItemIndex = 0;   // Track where we came from
static int currentMenuItemIndex = 0;// Track where we are going

void animateMenu(const ParamCollection& menu) {
    if (activeAnimation == 0) {
        // Standard State: No active transition requested, render static layout normally
        drawMenuItem(menu.getCurrentParam(), 0, 0);
    } 
    else {
        // Animation Active: Calculate step frame interpolation
        unsigned long elapsed = millis() - animStartTime;
        
        if (elapsed >= ANIM_DURATION) {
            // Animation complete: clean state tracking parameters
            activeAnimation = 0;
            drawMenuItem(menu.getCurrentParam(), 0, 0);
        } 
        else {
            // Linear Progress mapping percentage (0.0 -> 1.0 represented out of 1000)
            long progress = (elapsed * 1000) / ANIM_DURATION; 
            
            int offsetX_Prev = 0, offsetY_Prev = 0;
            int offsetX_Curr = 0, offsetY_Curr = 0;

            // Compute precise Pixel offsets based on target translation paths
            switch (activeAnimation) {
                case 1: // Animate Up (New item enters from bottom)
                    offsetY_Prev = - (OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = OLED_HEIGHT - (OLED_HEIGHT * progress) / 1000;
                    break;
                case 2: // Animate Down (New item enters from top)
                    offsetY_Prev = (OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = -OLED_HEIGHT + (OLED_HEIGHT * progress) / 1000;
                    break;
                case 3: // Animate Left (New folder enters from right)
                    offsetX_Prev = - (OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = OLED_WIDTH - (OLED_WIDTH * progress) / 1000;
                    break;
                case 4: // Animate Right (New folder enters from left)
                    offsetX_Prev = (OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = -OLED_WIDTH + (OLED_WIDTH * progress) / 1000;
                    break;
            }

            // Render both states into the active full-buffer layout sequentially
            drawMenuItem(menu.getParam(prevMenuItemIndex), offsetX_Prev, offsetY_Prev);
            drawMenuItem(menu.getParam(currentMenuItemIndex), offsetX_Curr, offsetY_Curr);
        }
    }    
}


// uint8_t direction
// 0 = No move/button
// 1 = Up
// 2 = Down
// 3 = Left
// 4 = Right
// 5 = Joystick button
// 6 = Built in button (short press)
// 7 = Built in button (long press)
// return value
// -1: Exit menu (back to prev screen)
//  0: normal operation (editing, or navigating menu)
//  1: Save menu item
//  2: Execute command??
int navigateMenu(ParamCollection &menu, uint8_t direction) {
    uint8_t nextAnimation;
    int returnVal = 0;
    if (direction == 0) return 0;
    switch (menuState) {
        case MENU_BROWSE:
            nextAnimation = 0;
            switch (direction) {
                case 1:
                    nextAnimation = menu.prevInFolder();
                    break;
                case 2:
                    nextAnimation = menu.nextInFolder();
                    break;
                case 3:
                    nextAnimation = menu.exitFolder();
                    break;
                case 4:
                    nextAnimation = menu.enterFolder();
                    break;
                case 5:
                    switch (menu.getCurrentParam().type) {
                        case CRSF_COMMAND:
                            //elrs.executeCommand(elrs.currentParam);
                            break;
                        case CRSF_UINT8:
                        case CRSF_INT8:
                        case CRSF_UINT16:
                        case CRSF_INT16:
                        case CRSF_UINT32:
                        case CRSF_INT32:
                        case CRSF_UINT64:
                        case CRSF_INT64:
                        case CRSF_FLOAT:
                        case CRSF_TEXT_SELECTION:
                        case CRSF_STRING:
                            menu.editParam();
                            menuState = MENU_EDIT;  // Change to edit mode
                            break;
                        default:
                            // Do nothing
                            break;   
                    }
                    break;
                case 6:
                    menu.selectedParam = 0;
                    returnVal = -1;  // Exit out of menu
                    //currentScreen = ELRS_STATS;  // Exit out of the menu
                    break;
                    // 7/Long press not handled in menu
            }
            // If the joystick movement resulted in an animation
            if (nextAnimation != 0) {
                activeAnimation = nextAnimation;
                animStartTime = millis(); // Start the timer clock
                
                // Capture current menu item index (selectedParam) before updating the active item
                prevMenuItemIndex = currentMenuItemIndex;
                currentMenuItemIndex = menu.selectedParam;
            }            
            break;
        case MENU_EDIT:
            switch (direction) {
                case 3:
                    menu.editParamPrev();
                    break;
                case 4:
                    menu.editParamNext();
                    break;
                case 5:
                    returnVal = 1;  // Indicate that we need to save the current item
                    menuState = MENU_BROWSE;
                    //menu.editParamSave();  // Save the menu (this function will also exit back to browse mode)
                    break;
                case 6:
                    menu.editParam(); // reset editValue to the param's current value
                    menuState = MENU_BROWSE;  // Go back to browse mode
                    break;
            }
            break;
        case MENU_POPUP:
            switch (direction) {
                case 5:
                    //elrs.sendCommandConfirm(elrs.currentParam);  // Confirm/execute
                    break;   
                case 6:
                    //elrs.sendCommandCancel(elrs.currentParam);  // Cancel current command
                    break;   
            }
            break;
    }

    return returnVal;
}


void drawMenuScreen(const ParamCollection& menu) {
    static int menuIndex;
    static const int tenToThePowerOf[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
    float floatVal;
    const crsfParameter &param = menu.getCurrentParam();

    display.clearBuffer();
    display.setFont(u8g2_font_NokiaSmallPlain_tr);    
    
    switch (menuState) {
        case MENU_POPUP:
            //drawPopupMenu(elrs);
            break;
        case MENU_EDIT:
            display.setCursor(OLED_X_OFFSET + 3, OLED_Y_OFFSET + 12);
            display.print(param.label);
            
            display.drawFrame(0, 14, 72, 26);        

            display.setFont(u8g2_font_NokiaSmallPlain_tr); 
            display.setCursor(OLED_X_OFFSET + 11, OLED_Y_OFFSET + 30);

            switch (param.type) {
                case CRSF_UINT8:
                case CRSF_INT8:
                case CRSF_UINT16:
                case CRSF_INT16:
                case CRSF_UINT32:
                case CRSF_INT32:
                case CRSF_UINT64:
                case CRSF_INT64:
                    display.print(menu.editValue);
                    display.print(param.units);
                    break;
                case CRSF_FLOAT:
                    floatVal = (float) menu.editValue / tenToThePowerOf[param.precision];
                    display.print(floatVal);
                    break;
                case CRSF_TEXT_SELECTION:
                    display.print(param.choices[menu.editValue].text);
                    display.print(param.units);
                case CRSF_STRING:
                    // No idea how to edit a string parameter - none in ELRS... yet
                    break;
                default:
                    // Do nothing
                    break;   
            }

            if (menu.editValue > param.minVal) {   // Left arrow
                display.setDrawColor(0);
                display.drawBox(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 21, 5, 11);
                display.setDrawColor(1);
                 display.drawTriangle(OLED_X_OFFSET + 8, OLED_Y_OFFSET + 22,
                                    OLED_X_OFFSET + 8, OLED_Y_OFFSET + 30,
                                    OLED_X_OFFSET + 3, OLED_Y_OFFSET + 26);
            }
            if (menu.editValue < param.maxVal) {   // Right Arrow
                display.setDrawColor(0);
                display.drawBox(OLED_X_OFFSET + 64, OLED_Y_OFFSET + 21, 5, 11);
                display.setDrawColor(1);
                display.drawTriangle(OLED_X_OFFSET + 65, OLED_Y_OFFSET + 22,
                                    OLED_X_OFFSET + 65, OLED_Y_OFFSET + 30,
                                    OLED_X_OFFSET + 70, OLED_Y_OFFSET + 26);
            }
            break;


        default: //MENU_BROWSE

            // Draw navigation arrows
            if (param.id > 0) {                          // Up Arrow
                display.setDrawColor(0);
                display.drawBox(OLED_X_OFFSET + 30, OLED_Y_OFFSET + 0, 11, 6);
                display.setDrawColor(1);
                display.drawTriangle(OLED_X_OFFSET + 31, OLED_Y_OFFSET + 5,
                                    OLED_X_OFFSET + 39, OLED_Y_OFFSET + 5,
                                    OLED_X_OFFSET + 35, OLED_Y_OFFSET + 0);
            }
            if (param.id < menu.paramCount) {   // Down Arrrow
                display.setDrawColor(0);
                display.drawBox(OLED_X_OFFSET + 30, OLED_Y_OFFSET + 35, 11, 6);
                display.setDrawColor(1);
                display.drawTriangle(OLED_X_OFFSET + 31, OLED_Y_OFFSET + 36,
                                    OLED_X_OFFSET + 39, OLED_Y_OFFSET + 36,
                                    OLED_X_OFFSET + 35, OLED_Y_OFFSET + 40);
            }    
            if (param.parentFolder > 0) {               // Left Arrow
                display.setDrawColor(0);
                display.drawBox(OLED_X_OFFSET + 0, OLED_Y_OFFSET + 14, 5, 11);
                display.setDrawColor(1);
                display.drawTriangle(OLED_X_OFFSET + 4, OLED_Y_OFFSET + 15,
                                    OLED_X_OFFSET + 4, OLED_Y_OFFSET + 23,
                                    OLED_X_OFFSET + 0, OLED_Y_OFFSET + 19);
            }    
            if (param.type == CRSF_FOLDER) {             // Right Arrow
                display.setDrawColor(0);
                display.drawBox(OLED_X_OFFSET + 66, OLED_Y_OFFSET + 14, 5, 11);
                display.setDrawColor(1);
                display.drawTriangle(OLED_X_OFFSET + 67, OLED_Y_OFFSET + 15,
                                    OLED_X_OFFSET + 67, OLED_Y_OFFSET + 23,
                                    OLED_X_OFFSET + 71, OLED_Y_OFFSET + 19);
            }

            // Draw or animate current menu item
            animateMenu(menu);
            break;
    }

    display.sendBuffer();
}



void drawMessage(char title[], char line1[], char line2[]) {
    // U8g2 full frame buffer drawing process
    display.clearBuffer();
    // Ensure draw color is set to white/on
    display.setDrawColor(1);
    // ==========================================
    // 1. Title
    // ==========================================
    // 7 pixel high Nokia font for title
    display.setFont(u8g2_font_squeezed_b6_tr); 
    
    int xOffset = (12 - strlen(title))*7;
    display.setCursor(xOffset + OLED_X_OFFSET, 8 + OLED_Y_OFFSET);
    display.print(title);
    
    display.setFont(u8g2_font_nokiafc22_tf); // Back to a tiny font (~5px baseline)

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


void drawCalibrationScreen(int stage, int secondsLeft) {
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




