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
 
#include "elrs.h"
#include "crsf.h" // Needed here so the compiler knows the methods inside CRSF

// Initialize the reference via the initializer list
ELRS::ELRS(CRSF& crsfInstance) : crsf(crsfInstance) {}

// Command builder functions
// ========================================================

void ELRS::sendCommand(uint8_t command, uint8_t value) {
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
#ifdef ELRSDEBUG        
    Serial.print("[ELRS] -> Send Command: ");
    Serial.print(command);
    Serial.print(", Value: ");
    Serial.print(value); Serial.println();
#endif
}

void ELRS::pingDevices() {
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

void ELRS::requestSetting(uint8_t settingIndex, uint8_t chunk) {
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
#ifdef ELRSDEBUG        
    Serial.print("[ELRS] -> Querying Menu Parameter Index: ");
    Serial.print(settingIndex);
    Serial.print(", chunk: ");
    Serial.print(chunk); Serial.println();
#endif
}

// Request Info Message by sending ELRS_SETTINGS_WRITE (0x2D) with param/value 0
void ELRS::requestElrsStatus() {
    sendCommand(0,0);
}


// Response Parser functions
// ========================================================

void ELRS::parseDeviceInfoPacket(uint8_t rxBuffer[], uint8_t length) {
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
    txModule.params.paramCount = 0;
#ifdef ELRSDEBUG        
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


void ELRS::parseElrsStatusPacket(uint8_t rxBuffer[], uint8_t length) {
    // rxBuffer[3] = destination (handset, 0xEF/0xEA), rxBuffer[4] = source (TX module, 0xEE).
    // Discard frames not originating from the ELRS TX module address.
    if (rxBuffer[4] != MODULE_ADDRESS) {
#ifdef ELRSDEBUG  
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

    // Update menu item 0, which is repurposed as our menu title
    snprintf(txModule.params[0].choices[0].text, sizeof(txModule.params[0].choices[0].text), "Pkts: %u/%u", elrsStatus.packetsGood, elrsStatus.packetsBad);

#ifdef ELRSDEBUG        
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


void ELRS::parseParameterInfoPacket(uint8_t rxBuffer[], uint8_t length) {
	  // tempParamStorage = fieldData from ELRS Lua (and tempStorageIndex = fieldData#)
    static uint8_t tempParamStorage[CRSF_PAYLOAD_SIZE_MAX * 5];  // Assume a max of 5 chunks per parameter
    static uint16_t tempStorageIndex = 0;
    static uint8_t expectedChunksRemaining = 0;
    uint8_t fieldIndex = rxBuffer[5]; //5
    if (fieldIndex != currentSettingsIndex) {
        memset(tempParamStorage, 0, sizeof(tempParamStorage));
        tempStorageIndex = 0; 
        currentChunk = 0;
        return;
    }

    int slot = txModule.params.getParamSlot(fieldIndex);
    chunksRemaining = rxBuffer[6]; //6
    if (slot == -1 || (tempStorageIndex > 0 && chunksRemaining != expectedChunksRemaining)) {
        return;
    }

    if (currentChunk == 0) {
        memset(tempParamStorage, 0, sizeof(tempParamStorage));
        memcpy(tempParamStorage, rxBuffer, length); // Copy entire packet including CRC
        tempStorageIndex = length;
    }
	  else {
        if (sizeof(tempParamStorage) > tempStorageIndex + (length-7)) {
            size_t offset = tempStorageIndex - 1; // Move one character back to overwrite the prev package CRC
            memcpy(tempParamStorage+offset, rxBuffer+7, length-7);
            tempStorageIndex += length-8;  // One less because we moved back to overwrite the previous CRC
        } 
        else { // Unexpected - buffer overrun trying to store chunk
            // add null pointer to close off strings
            // move on to next field.
            expectedChunksRemaining = 0;
            currentChunk=0;
            currentSettingsIndex = -1;
            return;
        }
	  } 
	
    // If there are any chunks remaining, move on to the next chunk
    if (chunksRemaining > 0) {
        currentChunk++;    	
        expectedChunksRemaining = chunksRemaining - 1;
    }	
	  else {
        parseParameter(tempParamStorage, tempStorageIndex);
        
        tempStorageIndex = 0;
        currentChunk=0;
        currentSettingsIndex = -1;
	  }
	
    lastParameterQueryTime = 0;  //Move on to the next parameter immediately
}


void ELRS::parseParameter(uint8_t rxBuffer[], uint8_t length) {
    uint8_t currentIdx = 5;
    uint8_t fieldIndex = rxBuffer[currentIdx++]; //5

    int slot = txModule.params.getParamSlot(fieldIndex);
    if (slot != -1) {
    // First, handle special entry 0
    if (fieldIndex == 0) {
        // ELRS returns this as a folder item with label HooJ and i am not sure what that means. I am repurposing this as my Config menu title
        txModule.params[0].parentFolder = 0;
        txModule.params[0].type = CRSF_TEXT_SELECTION;
        strcpy(txModule.params[0].label, "ELRS Config");
        snprintf(txModule.params[0].choices[0].text, sizeof(txModule.params[0].choices[0].text), "Pkts: %u/%u", elrsStatus.packetsGood, elrsStatus.packetsBad);
        txModule.params[0].currentVal = 0; 
        return;     
    }

    currentIdx++; // Skip the chunks remaining byte (6) 
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
     
    uint8_t unitCharCount = 0;
    uint8_t choiceCharCount = 0;
    uint8_t valueStringCharCount = 0;
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
 #ifdef ELRSDEBUG        
            Serial.print("[PARAMETER SAVED] ");
            Serial.print(txModule.params[slot].label);
            Serial.print(" ("); Serial.print(txModule.params[slot].minVal); 
            Serial.print("-"); Serial.print(txModule.params[slot].maxVal); Serial.print("): ");
            Serial.print(txModule.params[slot].currentVal); Serial.println(txModule.params[slot].units);
#endif
            break;
        case CRSF_TEXT_SELECTION:
            // Dropdown/options selection - the main type used in ELRS
            // Split the next null terminated string up on ';'
            txModule.params[slot].choicesCount = 0;
            while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && txModule.params[slot].choicesCount < CRSF_MAX_PARAMS) {
              char c = (char)rxBuffer[currentIdx++];
              if (c == ';') {
                  // Seal the current choice string
                  txModule.params[slot].choices[txModule.params[slot].choicesCount].text[choiceCharCount] = '\0';
                  txModule.params[slot].choicesCount++;
                  choiceCharCount = 0;
              }
              else if (choiceCharCount < (CRSF_MAX_STRING_LEN - 1)) {
                  txModule.params[slot].choices[txModule.params[slot].choicesCount].text[choiceCharCount++] = c;
              }
            }
            // Seal the final slot (unless we are at 32 choices, in which case it would already have been sealed)
            if (txModule.params[slot].choicesCount <= CRSF_MAX_PARAMS) {
                txModule.params[slot].choices[txModule.params[slot].choicesCount].text[choiceCharCount] = '\0';
                txModule.params[slot].choicesCount++;
            }
            
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
#ifdef ELRSDEBUG        
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
            break;
        case CRSF_FOLDER:
            // Menu folder containing sub-items
#ifdef ELRSDEBUG        
            Serial.print("[PARAMETER SAVED] ");
            Serial.print(txModule.params[slot].label);
            Serial.print(" | Menu (ID: "); Serial.print(txModule.params[slot].id); Serial.println(")");
#endif
            break;
        case CRSF_INFO:
            // Display non-editable static string
            while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && valueStringCharCount < (CRSF_MAX_PARAM_DATA_LEN - 1)) {
              txModule.params[slot].valueString[valueStringCharCount++] = (char)rxBuffer[currentIdx++];
            }
            txModule.params[slot].valueString[valueStringCharCount] = '\0';
#ifdef ELRSDEBUG        
            Serial.print("[PARAMETER SAVED] ");
            Serial.print(txModule.params[slot].label);
            Serial.print(" | Info String: "); Serial.println(txModule.params[slot].valueString);
#endif
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
            while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && valueStringCharCount < (CRSF_MAX_PARAM_DATA_LEN - 1)) {
              txModule.params[slot].valueString[valueStringCharCount++] = (char)rxBuffer[currentIdx++];
            }
            txModule.params[slot].valueString[valueStringCharCount] = '\0';

            if (txModule.params[slot].step > ELRS_COMMAND_IDLE && ready()) {
                // ELRS is in idle state & we received ELRS_COMMAND_EXECUTING or ELRS_COMMAND_ASKCONFIRM
                nextPopupPollingTime = millis() + txModule.params[slot].timeout;
            }
#ifdef ELRSDEBUG        
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
            break;
        default:
            // String, Float or Int types defined in spec but not used in ELRS
            // Move on to process the next sequential hardware parameter tree setting
#ifdef ELRSDEBUG        
            Serial.print("Unhanded info packet type. Packet data: ");
            for (int i = 0; i < length; i++){
              Serial.print(rxBuffer[i],HEX);Serial.print(" "); 
            } Serial.println();
#endif
        break;
    } // Switch
  } // If Slot!=-1
}


// Other utility functions
// ========================================================

void ELRS::clearModule() {
    memset(txModule.name, 0, sizeof(txModule.name));   
    txModule.params.clearParams();
    txModule.params.paramCount = 0;
    txModule.paramsLoaded = false;
    moduleInfoReceived = false;
    // serialNumber, hwVersion, swVersion, protocolVersion will be reset when a module is next detected
}


void ELRS::editParamSave() {
    sendCommand(txModule.params.selectedParam, txModule.params.editValue); // This will queue the command to update the parameter
    txModule.params[txModule.params.selectedParam].currentVal = txModule.params.editValue; // updating the local value manually - will update via the refreshStack too
    
    // Update parentFolder (if this is a child), and other items in this folder tree level
    loadOneParam(txModule.params.selectedParam); // Update this setting as well
    int parentFolder = txModule.params.getCurrentParam().parentFolder;
    if (parentFolder > 0) {
        for (uint8_t i = parentFolder+1; i < txModule.params.paramCount; i++) {
            if (i != txModule.params.selectedParam && 
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
}

// Call this function when the user clicks a COMMAND item or clicks confirm while in a popup
void ELRS::executeCommand() {
    if (popup()) {
        currentSettingsIndex = txModule.params.selectedParam;
        sendCommand(currentSettingsIndex, ELRS_COMMAND_CONFIRMED);
    }
    else if (ready() && txModule.params.getCurrentParam().type == CRSF_COMMAND) {
        currentSettingsIndex = txModule.params.selectedParam;
        sendCommand(currentSettingsIndex, ELRS_COMMAND_CLICK); 
    }
}

// Function to allow canceling or manually dismissing the UI dialog frame
void ELRS::cancelCommand() {
    if (popup()) {
        currentSettingsIndex = txModule.params.selectedParam;
        sendCommand(currentSettingsIndex, ELRS_COMMAND_CANCEL);
    }
}

void ELRS::loadAllParams(uint8_t totalCount) {
    loadQueue.clear();
    // Push backward so they pop in ascending order (1, 2, 3...)
    for (int i = totalCount; i >= 0; i--) {
        loadQueue.push(i);
    }
    connectionState = ELRS_LOAD_PARAMS; 
}

void ELRS::loadOneParam(uint8_t paramIndex) {
    if (loadQueue.push(paramIndex)) {
        // Intercept READY state and force a background cycle if needed
        if (connectionState == ELRS_READY) {
            connectionState = ELRS_LOAD_PARAMS;
        }
    }
}

// Main driver function
// ========================================================

void ELRS::update(bool menuActive) {
    // First poll for incoming telemetry packets
    if (crsf.telemetryQueue.hasItems()) {
      crsfPacket telemetryPacket;
      if (crsf.telemetryQueue.pop(telemetryPacket)) {
        // Queued packets should be something we are interested in here!
        uint8_t packetType = telemetryPacket.data[2];
#ifdef ELRSDEBUG        
        Serial.print("Received Packet type 0x"), Serial.println(packetType,HEX);
#endif
        switch (packetType) {
          case ELRS_DEVICE_INFO:
            moduleInfoReceived = true;
            parseDeviceInfoPacket(telemetryPacket.data, telemetryPacket.length);     
            break;           
          case ELRS_SETTINGS_RESPONSE:
            parseParameterInfoPacket(telemetryPacket.data, telemetryPacket.length);
            // parseSettingsPacket(telemetryPacket.data, telemetryPacket.length);
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
#ifdef ELRSDEBUG        
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
#ifdef ELRSDEBUG        
                Serial.println("[ELRS] Sending LinkStat Request (0x2D, 0, 0)...");
#endif
            }
            if (now - lastParameterQueryTime > 500) {
                if (currentSettingsIndex != -1) {
                    requestSetting(currentSettingsIndex, currentChunk); // Retry last setting, or get next chunk
                    lastParameterQueryTime = now;
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
            if (menuActive && now - lastHandshakeTime > 1000) {
                requestElrsStatus();
                lastHandshakeTime = now;
#ifdef ELRSDEBUG        
                Serial.println("[ELRS] Sending LinkStat Request (0x2D, 0, 0)...");
#endif
            }
            // Poll current popup status
            if (popup() && now > nextPopupPollingTime) {
                // Set currentSettingsIndex here or the received param will be discarded
                currentSettingsIndex = txModule.params.selectedParam;
                sendCommand(currentSettingsIndex, ELRS_COMMAND_QUERY);
                nextPopupPollingTime = now + (txModule.params.getCurrentParam().timeout > 0 ? txModule.params.getCurrentParam().timeout : 1000); // Default 1000 if param timeout is 0
#ifdef ELRSDEBUG        
                Serial.print("[ELRS] Polling command status (0x2D, "); Serial.print(currentSettingsIndex, HEX); Serial.print(", "); Serial.print(ELRS_COMMAND_QUERY, HEX); Serial.println(")...");
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
