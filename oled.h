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
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "elrsLua.h"

 //----- OLED Screen setup  -------------------
// Declare the display as 128x64 workspace to match the driver initialization profile
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Viewport layout boundary definitions matching the 72x40 screen placement
#define DISPLAY_WIDTH  72
#define DISPLAY_HEIGHT 40

enum displayState {MAIN_PAGE, ELRS_STATS_PAGE};
displayState currentScreen = MAIN_PAGE;


void initDisplay();
void updateHomeScreen(float voltage, uint8_t uplinkLQ, int16_t channels[]);
void updateElrsStatsScreen(const ELRSLua& luaInstance);
void nextScreen();  // Cycle screens