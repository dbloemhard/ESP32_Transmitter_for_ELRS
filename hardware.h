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
 
 // Hardware setup (Pins and UART selection)

// Port setup
// #define ELRS_PORT Serial1 - this is hard coded
// Comment out the next line to use full duplex (TX and RX wires)
//#define ELRS_HALF_DUPLEX
#define ELRS_TX   21
#define ELRS_RX   20

// pins that used for the Joystick
const uint8_t ANALOG_PIN_AILERON  = 0;
const uint8_t ANALOG_PIN_ELEVATOR = 1;
const uint8_t ANALOG_PIN_THROTTLE = 3;
const uint8_t ANALOG_PIN_RUDDER   = 4;
const uint8_t ANALOG_PIN_VOLTAGE  = 2;

// pins that used for the switch
const uint8_t DIGITAL_PIN_SWITCH_ARM =  7;  // Arm switch
const uint8_t DIGITAL_PIN_SWITCH_AUX2 = 10; //
const uint8_t DIGITAL_PIN_BUTTON = 9;
// pins that used for output
const uint8_t DIGITAL_PIN_LED =    8;  
const bool PIN_LED_LOW_ILLUM  =    true;

// pins used for OLED
const uint8_t I2C_SCL_OLED = 6;
const uint8_t I2C_SDA_OLED = 5;