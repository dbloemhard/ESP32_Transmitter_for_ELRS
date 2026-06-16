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

class CRSF;  // Forward Declaration

//#define LUADEBUG

#define CRSF_MAX_PACKET_SIZE            64
#define CRSF_PAYLOAD_SIZE_MAX           60
#define CRSF_PACKET_LENGTH              22
#define CRSF_PACKET_SIZE                26
#define CRSF_FRAME_LENGTH               24 // length of type + payload + crc
#define CRSF_CMD_PACKET_SIZE            8
#define CRSF_TLM_PACKET_SIZE            10
#define CRSF_MAX_PARAMS                 32
#define CRSF_MAX_STRING_LEN             32
#define CRSF_MAX_PARAM_DATA_LEN         256
#define CRSF_SUBTYPE_OPENTX_SYNC        0x10

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


// Individual Option/Choice Structure (For SELECT type parameters)
struct crsfOption {
    char text[CRSF_MAX_STRING_LEN];
};

typedef enum
{
    CRSF_UINT8 = 0,
    CRSF_INT8 = 1,
    CRSF_UINT16 = 2,
    CRSF_INT16 = 3,
    CRSF_UINT32 = 4,
    CRSF_INT32 = 5,
    CRSF_UINT64 = 6,
    CRSF_INT64 = 7,
    CRSF_FLOAT = 8,
    CRSF_TEXT_SELECTION = 9,
    CRSF_STRING = 10,
    CRSF_FOLDER = 11,
    CRSF_INFO = 12,
    CRSF_COMMAND = 13,
    CRSF_VTX = 15,
    CRSF_OUT_OF_RANGE = 127,
} crsfValueType;

// Individual Parameter Node Structure
struct crsfParameter {
    uint8_t id;                       // Field Index (3, 4, 5, etc.)
    uint8_t parentFolder;             // Parent Folder ID (0 for root)
    crsfValueType type;                     // 0x09 = Select, 0x0B = Folder, etc.
    int32_t currentVal;               // Currently active option index 
    int32_t minVal;                   // Used for numeric parameters
    int32_t maxVal;                   // Numeric parameters and String type max length
    int32_t precision;                // Only for 0x08 float type
    uint8_t step;                     // Adjustment step / Command state step
    uint32_t timeout;                  // Command timeout
   
    char label[CRSF_MAX_STRING_LEN];  
    char valueString[CRSF_MAX_PARAM_DATA_LEN];
    uint8_t valueStringCharCount;
    char units[CRSF_MAX_STRING_LEN];
    crsfOption choices[CRSF_MAX_PARAMS]; // Extracted selectable strings (e.g., {"Off", "On"}) 
    uint8_t choicesCount;                // Number of options successfully parsed
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

    crsfParameter params[CRSF_MAX_PARAMS];
};

class ELRSLua {
public:
    // Properties
    bool ready() const { return connectionState == ELRS_READY; }
    bool showMenu = false;
    crsfModule txModule;
    crsfElrsStatus elrsStatus;

    // Methods
    ELRSLua(CRSF& crsfInstance);   // initializer - pass in the CRSF instance so it can call its references.
    void update();                 // driver function

private:
    CRSF& crsf;  // Reference link to the CRSF instance
    
    enum crsfConnectState {ELRS_BOOT_DELAY, ELRS_PINGING, ELRS_CONNECTED, ELRS_READY};
    crsfConnectState connectionState = ELRS_BOOT_DELAY;
    bool moduleInfoReceived = false;
    uint32_t lastHandshakeTime = 0;   
    uint32_t lastParameterQueryTime = 0;
    bool parameterDiscoveryActive = false;
    uint8_t currentSettingsIndex = 0;
    uint8_t currentChunk = 0;
    uint8_t chunksRemaining = 0;
    uint8_t settingAttemptsCounter = 0;
    uint8_t totalSettingsCount;

    uint32_t lastLinkStatsFrameTime = 0;   
    uint32_t lastLinkStatRequestTime = 0;
    uint32_t lastSyncPacketDisplay = 0;

    void SendCommand(uint8_t command, uint8_t value);  // Enqueue command
    void PingDevices();
    void RequestElrsStatus();
    void RequestSetting(uint8_t settingIndex, uint8_t chunk);
    void ParseLinkStatsPacket(uint8_t rxBuffer[]);
    void ParseDeviceInfoPacket(uint8_t rxBuffer[], uint8_t length);
    void ParseSettingsPacket(uint8_t rxBuffer[], uint8_t length);   
    void ParseElrsStatusPacket(uint8_t rxBuffer[], uint8_t length);

    int getParamSlot(uint8_t id); 
    //uint8_t parseChoicesString(int slot, uint8_t* buffer, uint8_t startIdx, uint8_t maxLen);
    void parseChoicesString(int paramIndex);
    void clearModule();
};