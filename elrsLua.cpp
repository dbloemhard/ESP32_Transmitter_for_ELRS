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
 
#include "elrsLua.h"
#include "crsf.h" // Needed here so the compiler knows the methods inside CRSF

// Initialize the reference via the initializer list
ELRSLua::ELRSLua(CRSF& crsfInstance) : crsf(crsfInstance) {}


// Command builder functions
// ========================================================

void ELRSLua::sendCommand(uint8_t command, uint8_t value) {
    uint8_t packetCmd[CRSF_CMD_PACKET_SIZE];

    packetCmd[0] = MODULE_ADDRESS;
    packetCmd[1] = 6; // length of Command (4) + payload + crc
    packetCmd[2] = ELRS_SETTINGS_WRITE;
    packetCmd[3] = MODULE_ADDRESS; // Destination Address (0x00 = Parameter Broadcast Request)
    packetCmd[4] = HANDSET_ADDRESS; // Source Address (0xEA = Radio Handset Transmitter)
    packetCmd[5] = command;
    packetCmd[6] = value;
    packetCmd[7] = 0; // CRC calculated by the queue function

    crsf.crsfQueuePacket(packetCmd,CRSF_CMD_PACKET_SIZE);
#ifdef LUADEBUG        
    Serial.print("[ELRS] -> Send Command: ");
    Serial.print(command);
    Serial.print(", Value: ");
    Serial.print(value); Serial.println();
#endif
}

void ELRSLua::pingDevices() {
    uint8_t packetCmd[6];

    packetCmd[0] = MODULE_ADDRESS;
    packetCmd[1] = 4; // length of Command (2) + payload + crc
    packetCmd[2] = ELRS_DEVICE_PING; 
    packetCmd[3] = BROADCAST_ADDRESS; // Destination Address (0x00 = Parameter Broadcast Request)
    packetCmd[4] = HANDSET_ADDRESS; // Source Address (0xEA = Radio Handset Transmitter)
    packetCmd[5] = 0; // CRC calculated by the queue function
    //crsfWritePacket(packetCmd, 6);
    crsf.crsfQueuePacket(packetCmd, 6);
}

void ELRSLua::requestSetting(uint8_t settingIndex, uint8_t chunk) {
    uint8_t packetCmd[8];

    packetCmd[0] = MODULE_ADDRESS;        // 0xEE (Target Device: External TX Module)
    packetCmd[1] = 6;                     // LENGTH: 7 Bytes remaining (Type + 5 Payload Bytes + 1 CRC)
    packetCmd[2] = ELRS_SETTINGS_READ;    // 0x2C (CRSF_FRAMETYPE_PARAMETER_READ)
    packetCmd[3] = MODULE_ADDRESS;        // Destination Address: 0xEE (The Module itself)
    packetCmd[4] = LUA_SCRIPT_ADDRESS;    // HANDSET_ADDRESS;          // Source/Origin Address: 0xEA (Your Transmitter Handset) or 0xEF ELRS_LUA
    packetCmd[5] = settingIndex;          // Parameter Index position to read (1, 2, 3...)
    packetCmd[6] = chunk;                 // Value Chunk Offset parameter
    packetCmd[7] = 0; // CRC calculated by the queue function

    //crsfWritePacket(packetCmd, 8);
    crsf.crsfQueuePacket(packetCmd, 8);
#ifdef LUADEBUG        
    Serial.print("[ELRS] -> Querying Menu Parameter Index: ");
    Serial.print(settingIndex);
    Serial.print(", chunk: ");
    Serial.print(chunk); Serial.println();
#endif
}

// Request Info Message by sending ELRS_SETTINGS_WRITE (0x2D) with param/value 0
void ELRSLua::requestElrsStatus() {
    sendCommand(0,0);
}


// Response Parser functions
// ========================================================

void ELRSLua::parseDeviceInfoPacket(uint8_t rxBuffer[], uint8_t length) {
  // Clear any existing stored text profile settings
  memset(txModule.name, 0, sizeof(txModule.name)); 

  uint8_t currentIdx = 5;
  uint8_t charCount = 0;
  
  while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length - 1) && charCount < (CRSF_MAX_STRING_LEN - 1)) {
    txModule.name[charCount++] = (char)rxBuffer[currentIdx++];
  }
  txModule.name[charCount] = '\0';
  currentIdx++; 

  if (currentIdx + 14 <= (length + 2)) {
    // Extract 32-bit values (Sent in Big-Endian format)
    txModule.serialNumber[0] = rxBuffer[currentIdx++];
    txModule.serialNumber[1] = rxBuffer[currentIdx++];
    txModule.serialNumber[2] = rxBuffer[currentIdx++];
    txModule.serialNumber[3] = rxBuffer[currentIdx++];

    txModule.hwVersion[0] = rxBuffer[currentIdx++];
    txModule.hwVersion[1] = rxBuffer[currentIdx++];
    txModule.hwVersion[2] = rxBuffer[currentIdx++];
    txModule.hwVersion[3] = rxBuffer[currentIdx++];
    
    txModule.swVersion[0] = rxBuffer[currentIdx++];
    txModule.swVersion[1] = rxBuffer[currentIdx++];
    txModule.swVersion[2] = rxBuffer[currentIdx++];
    txModule.swVersion[3] = rxBuffer[currentIdx++];

    // Extract the final configuration structural control tags
    totalSettingsCount = rxBuffer[currentIdx++]; 
    txModule.protocolVersion   = rxBuffer[currentIdx++];

    // reset the currently loaded count
    txModule.paramCount = 0;
#ifdef LUADEBUG        
    // Debug output
    Serial.println("============ [ELRS DEVICE DETECTED] ============");
    Serial.print("Module Name         : "); Serial.println(txModule.name);
    Serial.print("Serial number       : "); Serial.print((char)txModule.serialNumber[0]); Serial.print((char)txModule.serialNumber[1]); Serial.print((char)txModule.serialNumber[2]); Serial.println((char)txModule.serialNumber[3]); 
    Serial.print("Hardware Version    : "); Serial.print(txModule.hwVersion[0], HEX); Serial.print(txModule.hwVersion[1], HEX); Serial.print(txModule.hwVersion[2], HEX); Serial.println(txModule.hwVersion[3], HEX); 
    Serial.print("Firmware Version    : "); Serial.print(txModule.swVersion[0], HEX); Serial.print(txModule.swVersion[1], HEX); Serial.print(txModule.swVersion[2], HEX); Serial.println(txModule.swVersion[3], HEX); 
    Serial.print("Available Settings  : "); Serial.print(totalSettingsCount); Serial.println(" Menu Items");
    Serial.print("Protocol Version    : "); Serial.println(txModule.protocolVersion);
    Serial.println("================================================\n");
#endif
  }
}


void ELRSLua::parseParameterInfoPacket(uint8_t rxBuffer[], uint8_t length) {
    static uint8_t tempParamStorage[CRSF_PAYLOAD_SIZE_MAX * 5]; 
    static uint16_t tempStorageIndex = 0;
    static uint8_t expectedChunksRemain = 0;
    uint8_t currentIdx = 5;
    uint8_t fieldIndex = rxBuffer[currentIdx++]; //5
    if (fieldIndex != activeExpectedParamIndex) {
        memset(tempParamStorage, 0, sizeof(tempParamStorage));
        tempStorageIndex = 0; 
        currentChunk = 0;
        return;
    }

    int slot = getParamSlot(fieldIndex);
    chunksRemaining = rxBuffer[currentIdx++]; //6
    if (slot == -1 || (tempStorageIndex > 0 && chunksRemaining != expectedChunksRemain)) {
        return;
    }

    // If data is chunked, copy it to persistent buffer
    if (chunksRemaining > 0 || currentChunk > 0) {
        // fieldData = fieldData or {}
        if (tempStorageIndex == 0) {
            memset(tempParamStorage, 0, sizeof(tempParamStorage));
            tempStorageIndex = 0; 

        }

    }
/*
  local offset
  -- If data is chunked, copy it to persistent buffer
  if chunksRemain > 0 or fieldChunk > 0 then
    fieldData = fieldData or {}
    for i=5, #data do
      fieldData[#fieldData + 1] = data[i]
      data[i] = nil
    end
    offset = 1
  else
    -- All data arrived in one chunk, operate directly on data
    fieldData = data
    offset = 5
  end

  if chunksRemain > 0 then
    fieldChunk = fieldChunk + 1
    expectChunksRemain = chunksRemain - 1
  else
    -- Field data stream is now complete, process into a field
    loadQ[#loadQ] = nil

    if #fieldData > (offset + 2) then
      field.id = fieldId
      field.parent = (fieldData[offset] ~= 0) and fieldData[offset] or nil
      field.type = bit32.band(fieldData[offset+1], 0x7f)
      field.hidden = bit32.btest(fieldData[offset+1], 0x80) or nil
      field.name, offset = fieldGetStrOrOpts(fieldData, offset+2, field.name)
      if functions[field.type+1].load then
        functions[field.type+1].load(field, fieldData, offset)
      end
      if field.min == 0 then field.min = nil end
      if field.max == 0 then field.max = nil end
    end

    fieldChunk = 0
    fieldData = nil

    -- Return value is if the screen should be updated
    -- If deviceId is TX module, then the Bad/Good drives the update; for other
    -- devices update each new item. and always update when the queue empties
    return deviceId ~= 0xEE or #loadQ == 0
  end
*/

}

void ELRSLua::parseSettingsPacket(uint8_t rxBuffer[], uint8_t length) {
  uint8_t currentIdx = 5;
  uint8_t fieldIndex = rxBuffer[currentIdx++]; //5

  int slot = getParamSlot(fieldIndex);
  if (slot != -1) {
    chunksRemaining = rxBuffer[currentIdx++]; //6

    //if (fieldIndex == currentSettingsIndex) settingAttemptsCounter = 0;

    if (currentChunk == 0) {
      // Parent folder and type only included on initial chunk packet
      txModule.params[slot].parentFolder = rxBuffer[currentIdx++]; //7
      txModule.params[slot].type = static_cast<crsfValueType>(rxBuffer[currentIdx] & 0x7F);  //8
      txModule.params[slot].hidden = (rxBuffer[currentIdx++] & 0x80);  //8

      // Parse out the Parameter Label String starting at Index 9
      uint8_t labelCharCount = 0;
      currentIdx = 9;  // (moot, since it will be set to that from the last currentIdx++ command - but just to be sure)
      while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && labelCharCount < (CRSF_MAX_STRING_LEN - 1)) {
        txModule.params[slot].label[labelCharCount++] = (char)rxBuffer[currentIdx++];
      }
      txModule.params[slot].label[labelCharCount] = '\0'; 
      currentIdx++; 

      // reset the valueString if it is not blank (this would happen if the item is being reloaded)
      if (txModule.params[slot].valueStringCharCount > 0) {
        memset(txModule.params[slot].valueString, 0, sizeof(txModule.params[slot].valueString));
        txModule.params[slot].valueStringCharCount = 0;
      }
    } 
             
    uint8_t unitCharCount = 0;
    switch(txModule.params[slot].type) {
        case CRSF_UINT8:
            // VTX Administrator contains a parameter of type UINT8. Other numeric types may appear in the future but just handling this one for now.
            // Packet data: EA 15 2B EA EE B 0 9 0 43 68 61 6E 6E 65 6C 0 1 1 8 0 0 6F 
            txModule.params[slot].currentVal = (int32_t)rxBuffer[currentIdx++];  // 1
            txModule.params[slot].minVal = (int32_t)rxBuffer[currentIdx++];      // 1
            txModule.params[slot].maxVal = (int32_t)rxBuffer[currentIdx++];      // 8
            currentIdx++;  // Not sure what this is for on uint8 type but there is an extra 0x0 in thedata 
            unitCharCount = 0;
            while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length + 2) && unitCharCount < (CRSF_MAX_STRING_LEN - 1)) {
                txModule.params[slot].units[unitCharCount++] = (char)rxBuffer[currentIdx++];
            }
            txModule.params[slot].units[unitCharCount] = '\0';  
 #ifdef LUADEBUG        
            Serial.print("[PARAMETER SAVED] ");
            Serial.print(txModule.params[slot].label);
            Serial.print(" ("); Serial.print(txModule.params[slot].minVal); 
            Serial.print("-"); Serial.print(txModule.params[slot].maxVal); Serial.print("): ");
            Serial.print(txModule.params[slot].currentVal); Serial.println(txModule.params[slot].units);
#endif
            currentChunk = 0;
            // currentSettingsIndex++;
            currentSettingsIndex = -1; // Move on to the next in the stack
            break;
        case CRSF_TEXT_SELECTION:
            // Dropdown/options selection - the main type used in ELRS
            // New parameters (currentChunk==0) start the choices after the label string.
            // Continuation chunks enter straight into choices strings at Index 7
            while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && txModule.params[slot].valueStringCharCount < (CRSF_MAX_PARAM_DATA_LEN - 1)) {
              txModule.params[slot].valueString[txModule.params[slot].valueStringCharCount++] = (char)rxBuffer[currentIdx++];
            }
            txModule.params[slot].valueString[txModule.params[slot].valueStringCharCount] = '\0';

            // --- FINAL CHUNK RECEPTION CLOSURE ---
            if (chunksRemaining == 0) {
              // The selected option is after the choices string null terminator
              currentIdx++;
              if (currentIdx < (length + 2)) {
                txModule.params[slot].currentVal = rxBuffer[currentIdx];
              }
              // Next byte is min selection
              currentIdx++;
              if (currentIdx < (length + 2)) {
                txModule.params[slot].minVal = rxBuffer[currentIdx];
              }     
              // Next byte is max selection (also number of choices)
              currentIdx++;
              if (currentIdx < (length + 2)) {
                txModule.params[slot].maxVal = rxBuffer[currentIdx];
              }    
              // Next byte is 0 in all cases i checked (maybe reserved for step?)
              currentIdx++;
              // Parse out the units String
              currentIdx++;
              unitCharCount = 0;
              while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length + 2) && unitCharCount < (CRSF_MAX_STRING_LEN - 1)) {
                txModule.params[slot].units[unitCharCount++] = (char)rxBuffer[currentIdx++];
              }
              txModule.params[slot].units[unitCharCount] = '\0';      

              parseChoicesString(slot);
#ifdef LUADEBUG        
              Serial.print("[PARAMETER SAVED] ");
              Serial.print(txModule.params[slot].label);
              Serial.print(" | Choices("); Serial.print(txModule.params[slot].minVal); Serial.print("-"); Serial.print(txModule.params[slot].maxVal); Serial.print("): ");
              if (txModule.params[slot].choicesCount > 0) {
                for (int i = 0; i < txModule.params[slot].choicesCount; i++) {
                    Serial.print(txModule.params[slot].choices[i].text); 
                    if (i < txModule.params[slot].choicesCount - 1) {
                        Serial.print(", ");
                    }
                } 
              }
              Serial.print(" | Active Selection: "); Serial.print(txModule.params[slot].choices[txModule.params[slot].currentVal].text);
              Serial.println(txModule.params[slot].units);
#endif
              // Move on to process the next sequential hardware parameter tree setting
              currentChunk = 0;
              // currentSettingsIndex++;
              currentSettingsIndex = -1; // Move on to the next in the stack
            }
            else {
              // Data payload remains chunked, increment block indicators to fetch remaining parts
              currentChunk++;
#ifdef LUADEBUG        
              Serial.print("Chunk stored");
              //Serial.print("Chunk stored. Current string: "); Serial.println(txModule.params[slot].valueString);
#endif
            }
            break;
        case CRSF_FOLDER:
            // Menu folder containing sub-items
            // Theoretically this could be chunked too, but not done in ELRS (Yet)
            // if (chunksRemaining == 0) {
#ifdef LUADEBUG        
            Serial.print("[PARAMETER SAVED] ");
            Serial.print(txModule.params[slot].label);
            Serial.print(" | Menu (ID: "); Serial.print(txModule.params[slot].id); Serial.println(")");
#endif
            // Move on to process the next sequential hardware parameter tree setting
            currentChunk = 0;
            // currentSettingsIndex++;
            currentSettingsIndex = -1; // Move on to the next in the stack
            break;
        case CRSF_INFO:
            // Display non-editable static string
            while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && txModule.params[slot].valueStringCharCount < (CRSF_MAX_PARAM_DATA_LEN - 1)) {
              txModule.params[slot].valueString[txModule.params[slot].valueStringCharCount++] = (char)rxBuffer[currentIdx++];
            }
            txModule.params[slot].valueString[txModule.params[slot].valueStringCharCount] = '\0';
#ifdef LUADEBUG        
            Serial.print("[PARAMETER SAVED] ");
            Serial.print(txModule.params[slot].label);
            Serial.print(" | Info String: "); Serial.println(txModule.params[slot].valueString);
#endif
            currentChunk = 0;
            // currentSettingsIndex++;
            currentSettingsIndex = -1; // Move on to the next in the stack
            break;
        case CRSF_COMMAND:
            // Execute command / status such as initiate bind or BLE joystick
            // Sample Packet: EA 17 2B EA EE F 0 E D 45 6E 61 62 6C 65 20 57 69 46 69 0 0 C8 0 5D 
            // Sync byte, Len, type (0x2b settings info), dest (0xEA handset), source (0xEE module), id (0x0f=15), chunks remaining (0), parent (0x0e=14), type (0x0d=command)
            // label (Enable WiFi), string terminator 0x0, step/state (0), timeout (0xc8=200=2s), null terminated status string
            // Label has been saved and currentIdx is sitting at step/state
            txModule.params[slot].step = rxBuffer[currentIdx++];         
            txModule.params[slot].timeout = rxBuffer[currentIdx++] * 10;
            // info/status stored in the valueString property
            while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && txModule.params[slot].valueStringCharCount < (CRSF_MAX_PARAM_DATA_LEN - 1)) {
              txModule.params[slot].valueString[txModule.params[slot].valueStringCharCount++] = (char)rxBuffer[currentIdx++];
            }
            txModule.params[slot].valueString[txModule.params[slot].valueStringCharCount] = '\0';
            // txModule.params[slot].valueStringCharCount--; // Decrement the valueString char counter because the loop above insists on picking up the crc byte in chunked params

#ifdef LUADEBUG        
            Serial.print("[PARAMETER SAVED] ");
            Serial.print(txModule.params[slot].label);
            Serial.print(" | Command (Current state "); 
            switch(txModule.params[slot].step) {
              case 0: // IDLE
                Serial.print("IDLE"); 
                break;
              case 1: // CLICK - user has clicked the command to execute
                Serial.print("CLICK"); 
                break;
              case 2: // EXECUTING - command is executing
                Serial.print("EXECUTING"); 
                break;
              case 3: // ASKCONFIRM - command pending user OK
                Serial.print("ASKCONFIRM"); 
                break;
              case 4: // CONFIRMED - user has clicked confirm
                Serial.print("CONFIRMED"); 
                break;
              case 5: // CANCEL - user has requested cancel
                Serial.print("CANCEL"); 
                break;
              case 6: // QUERY - host is requested updated status
                Serial.print("QUERY"); 
                break;
            }
            Serial.print(" | Timeout ");
            Serial.print(txModule.params[slot].timeout); 
            Serial.print(" | Info/Status : "); Serial.print(txModule.params[slot].valueString); Serial.println(")");
#endif
            currentChunk = 0;
            // currentSettingsIndex++;
            currentSettingsIndex = -1; // Move on to the next in the stack
            break;
        default:
            // String, Float or Int types defined in spec but not used in ELRS
            // Move on to process the next sequential hardware parameter tree setting
#ifdef LUADEBUG        
            Serial.print("Unhanded info packet type. Packet data: ");
            for (int i = 0; i < length; i++){
              Serial.print(rxBuffer[i],HEX);Serial.print(" "); 
            } Serial.println();
#endif
            currentChunk = 0;
            // currentSettingsIndex++;
            currentSettingsIndex = -1; // Move on to the next in the stack
        break;
    }
    lastParameterQueryTime = 0;  //Move on to the next parameter immediately
  }
}

void ELRSLua::parseElrsStatusPacket(uint8_t rxBuffer[], uint8_t length) {
    // rxBuffer[3] = destination (handset, 0xEF/0xEA), rxBuffer[4] = source (TX module, 0xEE).
    // Discard frames not originating from the ELRS TX module address.
    if (rxBuffer[4] != MODULE_ADDRESS) {
#ifdef LUADEBUG  
    Serial.print("\nUnexpected Origin Address in Link Stats Packet (0x2E):    0x"); Serial.println(rxBuffer[4], HEX);
#endif
        return;
    }
    elrsStatus.packetsBad  = rxBuffer[5];
    elrsStatus.packetsGood = ((uint16_t)rxBuffer[6] << 8) | rxBuffer[7]; 
    uint8_t rawFlags = rxBuffer[8]; 
    elrsStatus.isConnected   = (rawFlags & 0x01) != 0; // Bit 0 active
    elrsStatus.modelMismatch = (rawFlags & 0x04) != 0; // Bit 2 active
    elrsStatus.isArmed       = (rawFlags & 0x08) != 0; // Bit 3 active
    memset(elrsStatus.statusMessage, 0, sizeof(elrsStatus.statusMessage));
    uint8_t stringStartIdx = 9;
    uint8_t charCounter = 0;
    while (rxBuffer[stringStartIdx] != 0x00 && stringStartIdx < (length + 2) && charCounter < 31) {
        elrsStatus.statusMessage[charCounter++] = (char)rxBuffer[stringStartIdx++];
    }
    elrsStatus.statusMessage[charCounter] = '\0';

#ifdef LUADEBUG        
    // Debug output
    Serial.println("\n============ [ELRS LINK STATUS 0x2E] ============");
    // Serial.print("Raw Data            : ");
    // for (int i = 0; i < length; i++){
    //   Serial.print(rxBuffer[i],HEX);Serial.print(" "); 
    // }
    // Serial.println();
    Serial.print("Pkt Stream Health : Good: "); Serial.print(elrsStatus.packetsGood);
    Serial.print(" | Bad: "); Serial.println(elrsStatus.packetsBad);
    
    Serial.print("Link State Flags  : ");
    if (elrsStatus.isConnected)   Serial.print("[CONNECTED] ");   else Serial.print("[DISCONNECTED] ");
    if (elrsStatus.isArmed)       Serial.print("[ARMED] ");       else Serial.print("[DISARMED] ");
    if (elrsStatus.modelMismatch) Serial.print("[!!! MODEL MISMATCH !!!] ");
    Serial.println();

    if (strlen(elrsStatus.statusMessage) > 0) {
        Serial.print("Module Alert Msg  : "); Serial.println(elrsStatus.statusMessage);
    }
    Serial.println("=================================================");
#endif
}



// Response Parser functions
// ========================================================

void ELRSLua::clearModule() {
    memset(txModule.name, 0, sizeof(txModule.name));   
    for (int i = 0; i < txModule.paramCount; i++) {
        txModule.params[i].id = 0;
        txModule.params[i].parentFolder = 0;
        txModule.params[i].type = static_cast<crsfValueType>(0);
        txModule.params[i].hidden = false;
        txModule.params[i].currentVal = 0;
        memset(txModule.params[i].label, 0, sizeof(txModule.params[i].label)); 
        for (int j = 0; j < txModule.params[i].choicesCount; j++) {
            memset(txModule.params[i].choices[j].text, 0, sizeof(txModule.params[i].choices[j].text)); 
        }
        txModule.params[i].choicesCount = 0;
    }
    txModule.paramCount = 0;
    txModule.paramsLoaded = false;
    moduleInfoReceived = false;
    // serialNumber, hwVersion, swVersion, protocolVersion will be reset when a module is next detected
}

void ELRSLua::parseChoicesString(int paramIndex) {
    uint8_t currentIdx = 0;
    uint8_t activeOptionSlot = 0;
    uint8_t charCount = 0;
    txModule.params[paramIndex].choicesCount = 0;

    // Keep parsing until we hit the end of the packet payload max length
    while (currentIdx < CRSF_MAX_PARAM_DATA_LEN) {
        char c = txModule.params[paramIndex].valueString[currentIdx++];
        
        if (c == 0x00) {
            // Seal the final string choice segment and return.
            txModule.params[paramIndex].choices[activeOptionSlot].text[charCount] = '\0';
            txModule.params[paramIndex].choicesCount = activeOptionSlot + 1;
            return; 
        }
        else if (c == ';') {
            // Semicolon delimiter encountered: seal the current choice string segment
            txModule.params[paramIndex].choices[activeOptionSlot].text[charCount] = '\0';
            
            // Advance to the next safe storage choice slot parameter bounds
            if (activeOptionSlot < (CRSF_MAX_PARAMS - 1)) {
                activeOptionSlot++;
            }
            charCount = 0;
        } 
        else {
            if (charCount < (CRSF_MAX_STRING_LEN - 1)) {
                txModule.params[paramIndex].choices[activeOptionSlot].text[charCount++] = c;
            }
        }
    }
   
}

int ELRSLua::getParamSlot(uint8_t id) {
    // 1. Search if this parameter ID is already initialized
    for (int i = 0; i < txModule.paramCount; i++) {
        if (txModule.params[i].id == id) return i;
    }
    // 2. If it's a new ID, allocate a fresh index slot if we have space left
    if (txModule.paramCount < CRSF_MAX_PARAMS) {
        int freshSlot = txModule.paramCount;
        txModule.params[freshSlot].id = id;
        // Initialize all parameter data
        txModule.params[freshSlot].parentFolder = 0;
        txModule.params[freshSlot].type = static_cast<crsfValueType>(0);
        txModule.params[freshSlot].hidden = false;
        txModule.params[freshSlot].currentVal = 0;
        txModule.params[freshSlot].minVal = 0;
        txModule.params[freshSlot].maxVal = 0;
        txModule.params[freshSlot].precision = 0;
        txModule.params[freshSlot].step = 0;
        txModule.params[freshSlot].timeout = 0;
        memset(txModule.params[freshSlot].label, 0, sizeof(txModule.params[freshSlot].label));
        memset(txModule.params[freshSlot].valueString, 0, sizeof(txModule.params[freshSlot].valueString));
        txModule.params[freshSlot].valueStringCharCount = 0;
        memset(txModule.params[freshSlot].units, 0, sizeof(txModule.params[freshSlot].units));
        memset(txModule.params[freshSlot].choices, 0, sizeof(txModule.params[freshSlot].choices));
        txModule.params[freshSlot].choicesCount = 0;
        txModule.paramCount++;
        return freshSlot;
    } 
    else {
      Serial.println("Exceeded max parameter count. Cannot allocate free slot.");
    }
    return -1; // Out of safe tracking memory space
}


// Menu driver functions
// ========================================================
// Updates the selectedParam and returns a number indicating which way it navigated
// 0 = No move
// 1 = Up
// 2 = Down
// 3 = Left
// 4 = Right
uint8_t ELRSLua::nextInFolder() {
    uint8_t parentFolder = txModule.params[selectedParam].parentFolder;

    for (uint8_t i = selectedParam + 1; i < txModule.paramCount; i++) {
        if (txModule.params[i].parentFolder == parentFolder && !txModule.params[i].hidden) {
            selectedParam = i;
            return 1; // Moved down, scroll up
        }
    }
    // if we got here it means there were no other entries at this folder level
    // Now go back to the start and find the first item in the folder
    for (uint8_t i = 0; i < selectedParam; i++) {
        if (txModule.params[i].parentFolder == parentFolder && !txModule.params[i].hidden) {
            selectedParam = i;
            return 2;
        }
    }
    // not found (unexpected), dont updated selected param and return 'no move'
    return 0;
}

uint8_t ELRSLua::prevInFolder() {
    uint8_t parentFolder = txModule.params[selectedParam].parentFolder;

    for (uint8_t i = selectedParam - 1; i >= 0; i--) {
        if (txModule.params[i].parentFolder == parentFolder && !txModule.params[i].hidden) {
            selectedParam = i;
            return 2; // Moved up, scroll down
        }
    }
    // if we got here it means there were no other entries at this folder level
    // Now search backwards from the end and find the last item in the folder.
    for (uint8_t i = txModule.paramCount - 1; i > selectedParam ; i--) {
        if (txModule.params[i].parentFolder == parentFolder && !txModule.params[i].hidden) {
            selectedParam = i;
            return 1;
        }
    }
    // not found (unexpected), dont updated selected param and return 'no move'
    return 0;
}

uint8_t ELRSLua::enterFolder() {
    if (txModule.params[selectedParam].type != CRSF_FOLDER) return 0;
    for (uint8_t i = selectedParam + 1; i < txModule.paramCount; i++) {  // Assume child items will follow folder
        if (txModule.params[i].parentFolder == selectedParam && !txModule.params[i].hidden) {
            selectedParam = i;
            return 3; // Scroll left
        }
    }
    // not found (unexpected), dont updated selected param and return 'no move'
    return 0;
}

uint8_t ELRSLua::exitFolder() {
    if (txModule.params[selectedParam].parentFolder == 0) {
        if (selectedParam > 0) { // pressing left brings you back to the top of the menu
            selectedParam = 0;
            return 1;
        } else {
            return 0;
        }
    } else {
        selectedParam = txModule.params[selectedParam].parentFolder;
        return 4;  // Scroll right
    }
}

const crsfParameter& ELRSLua::getCurrentParam() const {
    return getParam(selectedParam);
}

const crsfParameter& ELRSLua::getParam(const uint8_t index) const {
    if (ready() && txModule.paramsLoaded && index >= 0 && index < txModule.paramCount) {
        if (index == 0) {  // Special handling for index 0
            static crsfParameter MenuTitle;
            MenuTitle.id = 0;
            MenuTitle.parentFolder = 0;
            MenuTitle.type = CRSF_TEXT_SELECTION;
            strcpy(MenuTitle.label, "ELRS Config");
            snprintf(MenuTitle.choices[0].text, sizeof(MenuTitle.choices[0].text), "Pkts: %u/%u", elrsStatus.packetsGood, elrsStatus.packetsBad);
            MenuTitle.currentVal = 0;
            return MenuTitle;
        } else {
            return txModule.params[index]; // Returns a reference directly to the array item
        }
    } else {
        static crsfParameter notFound;
        notFound.id = 0;
        notFound.parentFolder = 0;
        notFound.type = CRSF_INFO;
        strcpy(notFound.label, "Loading...");
        return notFound;
    }
}

int ELRSLua::findParamByLabel(const char searchString[]) const {
    for (uint8_t i = 1; i < txModule.paramCount; i++) {
        if (strncmp(txModule.params[i].label, searchString, strlen(searchString)) == 0 ) {
            return i;
        }
    }
    return -1;
}

// Edit Param functions
void ELRSLua::editParamPrev() { 
    switch(txModule.params[selectedParam].type) {
        case CRSF_UINT8:
        case CRSF_INT8:
        case CRSF_UINT16:
        case CRSF_INT16:
        case CRSF_UINT32:
        case CRSF_INT32:
        case CRSF_UINT64:
        case CRSF_INT64:
        case CRSF_TEXT_SELECTION:
            editValue--;
            if (editValue < txModule.params[selectedParam].minVal) editValue = txModule.params[selectedParam].minVal;
            break;
        case CRSF_FLOAT:
            editValue -= txModule.params[selectedParam].step;
            if (editValue < txModule.params[selectedParam].minVal) editValue = txModule.params[selectedParam].minVal;
            break;
        case CRSF_STRING:
            // Not sure on this one... Not needed for now
            break;
        default:
            // Do nothing
            break;   
    }
}

void ELRSLua::editParamNext() {
    switch(txModule.params[selectedParam].type) {
        case CRSF_UINT8:
        case CRSF_INT8:
        case CRSF_UINT16:
        case CRSF_INT16:
        case CRSF_UINT32:
        case CRSF_INT32:
        case CRSF_UINT64:
        case CRSF_INT64:
        case CRSF_TEXT_SELECTION:
            editValue++;
            if (editValue > txModule.params[selectedParam].maxVal) editValue = txModule.params[selectedParam].maxVal;
            break;
        case CRSF_FLOAT:
            editValue += txModule.params[selectedParam].step;
            if (editValue > txModule.params[selectedParam].maxVal) editValue = txModule.params[selectedParam].maxVal;
            break;
        case CRSF_STRING:
            // Not sure on this one... Not needed for now
            break;
        default:
            // Do nothing
            break;   
    }
}

void ELRSLua::editParamSave() {
    sendCommand(selectedParam, editValue); // This will queue the command to update the parameter
    txModule.params[selectedParam].currentVal = editValue; // updating the local value manually - will update via the refreshStack too
    
    // Update parentFolder (if this is a child), and other items in this folder tree level
    loadOneParam(selectedParam); // Update this setting as well
    int parentFolder = txModule.params[selectedParam].parentFolder;
    if (parentFolder > 0) {
        for (uint8_t i = parentFolder+1; i < txModule.paramCount; i++) {
            if (i != selectedParam && 
                txModule.params[i].parentFolder == parentFolder && 
                !txModule.params[i].hidden &&
                txModule.params[i].type != CRSF_FOLDER &&
                txModule.params[i].type != CRSF_COMMAND) {
                loadOneParam(i);
            }
        }
        loadOneParam(parentFolder);
    }

    lastParameterQueryTime = millis();
    lastHandshakeTime +=100;
    menuState = MENU_BROWSE;  // Set menu state back to browse
}


void ELRSLua::loadAllParams(uint8_t totalCount) {
    loadQueue.clear();
    // Push backward so they pop in ascending order (1, 2, 3...)
    for (int i = totalCount; i >= 0; i--) {
        loadQueue.push(i);
    }
    connectionState = ELRS_LOAD_PARAMS; 
}

void ELRSLua::loadOneParam(uint8_t paramIndex) {
    if (loadQueue.push(paramIndex)) {
        // Intercept READY state and force a background cycle if needed
        if (connectionState == ELRS_READY) {
            connectionState = ELRS_LOAD_PARAMS;
        }
    }
}

// Main driver function
// ========================================================

void ELRSLua::update() {
    // First poll for incoming telemetry packets
    if (crsf.telemetryQueue.hasItems()) {
      crsfPacket telemetryPacket;
      if (crsf.telemetryQueue.pop(telemetryPacket)) {
        // Queued packets should be something we are interested in here!
        uint8_t packetType = telemetryPacket.data[2];
#ifdef LUADEBUG        
        Serial.print("Received Packet type 0x"), Serial.println(packetType,HEX);
#endif
        switch (packetType) {
          case ELRS_DEVICE_INFO:
            moduleInfoReceived = true;
            parseDeviceInfoPacket(telemetryPacket.data, telemetryPacket.length);     
            break;           
          case ELRS_SETTINGS_RESPONSE:
            parseSettingsPacket(telemetryPacket.data, telemetryPacket.length);
            break;
          case ELRS_STATUS:
            parseElrsStatusPacket(telemetryPacket.data, telemetryPacket.length);
            break;
        }
      }
    }

    // Now handle initialization/configuration tasks
    uint32_t now = millis();

    switch(connectionState) {
        case ELRS_BOOT_DELAY:
            // Allow ELRS module hardware 3 seconds to fully boot up (3 seconds from system startup)
            if (millis() >= 3000) {
                connectionState = ELRS_PINGING;
            }
            break;

        case ELRS_PINGING:
            // Every 1000ms, send a Device Ping (0x28)
            if (now - lastHandshakeTime > 1000) {
                lastHandshakeTime = now;
                pingDevices(); // Sends 0x28
#ifdef LUADEBUG        
                Serial.println("[ELRS] Sending Device Ping (0x28)...");
#endif
            }
            
            // Transition condition: If the parser caught a 0x29 packet and saved a name
            if (strlen(txModule.name) > 0) {
                txModule.paramsLoaded = false;
                loadAllParams(totalSettingsCount);
                connectionState = ELRS_LOAD_PARAMS;
                // currentSettingsIndex = 0;  
                currentChunk = 0;      
                // parameterDiscoveryActive = true;
                lastParameterQueryTime = now;

                lastHandshakeTime = now;
                Serial.println("[ELRS] Module Detected! Getting Stats/Initializing param discovery...");  //Send this regardless of debug
                requestElrsStatus();
            }
            break;

        case ELRS_LOAD_PARAMS:
            if (now - lastHandshakeTime > 1000) {
                lastHandshakeTime = now;
                requestElrsStatus();
#ifdef LUADEBUG        
                Serial.println("[ELRS] Sending LinkStat Request (0x2D, 0, 0)...");
#endif
            }
            if (now - lastParameterQueryTime > 500) {
                if (currentSettingsIndex != -1) {
                    // if (now - lastParameterQueryTime > 500) {
                        requestSetting(currentSettingsIndex, currentChunk); // Retry last setting, or get next chunk
                        lastParameterQueryTime = now;
                    // }
                } else {
                    // Fetch the next item from the queue
                    if (!loadQueue.isEmpty()) {
                        currentSettingsIndex = loadQueue.pop();
                        currentChunk = 0;
                        requestSetting(currentSettingsIndex, currentChunk);
                        lastParameterQueryTime = now;
                    } else {
                        txModule.paramsLoaded = true;
                        Serial.println("[ELRS] >>> PARAMETER TREE MATRIX FULLY POPULATED AND CACHED <<<");
                        // Done! set state machine back to ready
                        connectionState = ELRS_READY;
                    }
                }
            }
            break;

        case ELRS_READY:
            // Normal operation loop
            // if (showMenu && now - lastHandshakeTime > 1000) {
            if (now - lastHandshakeTime > 1000) {
                lastHandshakeTime = now;
                requestElrsStatus();
#ifdef LUADEBUG        
                Serial.println("[ELRS] Sending LinkStat Request (0x2D, 0, 0)...");
#endif
            }
            break;
    }

    // If the crsf class detects the module is disconnected, drop back to pinging
    if (!crsf.moduleConnected && (connectionState > ELRS_PINGING)) {
        loadQueue.clear();
        clearModule();
        connectionState = ELRS_PINGING;
        Serial.println("[ELRS] Module connection Lost.");  // Print this regardless
    }        
}
