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
#include <Preferences.h>
#include <vector>
#include "config.h"

HandsetSettings globalSettings;
ParamCollection handsetSettingsMenu;
static Preferences configPrefs; // Using a static local instance to avoid global namespace pollution

void loadGlobalSettings(bool initialize) {
    // Empty the menu
    if (initialize) {handsetSettingsMenu.clearParams();}
    int idx = 0; 

    // Menu Header goes into Index 0, assume this will return ok
    addInfo(idx, idx, "Channel Menu", "");
    idx++;

    // Open the "settings" namespace in Read-Only mode (true)
    configPrefs.begin("settings", true);

    // Load standalone settings
    globalSettings.channelMode = configPrefs.getUChar("Stick Mode", 2);        // Default to Stick Mode 2
    addChoiceParam(idx, 0, "Stick Mode", {"","Mode 1","Mode 2","Mode 3","Mode 4"}, globalSettings.channelMode, 1, 4);
    idx++;

    globalSettings.adcFilter = configPrefs.getUChar("ADC Filter", 0);  // Default to filter OFF
    addChoiceParam(idx, 0, "ADC Filter", {"Off","On"}, globalSettings.adcFilter, 0, 1);
    idx++;

    globalSettings.deadband = configPrefs.getUShort("Deadband", 0); // Default 0 deadband
    addNumericParam(idx, 0, CRSF_UINT16,"Deadband", globalSettings.deadband, 0, 200, 10, " / 2048");
    idx++;

    globalSettings.auxLongPress = configPrefs.getUShort("Long Press", AUX_LONG_PRESS); // Default 1000ms long press
    addNumericParam(idx, 0, CRSF_UINT16,"Long Press", globalSettings.auxLongPress, 300, 5000, 100, "ms");
    idx++;

    // Load channel arrays dynamically using an index suffix string
    // Channel Meta Arrays mapping internal indexes 0, 1, 2, 3 to labels
    const char* folderLabels[4] = {"Ail/Roll", "Ele/Pitch", "Throttle", "Rud/Yaw"};
    const char* shortLabels[4]  = {"Ail", "Ele", "Thr", "Yaw"};
    char keyBuf[15]; 
    for (uint8_t ch = 0; ch < 4; ch++) {
        // Save the parent folder ID for this specific channel submenu container
        int currentFolderID = idx; 

        addFolder(idx, 0, folderLabels[ch]);
        idx++;

        if (ch == THROTTLE) { // Throttle-specific settings
            // 0. Throttle Channel middle center
            globalSettings.throttleCentered = configPrefs.getBool("Center Throt", false);  // Default standard quad throttle
            addChoiceParam(idx, currentFolderID, "Center Throt", {"No","Yes"}, globalSettings.throttleCentered, 0, 1);
            idx++;
        }

        // 1. Channel Reverse
        snprintf(keyBuf, sizeof(keyBuf), "%s Reverse", shortLabels[ch]);
        globalSettings.reverse[ch] = configPrefs.getBool(keyBuf, false);
        addChoiceParam(idx, currentFolderID, keyBuf, {"Normal","Reverse"}, globalSettings.reverse[ch], 0, 1);
        idx++;

        // 2. Channel Limit
        snprintf(keyBuf, sizeof(keyBuf), "%s Limit", shortLabels[ch]);
        globalSettings.limit[ch] = configPrefs.getUChar(keyBuf, 100); // Default 100% full travel throw
        addNumericParam(idx, currentFolderID, CRSF_UINT8,keyBuf, globalSettings.limit[ch], 10, 100, 10, "%");
        idx++;

        // 3. Channel Expo
        snprintf(keyBuf, sizeof(keyBuf), "%s Expo", shortLabels[ch]);
        globalSettings.expo[ch] = configPrefs.getUChar(keyBuf, 0); // Default 0% linear
        addNumericParam(idx, currentFolderID, CRSF_UINT8,keyBuf, globalSettings.expo[ch], 0, 100, 5, "%");
        idx++;    

        // 4. Additional settings
        // Any additional settings that would be available for all channels should have an array property in globalSettings to represent the 4 channels
        // Label must be less than 15 chars when combined with the short label "Ail " or "Thr "
    }

    configPrefs.end();
}

void saveGlobalSettings() {
    // Open the "settings" namespace in Read-Write mode (false)
    configPrefs.begin("settings", false);

    switch (handsetSettingsMenu.getCurrentParam().type) {
        case CRSF_UINT8:
        case CRSF_INT8:
        case CRSF_TEXT_SELECTION:
            configPrefs.putUChar(handsetSettingsMenu.getCurrentParam().label,handsetSettingsMenu.editValue);
            loadGlobalSettings(false);
            break;
        case CRSF_UINT16:
        case CRSF_INT16:
            configPrefs.putUShort(handsetSettingsMenu.getCurrentParam().label,handsetSettingsMenu.editValue);
            loadGlobalSettings(false);
            break;
        default:
            // Do nothing
            break;
    }
    configPrefs.end();
}

void executeSettingsCommand() {
    // Do something with the current command??
    switch (handsetSettingsMenu.selectedParam) {
        case 99:
            // Execute command 99 reset (eg Reset all settings)
            break;
    }
}

bool addFolder(int idx, int parent, const char* label) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = CRSF_FOLDER;
        strcpy(handsetSettingsMenu[idx].label, label);
        return true;
    }
    else return false;
}

bool addInfo(int idx, int parent, const char* label, const char* valueString) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = CRSF_INFO;
        strcpy(handsetSettingsMenu[idx].label, label);
        strcpy(handsetSettingsMenu[idx].valueString, valueString);
        return true;
    }
    else return false;
}

bool addChoiceParam(int idx, int parent, const char* label, const std::vector<const char*>& choices, int currVal, int min, int max) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = CRSF_TEXT_SELECTION;
        strcpy(handsetSettingsMenu[idx].label, label);
        // for (int i=0;i<sizeof(choices);i++) {
        for (int i=0;i<choices.size();i++) {            
            strcpy(handsetSettingsMenu[idx].choices[i].text, choices[i]);
        }
        handsetSettingsMenu[idx].minVal = min;
        handsetSettingsMenu[idx].maxVal = max;
        handsetSettingsMenu[idx].choicesCount = (max-min) + 1;
        handsetSettingsMenu[idx].currentVal = currVal;        
        return true;
    }
    else return false;
}

bool addNumericParam(int idx, int parent, crsfValueType type, const char* label, int currVal, int min, int max, int step, const char* units) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = type;
        strcpy(handsetSettingsMenu[idx].label, label);
        handsetSettingsMenu[idx].minVal = min;
        handsetSettingsMenu[idx].maxVal = max;
        handsetSettingsMenu[idx].step = step;
        strcpy(handsetSettingsMenu[idx].units, units);   
        handsetSettingsMenu[idx].currentVal = currVal;     
        return true;
    }
    else return false;
}