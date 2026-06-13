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

//#define USE_M7

/*
 =======================================================================================================
 Simple TX CONFIG OPTIONS (comment out unneeded options)
 =======================================================================================================
 */
// Define analog input limits
#define ADC_MIN 0
#define ADC_MID 511
#define ADC_MAX 1023

#define ANALOG_CUTOFF 150 // cut off lower and upper end to avoid un-symmetric joystick range in trade off resolution

// Define Reverse 1=reverse
int Is_Aileron_Reverse  =1;
int Is_Elevator_Reverse =0;
int Is_Throttle_Reverse =0;
int Is_Rudder_Reverse   =0;
int Is_Arm_Reverse   =0;

// Active buzzer only plays one tone (often used on Flight controllers as a beeper)
// Can also use a vibrator motor
//#define ACTIVE_BUZZER

//----- Voltage monitoring -------------------------
// Define battery warning voltage
const float VOLTAGE_SCALE = 436.0;
const float WARNING_VOLTAGE = 7.4; // 2S Lipo 3.7v per cell
const float BEEPING_VOLTAGE = 7.0; // 2S Lipo 3.5v per cell
const float ON_USB = 4.5;          // On USB power / no battery

// Define Commond for start Up Setting
#define RC_MIN_COMMAND 600
#define RC_MAX_COMMAND 1400

// Define stick unmove alarm time
#define STICK_ALARM_TIME 300000 // 300s or 5 minutes

// ELRS 3.x (ESP8266 based TX module): with thanks to r-u-t-r-A (https://github.com/r-u-t-r-A/STM32-ELRS-Handset/tree/v4.5)
//  1 : Set Lua [Packet Rate]= 0 - 50Hz / 1 - 100Hz Full / 2- 150Hz / 3 - 250Hz / 4 - 333Hz Full / 5 - 500Hz
//  2 : Set Lua [Telem Ratio]= 0 - Std / 1 - Off / 2 - 1:128 / 3 - 1:64 / 4 - 1:32 / 5 - 1:16 / 6 - 1:8 / 7 - 1:4 / 8 - 1:2 / 9 - Race
//  3 : Set Lua [Switch Mode]=0 -> Hybrid;Wide
//  4 : Set Lua [Model Match]=0 -> Off;On
//  5 : Set Lua [TX Power]=0 Submenu
// 6 : Set Lua [Max Power]=0 - 10mW / 1 - 25mW / 2 - 50mW /3 - 100mW/4 - 250mW  // dont force to change, but change after reboot if last power was greater
// 7 : Set Lua [Dynamic]=0 - Off / 1 - Dyn / 2 - AUX9 / 3 - AUX10 / 4 - AUX11 / 5 - AUX12
// 8 : Set Lua [VTX Administrator]=0 Submenu
// 9 : Set Lua [Band]=0 -> Off;A;B;E;F;R;L
// 10:  Set Lua [Channel]=0 -> 1;2;3;4;5;6;7;8
// 11 : Set Lua [Pwr Lvl]=0 -> -;1;2;3;4;5;6;7;8
// 12 : Set Lua [Pitmode]=0 -> Off;On 
// 13 : Set Lua [Send VTx]=0 sending response for [Send VTx] chunk=0 step=2
// 14 : Set Lua [WiFi Connectivity]=0 Submenu
// 15 : Set Lua [Enable WiFi]=0 sending response for [Enable WiFi] chunk=0 step=0
// 16 : Set Lua [Enable Rx WiFi]=0 sending response for [Enable Rx WiFi] chunk=0 step=2
// //17 : Set Lua [BLE Joystick]=0 sending response for [BLE Joystick] chunk=0 step=0  // not on ESP8266??
// //    Set Lua [BLE Joystick]=1 sending response for [BLE Joystick] chunk=0 step=3
// //    Set Lua [BLE Joystick]=2 sending response for [BLE Joystick] chunk=0 step=3
// 17: Set Lua [Bind]=0 -> 

// 3 Default Settings
#define SETTING_1_PktRate 5 // 500Hz (-105dB)
#define SETTING_1_Power 5   // 500mW
#define SETTING_1_Dynamic 1 // Dynamic power on

#define SETTING_2_PktRate 3 // 250Hz (-108dB)
#define SETTING_2_Power 3   // 100mW
#define SETTING_2_Dynamic 0 // Dynamic power off

enum chan_order
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