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
#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include "params.h"

class CRSF;  // Forward Declaration

// #define ELRSDEBUG

#define LABEL_PACKET_RATE               "Packet Rate"
#define LABEL_TX_POWER_STRING           "TX Power"
#define LABEL_MAX_POWER                 "Max Power"
#define LABEL_DYNAMIC_POWER             "Dynamic"
#define LABEL_BIND_TX                   "Bind"
#define LABEL_ENABLE_WIFI               "Enable WiFi"

#define STICK_POS_HIGH                  3000
#define STICK_POS_LOW                   1000
#define BUTTON_PRESS_DURATION           100

#define CRSF_MAX_PACKET_SIZE            64
#define CRSF_PAYLOAD_SIZE_MAX           60
#define CRSF_PACKET_LENGTH              22
#define CRSF_PACKET_SIZE                26
#define CRSF_FRAME_LENGTH               24 // length of type + payload + crc
#define CRSF_CMD_PACKET_SIZE            8
#define CRSF_TLM_PACKET_SIZE            10
// #define CRSF_MAX_PARAMS                 32
// #define CRSF_MAX_STRING_LEN             32
// #define CRSF_MAX_PARAM_DATA_LEN         256
#define CRSF_SUBTYPE_OPENTX_SYNC        0x10
#define CRSF_LOAD_QUEUE_MAX_ITEMS       64  // Can be anything, but lets just make it a bunch bigger than the expected size (21-22)

// ELRS (CRSF) Frame types
#define ELRS_CHANNELS                   0x16
#define ELRS_LINK_STATS                 0x14
#define ELRS_DEVICE_PING                0x28
#define ELRS_DEVICE_INFO                0x29
#define ELRS_SETTINGS_RESPONSE          0x2B
#define ELRS_SETTINGS_READ              0x2C
#define ELRS_HEARTBEAT                  0x2D
#define ELRS_STATUS                     0x2E
#define ELRS_RADIO_ID                   0x3A

// Device addressess
#define HANDSET_ADDRESS                 0xEA
#define MODULE_ADDRESS                  0xEE
#define LUA_SCRIPT_ADDRESS              0xEF
#define BROADCAST_ADDRESS               0x00
#define SYNC_BYTE                       0xC8


// Simple LIFO Stack for tracking pending parameters to query
struct LoadStack {
    uint8_t items[CRSF_LOAD_QUEUE_MAX_ITEMS];
    int8_t top = -1;

    bool push(uint8_t paramIndex) {
        if (top >= CRSF_LOAD_QUEUE_MAX_ITEMS - 1 || paramIndex < 0) return false;
        // Avoid adding duplicate IDs already waiting in line
        for (int i = 0; i <= top; i++) {
            if (items[i] == paramIndex) return true; 
        }
        items[++top] = paramIndex;
        return true;
    }

    int16_t pop() {
        if (top < 0) return -1;
        return items[top--];
    }

    bool isEmpty() {
        return top == -1;
    }

    void clear() {
        top = -1;
    }
};

// ELRS Status structure
struct crsfElrsStatus {
    uint8_t  packetsBad;
    uint16_t packetsGood;
    bool     isConnected;
    bool     modelMismatch;
    bool     isArmed;
    char     statusMessage[CRSF_MAX_STRING_LEN];
};

struct crsfModule {
    char name[CRSF_MAX_STRING_LEN];
    uint8_t serialNumber[4];
    uint8_t hwVersion[4];
    uint8_t swVersion[4];
    uint8_t paramCount;
    uint8_t protocolVersion;
    bool paramsLoaded;

    // crsfParameter params[CRSF_MAX_PARAMS];
    ParamCollection params;
};


class ELRS {
public:
    // Properties
    bool ready() const { return connectionState == ELRS_READY; }
    bool showMenu = false;
    bool popup = false;
    crsfModule txModule;
    crsfElrsStatus elrsStatus;


    // Methods
    ELRS(CRSF& crsfInstance);   // initializer - pass in the CRSF instance so it can call its references.
    void update();              // driver function
    void editParamSave();       // Save the selected parameter with the edit value
    // void executeParamCommand();

private:
    CRSF& crsf;  // Reference link to the CRSF instance
    enum crsfConnectState {ELRS_BOOT_DELAY, ELRS_PINGING, ELRS_LOAD_PARAMS, ELRS_READY};
    crsfConnectState connectionState = ELRS_BOOT_DELAY;
    LoadStack loadQueue;
    int16_t activeExpectedParamIndex = -1; 
    bool moduleInfoReceived = false;
    uint32_t lastHandshakeTime = 0;   
    uint32_t lastParameterQueryTime = 0;
    bool parameterDiscoveryActive = false;
    int16_t currentSettingsIndex = -1;
    uint8_t currentChunk = 0;
    uint8_t chunksRemaining = 0;
    uint8_t settingAttemptsCounter = 0;
    uint8_t totalSettingsCount;

    uint32_t lastLinkStatsFrameTime = 0;   
    uint32_t lastLinkStatRequestTime = 0;
    uint32_t lastSyncPacketDisplay = 0;

    void sendCommand(uint8_t command, uint8_t value);  // Enqueue command
    void pingDevices();
    void requestElrsStatus();
    void requestSetting(uint8_t settingIndex, uint8_t chunk);
    //void parseLinkStatsPacket(uint8_t rxBuffer[]);
    void parseDeviceInfoPacket(uint8_t rxBuffer[], uint8_t length);
    void parseSettingsPacket(uint8_t rxBuffer[], uint8_t length);   
    void parseElrsStatusPacket(uint8_t rxBuffer[], uint8_t length);
    
    // wip - copy functionality from Lua
    void parseParameterInfoPacket(uint8_t rxBuffer[], uint8_t length);

    void loadAllParams(uint8_t totalCount);
    void loadOneParam(uint8_t paramIndex);
    void parseChoicesString(int paramIndex);
    void clearModule();
};

/*
Parent   Index    Label
0     -  0     -  HooJ  | Menu (ID: 0)
0     -  1     -  Packet Rate | Choices(0-9): 50Hz(-115dBm), 100Hz Full(-112dBm), 150Hz(-112dBm), 250Hz(-108dBm), 333Hz Full(-105dBm), 500Hz(-105dBm), D250(-104dBm), D500(-104dBm), F500(-104dBm), F1000(-104dBm) | Active Selection: 500Hz(-105dBm)
0     -  2     -  Telem Ratio | Choices(0-9): Std, Off, 1:128, 1:64, 1:32, 1:16, 1:8, 1:4, 1:2, Race | Active Selection: 1:32 (546bps)
0     -  3     -  Switch Mode | Choices(0-1): Wide, Hybrid | Active Selection: Wide
0     -  4     -  Link Mode | Choices(0-1): Normal, MAVLink | Active Selection: Normal
0     -  5     -  Model Match | Choices(0-1): Off, On | Active Selection: Off (ID: 0)
0     -  6     -  TX Power (100mW Dyn) | Menu (ID: 6)
6     -  7     -  Max Power | Choices(0-5): 10, 25, 50, 100, 250, 500 | Active Selection: 100mW
6     -  8     -  Dynamic | Choices(0-5): Off, Dyn, AUX9, AUX10, AUX11, AUX12 | Active Selection: Dyn
0     -  9     -  VTX Administrator | Menu (ID: 9)
9     -  10    -  Band | Choices(0-6): Off, A, B, E, F, R, L | Active Selection: Off
9     -  11    -  Channel (1-8): 1
9     -  12    -  Pwr Lvl | Choices(0-8): -, 1, 2, 3, 4, 5, 6, 7, 8 | Active Selection: -
9     -  13    -  Pitmode | Choices(0-21): Off, On, AUX1�, AUX1�, AUX2�, AUX2�, AUX3�, AUX3�, AUX4�, AUX4�, AUX5�, AUX5�, AUX6�, AUX6�, AUX7�, AUX7�, AUX8�, AUX8�, AUX9�, AUX9�, AUX10�, AUX10� | Active Selection: Off
9     -  14    -  Send VTx | Command (Current state IDLE | Timeout 2000 | Info/Status : )
0     -  15    -  WiFi Connectivity | Menu (ID: 15)
15    -  16    -  Enable WiFi | Command (Current state IDLE | Timeout 2000 | Info/Status : )
15    -  17    -  Enable Rx WiFi | Command (Current state IDLE | Timeout 2000 | Info/Status : )
0     -  18    -  BLE Joystick | Command (Current state IDLE | Timeout 2000 | Info/Status : )
0     -  19    -  Bind | Command (Current state IDLE | Timeout 2000 | Info/Status : )
0     -  20    -  Bad/Good | Info String: 0/126
0     -  21    -  3.6.0 ISM
*/
