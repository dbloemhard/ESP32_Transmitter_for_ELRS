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
 
#ifndef PARAMS_H
#define PARAMS_H

#include <Arduino.h>

// Parameter definition based on CRSF standards
#define CRSF_MAX_PARAMS                 32
#define CRSF_MAX_STRING_LEN             32
#define CRSF_MAX_PARAM_DATA_LEN         256

// ELRS (CRSF) Command steps
#define ELRS_COMMAND_IDLE               0x00
#define ELRS_COMMAND_CLICK              0x01
#define ELRS_COMMAND_EXECUTING          0x02
#define ELRS_COMMAND_ASKCONFIRM         0x03
#define ELRS_COMMAND_CONFIRMED          0x04
#define ELRS_COMMAND_CANCEL             0x05
#define ELRS_COMMAND_QUERY              0x06

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
    crsfValueType type;               // 0x09 = Select, 0x0B = Folder, etc.
    bool hidden;                      // extracted from the type parameter
    int32_t currentVal;               // Currently active option index 
    int32_t minVal;                   // Used for numeric parameters
    int32_t maxVal;                   // Numeric parameters and String type max length
    int32_t precision;                // Only for 0x08 float type
    uint8_t step;                     // Adjustment step / Command state step
    uint32_t timeout;                  // Command timeout
   
    char label[CRSF_MAX_STRING_LEN];  
    char valueString[CRSF_MAX_PARAM_DATA_LEN];
    char units[CRSF_MAX_STRING_LEN];
    crsfOption choices[CRSF_MAX_PARAMS]; // Extracted selectable strings (e.g., {"Off", "On"}) 
    uint8_t choicesCount;                // Number of options successfully parsed
};


class ParamCollection {
public:
    // Properties
    uint8_t paramCount = 0;
    uint8_t selectedParam = 0;
    int32_t editValue = 0;
    bool popupActive() const { return (params[selectedParam].type == CRSF_COMMAND && params[selectedParam].step > ELRS_COMMAND_IDLE); }

    // Methods
    // Overload the [] operator for reading and writing data
    crsfParameter& operator[](int index) {
        return params[index]; 
    }
    // Overload for read-only access on constant Inventory objects
    const crsfParameter& operator[](int index) const {
        return params[index];
    }

    int findParamByLabel(const char searchString[]) const; // Returns the first parameter that matches the search string, -1 if not found
    uint8_t nextInFolder();  // Navigate to the next param in the same folder, returning the direction it navigated
    uint8_t prevInFolder();  // Navigate to the prev param in the same folder, returning the direction it navigated
    uint8_t enterFolder();   // Navigate into a folder item (if possible), returning the direction it navigated
    uint8_t exitFolder();    // Navigate back out of a folder (if possible), returning the direction it navigated
    uint8_t editParamDec();  // Increments the editValue of the currently selected param (returns true if editValue changed)
    uint8_t editParamInc();  // Decrements the editValue of the currently selected param (returns true if editValue changed)
    void    editParam();     // Copies the current param value into editValue; for modification before saving
    const crsfParameter& getCurrentParam() const;  // Get the currently selected param
    const crsfParameter& getParam(const uint8_t index) const; // Safe method to retrieve a specific param

    int getParamSlot(uint8_t id);   // Allocate a new slot for an additional parameter, if possible
    void clearParams();

private:
	crsfParameter params[CRSF_MAX_PARAMS];
};
#endif
