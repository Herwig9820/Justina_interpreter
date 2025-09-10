/************************************************************************************************************
*    Justina interpreter library                                                                            *
*                                                                                                           *
*    Copyright 2024, 2025 Herwig Taveirne                                                                        *
*                                                                                                           *
*    This file is part of the Justina Interpreter library.                                                  *
*    The Justina interpreter library is free software: you can redistribute it and/or modify it under       *
*    the terms of the GNU General Public License as published by the Free Software Foundation, either       *
*    version 3 of the License, or (at your option) any later version.                                       *
*                                                                                                           *
*    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;              *
*    without even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.             *
*    See the GNU General Public License for more details.                                                   *
*                                                                                                           *
*    You should have received a copy of the GNU General Public License along with this program. If not,     *
*    see <https://www.gnu.org/licenses/>.                                                                   *
*                                                                                                           *
*    The library is intended to work with 32 bit boards using the SAMD architecture ,                       *
*    the Arduino nano RP2040 and Arduino nano ESP32 boards.                                                 *
*                                                                                                           *
*    See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                           *
************************************************************************************************************/

#ifndef _JUSTINA_CONSTANTS_h
#define _JUSTINA_CONSTANTS_h

/*
By default, Justina sets the size of specific memory areas taking into account the available RAM of the Arduino board used.
- On ESP32 and RP2040 boards, program memory size is set to 65536 bytes (which is the absolute maximum). 
  The maximum number of user variables, program variable NAMES (distinct global, local and static variables may share the same name), static variables...
  ...and functions defined in a Justina program is set to 255 (which is the absolute maximum).
- On SAMD boards, which have less RAM memory, program memory size is set to 4000. Maximum number of user variables is set to 64, ...
  ...program variable NAMES: 64, static variables: 32, user functions: 32.

Depending on your specific requirements, these sizes can be increased or decreased. For instance, if you use quite big arrays, consuming a lot of memory,...
...it could be useful to decrease the program memory size.
 

To change allocated memory sizes WITHOUT CHANGING ANY OF THE FILES IN THE JUSTINA LIBRARY:
- in the Arduino IDE, look  up the sketchbook location (File -> Preferences -> Settings)
- within the sketchbook folder, locate the folder 'libraries' (this is the folder containing the Justina library and probably many others)
- within folder 'libraries', create a folder named "Justina_constants" 
- store this "Justina_constants.h" file in folder "Justina_constants"
- adapt the values in the #define directives
*/

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_NRF52840)
#define PROGMEM_SIZE 30000	 	// program memory size, in bytes. Maximum: 2^16 = 65536. Minimum is 2000 (of which 500 are reserved for immediate mode (user) commands)
#else
#define PROGMEM_SIZE 5000		// some microcontrollers have limited memory. Test carefully if you increase this limit. Also, don't go lower than 2000 	 	
#endif

#define MAXVAR_USER 100         // max. distinct user variables allowed. Absolute limit: 255
#define MAXVAR_PROG 100         // max. program variable NAMES allowed (distinct global, static, local/parameter variables may share the same name). Absolute limit: 255
#define MAXVAR_STAT 100         // max. distinct static variables allowed. Absolute limit: 255
#define MAXFUNC 50              // max. Justina functions allowed. Absolute limit: 255

#endif