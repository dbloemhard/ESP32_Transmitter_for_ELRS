/*
// Simple ESP32 C3 transmitter
// ESP32 C3 0.42" OLED
// ELRS 2.4G NANO RX as a TX module
// Based on SimpleTX by kkbin505
// https://github.com/kkbin505/Arduino-Transmitter-for-ELRS

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

#include <Arduino.h>
#include <Preferences.h>
#include "hardware.h"
#include "config.h"
#include "crsf.h"
#include "elrsLua.h"
#include "oled.h"

//#define DEBUG // if not commented out, Serial.print() is active! For debugging only!!
//#define TIMINGDEBUG  // When this is active Serial port will print out timing details

//----- global variables for inputs -------------------
struct InputValues {
    int16_t aileron;
    int16_t elevator;
    int16_t throttle;
    int16_t rudder;
    int16_t AUX1;
    int16_t AUX2;
    int16_t AUX3;
};
InputValues stickValues;

struct ADCValues {
    int16_t aileron;
    int16_t elevator;
    int16_t throttle;
    int16_t rudder;
    int16_t voltage;
};
ADCValues rawValues;

float batteryVoltage;
bool calibrationRequested=false;
bool ARMED = false;

int previous_throttle = 191;
const uint32_t longPressDuration = 2000;
const uint32_t shortPressDuration = 100;

// 0 = No move/button
// 1 = Up
// 2 = Down
// 3 = Left
// 4 = Right
// 5 = Joystick button
// 6 = Built in button (short press)
// 7 = Built in button (long press)
uint8_t navigate = 0;

//----- global variables for tone/led --------------
unsigned long previousToneMillis = 0;
bool started=false;
uint8_t ledState = LOW;
unsigned long previousLedMillis = 0;
void blinkLED(int ledPin, uint16_t blinkRate);
void playTone(uint8_t timesPerSecond);

//----- global variables for ELRS -------------------------
bool bindStarted = false;
bool wifiStarted = false;

int16_t rcChannels[CRSF_MAX_CHANNEL];
uint32_t crsfTime = 0;
uint32_t lastModuleRequestTime = 0;
CRSF crsfClass;

// Instantiate application layer, passing driver object to it
ELRSLua elrsLua(crsfClass);

// -----------------------------------------------------------------------------------------------------
// Calibration
Preferences prefs;
#define CALIB_CENT_TMO  5000    //ms
#define CALIB_MOVE_TMO  15000   // ms
uint32_t calibrationTimerStart;
int      cal_reset = 0 ;

struct CalibValues {
    int aileronMin     = 0;
    int aileronMax     = 4095;
    int aileronCenter  = 2048;
    int elevatorMin    = 0;
    int elevatorMax   = 4095;
    int elevatorCenter = 2048;
    int throttleMin    = 0;
    int throttleCenter = 2048;
    int throttleMax   = 4095;
    int rudderMin      = 0;
    int rudderMax     = 4095;
    int rudderCenter  = 2048;
};
CalibValues calValues;

void calibrationSave() {
    // Open the "gimbal" namespace in Read-Write mode (false)
    prefs.begin("gimbal", false);
    
    prefs.putShort("ailMin",  calValues.aileronMin);
    prefs.putShort("ailMid", calValues.aileronCenter);
    prefs.putShort("ailMax",  calValues.aileronMax);
    
    prefs.putShort("eleMin",  calValues.elevatorMin);
    prefs.putShort("eleMid", calValues.elevatorCenter);
    prefs.putShort("eleMax",  calValues.elevatorMax);
    
    prefs.putShort("thrMin",  calValues.throttleMin);
    prefs.putShort("thrMid",  calValues.throttleCenter);
    prefs.putShort("thrMax",  calValues.throttleMax);
    
    prefs.putShort("rudMin",  calValues.rudderMin);
    prefs.putShort("rudMid", calValues.rudderCenter);
    prefs.putShort("rudMax",  calValues.rudderMax);
    
    prefs.end();
}

void calibrationLoad() {
    // Open the "gimbal" namespace in Read-Only mode
    prefs.begin("gimbal", true);
    
    // Read values, providing safe defaults if no data has been saved yet
    calValues.aileronMin    = prefs.getShort("ailMin", 0);
    calValues.aileronCenter = prefs.getShort("ailMid", 2048);
    calValues.aileronMax    = prefs.getShort("ailMax", 4095);
    
    calValues.elevatorMin    = prefs.getShort("eleMin", 0);
    calValues.elevatorCenter = prefs.getShort("eleMid", 2048);
    calValues.elevatorMax    = prefs.getShort("eleMax", 4095);
    
    calValues.throttleMin    = prefs.getShort("thrMin", 0);
    calValues.throttleCenter = prefs.getShort("thrMid", 2048);
    calValues.throttleMax    = prefs.getShort("thrMax", 4095);
    
    calValues.rudderMin    = prefs.getShort("rudMin", 0);
    calValues.rudderCenter = prefs.getShort("rudMid", 2048);
    calValues.rudderMax    = prefs.getShort("rudMax", 4095);
    
    prefs.end();

}

//Flash LED and beep for (times)
void calibrationChirp(uint8_t times) {
    
    digitalWrite(DIGITAL_PIN_LED, HIGH);
    delay(500);
    digitalWrite(DIGITAL_PIN_LED, LOW);
    delay(500);

    for (uint8_t i = 0; i < times; i++) {
        //tone(DIGITAL_PIN_BUZZER,5000,100);
        digitalWrite(DIGITAL_PIN_LED, HIGH);
        delay(100);
        //digitalWrite(DIGITAL_PIN_BUZZER, LOW);
        digitalWrite(DIGITAL_PIN_LED, LOW);
        delay(100);
    }
    digitalWrite(DIGITAL_PIN_LED, LOW);
}

bool calibrationRun() {
    if (ARMED){
        Serial.println("Cannot calibrate while arm switch is active");
        cal_reset = 0;
        calibrationRequested = false;
        delay(1000);
        return false;
    }

    // Reset variables to "centers"
    while(cal_reset<1){
    Serial.println("Starting Calibration");
    drawCalibrationScreen(1,0);  // OLED display update
    calibrationChirp(2);    // start calibration
    calibrationTimerStart = millis();
    const int centerValue = (1023 - ANALOG_CUTOFF - ANALOG_CUTOFF) / 2;
    calValues.aileronMin     = 65535;
    calValues.aileronMax     = 0;
    calValues.aileronCenter  = 0;
    calValues.elevatorMin    = 65535;
    calValues.elevatorMax    = 0;
    calValues.elevatorCenter = 0;
    calValues.throttleMin    = 65535;
    calValues.throttleMax    = 0;
    calValues.throttleCenter = 0;
    calValues.rudderMin      = 65535;
    calValues.rudderMax      = 0;
    calValues.rudderCenter   = 0;
    cal_reset++;
    }

    uint32_t currentMillis = millis();

    if (currentMillis <= (calibrationTimerStart + CALIB_CENT_TMO)){
        // A Center
        int val = analogRead(ANALOG_PIN_AILERON);
        calValues.aileronCenter = val;
        
        // E Center
        val = analogRead(ANALOG_PIN_ELEVATOR);
        calValues.elevatorCenter = val;

        // T Center
        val = analogRead(ANALOG_PIN_THROTTLE);
        calValues.throttleCenter = val;
    
        // R Center
        val = analogRead(ANALOG_PIN_RUDDER);
        calValues.rudderCenter = val;
 
        drawCalibrationScreen(2,((int)(calibrationTimerStart + CALIB_CENT_TMO - currentMillis)/1000)+1);  // OLED display update

        Serial.print("Center All Sticks:   Aileron Center:");
        Serial.print(calValues.aileronCenter);
        Serial.print(" Elevator Center:");
        Serial.print(calValues.elevatorCenter);
        Serial.print(" Throttle Center:");
        Serial.print(calValues.elevatorCenter);
        Serial.print(" Rudder Center:");
        Serial.print(calValues.rudderCenter);
        Serial.println();

        blinkLED(DIGITAL_PIN_LED, 1000);  // Slow flash while centering
        
    }
    // 15 seconds for moving sticks
    else if (currentMillis > (calibrationTimerStart + CALIB_CENT_TMO) && currentMillis <  (calibrationTimerStart + CALIB_MOVE_TMO)){
        // A Min-Max
        int val = analogRead(ANALOG_PIN_AILERON);
        if (val < calValues.aileronMin) {
            calValues.aileronMin = val;
        } 
        if (val > calValues.aileronMax) {
            calValues.aileronMax = val;
        }
        // E Min-Max
        val = analogRead(ANALOG_PIN_ELEVATOR);
        if (val < calValues.elevatorMin) {
            calValues.elevatorMin = val;
        } 
        if (val > calValues.elevatorMax) {
            calValues.elevatorMax = val;
        }

        // T Min-Max
        val = analogRead(ANALOG_PIN_THROTTLE);
        if (val < calValues.throttleMin) {
            calValues.throttleMin = val;
        } 
        if (val > calValues.throttleMax) {
            calValues.throttleMax = val;
        }

        // R Min-Max
        val = analogRead(ANALOG_PIN_RUDDER);
        if (val < calValues.rudderMin) {
            calValues.rudderMin = val;
        } 
        if (val > calValues.rudderMax) {
            calValues.rudderMax = val;
        }

        drawCalibrationScreen(3,((int)(calibrationTimerStart + CALIB_MOVE_TMO - currentMillis)/1000)+1);  // OLED display update

        Serial.print("Move sticks full range: Aileron Min:");
        Serial.print(calValues.aileronMin);
        Serial.print(" Max:");
        Serial.print(calValues.aileronMax);
        Serial.print(" Elevator Min:");
        Serial.print(calValues.elevatorMin);
        Serial.print(" Max:");
        Serial.print(calValues.elevatorMax);
        Serial.print(" Rudder Min:");
        Serial.print(calValues.rudderMin);
        Serial.print(" Max:");
        Serial.print(calValues.rudderMax);
        Serial.print(" Throttle Min:");
        Serial.print(calValues.throttleMin);
        Serial.print(" Max:");
        Serial.print(calValues.throttleMax);
        Serial.println();

        blinkLED(DIGITAL_PIN_LED, 100);  // Fast flash while moving to limits
    }
    else {
        Serial.println("Calibration Done");  
        calibrationSave();

        drawCalibrationScreen(4,0);  // OLED display update

        cal_reset = 0;
        calibrationRequested = false;
        calibrationChirp(3);    // ok
    }
    return true;

}

// -----------------------------------------------------------------------------------------------------
// Handle analog input
// -----------------------------------------------------------------------------------------------------
// Piece-wise map calculation to handle offsets when stick centers aren't perfectly uniform
int16_t calibrate(int16_t raw, int16_t minVal, int16_t centerVal, int16_t maxVal) {
    // Clamp incoming raw value to absolute recorded physical boundaries
    raw = constrain(raw, minVal, maxVal);
    
    if (raw <= centerVal) {
        // Map lower half of stick deflection to standard RC 1000 - 1500 range
        return map(raw, minVal, centerVal, ADC_MIN, ADC_MID);
    } else {
        // Map upper half of stick deflection to standard RC 1500 - 2000 range
        return map(raw, centerVal, maxVal, ADC_MID+1, ADC_MAX);
    }
}

void getAnalogInputs() {
    // Read Voltage
    rawValues.voltage = analogRead(ANALOG_PIN_VOLTAGE);
    batteryVoltage = (float)rawValues.voltage / VOLTAGE_SCALE; // 98.5    

    // 1. Read raw 12-bit ADC integers directly from hardware (0 - 4095)
    rawValues.aileron = analogRead(ANALOG_PIN_AILERON);
    rawValues.elevator = analogRead(ANALOG_PIN_ELEVATOR);
    rawValues.throttle = analogRead(ANALOG_PIN_THROTTLE);
    rawValues.rudder = analogRead(ANALOG_PIN_RUDDER);
    
    // 2. Map raw values into calibrated 0-1023 values
    stickValues.aileron  = calibrate(rawValues.aileron, calValues.aileronMin, calValues.aileronCenter, calValues.aileronMax);
    stickValues.elevator = calibrate(rawValues.elevator, calValues.elevatorMin, calValues.elevatorCenter, calValues.elevatorMax);
    stickValues.throttle = calibrate(rawValues.throttle, calValues.throttleMin, calValues.throttleCenter, calValues.throttleMax);
    stickValues.rudder   = calibrate(rawValues.rudder, calValues.rudderMin, calValues.rudderCenter, calValues.rudderMax);

    // 3. Handle reverse
    if (Is_Aileron_Reverse == 1){
        stickValues.aileron  = ADC_MAX-stickValues.aileron;
    }
    if (Is_Elevator_Reverse == 1){
        stickValues.elevator = ADC_MAX-stickValues.elevator;
    }
    if (Is_Throttle_Reverse == 1){
        stickValues.throttle = ADC_MAX-stickValues.throttle;
    }
    if (Is_Rudder_Reverse == 1){
        stickValues.rudder   = ADC_MAX-stickValues.rudder;
    }

    // 3.5 Expo, curves, mixing?
    // TBD
}


void mapChannels() {
    // Map stick values into traditional standard CRSF channel microsecond positions (1000us to 2000us)
    rcChannels[AILERON]   = map(stickValues.aileron,  ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX); 
    rcChannels[ELEVATOR]  = map(stickValues.elevator, ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX);
    rcChannels[THROTTLE]  = map(stickValues.throttle, ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX);
    rcChannels[RUDDER]    = map(stickValues.rudder,   ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX);  
}


void getDigitalInputs(){
    /*
     * Handle digital input
     */
    static uint32_t buttonPressedTime = 0;
    if (digitalRead(DIGITAL_PIN_BUTTON) == LOW) {
        if (buttonPressedTime == 0)
            buttonPressedTime = millis(); // Record start time
    } else {
        if (buttonPressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - buttonPressedTime;
            buttonPressedTime = 0;
            if (pressDuration >= longPressDuration) {
                navigate = 7;
                // Set flag to start calibration
                // calibrationRequested = true;
            } else if (pressDuration >= shortPressDuration) {
                navigate = 6; // Built in button pressed
                //nextScreen();
            }
        }
    }

    static uint32_t aux1PressedTime = 0;
    if (digitalRead(DIGITAL_PIN_SWITCH_ARM) == LOW) {
        if (aux1PressedTime == 0)
            aux1PressedTime = millis(); // Record time button was pressed
    } else {
        if (aux1PressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - aux1PressedTime;
            aux1PressedTime = 0;
            if (pressDuration >= AUX_LONG_PRESS) {
                if (!ARMED) {
                    // Not armed, long press - initiate menu? TBD
                    // this also protects against accidental presses - just hold longer to 'cancel'
                }
            } else if (pressDuration >= shortPressDuration) {
                stickValues.AUX1 = !stickValues.AUX1; // Toggle AUX1
            }
        }
    }

    static uint32_t aux2PressedTime = 0;
    if (digitalRead(DIGITAL_PIN_SWITCH_AUX2) == LOW) {
        if (aux2PressedTime == 0)
            aux2PressedTime = millis(); // Record time button was pressed
    } else {
        if (aux2PressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - aux2PressedTime;
            aux2PressedTime = 0;
            if (pressDuration >= AUX_LONG_PRESS) {
                //if (!ARMED) {
                   // Not armed, long press - initiate menu? TBD
                //}
                stickValues.AUX3 = !stickValues.AUX3; // Toggle AUX3
            } else if (pressDuration >= shortPressDuration) {
                stickValues.AUX2 = !stickValues.AUX2; // Toggle AUX2
                navigate = 5; // Joystick button pressed
            }
        }
    }
    rcChannels[AUX1] = (stickValues.AUX1 == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX2] = (stickValues.AUX2 == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX3] = (stickValues.AUX3 == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;

    // Global ARMED indicator
    ARMED = stickValues.AUX1;
}

// -----------------------------------------------------------------------------------------------------
// Startup stick commands
// -----------------------------------------------------------------------------------------------------
void checkGestureSetting() {
    bool currentArmState = stickValues.AUX1; 
    static bool lastArmState = false;        // Tracks previous arm switch position
    static uint32_t armTimer = 0;            // Records when the switch was turned on
    static bool gestureTriggerReady = false; // Arming arm-switch trigger sequence flag

    // 1. Detect the moment the switch goes from DISARMED to ARMED (Rising Edge)
    if (currentArmState && !lastArmState) {
        armTimer = millis();
        gestureTriggerReady = true; 
    }

    // 2. Detect the moment the switch goes from ARMED back to DISARMED (Falling Edge)
    if (!currentArmState && lastArmState) {
        // Debounce: Ensure it was held armed for at least 1000ms
        if (gestureTriggerReady && (millis() - armTimer >= 1000)) {
            selectSetting(); 
            gestureTriggerReady = false; 
        }
    }
    lastArmState = currentArmState;
}

void selectSetting() {
    // startup stick commands (rate/power selection / initiate bind / turn on tx module wifi)
    // Right stick:
    // Up Left - Rate/Power setting 1 (250hz / 100mw / Dynamic)
    // Up Right - Rate/Power setting 2 (50hz / 100mw)
    // Down Left - Start TX bind (for 3.4.2 it is now possible to bind to RX easily). Power cycle after binding
    // Down Right - Start TX module wifi (for firmware update etc)
    // Left Stick
    // Throttle MAX - Enable Head Tracking (if defines are set)

    if (stickValues.aileron < RC_MIN_COMMAND && stickValues.elevator > RC_MAX_COMMAND) { // Elevator up + aileron left
        crsfClass.crsfSendCommand(ELRS_PKT_RATE_COMMAND, SETTING_1_PktRate);
        crsfClass.crsfSendCommand(ELRS_POWER_COMMAND, SETTING_1_Power);
        crsfClass.crsfSendCommand(ELRS_DYNAMIC_POWER_COMMAND, SETTING_1_Dynamic);
    } else if (stickValues.aileron > RC_MAX_COMMAND && stickValues.elevator > RC_MAX_COMMAND) { // Elevator up + aileron right
        crsfClass.crsfSendCommand(ELRS_PKT_RATE_COMMAND, SETTING_2_PktRate);
        crsfClass.crsfSendCommand(ELRS_POWER_COMMAND, SETTING_2_Power);
        crsfClass.crsfSendCommand(ELRS_DYNAMIC_POWER_COMMAND, SETTING_2_Dynamic);
    } else if (stickValues.aileron < RC_MIN_COMMAND && stickValues.elevator < RC_MIN_COMMAND) { // Elevator down + aileron left
        if (!wifiStarted) {  // cant start bind while wifi is active
            if (bindStarted) {
                crsfClass.crsfSendCommand(ELRS_BIND_COMMAND, ELRS_END_COMMAND);
                bindStarted = false;
            }
            else {
                crsfClass.crsfSendCommand(ELRS_BIND_COMMAND, ELRS_START_COMMAND);
                bindStarted = true;
            }
        }
    } else if (stickValues.aileron > RC_MAX_COMMAND && stickValues.elevator < RC_MIN_COMMAND) { // Elevator down + aileron right
        crsfClass.crsfSendCommand(ELRS_WIFI_COMMAND, ELRS_START_COMMAND);
        if (!bindStarted) {  // cant start wifi while bind is active
            if (wifiStarted) {
                crsfClass.crsfSendCommand(ELRS_WIFI_COMMAND, ELRS_END_COMMAND);
                wifiStarted = false;
            }
            else {
                crsfClass.crsfSendCommand(ELRS_WIFI_COMMAND, ELRS_START_COMMAND);
                wifiStarted = true;
            }
        }
    }
}

bool checkStickMove(){
    static int stickMoved = 0;
    static uint32_t stickMovedMillis = 0;

    // check if stick moved, warring after 10 minutes
    if(abs(previous_throttle - rcChannels[THROTTLE]) < 30){
        stickMoved = 0;
        //Serial.println(abs(previous_throttle - rcChannels[THROTTLE]));
    }else{
        previous_throttle = rcChannels[THROTTLE];
        stickMovedMillis = millis();
        stickMoved = 1;
    }

    if (millis() - stickMovedMillis > STICK_ALARM_TIME){
       // Serial.println((millis() - stickMovedMillis));
        return true;
    }else{
        return false;
    }
}

void playTone(uint8_t timesPerSecond) {
    // unsigned long currentMillis = millis();
    // //ACTIVE_BUZZER / VIBRATOR
    // if (currentMillis - previousToneMillis <= timesPerSecond*100) {
    //     analogWrite(DIGITAL_PIN_BUZZER, 128);
    // }else if(currentMillis - previousToneMillis > timeSeconde*100 && currentMillis - previousToneMillis <= timeSeconde*200){
    //     analogWrite(DIGITAL_PIN_BUZZER, 0);
    // }else if(currentMillis - previousToneMillis > timeSeconde*200 && currentMillis - previousToneMillis <= timeSeconde*300){
    //     analogWrite(DIGITAL_PIN_BUZZER, 128);
    // }else if(currentMillis - previousToneMillis > timeSeconde*300 && currentMillis - previousToneMillis <= 5000){
    //     analogWrite(DIGITAL_PIN_BUZZER, 0);
    // }else{
    //     previousToneMillis = currentMillis;     // save the last time buzzer play tone
    //     analogWrite(DIGITAL_PIN_BUZZER, 0);
    // }
}

void blinkLED(int ledPin, uint16_t blinkRate) {
    uint32_t currentMillis = millis();

    if (currentMillis - previousLedMillis >= blinkRate) {
        previousLedMillis = currentMillis;     // save the last time you blinked the LED
        ledState ^= 1;                      // if the LED is off turn it on and vice-versa
        digitalWrite(ledPin, ledState);
    }
}


void stickMenuNavigation() {
    // Timers for filtering noise (debouncing)
    static uint32_t centerStartTime = 0;
    static uint32_t deflectionStartTime = 0;
    
    // State tracking variables
    static bool isCentered = true;       // True if joystick is confirmed at center
    static bool hasNavigated = false;    // Prevents continuous triggers while holding
    static uint8_t pendingDirection = 0; // Temporarily stores the direction being debounced

    uint32_t currentMillis = millis();
    navigate = 0; // Reset navigate variable at the start of each loop

    // 1. Check if joystick is currently physically in the center zone
    bool stickCentered = (stickValues.elevator > RC_MIN_COMMAND && stickValues.elevator < RC_MAX_COMMAND &&
                               stickValues.aileron  > RC_MIN_COMMAND && stickValues.aileron  < RC_MAX_COMMAND);

    // 2. Handle Center Debouncing
    if (stickCentered) {
        deflectionStartTime = 0; // Reset deflection timer
        if (centerStartTime == 0) {
            centerStartTime = currentMillis; // Start center debounce timer
        } else if (currentMillis - centerStartTime >= shortPressDuration) {
            isCentered = true;   // Joystick is officially centered
            hasNavigated = false; // Reset navigation lock for the next movement
        }
    } 
    // 3. Handle Deflection Debouncing & Navigation
    else {
        centerStartTime = 0; // Reset center timer
        
        // Determine current physical direction of deflection
        uint8_t currentDirection = 0;
        if (stickValues.elevator >= RC_MAX_COMMAND)      currentDirection = 1; // Up
        else if (stickValues.elevator <= RC_MIN_COMMAND) currentDirection = 2; // Down
        else if (stickValues.aileron <= RC_MIN_COMMAND)  currentDirection = 3; // Left
        else if (stickValues.aileron >= RC_MAX_COMMAND)  currentDirection = 4; // Right

        // If the direction changed mid-deflection, reset the deflection timer
        if (currentDirection != pendingDirection) {
            deflectionStartTime = currentMillis;
            pendingDirection = currentDirection;
        } 
        // If held in the same direction for long enough, check if we can navigate
        else if (currentMillis - deflectionStartTime >= shortPressDuration) {
            if (isCentered && !hasNavigated) {
                navigate = pendingDirection; // Trigger the menu action
                hasNavigated = true;         // Lock execution until center return
                isCentered = false;          // No longer marked as centered
            }
        }
    }
}

// Flash Led and play buzzer depending on current state
void statusDisplay(){
    if (batteryVoltage < WARNING_VOLTAGE && batteryVoltage >= BEEPING_VOLTAGE) {
        blinkLED(DIGITAL_PIN_LED, 500);
    }else if(batteryVoltage < BEEPING_VOLTAGE && batteryVoltage >= ON_USB){
        blinkLED(DIGITAL_PIN_LED, 250);
        playTone(2);
    }else if(bindStarted){
        blinkLED(DIGITAL_PIN_LED, 100);  // Bind (fast flash)
    }else if(wifiStarted){
        blinkLED(DIGITAL_PIN_LED, 1000); // Wifi (slow flash)
    }

    // check sticks
    if (checkStickMove() == true){
        blinkLED(DIGITAL_PIN_LED, 100);
        playTone(5);
    }

    // OLED Screen update
    // If we are in the ELRS Menu or Channel Menu (And neither joystick (5) or builtin button (6,7) have been pressed), 
    // check if the stick movements should navigate the menu.
    // This will update the navigate variable with a number 0~4
    if ((currentScreen == ELRS_MENU || currentScreen == CHANNEL_MENU) && navigate < 5 ) {
        stickMenuNavigation();
    }
    if (navigate > 0) {  // Navigation has occurred
        // 0 = No move/button
        // 1 = Up
        // 2 = Down
        // 3 = Left
        // 4 = Right
        // 5 = Joystick button
        // 6 = Built in button (short press)
        // 7 = Built in button (long press)
        if (navigate == 7) {
            switch (currentScreen) {
                case MAIN_PAGE:
                    calibrationRequested = true;
                    break;
                case ELRS_STATS:
                    currentScreen = ELRS_MENU;
                    break;
                case CHANNEL_OUTPUTS: 
                    //currentScreen = CHANNEL_MENU;
                    break;
                case BT_JOYSTICK: 
                    //startBtJoystick();
                    //currentScreen = MAIN_PAGE;
                    break;
                case BT_TRANSMITTER: 
                    //startBtTx();
                    //currentScreen = MAIN_PAGE;
                    break;
                // Ignore long press builtin button in ELRS_MENU & CHANNEL_MENU
            }
        } else {
            if (currentScreen == ELRS_MENU) {
                navigateELRSMenu(elrsLua, navigate);
            } else if (currentScreen == CHANNEL_MENU) {
                //navigateChannelMenu(navigate);
            } else if (navigate == 6) {
                nextScreen();
            }
        } 
        navigate = 0; // Reset navigation
    }

    int connectionState = 0;
    uint32_t currentTime = millis();
    static uint32_t lastScreenUpdateTime = 0;
    uint16_t screenUpdateTime = (ARMED)? 500 : 100; // 2hz while armed, 10hz while disarmed
    if (currentTime - lastScreenUpdateTime >= screenUpdateTime) { 
        lastScreenUpdateTime = currentTime;
        int16_t currentValues[] = {stickValues.aileron,stickValues.elevator,stickValues.throttle,stickValues.rudder,stickValues.AUX1,stickValues.AUX2,stickValues.AUX3};
        
        switch (currentScreen) {
            case MAIN_PAGE:       
                // 0 = ELRS not connected, 1 = Telemetry Active, 2 = BT Transmitter mode ELRS not connected, 3 = BT Transmitter mode ELRS connected, 4 = BT Joystick mode
                connectionState = (crsfClass.telemetryActive) ? 1 : 0;
                // connectionState += (btTransmitterActive) ? 2 : 0
                // connectionState = (btJoystickModeActive) ? 4 : connectionState
                // updateHomeScreen(batteryVoltage, connectionState, crsfClass.linkStats.uplinkLQ, rcChannels,CRSF_DIGITAL_CHANNEL_MIN,CRSF_DIGITAL_CHANNEL_MAX);
                drawHomeScreen(batteryVoltage, connectionState, crsfClass.linkStats.uplinkLQ, currentValues,ADC_MIN,ADC_MAX);
                break;
            case ELRS_STATS: 
                drawElrsStatsScreen(elrsLua);
                break; 
            case ELRS_MENU: 
                drawElrsMenuScreen(elrsLua);
                break; 
            // case CHANNEL_OUTPUTS: 
            //     drawChannelOutputsScreen(rcChannels);
            //     break;
            // case CHANNEL_MENU: 
            //     drawChannelMenuScreen(elrsLua);
            //     break; 
            // case BT_JOYSTICK: 
            //     drawBtJoystickScreen();
            //     break;
            // case BT_TRANSMITTER: 
            //     drawBtTxScreen();
            //     break;
        }     
    }
}

void logData(){
    static uint32_t lastDisplayTime = 0;
    if (millis() - lastDisplayTime > 250) {
        lastDisplayTime = millis();

        char buf[6]; 
        // Serial.print("Raw AETR:");
        // sprintf(buf, "%5d", rawValues.aileron);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.elevator);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.throttle);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.rudder);
        // Serial.print(buf);

        Serial.print("Bat:");
        sprintf(buf, "%5.2f", batteryVoltage);
        Serial.print("v Channels AETR: ");
        sprintf(buf, "%5d", rcChannels[AILERON]);
        Serial.print(buf);
        sprintf(buf, "%5d", rcChannels[ELEVATOR]);
        Serial.print(buf);
        sprintf(buf, "%5d", rcChannels[THROTTLE]);
        Serial.print(buf);
        sprintf(buf, "%5d", rcChannels[RUDDER]);
        Serial.print(buf);
        Serial.print(" AUX: ");
        Serial.print(stickValues.AUX1);
        Serial.print(stickValues.AUX2);
        Serial.print(stickValues.AUX3);
        if (ARMED) Serial.print("(ARMED)");
        Serial.print("  "); 
        if (crsfClass.moduleConnected){
            // Generate the 4-line bar metric matching EdgeTX's logic
            String barIndicator = "[    ]";
            Serial.print("ELRS ");
            if (crsfClass.telemetryActive) {
                Serial.print("RSSI1: "); Serial.print(crsfClass.linkStats.uplinkRssi1); Serial.print("dBm");
                Serial.print(" | TX Pwr: "); Serial.print(crsfClass.linkStats.txPower); 
                Serial.print(" | LQ: "); Serial.print(crsfClass.linkStats.rfMode); Serial.print(" : "); Serial.print(crsfClass.linkStats.uplinkLQ); Serial.print(" ");

                if (crsfClass.linkStats.uplinkLQ >= 80)      barIndicator = "[====]";
                else if (crsfClass.linkStats.uplinkLQ >= 50) barIndicator = "[=== ]";
                else if (crsfClass.linkStats.uplinkLQ >= 20) barIndicator = "[==  ]";
                else if (crsfClass.linkStats.uplinkLQ > 0)   barIndicator = "[=   ]";
            }
            Serial.print(barIndicator); Serial.print("  "); 
        }

        Serial.println();

    }
}

// -----------------------------------------------------------------------------------------------------
// Board setup
// -----------------------------------------------------------------------------------------------------
void setup()
{
    // inialize rc data
    for (uint8_t i = 0; i < CRSF_MAX_CHANNEL; i++) {
        rcChannels[i] = CRSF_DIGITAL_CHANNEL_MIN;
    }

    // Configure ESP32 analog inputs to 12-bit resolution (0 - 4095)
    // Native ESP32 configuration commands
    analogReadResolution(12);       // Fix resolution layout at 12-bit scale
    analogSetAttenuation(ADC_11db);  // Force 0 - 3.3V full dynamic voltage reading scale

    pinMode(ANALOG_PIN_AILERON, INPUT);
    pinMode(ANALOG_PIN_ELEVATOR, INPUT);
    pinMode(ANALOG_PIN_THROTTLE, INPUT);
    pinMode(ANALOG_PIN_RUDDER, INPUT);
    pinMode(ANALOG_PIN_VOLTAGE, INPUT);

    pinMode(DIGITAL_PIN_SWITCH_ARM, INPUT_PULLUP);
    pinMode(DIGITAL_PIN_SWITCH_AUX2, INPUT_PULLUP);

    pinMode(DIGITAL_PIN_LED, OUTPUT);    // LED
    //pinMode(DIGITAL_PIN_BUZZER, OUTPUT); // BUZZER
    pinMode(DIGITAL_PIN_BUTTON, INPUT_PULLUP);
    
    calibrationLoad();

    // passive buzzer, no startup tone
    // digitalWrite(DIGITAL_PIN_BUZZER, HIGH); //BUZZER OFF
    digitalWrite(DIGITAL_PIN_LED, LOW); // LED ON
    
    // Initialize OLED
    initDisplay();
    drawBootLogo();

    // Serial output over USB (Uart6)
    Serial.begin(9600);

// CRSF over Uart1
#ifdef ELRS_HALF_DUPLEX
    crsfClass.begin(ELRS_RX, ELRS_TX, true);
#else
    crsfClass.begin(ELRS_RX, ELRS_TX, false);
#endif
    delay(3000);
}

#ifdef TIMINGDEBUG
uint32_t startLoopUs = 0;
uint32_t loopCounter = 0;
uint32_t maxLoopTime = 0;
uint32_t minLoopTime = 999999;
uint32_t maxOledTime = 0;
uint32_t minOledTime = 999999;
uint32_t cumOledTime = 0;
uint32_t cntOledRuns = 0;
// uint32_t totLoopTime = 0;
uint32_t maxElrsTime = 0;
uint32_t minElrsTime = 999999;
// uint32_t totElrsTime = 0;
// uint32_t elrsCounter = 0;
uint32_t lastStatsPrintTime = 0;
uint32_t totalTransmitFrames = 0;
#endif
uint32_t lastSyncPrintTime = 0;

// -----------------------------------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------------------------------
void loop()
{
#ifdef TIMINGDEBUG
    static uint32_t nextLogTimeUs = 0;
    uint32_t nowUs = micros();

    if (nextLogTimeUs = 0) nextLogTimeUs = nowUs + 4000;
    loopCounter++;
    if (nowUs >= nextLogTimeUs) {
        startLoopUs = micros();
    }
#endif
    // Calibration (if requested)
    if (calibrationRequested)
        calibrationRun();
    else {
        // Get the gimbal stick values
        getAnalogInputs();

        // Read AUX channels and buttons
        getDigitalInputs();

        // Read Connected bluetooth device inputs
        // getBluetoothInputs();

        // Check for quick settings change via arm toggle gesture
        checkGestureSetting(); 
        
        mapChannels();

#ifdef DEBUG        
        // Debug data
        logData();
#endif
 
        // ---------------------------------------------------------------------------
        // Read Telemetry data every loop (telemetry processed in crsfCheckTelemetry)
        crsfClass.crsfReadTelemetry();

        // -------------------------------------------------
        // Poll the telemetry data and run lua script logic
        elrsLua.update();       

        // --------------------------------
        // Send RC data to external module
        uint32_t currentMicros = micros();
        if (currentMicros > crsfTime) {
            // Command packet interleaving
            if (crsfClass.commandQueue.hasItems()) {
                crsfClass.crsfSendQueuedCommand();
            } else {
                crsfClass.crsfSendChannels(rcChannels);
            }

#ifdef TIMINGDEBUG
            uint32_t elrsTime = micros() - currentMicros;
            if (elrsTime > maxElrsTime) maxElrsTime = elrsTime;
            if (elrsTime < minElrsTime) minElrsTime = elrsTime;
#endif
            uint32_t adjustment = crsfClass.crsfNextInterval();
            crsfTime = currentMicros + adjustment; //CRSF_TIME_BETWEEN_FRAMES_US;
#ifdef DEBUG
            if (millis() - lastSyncPrintTime > 2000) {
                Serial.println("\n============ [Loop timing] ============");
                Serial.print("Next Interval : "); Serial.print(adjustment); Serial.println(" microseconds");
                Serial.println("=======================================");
                lastSyncPrintTime = millis();
            }
#endif
        }

#ifdef TIMINGDEBUG
        uint32_t oledStartTime = micros();
#endif  

        // Update OLED display, flash led and use beeper
        statusDisplay();

#ifdef TIMINGDEBUG
        uint32_t oledTime = micros() - oledStartTime;
        if (oledTime > maxOledTime) maxOledTime = oledTime;
        if (oledTime < minOledTime) minOledTime = oledTime;
        cumOledTime += oledTime;
        cntOledRuns++;
#endif

    } // Not calibrating
#ifdef TIMINGDEBUG
    if (nowUs >= nextLogTimeUs) {
        uint32_t elapsedUs = micros() - startLoopUs;
        if (elapsedUs > maxLoopTime) maxLoopTime = elapsedUs;
        if (elapsedUs < minLoopTime) minLoopTime = elapsedUs;
        totalTransmitFrames++;

        nextLogTimeUs += 4000; 
    }
    // ---------------------------------------------------------------------------
    if (millis() - lastStatsPrintTime > 5000) {
        lastStatsPrintTime = millis();      
        Serial.println("\n============ [HANDSET PERFORMANCE METRICS] ============");
        Serial.print("Max Loop Duration : "); Serial.print(maxLoopTime); Serial.println(" microseconds");
        Serial.print("Min Loop Duration : "); Serial.print(minLoopTime); Serial.println(" microseconds");
        Serial.print("Avg Loop Duration : "); Serial.print(5000000/totalTransmitFrames); Serial.println(" microseconds");
        Serial.print("Total Transmit Frames   : "); Serial.println(totalTransmitFrames);
        Serial.print("Max OLED Duration : "); Serial.print(maxOledTime); Serial.println(" microseconds");
        Serial.print("Min OLED Duration : "); Serial.print(minOledTime); Serial.println(" microseconds");
        Serial.print("Avg OLED Duration : "); Serial.print(cumOledTime/cntOledRuns); Serial.println(" microseconds");
        Serial.print("Total ELRS Frames   : "); Serial.println(loopCounter);
        Serial.print("Max ELRS Duration : "); Serial.print(maxElrsTime); Serial.println(" microseconds");
        Serial.print("Min ELRS Duration : "); Serial.print(minElrsTime); Serial.println(" microseconds");
        Serial.print("Avg ELRS Duration : "); Serial.print(5000000/loopCounter); Serial.println(" microseconds");
        Serial.print("Total ELRS Frames   : "); Serial.println(loopCounter);
        Serial.println("=======================================================");
        
        // Reset max peak-hold values to check for performance dips in the next window
        maxLoopTime = 0;
        minLoopTime = 0;
        maxElrsTime = 0;
        minElrsTime = 0;
        totalTransmitFrames = 0;
        loopCounter = 0;
    }    
#endif
}
