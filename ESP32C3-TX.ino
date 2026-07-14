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
 * <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <Preferences.h>
#include "hardware.h"
#include "config.h"
#include "crsf.h"
#include "elrs.h"
#include "oled.h"

//#define DEBUG // if not commented out, Serial.print() is active! For debugging only!!
//#define TIMINGDEBUG  // When this is active Serial port will print out timing details

//----- global variables for inputs -------------------
struct InputValues {
    int32_t leftX;
    int32_t leftY;
    int32_t rightX;
    int32_t rightY;
    int16_t AUX1;
    int16_t AUX2;
    int16_t AUX3;
};
InputValues stickValues;

struct ADCValues {
    int32_t leftX;
    int32_t leftY;
    int32_t rightX;
    int32_t rightY;
    int32_t voltage;
};
ADCValues rawValues;

struct FilterState {
    int32_t rightX;
    int32_t rightY;
    int32_t leftY;
    int32_t leftX;
};
FilterState filteredRaw;

float batteryVoltage;

bool ARMED = false;

// 0 = No move/button
// 1 = Up
// 2 = Down
// 3 = Left
// 4 = Right
// 5 = Joystick button
// 6 = Built in button (short press)
// 7 = Built in button (long press)
uint8_t navigate = 0;

//----- function headers ----------------------------------
inline int32_t lowLatencyFilter(int32_t currentRaw, int32_t *previousFiltered);
int16_t calibrate(int16_t raw, int16_t minVal, int16_t centerVal, int16_t maxVal);
int32_t applyCyclicExpo(int32_t stickDiff, uint8_t expoFactor);
int32_t applyThrottleExpo(int32_t adcVal, uint8_t expoFactor);
uint16_t processStick(int32_t adcValue, uint8_t chIndex);
void mapChannels();
void getAnalogInputs();
void getDigitalInputs();
void checkGestureSetting();
bool checkStickMove();
void playTone(uint8_t timesPerSecond);
void blinkLED(int ledPin, uint16_t blinkRate);
void stickMenuNavigation();
void statusDisplay();
void logData();

//----- global variables for ELRS -------------------------
int16_t rcChannels[CRSF_MAX_CHANNEL];
CRSF crsfClass;
// Instantiate ELRS (Module) class, passing CRSF driver object to it
ELRS elrsClass(crsfClass);

// -----------------------------------------------------------------------------------------------------
// Calibration functions
// -----------------------------------------------------------------------------------------------------
void calibrationLoad();
void calibrationSave();
void calibrationChirp(uint8_t times);
bool calibrationRun();

Preferences prefs;
#define CALIB_CENT_TMO  5000    //ms
#define CALIB_MOVE_TMO  15000   // ms
bool calibrationRequested=false;
uint32_t calibrationTimerStart;
int      cal_reset = 0 ;

struct CalibValues {
    int rightXMin  = 0;
    int rightXMax  = 4095;
    int rightXMid  = 2048;
    int rightYMin  = 0;
    int rightYMax  = 4095;
    int rightYMid  = 2048;
    int leftXMin   = 0;
    int leftXMax   = 4095;
    int leftXMid   = 2048;
    int leftYMin   = 0;
    int leftYMax   = 4095;
    int leftYMid   = 2048;
};
CalibValues calValues;

void calibrationSave() {
    // Open the "gimbal" namespace in Read-Write mode (false)
    prefs.begin("gimbal", false);
    
    prefs.putShort("rightXMin",  calValues.rightXMin);
    prefs.putShort("rightXMid", calValues.rightXMid);
    prefs.putShort("rightXMax",  calValues.rightXMax);
    
    prefs.putShort("rightYMin",  calValues.rightYMin);
    prefs.putShort("rightYMid", calValues.rightYMid);
    prefs.putShort("rightYMax",  calValues.rightYMax);
    
    prefs.putShort("leftXMin",  calValues.leftXMin);
    prefs.putShort("leftXMid", calValues.leftXMid);
    prefs.putShort("leftXMax",  calValues.leftXMax);

    prefs.putShort("leftYMin",  calValues.leftYMin);
    prefs.putShort("leftYMid",  calValues.leftYMid);
    prefs.putShort("leftYMax",  calValues.leftYMax);
        
    prefs.end();
}

void calibrationLoad() {
    // Open the "gimbal" namespace in Read-Only mode
    prefs.begin("gimbal", true);
    
    // Read values, providing safe defaults if no data has been saved yet
    calValues.rightXMin = prefs.getShort("rightXMin", 0);
    calValues.rightXMid = prefs.getShort("rightXMid", 2048);
    calValues.rightXMax = prefs.getShort("rightXMax", 4095);
    
    calValues.rightYMin = prefs.getShort("rightYMin", 0);
    calValues.rightYMid = prefs.getShort("rightYMid", 2048);
    calValues.rightYMax = prefs.getShort("rightYMax", 4095);
    
    calValues.leftXMin  = prefs.getShort("leftXMin", 0);
    calValues.leftXMid  = prefs.getShort("leftXMid", 2048);
    calValues.leftXMax  = prefs.getShort("leftXMax", 4095);
    
    calValues.leftYMin  = prefs.getShort("leftYMin", 0);
    calValues.leftYMid  = prefs.getShort("leftYMid", 2048);
    calValues.leftYMax  = prefs.getShort("leftYMax", 4095);
    
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
    calValues.rightXMin = 65535;
    calValues.rightXMax = 0;
    calValues.rightXMid = 0;
    calValues.rightYMin = 65535;
    calValues.rightYMax = 0;
    calValues.rightYMid = 0;
    calValues.leftXMin  = 65535;
    calValues.leftXMax  = 0;
    calValues.leftXMid  = 0;
    calValues.leftYMin  = 65535;
    calValues.leftYMax  = 0;
    calValues.leftYMid  = 0;
    cal_reset++;
    }

    uint32_t currentMillis = millis();

    if (currentMillis <= (calibrationTimerStart + CALIB_CENT_TMO)){
        // A Center
        int val = analogRead(STICK_RIGHT_X);
        calValues.rightXMid = val;
        
        // E Center
        val = analogRead(STICK_RIGHT_Y);
        calValues.rightYMid = val;

        // T Center
        val = analogRead(STICK_LEFT_Y);
        calValues.leftYMid = val;
    
        // R Center
        val = analogRead(STICK_LEFT_X);
        calValues.leftXMid = val;
 
        drawCalibrationScreen(2,((int)(calibrationTimerStart + CALIB_CENT_TMO - currentMillis)/1000)+1);  // OLED display update

        Serial.print("Center All Sticks:   Aileron Center:");
        Serial.print(calValues.rightXMid);
        Serial.print(" Elevator Center:");
        Serial.print(calValues.rightYMid);
        Serial.print(" Throttle Center:");
        Serial.print(calValues.leftYMid);
        Serial.print(" Rudder Center:");
        Serial.print(calValues.leftXMid);
        Serial.println();

        blinkLED(DIGITAL_PIN_LED, 1000);  // Slow flash while centering
        
    }
    // 15 seconds for moving sticks
    else if (currentMillis > (calibrationTimerStart + CALIB_CENT_TMO) && currentMillis <  (calibrationTimerStart + CALIB_MOVE_TMO)){
        // A Min-Max
        int val = analogRead(STICK_RIGHT_X);
        if (val < calValues.rightXMin) {
            calValues.rightXMin = val;
        } 
        if (val > calValues.rightXMax) {
            calValues.rightXMax = val;
        }
        // E Min-Max
        val = analogRead(STICK_RIGHT_Y);
        if (val < calValues.rightYMin) {
            calValues.rightYMin = val;
        } 
        if (val > calValues.rightYMax) {
            calValues.rightYMax = val;
        }

        // T Min-Max
        val = analogRead(STICK_LEFT_Y);
        if (val < calValues.leftYMin) {
            calValues.leftYMin = val;
        } 
        if (val > calValues.leftYMax) {
            calValues.leftYMax = val;
        }

        // R Min-Max
        val = analogRead(STICK_LEFT_X);
        if (val < calValues.leftXMin) {
            calValues.leftXMin = val;
        } 
        if (val > calValues.leftXMax) {
            calValues.leftXMax = val;
        }

        drawCalibrationScreen(3,((int)(calibrationTimerStart + CALIB_MOVE_TMO - currentMillis)/1000)+1);  // OLED display update

        Serial.print("Move sticks full range: Aileron Min:");
        Serial.print(calValues.rightXMin);
        Serial.print(" Max:");
        Serial.print(calValues.rightXMax);
        Serial.print(" Elevator Min:");
        Serial.print(calValues.rightYMin);
        Serial.print(" Max:");
        Serial.print(calValues.rightYMax);
        Serial.print(" Rudder Min:");
        Serial.print(calValues.leftXMin);
        Serial.print(" Max:");
        Serial.print(calValues.leftXMax);
        Serial.print(" Throttle Min:");
        Serial.print(calValues.leftYMin);
        Serial.print(" Max:");
        Serial.print(calValues.leftYMax);
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
        lastScreen = CALIBRATION;
    }
    return true;

}

// -----------------------------------------------------------------------------------------------------
// Handle analog input
// -----------------------------------------------------------------------------------------------------
void getAnalogInputs() {
    // Read Voltage
    rawValues.voltage = analogRead(ANALOG_PIN_VOLTAGE);
    batteryVoltage = (float)rawValues.voltage / VOLTAGE_SCALE;

    // 1. Read raw 12-bit ADC values (0 - 4095)
    int32_t currentRightX = analogRead(STICK_RIGHT_X);
    int32_t currentRightY = analogRead(STICK_RIGHT_Y);
    int32_t currentLeftY  = analogRead(STICK_LEFT_Y);
    int32_t currentLeftX  = analogRead(STICK_LEFT_X);
    
    // 2. Apply fast IIR filter to raw hardware values (if enabled in globalSettings)
    rawValues.rightX = lowLatencyFilter(currentRightX, &filteredRaw.rightX);
    rawValues.rightY = lowLatencyFilter(currentRightY, &filteredRaw.rightY);
    rawValues.leftY  = lowLatencyFilter(currentLeftY,  &filteredRaw.leftY);
    rawValues.leftX  = lowLatencyFilter(currentLeftX,  &filteredRaw.leftX);
    
    // 3. Apply calibration mappings
    stickValues.rightX = calibrate(rawValues.rightX, calValues.rightXMin, calValues.rightXMid, calValues.rightXMax);
    stickValues.rightY = calibrate(rawValues.rightY, calValues.rightYMin, calValues.rightYMid, calValues.rightYMax);
    stickValues.leftY  = calibrate(rawValues.leftY,  calValues.leftYMin, calValues.leftYMid, calValues.leftYMax);
    stickValues.leftX  = calibrate(rawValues.leftX,  calValues.leftXMin, calValues.leftXMid, calValues.leftXMax);

    // 4. Hard-coded reverse (Gimbal configuration)
    #ifdef AILERON_REVERSE
        stickValues.rightX  = ADC_MAX-stickValues.rightX;
    #endif
    #ifdef ELEVATOR_REVERSE
        stickValues.rightY = ADC_MAX-stickValues.rightY;
    #endif
    #ifdef THROTTLE_REVERSE
        stickValues.leftY = ADC_MAX-stickValues.leftY;
    #endif
    #ifdef RUDDER_REVERSE
        stickValues.leftX   = ADC_MAX-stickValues.leftX;
    #endif

    // 5. Check if the current movement qualifies for menu navigation
    stickMenuNavigation();

}

// Use right joystick for menu navigation
// Moving past the threshold (50% throw) will move once in that direction
// Go back below the threshold for 100ms before you can initiate another move in any direction
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

    // Check if joystick is currently physically in the center zone
    bool stickCentered = (stickValues.rightY > RC_MIN_COMMAND && stickValues.rightY < RC_MAX_COMMAND &&
                               stickValues.rightX  > RC_MIN_COMMAND && stickValues.rightX  < RC_MAX_COMMAND);

    // Handle Center Debouncing
    if (stickCentered) {
        deflectionStartTime = 0; // Reset deflection timer
        if (centerStartTime == 0) {
            centerStartTime = currentMillis; // Start center debounce timer
        } else if (currentMillis - centerStartTime >= shortPressDuration) {
            isCentered = true;   // Joystick is officially centered
            hasNavigated = false; // Reset navigation lock for the next movement
        }
    } 
    // Handle Deflection Debouncing & Navigation
    else {
        centerStartTime = 0; // Reset center timer
        
        // Determine current physical direction of deflection
        uint8_t currentDirection = 0;
        if (stickValues.rightY >= RC_MAX_COMMAND)       currentDirection = 1; // Up
        else if (stickValues.rightY <= RC_MIN_COMMAND)  currentDirection = 2; // Down
        else if (stickValues.rightX <= RC_MIN_COMMAND)  currentDirection = 3; // Left
        else if (stickValues.rightX >= RC_MAX_COMMAND)  currentDirection = 4; // Right

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

// ADC Filtering, since ESP32 ADCs are supposed to be noisy
inline int32_t lowLatencyFilter(int32_t currentRaw, int32_t *previousFiltered) {
    if (globalSettings.adcFilter && *previousFiltered > 0) {
        // IIR Filter implementation using fast integer bit-shifting:
        // NewFiltered = PreviousFiltered + ((CurrentRaw - PreviousFiltered) >> FILTER_SHIFT)
        *previousFiltered = *previousFiltered + ((currentRaw - *previousFiltered) >> FILTER_SHIFT);
    }
    else {
        *previousFiltered = currentRaw;
    }
    return *previousFiltered;
}

// Calibration maps top and bottom parts of stick deflections separately to account for uneven ADC ranges
int16_t calibrate(int16_t raw, int16_t minVal, int16_t centerVal, int16_t maxVal) {
    raw = constrain(raw, minVal, maxVal);
    
    if (raw <= centerVal) {
        // Map lower half of stick deflection 
        return map(raw, minVal, centerVal, ADC_MIN, ADC_MID);
    } else {
        // Map upper half of stick deflection
        return map(raw, centerVal, maxVal, ADC_MID+1, ADC_MAX);
    }
}

// Expo function for Aileron, Elevator, Rudder (Zero-Centered: -1024 to +1024)
int32_t applyCyclicExpo(int32_t stickDiff, uint8_t expoFactor) {
    if (expoFactor == 0) return stickDiff;
    int32_t stickCubed = (stickDiff * stickDiff * stickDiff) >> 20; 
    return ((100 - expoFactor) * stickDiff + expoFactor * stickCubed) / 100;
}

// For Throttle (Full Range: 0 to 2047)
int32_t applyThrottleExpo(int32_t adcVal, uint8_t expoFactor) {
    if (expoFactor == 0) return adcVal;
    // Shift down to 0-1023 to avoid 32-bit integer overflow during cubing
    int32_t downscaled = adcVal >> 1; 
    int32_t cubed = (downscaled * downscaled * downscaled) >> 20; // 0 to 1023
    int32_t throttleCubed = cubed << 1; // Scale back up to 0-2047
    return ((100 - expoFactor) * adcVal + expoFactor * throttleCubed) / 100;
}

uint16_t processStick(int32_t adcValue, uint8_t chIndex) {
    if (adcValue < ADC_MIN) adcValue = ADC_MIN;
    if (adcValue > ADC_MAX) adcValue = ADC_MAX;

    int32_t crsfOutput = CRSF_DIGITAL_CHANNEL_MAX - CRSF_DIGITAL_CHANNEL_MIN >> 1; // Default to center value
    uint8_t expo = globalSettings.expo[chIndex];
    uint8_t limit = globalSettings.limit[chIndex];

    if (chIndex == THROTTLE && !globalSettings.throttleCentered) {
        // Throttle-specific Expo (Curved from 0)
        adcValue = applyThrottleExpo(adcValue, expo);

        // Top-End Throttle Limit
        adcValue = (adcValue * limit) / 100;

        // User-configured reverse
        if (globalSettings.reverse[chIndex]) {
            adcValue = 2047 - adcValue;
        }

        // Map to CRSF Range (172 to 1812)
        // Ratio: 1640 / 2048 = 0.80078125 -> approximated by (* 820) >> 10
        crsfOutput = ((adcValue * 820) >> 10) + 172;

    } else {
        int32_t stickCentered = adcValue - ADC_MID;

        // Apply Dynamic Deadband
        if (globalSettings.deadband > 0) {
            if (abs(stickCentered) <= globalSettings.deadband) {
                stickCentered = 0;
            } else {
                // Interpolate the stick curve outward past the deadband edge
                if (stickCentered > 0) {
                    stickCentered = ((stickCentered - globalSettings.deadband) * 1024) / (1024 - globalSettings.deadband);
                } else {
                    stickCentered = ((stickCentered + globalSettings.deadband) * 1024) / (1024 - globalSettings.deadband);
                }
            }
        }

        // Expo around center stick
        stickCentered = applyCyclicExpo(stickCentered, expo);

        // Channel limiting
        stickCentered = (stickCentered * limit) / 100;

        // User-configured reverse
        if (globalSettings.reverse[chIndex]) {
            stickCentered = -stickCentered;
        }

        // Map to CRSF Range and center it on 992
        int32_t crsfScaled = (stickCentered * 820) >> 10;
        crsfOutput = crsfScaled + 992;
    }

    if (crsfOutput < CRSF_DIGITAL_CHANNEL_MIN) crsfOutput = CRSF_DIGITAL_CHANNEL_MIN;
    if (crsfOutput > CRSF_DIGITAL_CHANNEL_MAX) crsfOutput = CRSF_DIGITAL_CHANNEL_MAX;

    return (uint16_t)crsfOutput;
}


// Map stick values into traditional standard CRSF channel microsecond positions (1000us to 2000us)
void mapChannels() {
    int mode = globalSettings.channelMode;
    if (mode < 1 || mode > 4) mode = 2; // Default to Mode 2

    switch (mode) {
        case 1:
            rcChannels[AILERON]   = processStick(stickValues.rightX, 0); 
            rcChannels[ELEVATOR]  = processStick(stickValues.leftY,  1);
            rcChannels[THROTTLE]  = processStick(stickValues.rightY, 2);
            rcChannels[RUDDER]    = processStick(stickValues.leftX,  3); 
            break;
        case 2:
            rcChannels[AILERON]   = processStick(stickValues.rightX, 0); 
            rcChannels[ELEVATOR]  = processStick(stickValues.rightY, 1);
            rcChannels[THROTTLE]  = processStick(stickValues.leftY,  2);
            rcChannels[RUDDER]    = processStick(stickValues.leftX,  3); 
            break;
        case 3:
            rcChannels[AILERON]   = processStick(stickValues.leftX,  0); 
            rcChannels[ELEVATOR]  = processStick(stickValues.leftY,  1);
            rcChannels[THROTTLE]  = processStick(stickValues.rightY, 2);
            rcChannels[RUDDER]    = processStick(stickValues.rightX, 3); 
            break;
        case 4:
            rcChannels[AILERON]   = processStick(stickValues.leftX,  0); 
            rcChannels[ELEVATOR]  = processStick(stickValues.rightY, 1);
            rcChannels[THROTTLE]  = processStick(stickValues.leftY,  2);
            rcChannels[RUDDER]    = processStick(stickValues.rightX, 3); 
            break;
    }

    rcChannels[AUX1] = (stickValues.AUX1 == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX2] = (stickValues.AUX2 == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX3] = (stickValues.AUX3 == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;    
}


void getDigitalInputs(){
    /*
     * Handle digital input
     */
    
    // ESP32-C3 Built in Button 
    static uint32_t buttonPressedTime = 0;
    if (digitalRead(DIGITAL_PIN_BUTTON) == LOW) {
        if (buttonPressedTime == 0)
            buttonPressedTime = millis(); // Record start time
    } else {
        if (buttonPressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - buttonPressedTime;
            buttonPressedTime = 0;
            if (pressDuration >= longPressDuration) { // LongPress Duration for this is separate and hard-coded to 2s
                navigate = 7;
                // Set flag to start calibration
                // calibrationRequested = true;
            } else if (pressDuration >= shortPressDuration) {
                navigate = 6; // Built in button pressed
                //nextScreen();
            }
        }
    }

    // Left Joystick/Arm button
    static uint32_t aux1PressedTime = 0;
    if (digitalRead(DIGITAL_PIN_SWITCH_ARM) == LOW) {
        if (aux1PressedTime == 0)
            aux1PressedTime = millis(); // Record time button was pressed
    } else {
        if (aux1PressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - aux1PressedTime;
            aux1PressedTime = 0;
            if (pressDuration >= globalSettings.auxLongPress) {
                if (!ARMED) {
                    // Not armed, long press - initiate menu? TBD
                    // this also protects against accidental presses - just hold longer to 'cancel'
                }
            } else if (pressDuration >= shortPressDuration) {
                stickValues.AUX1 = !stickValues.AUX1; // Toggle AUX1

                // Global ARMED indicator
                ARMED = stickValues.AUX1;
            }
        }
    }

    // Right Joystick/Aux2/3 button
    static uint32_t aux2PressedTime = 0;
    if (digitalRead(DIGITAL_PIN_SWITCH_AUX2) == LOW) {
        if (aux2PressedTime == 0)
            aux2PressedTime = millis(); // Record time button was pressed
    } else {
        if (aux2PressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - aux2PressedTime;
            aux2PressedTime = 0;
            if (pressDuration >= globalSettings.auxLongPress) {
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
}

// -----------------------------------------------------------------------------------------------------
// Startup stick commands
// -----------------------------------------------------------------------------------------------------
void checkGestureSetting() {
    bool currentArmState = stickValues.AUX1; 
    static bool lastArmState = false;        // Tracks previous arm switch position
    static uint32_t armTimer = 0;            // Records when the switch was turned on
    static bool gestureTriggerReady = false; // Arming arm-switch trigger sequence flag

    // Detect the moment the switch goes from DISARMED to ARMED (Rising Edge)
    if (currentArmState && !lastArmState) {
        armTimer = millis();
        gestureTriggerReady = true; 
    }

    // Detect the moment the switch goes from ARMED back to DISARMED (Falling Edge)
    if (!currentArmState && lastArmState) {
        // Debounce: Ensure it was held armed for at least 1000ms
        if (gestureTriggerReady && (millis() - armTimer >= 1000)) {
            //selectSetting();    // Previously used for selecting from a set of ELRS presets. Future use for selecting between model presets?
            gestureTriggerReady = false; 
        }
    }
    lastArmState = currentArmState;
}

bool checkThrottleIdleTimeout(){
    static int previous_throttle = 191;
    static int stickMoved = 0;
    static uint32_t stickMovedMillis = 0;

    // check if throttle stick moved, warning after 5 minutes
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
    // static uint32_t previousToneMillis = 0;
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
    static uint8_t ledState = LOW;
    static uint32_t previousLedMillis = 0;
    uint32_t currentMillis = millis();

    if (currentMillis - previousLedMillis >= blinkRate) {
        previousLedMillis = currentMillis;     // save the last time you blinked the LED
        ledState ^= 1;                      // if the LED is off turn it on and vice-versa
        digitalWrite(ledPin, ledState);
    }
}


// Flash Led and play buzzer depending on current state
void statusDisplay(){
    static int avg = -120;
    int raw = -120;
    int strength = 0;
    char* type;

    if (currentScreen == MODEL_FINDER) {
        // If we are on the model finder screen, prioritize the LED display to show current state
        // Calculations from https://github.com/iamsunilchahal/edgetx-lua-scripts-bw/blob/main/SCRIPTS/TOOLS/ELRS_Finder.lua
        raw = crsfClass.telemetryActive ? crsfClass.linkStats.uplinkRssi1 : -120;
        // Exponential moving average for stability
        avg = 0.8 * avg + 0.2 * (raw);

        // Map avg (~-110..-40) to 0..100 “strength”
        strength = constrain( (avg + 110) * (100/(70)), 0, 100 );    // -110→0, -40→100

        uint16_t period = constrain (1200 - (strength * 10), 100, 1200); // ticks (10ms each): 120→1.2s, 10→0.1s
        blinkLED(DIGITAL_PIN_LED, period);

    } else if (batteryVoltage < WARNING_VOLTAGE && batteryVoltage >= BEEPING_VOLTAGE) {
        blinkLED(DIGITAL_PIN_LED, 500);
    }else if(batteryVoltage < BEEPING_VOLTAGE && batteryVoltage >= ON_USB){
        blinkLED(DIGITAL_PIN_LED, 250);
        playTone(2);
    }

    // check stick idle timer
    if (checkThrottleIdleTimeout() == true){
        blinkLED(DIGITAL_PIN_LED, 100);
        playTone(5);
    }

    // OLED Screen update
    // Global variable navigate will have been set by the getAnalogInputs or getDigitalInputs functions 
    // If no movement was made, navigate will be zero. Otherwise, Up = 1, Down = 2, Left = 3, Right = 4.
    // Joystick (5) or builtin button (6,7 for short/long press) take precedence over joystick movement
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
                    currentScreen = CHANNEL_MENU;
                    break;
                case BT_JOYSTICK: 
                    //startBtJoystick();
                    //currentScreen = MAIN_PAGE;
                    break;
                // Ignore long press builtin button in ELRS_MENU & CHANNEL_MENU & MODEL_FINDER
            }
        } else {
            if (currentScreen == ELRS_MENU) {
                int result = navigateMenu(elrsClass.txModule.params, navigate);
                switch (result) {
                    case -1: // Back
                        if (elrsClass.popup()) {
                            elrsClass.cancelCommand();
                        } 
                        else {
                            currentScreen = ELRS_STATS;  // Exit out of menu
                        }
                        break;
                    case 1: // Save
                        elrsClass.editParamSave();
                        break;
                    case 2:  // Execute Command
                        elrsClass.executeCommand();
                        break;
                    default: // Nothing to do
                        break;
                }
            } else if (currentScreen == CHANNEL_MENU) {
                int result = navigateMenu(handsetSettingsMenu, navigate);
                switch (result) {
                    case -1:
                        // Not using popups in channel menu, so no need to check if popupActive()
                        currentScreen = CHANNEL_OUTPUTS;  // Exit out of menu
                        break;
                    case 1:
                        saveGlobalSettings();
                        break;
                    case 2:  // Execute Command
                        executeSettingsCommand();
                        break;                        
                    default: // Nothing to do
                        break;
                }
            } else {
                if (navigate == 6) {
                    nextScreen();  // Cycle screens when not in a menu
                }
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
        int16_t currentValues[] = {stickValues.rightX,stickValues.rightY,stickValues.leftY,stickValues.leftX,stickValues.AUX1,stickValues.AUX2,stickValues.AUX3};
        
        switch (currentScreen) {
            case MAIN_PAGE:       
                // 0 = ELRS not connected, 1 = Telemetry Active, 4 = BT Joystick mode
                connectionState = (crsfClass.telemetryActive) ? 1 : 0;
                // connectionState = (btJoystickModeActive) ? 4 : connectionState
                drawHomeScreen(batteryVoltage, connectionState, crsfClass.linkStats.uplinkLQ, currentValues,ADC_MIN,ADC_MAX);
                break;
            case ELRS_STATS: 
                drawElrsStatsScreen(elrsClass);
                break; 
            case ELRS_MENU: 
                drawMenuScreen(elrsClass.txModule.params);
                break; 
            case CHANNEL_OUTPUTS: 
                drawChannelOutputsScreen(rcChannels,CRSF_DIGITAL_CHANNEL_MIN,CRSF_DIGITAL_CHANNEL_MAX);
                break;
            case CHANNEL_MENU: 
                drawMenuScreen(handsetSettingsMenu);
                break; 
            // case BT_JOYSTICK: 
            //     drawBtJoystickScreen();
            //     break;
            case MODEL_FINDER: 
                drawModelFinderScreen(raw,avg,strength);
                break;
        }     
    }
}

void logData(){
    static uint32_t lastDisplayTime = 0;
    if (millis() - lastDisplayTime > 250) {
        lastDisplayTime = millis();

        char buf[6]; 
        // Serial.print("Raw AETR:");
        // sprintf(buf, "%5d", rawValues.rightX);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.rightY);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.leftY);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.leftX);
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

    pinMode(STICK_RIGHT_X, INPUT);
    pinMode(STICK_RIGHT_Y, INPUT);
    pinMode(STICK_LEFT_Y, INPUT);
    pinMode(STICK_LEFT_X, INPUT);
    pinMode(ANALOG_PIN_VOLTAGE, INPUT);

    pinMode(DIGITAL_PIN_SWITCH_ARM, INPUT_PULLUP);
    pinMode(DIGITAL_PIN_SWITCH_AUX2, INPUT_PULLUP);

    pinMode(DIGITAL_PIN_LED, OUTPUT);    // LED
    //pinMode(DIGITAL_PIN_BUZZER, OUTPUT); // BUZZER
    pinMode(DIGITAL_PIN_BUTTON, INPUT_PULLUP);
    
    calibrationLoad();
    loadGlobalSettings(true);
    
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
uint32_t maxElrsTime = 0;
uint32_t minElrsTime = 999999;
uint32_t lastStatsPrintTime = 0;
uint32_t totalTransmitFrames = 0;
#endif
uint32_t lastSyncPrintTime = 0;

// -----------------------------------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------------------------------
void loop()
{
    static uint32_t crsfTime = 0;
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
        elrsClass.update(currentScreen == ELRS_MENU);       

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
