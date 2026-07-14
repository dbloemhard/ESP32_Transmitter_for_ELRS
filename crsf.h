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
 * <http://www.gnu.org/licenses/>.
 */
 
#include <Arduino.h>
#include <stdint.h>

/*
 =======================================================================================================
 * CRSF protocol
 *
 * CRSF protocol uses a single wire half duplex uart connection.
 * The master sends one frame every 4ms and the slave replies between two frames from the master.
 *
 * 420000 baud
 * not inverted
 * 8 Bit
 * 1 Stop bit
 * Big endian
 * ELRS uses crossfire protocol at many different baud rates supported by EdgeTX i.e. 115k, 400k, 921k, 1.87M, 3.75M
 * 115000 bit/s = 14400 byte/s
 * 420000 bit/s = 46667 byte/s (including stop bit) = 21.43us per byte
 * Max frame size is 64 bytes
 * A 64 byte frame plus 1 sync byte can be transmitted in 1393 microseconds.
 *
 * CRSF_TIME_NEEDED_PER_FRAME_US is set conservatively at 1500 microseconds
 *
 * Every frame has the structure:
 * <Device address><Frame length><Type><Payload><CRC>
 *
 * Device address: (uint8_t)
 * Frame length:   length in  bytes including Type (uint8_t)
 * Type:           (uint8_t)
 * CRC:            (uint8_t)
 *
 */
//#define CRSFDEBUG

#define ELRS_PORT Serial0

// Basic setup
#define CRSF_MAX_CHANNEL        16
#define CRSF_FRAME_SIZE_MAX     64
// Define channel input limits
#define CRSF_DIGITAL_CHANNEL_MIN 172
#define CRSF_DIGITAL_CHANNEL_MAX 1811

// internal crsf variables
#define CRSF_TIME_NEEDED_PER_FRAME_US   1100 // 700 ms + 400 ms for potential ad-hoc request
//#define SERIAL_BAUDRATE                 115200 //low baud for Arduino Nano , the TX module will auto detect baud. 115200/400000
//#define CRSF_TIME_BETWEEN_FRAMES_US     4000 // 4 ms 250Hz
#define SERIAL_BAUDRATE                 921600 //400000
#define CRSF_TIME_BETWEEN_FRAMES_US     2000   // 500Hz
#define CRSF_PAYLOAD_OFFSET             offsetof(crsfFrameDef_t, type)
#define CRSF_MAX_PACKET_SIZE            64
#define CRSF_QUEUE_SIZE                 8  // Must be a power of 2 (2, 4, 8, 16) for fast masking
#define CRSF_MSP_RX_BUF_SIZE            128
#define CRSF_MSP_TX_BUF_SIZE            128
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

// ELRS commands (for Jumper Aion ELRS3.6.0)
#define ELRS_PKT_RATE_COMMAND           0x01
#define ELRS_TLM_RATIO_COMMAND          0x02
#define ELRS_SWITCH_MODE_COMMAND        0x03
#define ELRS_MODEL_MATCH_COMMAND        0x05
#define ELRS_POWER_COMMAND              0x07
#define ELRS_DYNAMIC_POWER_COMMAND      0x08
#define ELRS_WIFI_COMMAND               0x10
#define ELRS_RXWIFI_COMMAND             0x11
#define ELRS_BLE_JOYSTIC_COMMAND        0x12 //  Not on ESP8266
#define ELRS_BIND_COMMAND               0x13
#define ELRS_START_COMMAND              0x04
#define ELRS_END_COMMAND                0x05
#define ELRS_SETTINGS_WRITE             0x2D
#define SYNC_BYTE                       0xC8

// ELRS module Serial1
extern HardwareSerial ELRS_PORT;

// Structure to hold an individual raw command packet
struct crsfPacket {
    uint8_t length;
    uint8_t data[CRSF_MAX_PACKET_SIZE];
};

class crsfPacketQueue {
private:
    crsfPacket _buffer[CRSF_QUEUE_SIZE];
    volatile uint8_t _head; // Points to the next write slot
    volatile uint8_t _tail; // Points to the next read slot
    // Mask helper for ultra-fast ring wrapping without using slow modulo (%) math
    static const uint8_t _mask = CRSF_QUEUE_SIZE - 1;

public:
    crsfPacketQueue() : _head(0), _tail(0) {}

    // Check if queue has data
    bool hasItems() {return _head != _tail;}

    // Push a raw packet into the queue
    bool push(const uint8_t* rawData, uint8_t len) {
        if (len > CRSF_MAX_PACKET_SIZE) return false; // Safety check
        uint8_t nextHead = (_head + 1) & _mask;
        if (nextHead == _tail) {
            return false; // Queue Overflow (Full)
        }
        _buffer[_head].length = len;
        memcpy(_buffer[_head].data, rawData, len);
        _head = nextHead; // Atomic index advance
        return true;
    }

    // Pull the next packet out of the queue
    bool pop(crsfPacket& outPacket) {
        if (_head == _tail) {
            return false; // Queue Underflow (Empty)
        }
        outPacket = _buffer[_tail];
        _tail = (_tail + 1) & _mask; // Atomic index advance
        return true;
    }
};


struct crsfLinkStats {
  int16_t uplinkRssi1;     // Antenna 1 RSSI (converted to negative dBm)
  int16_t uplinkRssi2;     // Antenna 2 RSSI (converted to negative dBm)
  uint8_t uplinkLQ;        // Link Quality (0 - 100%)
  int8_t  uplinkSNR;       // Signal to Noise Ratio (dB)
  uint8_t activeAntenna;   // 0 = Ant1, 1 = Ant2
  uint8_t rfMode;          // ExpressLRS Air Rate Index
  uint8_t txPower;         // Module Power Level Index
  int16_t downlinkRssi;    // Telemetry RSSI (converted to negative dBm)
  uint8_t downlinkLQ;      // Telemetry Link Quality (0 - 100%)
  int8_t  downlinkSNR;     // Telemetry SNR (dB)
};


class CRSF {
public:
   // Properties
   crsfLinkStats linkStats;
   bool moduleConnected = false;
   bool telemetryActive = false;
   uint32_t lastValidFrameTime = 0;   
   crsfPacketQueue commandQueue;    // Outgoing
   crsfPacketQueue telemetryQueue;  // Incoming

   // Methods
   void begin(uint8_t rxPin, uint8_t txPin, bool halfDuplex);
   void crsfSendChannels(int16_t channels[]);
   void crsfSendCommand(uint8_t command, uint8_t value);  // Enqueue command
   void crsfQueuePacket(uint8_t packet[], uint8_t packetLength);
   void crsfSendQueuedCommand();  // Send one command from command queue
   void crsfReadTelemetry();  //Read and enqueue telem packets
   uint32_t crsfNextInterval();

private:
   bool isHalfDuplex = false;
   // Variables to track sync offsets
   volatile uint32_t targetIntervalUs;
   volatile int32_t currentPhaseShift;
   volatile int32_t averagePhaseShift;
   volatile int32_t cumulativePhaseShift = 0;
   int16_t phaseShiftHistory[256];
   uint8_t syncPacketCount = 0;
   volatile bool syncPacketReceived;
   uint32_t lastSyncPacketTime = 0;

   uint32_t lastLinkStatsFrameTime = 0;   
   uint32_t lastLinkStatRequestTime = 0;
   uint32_t lastSyncPacketDisplay = 0;

   void crsfWritePacket(uint8_t packet[], uint8_t packetLength);
   void crsfParseLinkStatsPacket(uint8_t rxBuffer[]);
   void crsfParseElrsSyncPacket(uint8_t rxBuffer[]);
};

// Declare your class object as external so the background serial handler can see it
extern CRSF crsfClass;


/* 
LUA debug output

[ELRS] -> Querying Menu Parameter Index: 0, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] HooJ | Menu (ID: 0)
[ELRS] -> Querying Menu Parameter Index: 1, chunk: 0
Received Packet type 0x2B
Chunk stored[ELRS] -> Querying Menu Parameter Index: 1, chunk: 1
Received Packet type 0x2B
Chunk stored[ELRS] -> Querying Menu Parameter Index: 1, chunk: 2
Received Packet type 0x2B
Chunk stored[ELRS] -> Querying Menu Parameter Index: 1, chunk: 3
Received Packet type 0x2B
[PARAMETER SAVED] Packet Rate | Choices(0-9): 50Hz(-115dBm), 100Hz Full(-112dBm), 150Hz(-112dBm), 250Hz(-108dBm), 333Hz Full(-105dBm), 500Hz(-105dBm), D250(-104dBm), D500(-104dBm), F500(-104dBm), F1000(-104dBm) | Active Selection: 500Hz(-105dBm)
[ELRS] -> Querying Menu Parameter Index: 2, chunk: 0
Received Packet type 0x2B
Chunk stored[ELRS] -> Querying Menu Parameter Index: 2, chunk: 1
Received Packet type 0x2B
[PARAMETER SAVED] Telem Ratio | Choices(0-9): Std, Off, 1:128, 1:64, 1:32, 1:16, 1:8, 1:4, 1:2, Race | Active Selection: 1:32 (546bps)
[ELRS] -> Querying Menu Parameter Index: 3, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Switch Mode | Choices(0-1): Wide, Hybrid | Active Selection: Wide
[ELRS] -> Querying Menu Parameter Index: 4, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Link Mode | Choices(0-1): Normal, MAVLink | Active Selection: Normal
[ELRS] -> Querying Menu Parameter Index: 5, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Model Match | Choices(0-1): Off, On | Active Selection: Off (ID: 0)
[ELRS] -> Querying Menu Parameter Index: 6, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] TX Power (100mW Dyn) | Menu (ID: 6)
[ELRS] -> Querying Menu Parameter Index: 7, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Max Power | Choices(0-5): 10, 25, 50, 100, 250, 500 | Active Selection: 100mW
[ELRS] -> Querying Menu Parameter Index: 8, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Dynamic | Choices(0-5): Off, Dyn, AUX9, AUX10, AUX11, AUX12 | Active Selection: Dyn
[ELRS] -> Querying Menu Parameter Index: 9, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] VTX Administrator | Menu (ID: 9)
[ELRS] -> Querying Menu Parameter Index: 10, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Band | Choices(0-6): Off, A, B, E, F, R, L | Active Selection: Off
[ELRS] -> Querying Menu Parameter Index: 11, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Channel (1-8): 1
[ELRS] -> Querying Menu Parameter Index: 12, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Pwr Lvl | Choices(0-8): -, 1, 2, 3, 4, 5, 6, 7, 8 | Active Selection: -
[ELRS] -> Querying Menu Parameter Index: 13, chunk: 0
Received Packet type 0x2B
Chunk stored[ELRS] -> Querying Menu Parameter Index: 13, chunk: 1
Received Packet type 0x2B
Chunk stored[ELRS] -> Querying Menu Parameter Index: 13, chunk: 2
Received Packet type 0x2B
[PARAMETER SAVED] Pitmode | Choices(0-21): Off, On, AUX1�, AUX1�, AUX2�, AUX2�, AUX3�, AUX3�, AUX4�, AUX4�, AUX5�, AUX5�, AUX6�, AUX6�, AUX7�, AUX7�, AUX8�, AUX8�, AUX9�, AUX9�, AUX10�, AUX10� | Active Selection: Off
[ELRS] -> Querying Menu Parameter Index: 14, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Send VTx | Command (Current state IDLE | Timeout 2000 | Info/Status : )
[ELRS] -> Querying Menu Parameter Index: 15, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] WiFi Connectivity | Menu (ID: 15)
[ELRS] -> Querying Menu Parameter Index: 16, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Enable WiFi | Command (Current state IDLE | Timeout 2000 | Info/Status : )
[ELRS] -> Querying Menu Parameter Index: 17, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Enable Rx WiFi | Command (Current state IDLE | Timeout 2000 | Info/Status : )
[ELRS] -> Querying Menu Parameter Index: 18, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] BLE Joystick | Command (Current state IDLE | Timeout 2000 | Info/Status : )
[ELRS] -> Querying Menu Parameter Index: 19, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Bind | Command (Current state IDLE | Timeout 2000 | Info/Status : )
[ELRS] -> Querying Menu Parameter Index: 20, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] Bad/Good | Info String: 0/126
[ELRS] -> Querying Menu Parameter Index: 21, chunk: 0
Received Packet type 0x2B
[PARAMETER SAVED] 3.6.0 ISM2G4 | Info String: ff41f6
[ELRS] >>> PARAMETER TREE MATRIX FULLY POPULATED AND CACHED <<<






ESP32 Team900
https://github.com/danxdz/simpleTx_esp32/blob/master/src/Simple_TX.cpp

// buildElrsPacket(crsfCmdPacket,X,3);
// 0 : ELRS status request => ??
// 1 : Set Lua [Packet Rate]= 0 - 25Hz / 1 - 50Hz / 2 - 100Hz / 3 - 200Hz
// 2 : Set Lua [Telem Ratio]= 0 - off / 1 - 1:128 / 2 - 1:64 / 3 - 1:32 / 4 - 1:16 / 5 - 1:8 / 6 - 1:4 / 7 - 1:2
// 3 : Set Lua [Switch Mode]=0 -> Hybrid;Wide
// 4 : Set Lua [Model Match]=0 -> Off;On
// 5 : Set Lua [TX Power]=0 - 10mW / 1 - 25mW
// 6 : Set Lua [Max Power]=0 - 10mW / 1 - 25mW *dont force to change, but change after reboot if last power was greater
// 7 : Set Lua [Dynamic]=0 -> Off;On;AUX9;AUX10;AUX11;AUX12 -> * @ ttgo screen
// 8 : Set Lua [VTX Administrator]=0
// 9 : Set Lua [Band]=0 -> Off;A;B;E;F;R;L
// 10: Set Lua [Channel]=0 -> 1;2;3;4;5;6;7;8
// 11: Set Lua [Pwr Lvl]=0 -> -;1;2;3;4;5;6;7;8
// 12: Set Lua [Pitmode]=0 -> Off;On
// 13: Set Lua [Send VTx]=0 sending response for [Send VTx] chunk=0 step=2
// 14: Set Lua [WiFi Connectivity]=0
// 15: Set Lua [Enable WiFi]=0 sending response for [Enable WiFi] chunk=0 step=0
// 16: Set Lua [Enable Rx WiFi]=0 sending response for [Enable Rx WiFi] chunk=0 step=2
// 17: Set Lua [BLE Joystick]=0 sending response for [BLE Joystick] chunk=0 step=0
//     Set Lua [BLE Joystick]=1 sending response for [BLE Joystick] chunk=0 step=3
//     Set Lua [BLE Joystick]=2 sending response for [BLE Joystick] chunk=0 step=3
//     Set Lua [BLE Joystick]=3 sending response for [BLE Joystick] chunk=0 step=3
//     Set Lua [BLE Joystick]=4 to enable
//                hwTimer stop
//                Set Lua [TX Power]=0
//                hwTimer interval: 5000
//                Adjusted max packet size 22-22
//                Starting BLE Joystick!
// 19: Set Lua [Bad/Good]=0
// 20: Set Lua [2.1.0 EU868]=0 =1 ?? get

0:Packet Rate:0:9:0:4:4;25Hz(-123dBm):50Hz(-120dBm):100Hz(-117dBm):100Hz Full(-112dBm):200Hz(-112dBm): :: OPT
1:Telem Ratio:0:9:0:9:0;Std:Off:1:128:1:64:1:32:1:16:1:8:1:4:1:2:Race: :: OPT
2:Switch Mode:0:9:0:1:0;Wide:Hybrid: :: OPT
3:Model Match:0:9:0:1:0;Off:On: :: OPT
4:TX Power (50mW):0:11:0:0:0 :: MainMenuItem 
5:Max Power:5:9:0:2:2;10:25:50: :: OPT
6:Dynamic:5:9:0:5:0;Off:Dyn:AUX9:AUX10:AUX11:AUX12: :: OPT
7:VTX Administrator:0:11:0:0:0 :: MainMenuItem
8:Band:8:9:0:6:0;Off:A:B:E:F:R:L: :: OPT
9:Channel:8:9:0:7:0;1:2:3:4:5:6:7:8: :: OPT
10:Pwr Lvl:8:9:0:8:0;-:1:2:3:4:5:6:7:8: :: OPT
11:Pitmode:8:9:0:8:79;Off:On:AUX1�:AUX1�:AUX2�:AUX2�:AUX3�:AUX3�:AUX  Pitmode: :: OPT
12:Send VTx:8:13:0:0:0 :: CMD
13:WiFi Connectivity:0:11:0:0:0 :: MainMenuItem
14:Enable WiFi:14:13:0:0:0 :: CMD
15:Enable Rx WiFi:14:13:0:0:0 :: CMD
16:BLE Joystick:0:13:0:0:0 :: CMD
17:Bind:0:13:0:0:0 :: CMD
18:Bad/Good:0:12:0:0:0 :: INFO
19:3.1.2 EU868:0:12:0:0:0 :: INFO

as menu:

Main Menu
|- Packet Rate (OPT)
   |- 25Hz(-123dBm)
   |- 50Hz(-120dBm)
   |- 100Hz(-117dBm)
   |- 100Hz Full(-112dBm)
   |- 200Hz(-112dBm)
|- Telem Ratio (OPT)
   |- Std
   |- Off
   |- 1:128
   |- 1:64
   |- 1:32
   |- 1:16
   |- 1:8
   |- 1:4
   |- 1:2
   |- Race
|- Switch Mode (OPT)
   |- Wide
   |- Hybrid
|- Model Match (OPT)
   |- Off
   |- On
|- TX Power (50mW)
|  |- Max Power (OPT)
   |  |- 10
   |  |- 25
   |  |- 50
   |- Dynamic (OPT)
   |  |- Off
   |  |- Dyn
   |  |- AUX9
   |  |- AUX10
   |  |- AUX11
   |  |- AUX12
|- VTX Administrator
|  |- Band (OPT)
   |  |- Off
   |  |- A
   |  |- B
   |  |- E
   |  |- F
   |  |- R
   |  |- L
   |- Channel (OPT)
   |  |- 1
   |  |- 2
   |  |- 3
   |  |- 4
   |  |- 5
   |  |- 6
   |  |- 7
   |  |- 8
   |- Pwr Lvl (OPT)
   |  |- -
   |  |- 1
   |  |- 2
   |  |- 3
   |  |- 4
   |  |- 5
   |  |- 6
   |  |- 7
   |  |- 8
   |- Pitmode (OPT)
      |- Off
      |- On
|- Send VTx
|- WiFi Connectivity
|  |- Enable WiFi
   |- Enable Rx WiFi
|- BLE Joystick
|- Bind
|- Bad/Good
|- 3.1.2 EU868

*/
