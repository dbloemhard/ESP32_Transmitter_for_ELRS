/*
 * This file is part of Simple TX
 *
 * Simple TX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Simple TX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "crsf.h"

// crc implementation from CRSF protocol document rev7
static uint8_t crsf_crc8tab[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54, 0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06, 0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0, 0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2, 0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9, 0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B, 0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D, 0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F, 0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB, 0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9, 0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F, 0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D, 0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26, 0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74, 0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82, 0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0, 0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9};

uint8_t crsf_crc8(const uint8_t *ptr, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++){
        crc = crsf_crc8tab[crc ^ *ptr++];
    }
    return crc;
}

bool checkCrc(uint8_t *packet, uint8_t length) {
    return (crsf_crc8(&packet[2], packet[1] - 1) == packet[length - 1]);
}

// Serial begin
void CRSF::begin(uint8_t rxPin, uint8_t txPin, bool halfDuplex) {
  isHalfDuplex = halfDuplex;
  if (isHalfDuplex) {
    // 921600 baud, SERIAL_8N1 protocol, RX=GPIO21, TX=GPIO21 (TX and RX defined as same pin)
    ELRS_PORT.begin(SERIAL_BAUDRATE, SERIAL_8N1, txPin, txPin); 
    // 2. Configure the GPIO matrix pin to operate in Open-Drain mode with a pull-up
    pinMode(txPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(txPin, HIGH); // Pull line high (idle state)
  } else {
    // 921600 baud, SERIAL_8N1 protocol, RX=GPIO20, TX=GPIO21
    ELRS_PORT.begin(SERIAL_BAUDRATE, SERIAL_8N1, rxPin, txPin); 
  }
}

// prepare data packet
//void CRSF::crsfPrepareDataPacket(uint8_t packet[], int16_t channels[]) {
// Send RC Channels
void CRSF::crsfSendChannels(int16_t channels[]) {
    uint8_t packet[CRSF_PACKET_SIZE];
    // const uint8_t crc = crsf_crc8(&packet[2], CRSF_PACKET_SIZE-3);
    /*
     * Map 1000-2000 with middle at 1500 chanel values to
     * 173-1811 with middle at 992 S.BUS protocol requires
    */

    // packet[0] = UART_SYNC; //Header
    packet[0] = MODULE_ADDRESS; // Header
    packet[1] = 24;           // length of type (24) + payload + crc
    packet[2] = ELRS_CHANNELS;
    packet[3] = (uint8_t)(channels[0] & 0x07FF);
    packet[4] = (uint8_t)((channels[0] & 0x07FF) >> 8 | (channels[1] & 0x07FF) << 3);
    packet[5] = (uint8_t)((channels[1] & 0x07FF) >> 5 | (channels[2] & 0x07FF) << 6);
    packet[6] = (uint8_t)((channels[2] & 0x07FF) >> 2);
    packet[7] = (uint8_t)((channels[2] & 0x07FF) >> 10 | (channels[3] & 0x07FF) << 1);
    packet[8] = (uint8_t)((channels[3] & 0x07FF) >> 7 | (channels[4] & 0x07FF) << 4);
    packet[9] = (uint8_t)((channels[4] & 0x07FF) >> 4 | (channels[5] & 0x07FF) << 7);
    packet[10] = (uint8_t)((channels[5] & 0x07FF) >> 1);
    packet[11] = (uint8_t)((channels[5] & 0x07FF) >> 9 | (channels[6] & 0x07FF) << 2);
    packet[12] = (uint8_t)((channels[6] & 0x07FF) >> 6 | (channels[7] & 0x07FF) << 5);
    packet[13] = (uint8_t)((channels[7] & 0x07FF) >> 3);
    packet[14] = (uint8_t)((channels[8] & 0x07FF));
    packet[15] = (uint8_t)((channels[8] & 0x07FF) >> 8 | (channels[9] & 0x07FF) << 3);
    packet[16] = (uint8_t)((channels[9] & 0x07FF) >> 5 | (channels[10] & 0x07FF) << 6);
    packet[17] = (uint8_t)((channels[10] & 0x07FF) >> 2);
    packet[18] = (uint8_t)((channels[10] & 0x07FF) >> 10 | (channels[11] & 0x07FF) << 1);
    packet[19] = (uint8_t)((channels[11] & 0x07FF) >> 7 | (channels[12] & 0x07FF) << 4);
    packet[20] = (uint8_t)((channels[12] & 0x07FF) >> 4 | (channels[13] & 0x07FF) << 7);
    packet[21] = (uint8_t)((channels[13] & 0x07FF) >> 1);
    packet[22] = (uint8_t)((channels[13] & 0x07FF) >> 9 | (channels[14] & 0x07FF) << 2);
    packet[23] = (uint8_t)((channels[14] & 0x07FF) >> 6 | (channels[15] & 0x07FF) << 5);
    packet[24] = (uint8_t)((channels[15] & 0x07FF) >> 3);
    packet[25] = crsf_crc8(&packet[2], packet[1] - 1); // CRC
    
    crsfWritePacket(packet,CRSF_PACKET_SIZE);
}

// prepare elrs setup packet (power, packet rate...)
//void CRSF::crsfPrepareCmdPacket(uint8_t packetCmd[], uint8_t command, uint8_t value) {
void CRSF::crsfSendCommand(uint8_t command, uint8_t value) {
    uint8_t packetCmd[CRSF_CMD_PACKET_SIZE];

    packetCmd[0] = MODULE_ADDRESS;
    packetCmd[1] = 6; // length of Command (4) + payload + crc
    packetCmd[2] = ELRS_SETTINGS_WRITE;
    packetCmd[3] = MODULE_ADDRESS; // Destination Address (0x00 = Parameter Broadcast Request)
    packetCmd[4] = HANDSET_ADDRESS; // Source Address (0xEA = Radio Handset Transmitter)
    packetCmd[5] = command;
    packetCmd[6] = value;
    packetCmd[7] = crsf_crc8(&packetCmd[2], packetCmd[1] - 1); // CRC

    //crsfWritePacket(packetCmd,CRSF_CMD_PACKET_SIZE);
    crsfQueuePacket(packetCmd,CRSF_CMD_PACKET_SIZE);
}

void CRSF::crsfSendQueuedCommand() {
    if (commandQueue.hasItems()) {
        crsfPacket pendingPacket;
        if (commandQueue.pop(pendingPacket)) {
            crsfWritePacket(pendingPacket.data, pendingPacket.length);
            // Serial.write("Pushing pending 0x"); Serial.println(pendingPacket.data[2],HEX);
            // ELRS_PORT.write(pendingPacket.data, pendingPacket.length);
            // if (ELRS_PORT.isHalfDuplex()) {
            //   ELRS_PORT.enableHalfDuplexRx();
            // }
            // Serial.print("Sent: ");
            // for (int i = 0; i < pendingPacket.length; i++) {
            //   Serial.print(pendingPacket.data[i],HEX); Serial.print(" ");
            // } Serial.println();
        }
    }
}

void CRSF::crsfQueuePacket(uint8_t packet[], uint8_t packetLength) {
    // Calculate CRC for the packet here
    packet[packetLength-1] = crsf_crc8(&packet[2], packetLength-3); 
    // Push it onto the ring buffer queue
    if (!commandQueue.push(packet, packetLength)) {
        // Handle full buffer error if necessary
    }
}

void CRSF::crsfWritePacket(uint8_t packet[], uint8_t packetLength) {
    ELRS_PORT.write(packet, packetLength);
    if (isHalfDuplex) {
      // CRITICAL for Half-Duplex: If you don't flush, your incoming listening code will get corrupted by your own echo.
      ELRS_PORT.flush(); 
      // Not sure if this will interfere with the ELRS response messages?
      while (ELRS_PORT.available() > 0) {
        ELRS_PORT.read(); // Discard the transmitted echo
      }   
    } 
}

void CRSF::crsfParseLinkStatsPacket(uint8_t rxBuffer[]) {
  if (!telemetryActive) {
      telemetryActive = true;
  #ifdef CRSFDEBUG
      Serial.println("\n=================================");
      Serial.println("[INFO] -> TELEMETRY RECOVERED <-");
      Serial.println("=================================");
  #endif
  }
  lastLinkStatsFrameTime = millis(); // Refresh our active timestamp clock

  linkStats.uplinkRssi1   = (int8_t)rxBuffer[3]; 
  linkStats.uplinkRssi2   = (int8_t)rxBuffer[4]; 
  linkStats.uplinkLQ      = rxBuffer[5];
  linkStats.uplinkSNR     = (int8_t)rxBuffer[6]; 
  linkStats.activeAntenna = rxBuffer[7];
  linkStats.rfMode        = rxBuffer[8];
  linkStats.txPower       = rxBuffer[9];
  linkStats.downlinkRssi  = (int8_t)rxBuffer[10];
  linkStats.downlinkLQ    = rxBuffer[11];
  linkStats.downlinkSNR   = (int8_t)rxBuffer[12];
}

void CRSF::crsfParseElrsSyncPacket(uint8_t rxBuffer[]) {
    // Ensure the packet matches the OpenTX Sync Subtype
    // rxBuffer[0] = Sync byte
    // rxBuffer[1] = length
    // rxBuffer[2] = Packet Type 0x3A
    // rxBuffer[3] = Source 0xEE
    // rxBuffer[4] = Destination 0xEF
    
    if (rxBuffer[1] >= 11 && rxBuffer[5] == CRSF_SUBTYPE_OPENTX_SYNC) {      
        // Extract 32-bit Big-Endian Rate Interval (bytes 1-4 of payload)
        uint32_t rawInterval = ((uint32_t)rxBuffer[6] << 24) | 
                               ((uint32_t)rxBuffer[7] << 16) | 
                               ((uint32_t)rxBuffer[8] << 8)  | 
                                rxBuffer[9];
        // Extract 32-bit Signed Big-Endian Phase Shift (bytes 5-8 of payload)
        int32_t rawShift = ((int32_t)rxBuffer[10] << 24) | 
                           ((int32_t)rxBuffer[11] << 16) | 
                           ((int32_t)rxBuffer[12] << 8)  | 
                            rxBuffer[13];
        // ExpressLRS scales microsecond telemetry values by 10
        targetIntervalUs   = (uint32_t)((float)rawInterval * 0.1f);
        currentPhaseShift  = (int32_t)((float)rawShift * 0.1f);
        currentPhaseShift = constrain(currentPhaseShift, -(int32_t)targetIntervalUs, (int32_t)targetIntervalUs); // Phase shift shouldnt be more than the current loop cycle

        if (currentPhaseShift > (int32_t)(targetIntervalUs/2)) currentPhaseShift -= (int32_t)targetIntervalUs;  // If the phase shift is 1650 on a 2000us interval, that is actually -350 on the next interval
        else if (-currentPhaseShift > (int32_t)(targetIntervalUs/2)) currentPhaseShift += (int32_t)targetIntervalUs; // conversely a phase shift of -1800 is actually a 200 phase shift on the next interva,  
        
        syncPacketReceived = true;      
        lastSyncPacketTime = micros();

        phaseShiftHistory[syncPacketCount++] = currentPhaseShift;
        if (syncPacketCount==0) cumulativePhaseShift=0;   //syncPacketCount is a uin8_t so only goes up to 255. If it is 0 here it means it wrapped around.
        else cumulativePhaseShift += currentPhaseShift;

        if (millis()>lastSyncPacketDisplay+1000) {
          averagePhaseShift = (syncPacketCount == 0) ? 0 : cumulativePhaseShift / (int16_t)syncPacketCount;
#ifdef CRSFDEBUG        
          Serial.println("\n============ [CRSFShot] ============");
          Serial.print("Received "); Serial.print(syncPacketCount); Serial.println(" packets in 1000ms");
          Serial.print("Last Sync Packet. Interval: ");Serial.print(targetIntervalUs);Serial.print(" Phase Shift: ");Serial.println(currentPhaseShift);
          Serial.print("Average shift: "); Serial.println(averagePhaseShift);
          Serial.print("Past packets : ");
          for (int i = 0; i < syncPacketCount; i++){
            Serial.print(phaseShiftHistory[i]);Serial.print(" "); 
          } Serial.println();
          Serial.println("====================================");
#endif
          memset(phaseShiftHistory, 0, sizeof(phaseShiftHistory));   
          cumulativePhaseShift = 0;
          syncPacketCount = 0;
          lastSyncPacketDisplay = millis();
        }
    }
}

uint32_t CRSF::crsfNextInterval() {
        // Static baseline interval from received Sync packet, or hardcoded interval
        uint32_t baseInterval = (targetIntervalUs > 0) ? targetIntervalUs : CRSF_TIME_BETWEEN_FRAMES_US;
        
        //int32_t correction = (syncPacketReceived) ? averagePhaseShift : 0;
        // To do - implement dynamic phase shifting based on the CRSFShot sync adjustment
        // int32_t correction = 0;
        // if (syncPacketReceived) {
        //     if (micros() - lastSyncPacketTime > baseInterval * 100) {
        //       syncPacketReceived = false; // Reset the flag
        //     }
        //     else {
        //       correction = currentPhaseShift;
        //     }
        // }
        int32_t correction = 0; // -(int32_t)baseInterval/10;  // Simple correction value based on loop timing observations
        // Return the dynamic duration for the *next* single loop cycle
        return baseInterval + correction;
}
void CRSF::crsfReadTelemetry() {
    while (ELRS_PORT.available() > 0) {
        if (ELRS_PORT.peek() != HANDSET_ADDRESS && ELRS_PORT.peek() != LUA_SCRIPT_ADDRESS) {
            ELRS_PORT.read(); // Clear junk/out of sync bytes immediately
            continue;
        }

        if (ELRS_PORT.available() < 2) return;    // We need at least 2 bytes available to extract the length
        uint8_t addrByte = ELRS_PORT.read();      // Read header byte
        uint8_t payloadLength = ELRS_PORT.read(); // Read length byte

        if (payloadLength < 2 || payloadLength > (CRSF_MAX_PACKET_SIZE - 2)) {
            continue; // Bad tracking frame size, continue flushing
        }

        // non-blocking hardware timeout until the rest of the block finishes arriving
        uint32_t startWait = micros();
        while (ELRS_PORT.available() < payloadLength) {
            if (micros() - startWait > 1000) { // 1ms timeout cap limit
                return; // Packet assembly timed out or fragmented, drop frame fragments
            }
        }

        uint8_t localPacketBuffer[CRSF_MAX_PACKET_SIZE];
        localPacketBuffer[0] = addrByte;
        localPacketBuffer[1] = payloadLength;
        for (uint8_t i = 0; i < payloadLength; i++) {
            localPacketBuffer[2 + i] = ELRS_PORT.read();
        }
        uint8_t totalPacketBytesCount = payloadLength + 2;

        if (checkCrc(localPacketBuffer, totalPacketBytesCount)) {
          // Refresh watchdog timer upon any valid frame from the module
          moduleConnected = true;
          lastValidFrameTime = millis(); // Refresh our active timestamp clock
          
          uint8_t packetType = localPacketBuffer[2];
          switch (packetType) {
            case ELRS_LINK_STATS:
              crsfParseLinkStatsPacket(localPacketBuffer);  // Process link stats packet immediately/dont add to queue
              break;
            case ELRS_RADIO_ID:
              crsfParseElrsSyncPacket(localPacketBuffer);  // Process sync packet immediately/dont add to queue
              break;
            // Handle other telemetry types here, otherwise they get pushed to the telemetry queue packet (to be processed by elrsLua)
            default:
              telemetryQueue.push(localPacketBuffer, totalPacketBytesCount);
              break;
          }    
        }
    }

    // Timeout processing
    // ================================
    uint32_t now = millis();
    if (telemetryActive && (now - lastLinkStatsFrameTime > 1000)) {
      telemetryActive = false;
#ifdef CRSFDEBUG
      Serial.println("\n=================================");
      Serial.println("[ALERT] !!! TELEMETRY LOST !!!");
      Serial.println("=================================");
#endif
    }
    // Standard operational watchdog: If no data for 2000ms, drop back to pinging
    if (moduleConnected && (now - lastValidFrameTime > 2000)) {
        moduleConnected = false;
        syncPacketReceived = false;
        Serial.println("[HANDSHAKE] Telemetry Timeout. Module connection Lost.");  // Print this regardless
    }   
}