#include "params.h"
// #include "menu.h"

// Collection management functions
// ========================================================
void ParamCollection::clearParams() {
    for (int i = 0; i < paramCount; i++) {
        params[i].id = 0;
        params[i].parentFolder = 0;
        params[i].type = static_cast<crsfValueType>(0);
        params[i].hidden = false;
        params[i].currentVal = 0;
        memset(params[i].label, 0, sizeof(params[i].label)); 
        for (int j = 0; j < params[i].choicesCount; j++) {
            memset(params[i].choices[j].text, 0, sizeof(params[i].choices[j].text)); 
        }
        params[i].choicesCount = 0;
    }
    paramCount = 0;
}

void ParamCollection::parseChoicesString(int paramIndex) {
    uint8_t currentIdx = 0;
    uint8_t activeOptionSlot = 0;
    uint8_t charCount = 0;
    params[paramIndex].choicesCount = 0;

    // Keep parsing until we hit the end of the packet payload max length
    while (currentIdx < CRSF_MAX_PARAM_DATA_LEN) {
        char c = params[paramIndex].valueString[currentIdx++];
        
        if (c == 0x00) {
            // Seal the final string choice segment and return.
            params[paramIndex].choices[activeOptionSlot].text[charCount] = '\0';
            params[paramIndex].choicesCount = activeOptionSlot + 1;
            return; 
        }
        else if (c == ';') {
            // Semicolon delimiter encountered: seal the current choice string segment
            params[paramIndex].choices[activeOptionSlot].text[charCount] = '\0';
            
            // Advance to the next safe storage choice slot parameter bounds
            if (activeOptionSlot < (CRSF_MAX_PARAMS - 1)) {
                activeOptionSlot++;
            }
            charCount = 0;
        } 
        else {
            if (charCount < (CRSF_MAX_STRING_LEN - 1)) {
                params[paramIndex].choices[activeOptionSlot].text[charCount++] = c;
            }
        }
    }
   
}

int ParamCollection::getParamSlot(uint8_t id) {
    // 1. Search if this parameter ID is already initialized
    for (int i = 0; i < paramCount; i++) {
        if (params[i].id == id) return i;
    }
    // 2. If it's a new ID, allocate a fresh index slot if we have space left
    if (paramCount < CRSF_MAX_PARAMS) {
        int freshSlot = paramCount;
        params[freshSlot].id = id;
        // Initialize all parameter data
        params[freshSlot].parentFolder = 0;
        params[freshSlot].type = static_cast<crsfValueType>(0);
        params[freshSlot].hidden = false;
        params[freshSlot].currentVal = 0;
        params[freshSlot].minVal = 0;
        params[freshSlot].maxVal = 0;
        params[freshSlot].precision = 0;
        params[freshSlot].step = 0;
        params[freshSlot].timeout = 0;
        memset(params[freshSlot].label, 0, sizeof(params[freshSlot].label));
        memset(params[freshSlot].valueString, 0, sizeof(params[freshSlot].valueString));
        params[freshSlot].valueStringCharCount = 0;
        memset(params[freshSlot].units, 0, sizeof(params[freshSlot].units));
        memset(params[freshSlot].choices, 0, sizeof(params[freshSlot].choices));
        params[freshSlot].choicesCount = 0;
        paramCount++;
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
uint8_t ParamCollection::nextInFolder() {
    uint8_t parentFolder = params[selectedParam].parentFolder;

    for (uint8_t i = selectedParam + 1; i < paramCount; i++) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 1; // Moved down, scroll up
        }
    }
    // if we got here it means there were no other entries at this folder level
    // Now go back to the start and find the first item in the folder
    for (uint8_t i = 0; i < selectedParam; i++) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 2;
        }
    }
    // not found (unexpected), dont updated selected param and return 'no move'
    return 0;
}

uint8_t ParamCollection::prevInFolder() {
    uint8_t parentFolder = params[selectedParam].parentFolder;

    for (uint8_t i = selectedParam - 1; i >= 0; i--) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 2; // Moved up, scroll down
        }
    }
    // if we got here it means there were no other entries at this folder level
    // Now search backwards from the end and find the last item in the folder.
    for (uint8_t i = paramCount - 1; i > selectedParam ; i--) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 1;
        }
    }
    // not found (unexpected), dont updated selected param and return 'no move'
    return 0;
}

uint8_t ParamCollection::enterFolder() {
    if (params[selectedParam].type != CRSF_FOLDER) return 0;
    for (uint8_t i = selectedParam + 1; i < paramCount; i++) {  // Assume child items will follow folder
        if (params[i].parentFolder == selectedParam && !params[i].hidden) {
            selectedParam = i;
            return 3; // Scroll left
        }
    }
    // not found (unexpected), dont updated selected param and return 'no move'
    return 0;
}

uint8_t ParamCollection::exitFolder() {
    if (params[selectedParam].parentFolder == 0) {
        if (selectedParam > 0) { // pressing left brings you back to the top of the menu
            selectedParam = 0;
            return 1;
        } else {
            return 0;
        }
    } else {
        selectedParam = params[selectedParam].parentFolder;
        return 4;  // Scroll right
    }
}

const crsfParameter& ParamCollection::getCurrentParam() const {
    return getParam(selectedParam);
}

const crsfParameter& ParamCollection::getParam(const uint8_t index) const {
    if (index >= 0 && index < paramCount) {
        return params[index]; // Returns a reference directly to the array item
    } else {
        static crsfParameter notFound;
        notFound.id = 0;
        notFound.parentFolder = 0;
        notFound.type = CRSF_INFO;
        strcpy(notFound.label, "Loading...");
        return notFound;
    }
}

int ParamCollection::findParamByLabel(const char searchString[]) const {
    for (uint8_t i = 1; i < paramCount; i++) {
        if (strncmp(params[i].label, searchString, strlen(searchString)) == 0 ) {
            return i;
        }
    }
    return -1;
}

// Edit Param functions
void ParamCollection::editParamPrev() { 
    switch(params[selectedParam].type) {
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
            if (editValue < params[selectedParam].minVal) editValue = params[selectedParam].minVal;
            break;
        case CRSF_FLOAT:
            editValue -= params[selectedParam].step;
            if (editValue < params[selectedParam].minVal) editValue = params[selectedParam].minVal;
            break;
        case CRSF_STRING:
            // Not sure on this one... Not needed for now
            break;
        default:
            // Do nothing
            break;   
    }
}

void ParamCollection::editParamNext() {
    switch(params[selectedParam].type) {
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
            if (editValue > params[selectedParam].maxVal) editValue = params[selectedParam].maxVal;
            break;
        case CRSF_FLOAT:
            editValue += params[selectedParam].step;
            if (editValue > params[selectedParam].maxVal) editValue = params[selectedParam].maxVal;
            break;
        case CRSF_STRING:
            // Not sure on this one... Not needed for now
            break;
        default:
            // Do nothing
            break;   
    }
}

void ParamCollection::editParam() {
    editValue = params[selectedParam].currentVal;
}
