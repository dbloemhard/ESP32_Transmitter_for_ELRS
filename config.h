#pragma once
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
#include "params.h"

//#define USE_M7

/*
 =======================================================================================================
 Simple TX CONFIG OPTIONS (comment out unneeded options)
 =======================================================================================================
 */
#define AUX_LONG_PRESS 1000   // default 1s press for enabling/disabling aux 3 (can be changed in handset settings)

// Define hard-coded gimbal reverse (uncomment to reverse that channel)
#define AILERON_REVERSE
//#define ELEVATOR_REVERSE
#define THROTTLE_REVERSE
//#define RUDDER_REVERSE

// Active buzzer only plays one tone (often used on Flight controllers as a beeper)
// Can also use a vibrator motor
//#define ACTIVE_BUZZER

//----- Voltage monitoring -------------------------
// Define battery warning voltage
const float VOLTAGE_SCALE = 454.5;
const float WARNING_VOLTAGE = 3.7; // 1S Lipo 3.7v per cell
const float BEEPING_VOLTAGE = 3.5; // 1S Lipo 3.5v per cell
const float ON_USB = 1.0;          // On USB power / no battery

// Define Commond for start Up Setting
// ~50% stick deflection
#define RC_MIN_COMMAND 511
#define RC_MAX_COMMAND 1535

// Define stick idle alarm time
#define STICK_ALARM_TIME 300000 // 300s or 5 minutes

// Default Settings
#define SETTING_1_PktRate 5 // 500Hz (-105dB)
#define SETTING_1_Power 5   // 500mW
#define SETTING_1_Dynamic 1 // Dynamic power on

#define SETTING_2_PktRate 3 // 250Hz (-108dB)
#define SETTING_2_Power 3   // 100mW
#define SETTING_2_Dynamic 0 // Dynamic power off

// Define analog input limits. ESP32 supports 12-bit resolution (0 - 4095), but in reality only half of that is useable
#define ADC_MIN 0
#define ADC_MID 1023
#define ADC_MAX 2047

// Filter coefficient: Higher = smoother but more latency. 
// A value of 2 (shifts by 2) means 25% new raw sample, 75% old history. 
#define FILTER_SHIFT 2

enum ChannelOrder
{
    AILERON,
    ELEVATOR,
    THROTTLE,
    RUDDER,
    AUX1, // (CH5)  ARM switch for Expresslrs
    AUX2, // (CH6)  angel / airmode change
    AUX3, // (CH7)  flip after crash
    AUX4, // (CH8)
    AUX5, // (CH9)
    AUX6, // (CH10)
    AUX7, // (CH11)
    AUX8, // (CH12)
};

struct HandsetSettings {
    uint8_t channelMode;
    bool adcFilter;
    uint16_t deadband;
    uint16_t auxLongPress;
    bool throttleCentered;

    bool reverse[4];  // True = reversed for A, E, T, R
    uint8_t limit[4]; // 0 to 100 (%) for A, E, T, R
    uint8_t expo[4];  // 0 to 100 (%) for A, E, T, R
};
extern HandsetSettings globalSettings;
extern ParamCollection handsetSettingsMenu;

void loadGlobalSettings(bool initialize); // pass in true to rebuild from scratch, false will update existing entries
void saveGlobalSettings();  // Saves the currently selected global settings menu item with the current edit value
void executeSettingsCommand(); // Executes the currently selected command, eg reset all settings. Not currently used.
void populateSettingsMenu();
bool addFolder(int idx, int parent, const char* label) ;
bool addInfo(int idx, int parent, const char* label, const char* valueString);
bool addChoiceParam(int idx, int parent, const char* label, const std::vector<const char*>& choices, int currVal, int min, int max);
bool addNumericParam(int idx, int parent, crsfValueType type, const char* label, int currVal, int min, int max, int step, const char* units);

// External Module responses;
// [CRSF] -> Querying Menu Parameter Index: 1, chunk: 4
// Received Packet type 0x2B
// [PARAMETER SAVED] Packet Rate | Choices(0-9): 50Hz(-115dBm), 100Hz Full(-112dBm), 150Hz(-112dBm), 250Hz(-108dBm), 333Hz Full(-105dBm), 500Hz(-105dBm), D250(-104dBm), D500(-104dBm), F500(-104dBm), F1000(-104dBm) | Active Selection: 333Hz Full(-105dBm)
// [CRSF] -> Querying Menu Parameter Index: 2, chunk: 1
// Received Packet type 0x2B
// [PARAMETER SAVED] Telem Ratio | Choices(0-9): Std, Off, 1:128, 1:64, 1:32, 1:16, 1:8, 1:4, 1:2, Race | Active Selection: 1:8 (3219bps)
// Chunk stored[CRSF] -> Querying Menu Parameter Index: 3, chunk: 1
// Received Packet type 0x2B
// [PARAMETER SAVED] Switch Mode | Choices(0-2): 8ch, 16ch Rate/2, 12ch Mixed | Active Selection: 8ch
// [CRSF] -> Querying Menu Parameter Index: 4, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] Model Match | Choices(0-1): Off, On | Active Selection: Off (ID: 0)
// [CRSF] -> Querying Menu Parameter Index: 5, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] TX Power (100mW Dyn) | Menu (ID: 5)
// [CRSF] -> Querying Menu Parameter Index: 6, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] Max Power | Choices(0-5): 10, 25, 50, 100, 250, 500 | Active Selection: 100mW
// Chunk stored[CRSF] -> Querying Menu Parameter Index: 7, chunk: 1
// Received Packet type 0x2B
// [PARAMETER SAVED] Dynamic | Choices(0-5): Off, Dyn, AUX9, AUX10, AUX11, AUX12 | Active Selection: Dyn
// [CRSF] -> Querying Menu Parameter Index: 8, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] VTX Administrator | Menu (ID: 8)
// [CRSF] -> Querying Menu Parameter Index: 9, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] Band | Choices(0-6): Off, A, B, E, F, R, L | Active Selection: Off
// [CRSF] -> Querying Menu Parameter Index: 10, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] Channel | Choices(0-7): 1, 2, 3, 4, 5, 6, 7, 8 | Active Selection: 1
// [CRSF] -> Querying Menu Parameter Index: 11, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] Pwr Lvl | Choices(0-8): -, 1, 2, 3, 4, 5, 6, 7, 8 | Active Selection: -
// Chunk stored[CRSF] -> Querying Menu Parameter Index: 12, chunk: 3
// Received Packet type 0x2B
// [PARAMETER SAVED] Pitmode | Choices(0-21): Off, On, AUX1�, AUX1�, AUX2�, AUX2�, AUX3�, AUX3�, AUX4�, AUX4�, AUX5�, AUX5�, AUX6�, AUX6�, AUX7�, AUX10� | Active Selection: Off
// [CRSF] -> Querying Menu Parameter Index: 13, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] Send VTx | Command (Current state IDLE | Timeout 2000 | Info/Status : )
// [CRSF] -> Querying Menu Parameter Index: 14, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] WiFi Connectivity | Menu (ID: 14)
// [CRSF] -> Querying Menu Parameter Index: 15, chunk: 0
// Received Packet type 0x2B
// [PARAMETER SAVED] Enable WiFi | Command (Current state IDLE | Timeout 2000 | Info/Status : )