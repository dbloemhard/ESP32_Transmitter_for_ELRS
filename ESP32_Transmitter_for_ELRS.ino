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
#define TIMINGDEBUG  // When this is active Serial port will print out timing details

//----- global variables for inputs -------------------
int Aileron_value = 0; // values read from the pot
int Elevator_value = 0;
int Throttle_value = 0;
int Rudder_value = 0;
int previous_throttle = 191;

struct ADCValues {
    int16_t aileron;
    int16_t elevator;
    int16_t throttle;
    int16_t rudder;
    int16_t voltage;
};
ADCValues rawValues;

int AUX1_Arm = 0; // switch values read from the digital pin
int AUX2_value = 0;
int AUX3_value = 0;
int AUX4_value = 0;

float batteryVoltage;
unsigned long buttonStartTime = 0;
const unsigned long longPressDuration = 2000;
const unsigned long shortPressDuration = 100;
void checkButtonPress();
//----- global variables for tone/led --------------
unsigned long previousToneMillis = 0;
bool started=false;
uint8_t ledState = LOW;
unsigned long previousLedMillis = 0;
void blinkLED(int ledPin, uint16_t blinkRate);
void playTone(uint8_t timesPerSecond);

//----- global variables for ELRS -------------------------
int loopCount = 0; // for ELRS seeting

int currentPktRate = 0;
int currentPower = 0;
int currentDynamic = 0;
int currentSetting = 0;
bool bindStarted = false;
bool wifiStarted = false;
int stickMoved = 0;
int stickInt = 0;
uint32_t stickMovedMillis = 0;
bool calibrationRequested=false;

bool lastArmState = false;        // Tracks previous arm switch position
uint32_t armTimer = 0;            // Records when the switch was turned on
bool gestureTriggerReady = false; // Arming arm-switch trigger sequence flag

uint32_t currentMillis = 0;
uint32_t lastDisplayTime = 0;

int16_t rcChannels[CRSF_MAX_CHANNEL];
uint32_t crsfTime = 0;
uint32_t lastModuleRequestTime = 0;
CRSF crsfClass;

// Instantiate application layer, passing driver object to it
ELRSLua elrsLua(crsfClass);

// -----------------------------------------------------------------------------------------------------
// Calibration
Preferences prefs;

#define CALIB_CNT       50       // times of switch on/off
#define CALIB_CENT_TMO  5000    //ms
#define CALIB_TMO       15000   // ms
int     cal_reset = 0 ;

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

uint8_t aux2cnt = 0;

unsigned long calibrationTimerStart;

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
    if (AUX1_Arm){
        Serial.println("Cannot calibrate while arm switch is active");
        cal_reset = 0;
        calibrationRequested = false;
        delay(1000);
        return false;
    }

    // Reset variables to "centers"
    while(cal_reset<1){
    Serial.println("Starting Calibration");
    showCalibrationScreen(1,0);  // OLED display update
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

    currentMillis = millis();

    if (currentMillis <= (calibrationTimerStart + CALIB_CENT_TMO)){
        // A Center
        int val = analogRead(analogInPinAileron);
        calValues.aileronCenter = val;
        
        // E Center
        val = analogRead(analogInPinElevator);
        calValues.elevatorCenter = val;

        // T Center
        val = analogRead(analogInPinThrottle);
        calValues.throttleCenter = val;
    
        // R Center
        val = analogRead(analogInPinRudder);
        calValues.rudderCenter = val;
 
        showCalibrationScreen(2,((int)(calibrationTimerStart + CALIB_CENT_TMO - currentMillis)/1000)+1);  // OLED display update

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
    else if (currentMillis > (calibrationTimerStart + CALIB_CENT_TMO) && currentMillis <  (calibrationTimerStart + CALIB_TMO)){
        // A Min-Max
        int val = analogRead(analogInPinAileron);
        if (val < calValues.aileronMin) {
            calValues.aileronMin = val;
        } 
        if (val > calValues.aileronMax) {
            calValues.aileronMax = val;
        }
        // E Min-Max
        val = analogRead(analogInPinElevator);
        if (val < calValues.elevatorMin) {
            calValues.elevatorMin = val;
        } 
        if (val > calValues.elevatorMax) {
            calValues.elevatorMax = val;
        }

        // T Min-Max
        val = analogRead(analogInPinThrottle);
        if (val < calValues.throttleMin) {
            calValues.throttleMin = val;
        } 
        if (val > calValues.throttleMax) {
            calValues.throttleMax = val;
        }

        // R Min-Max
        val = analogRead(analogInPinRudder);
        if (val < calValues.rudderMin) {
            calValues.rudderMin = val;
        } 
        if (val > calValues.rudderMax) {
            calValues.rudderMax = val;
        }

        showCalibrationScreen(3,((int)(calibrationTimerStart + CALIB_TMO - currentMillis)/1000)+1);  // OLED display update

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

        showCalibrationScreen(4,0);  // OLED display update

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
int16_t mapStick(int16_t raw, int16_t minVal, int16_t centerVal, int16_t maxVal) {
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

void getStickVals() {
    // 1. Read raw 12-bit ADC integers directly from hardware (0 - 4095)
    rawValues.aileron = analogRead(analogInPinAileron);
    rawValues.elevator = analogRead(analogInPinElevator);
    rawValues.throttle = analogRead(analogInPinThrottle);
    rawValues.rudder = analogRead(analogInPinRudder);
    
    // 2. Map raw values into traditional standard CRSF channel microsecond positions (1000us to 2000us)
    Aileron_value  = mapStick(rawValues.aileron, calValues.aileronMin, calValues.aileronCenter, calValues.aileronMax);
    Elevator_value = mapStick(rawValues.elevator, calValues.elevatorMin, calValues.elevatorCenter, calValues.elevatorMax);
    Throttle_value = mapStick(rawValues.throttle, calValues.throttleMin, calValues.throttleCenter, calValues.throttleMax);
    Rudder_value   = mapStick(rawValues.rudder, calValues.rudderMin, calValues.rudderCenter, calValues.rudderMax);

    // 3. Handle reverse
    if (Is_Aileron_Reverse == 1){
        Aileron_value  = 1023-Aileron_value;
    }
    if (Is_Elevator_Reverse == 1){
        Elevator_value = 1023-Elevator_value;
    }
    if (Is_Throttle_Reverse == 1){
        Throttle_value = 1023-Throttle_value;
    }
    if (Is_Rudder_Reverse == 1){
        Rudder_value   = 1023-Rudder_value;
    }

    // 4. Expo, curves, mixing?
    // TBD
}



// Map Stick data to rcChannels array
void mapChannels(){
    rcChannels[AILERON]   = map(Aileron_value,  ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX); 
    rcChannels[ELEVATOR]  = map(Elevator_value, ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX);
    rcChannels[THROTTLE]  = map(Throttle_value, ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX);
    rcChannels[RUDDER]    = map(Rudder_value,   ADC_MIN, ADC_MAX, CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX);

    if(stickInt=0){
        previous_throttle=rcChannels[THROTTLE];
        stickInt=1;
    }
}

void getAUXInputs(){
    /*
     * Handle digital input
     */

    static uint32_t aux1PressedTime = 0;
    if (digitalRead(DIGITAL_PIN_SWITCH_ARM) == LOW) {
        if (aux1PressedTime == 0)
            aux1PressedTime = millis(); // Record time button was pressed
    } else {
        if (aux1PressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - aux1PressedTime;
            aux1PressedTime = 0;
            if (pressDuration >= AUX_LONG_PRESS) {
                if (!AUX1_Arm) {
                    // Not armed, long press - initiate menu? TBD
                    // this also protects against accidental presses - just hold longer to 'cancel'
                }
            } else if (pressDuration >= shortPressDuration) {
                AUX1_Arm = !AUX1_Arm;
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
                //if (!AUX1_Arm) {
                   // Not armed, long press - initiate menu? TBD
                //}
                AUX3_value = !AUX3_value;
            } else if (pressDuration >= shortPressDuration) {
                AUX2_value = !AUX2_value;
            }
        }
    }
    rcChannels[AUX1] = (AUX1_Arm == 1)   ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX2] = (AUX2_value == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX3] = (AUX3_value == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
}

// -----------------------------------------------------------------------------------------------------
// Startup stick commands
// -----------------------------------------------------------------------------------------------------
void checkGestureSetting() {
    bool currentArmState = AUX1_Arm; 

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

    if (rcChannels[AILERON] < RC_MIN_COMMAND && rcChannels[ELEVATOR] > RC_MAX_COMMAND) { // Elevator up + aileron left
        // currentPktRate = SETTING_1_PktRate;
        // currentPower = SETTING_1_Power;
        // currentDynamic = SETTING_1_Dynamic;
        // currentSetting = 1;
        crsfClass.crsfSendCommand(ELRS_PKT_RATE_COMMAND, SETTING_1_PktRate);
        crsfClass.crsfSendCommand(ELRS_POWER_COMMAND, SETTING_1_Power);
        crsfClass.crsfSendCommand(ELRS_DYNAMIC_POWER_COMMAND, SETTING_1_Dynamic);
    } else if (rcChannels[AILERON] > RC_MAX_COMMAND && rcChannels[ELEVATOR] > RC_MAX_COMMAND) { // Elevator up + aileron right
        // currentPktRate = SETTING_2_PktRate;
        // currentPower = SETTING_2_Power;
        // currentDynamic = SETTING_2_Dynamic;
        // currentSetting = 2;
        crsfClass.crsfSendCommand(ELRS_PKT_RATE_COMMAND, SETTING_2_PktRate);
        crsfClass.crsfSendCommand(ELRS_POWER_COMMAND, SETTING_2_Power);
        crsfClass.crsfSendCommand(ELRS_DYNAMIC_POWER_COMMAND, SETTING_2_Dynamic);
    } else if (rcChannels[AILERON] < RC_MIN_COMMAND && rcChannels[ELEVATOR] < RC_MIN_COMMAND) { // Elevator down + aileron left
        // currentSetting = 3;  // Bind
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
    } else if (rcChannels[AILERON] > RC_MAX_COMMAND && rcChannels[ELEVATOR] < RC_MIN_COMMAND) { // Elevator down + aileron right
        // currentSetting = 4;  // TX Wifi
        crsfClass.crsfSendCommand(ELRS_WIFI_COMMAND, ELRS_START_COMMAND);
        if (!bindStarted) {  // cant start bind while wifi is active
            if (wifiStarted) {
                crsfClass.crsfSendCommand(ELRS_WIFI_COMMAND, ELRS_END_COMMAND);
                wifiStarted = false;
            }
            else {
                crsfClass.crsfSendCommand(ELRS_WIFI_COMMAND, ELRS_START_COMMAND);
                wifiStarted = true;
            }
        }
    // } else {
    //     currentSetting = 0;
    }
}

bool checkStickMove(){
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

void checkButtonPress(){
    if (digitalRead(DIGITAL_PIN_BUTTON) == LOW) {
        if (buttonStartTime == 0)
            buttonStartTime = millis(); // Record start time
    } else {
        if (buttonStartTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - buttonStartTime;
            buttonStartTime = 0;
            if (pressDuration >= longPressDuration) {
                // Set flag to start calibration
                calibrationRequested = true;
            } else if (pressDuration >= shortPressDuration) {
                nextScreen();
            }
        }
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
    unsigned long currentMillis = millis();

    if (currentMillis - previousLedMillis >= blinkRate) {
        previousLedMillis = currentMillis;     // save the last time you blinked the LED
        ledState ^= 1;                      // if the LED is off turn it on and vice-versa
        digitalWrite(ledPin, ledState);
    }
}


// Flash Led and play buzzer depending on current state
void statusDisplay(){
    if (batteryVoltage < WARNING_VOLTAGE && batteryVoltage >= BEEPING_VOLTAGE) {
        blinkLED(DIGITAL_PIN_LED, 500);
    }else if(batteryVoltage < BEEPING_VOLTAGE && batteryVoltage >= ON_USB){
        blinkLED(DIGITAL_PIN_LED, 250);
        playTone(2);
    }else if(currentSetting == 3){
        blinkLED(DIGITAL_PIN_LED, 100);  // Bind (fast flash)
    }else if(currentSetting == 4){
        blinkLED(DIGITAL_PIN_LED, 1000); // Wifi (slow flash)
    }

    // check sticks
    if (checkStickMove() == true){
        blinkLED(DIGITAL_PIN_LED, 100);
        playTone(5);
    }

    int connectionState = 0;
    uint32_t currentTime = millis();
    static uint32_t lastScreenUpdateTime = 0;
    uint16_t screenUpdateTime = (AUX1_Arm == 1)? 500 : 100; // 2hz while armed, 10hz while disarmed
    if (currentTime - lastScreenUpdateTime >= screenUpdateTime) { 
        lastScreenUpdateTime = currentTime;
        switch (currentScreen) {
            case MAIN_PAGE:       
                // 0 = ELRS not connected, 1 = Telemetry Active, 2 = BT Transmitter mode ELRS not connected, 3 = BT Transmitter mode ELRS connected, 4 = BT Joystick mode
                connectionState = (crsfClass.telemetryActive) ? 1 : 0;
                // connectionState += (btTransmitterActive) ? 2 : 0
                // connectionState = (btJoystickModeActive) ? 4 : connectionState
                updateHomeScreen(batteryVoltage, connectionState, crsfClass.linkStats.uplinkLQ, rcChannels,CRSF_DIGITAL_CHANNEL_MIN,CRSF_DIGITAL_CHANNEL_MAX);
                break;
            case ELRS_STATS_PAGE: 
                updateElrsStatsScreen(elrsLua);
                break; 
            // case ELRS_MENU: 
            //     updateElrsStatsScreen(elrsLua);
            //     break; 
            // case CHANNEL_OUTPUTS: 
            //     updateElrsStatsScreen(elrsLua);
            //     break;
            // case CHANNEL_MENU: 
            //     updateElrsStatsScreen(elrsLua);
            //     break; 
            // case BT_JOYSTICK: 
            //     updateBtJoystickScreen(elrsLua);
            //     break;
            // case BT_TRANSMITTER: 
            //     updateBtTxScreen(elrsLua);
            //     break;
        }     
    }
}

void logData(){
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
        Serial.print(AUX1_Arm);
        Serial.print(AUX2_value);
        Serial.print(AUX3_value);
        Serial.print(AUX4_value);
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

    /*
    if (digitalRead(DIGITAL_PIN_BUTTON) == LOW) {
        Serial.print("Button LOW ");
    } else {
        Serial.print("Button HIGH ");   
    }    
    Serial.print("buttonStartTime: ");
    Serial.print(buttonStartTime);
    Serial.print(" millis(): ");
    Serial.println(millis());
    */

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

    pinMode(analogInPinAileron, INPUT);
    pinMode(analogInPinElevator, INPUT);
    pinMode(analogInPinThrottle, INPUT);
    pinMode(analogInPinRudder, INPUT);
    pinMode(VOLTAGE_READ_PIN, INPUT);

    pinMode(DIGITAL_PIN_SWITCH_ARM, INPUT_PULLUP);
    pinMode(DIGITAL_PIN_SWITCH_AUX2, INPUT_PULLUP);

    pinMode(DIGITAL_PIN_LED, OUTPUT);    // LED
    //pinMode(DIGITAL_PIN_BUZZER, OUTPUT); // BUZZER
    pinMode(DIGITAL_PIN_BUTTON, INPUT_PULLUP);
    
    calibrationLoad();
    // inialize voltage:
    batteryVoltage = 0.0;

    // passive buzzer, no startup tone
    // digitalWrite(DIGITAL_PIN_BUZZER, HIGH); //BUZZER OFF
    digitalWrite(DIGITAL_PIN_LED, LOW); // LED ON
    
    // Initialize OLED
    initDisplay();
    showBootLogo();

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
    // Check for long press of the boot button (and set calibration flag)
    checkButtonPress();

    // Calibration (if requested)
    //calibrationRun(AUX1_Arm, AUX2_value);
    if (calibrationRequested)
        calibrationRun();
    else {
        // Read Voltage
        rawValues.voltage = analogRead(VOLTAGE_READ_PIN);
        batteryVoltage = (float)rawValues.voltage / VOLTAGE_SCALE; // 98.5

#ifdef TIMINGDEBUG
        uint32_t oledStartTime = micros();
#endif        // Flash led and use beeper
        statusDisplay();
#ifdef TIMINGDEBUG
            uint32_t oledTime = micros() - oledStartTime;
            if (oledTime > maxOledTime) maxOledTime = oledTime;
            if (oledTime < minOledTime) minOledTime = oledTime;
            cumOledTime += oledTime;
            cntOledRuns++;
#endif
        // Get the gimbal stick values
        getStickVals();

        // map sticks/SBUS data to rcChannels
        mapChannels();

        // Read AUX channels
        getAUXInputs();

        // Check for quick settings change via arm toggle gesture
        checkGestureSetting(); 

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
        Serial.print("Total ELRS Frames   : "); Serial.println(loopCount);
        Serial.print("Max ELRS Duration : "); Serial.print(maxElrsTime); Serial.println(" microseconds");
        Serial.print("Min ELRS Duration : "); Serial.print(minElrsTime); Serial.println(" microseconds");
        Serial.print("Avg ELRS Duration : "); Serial.print(5000000/loopCount); Serial.println(" microseconds");
        Serial.print("Total ELRS Frames   : "); Serial.println(loopCount);
        Serial.println("=======================================================");
        
        // Reset max peak-hold values to check for performance dips in the next window
        maxLoopTime = 0;
        minLoopTime = 0;
        maxElrsTime = 0;
        minElrsTime = 0;
        totalTransmitFrames = 0;
        loopCount = 0;
    }    
#endif
}
