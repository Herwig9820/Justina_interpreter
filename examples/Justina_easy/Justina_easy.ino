/************************************************************************************************************
*    Sample program, demonstrating how to launch the Justina interpreter.                                   *
*                                                                                                           *
*    Copyright 2024, Herwig Taveirne                                                                        *
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

#include "Justina.h"

// define between 1 and 4 alternative input streams and alternative output streams. 
// the first stream will function as the default Justina console.
constexpr int terminalCount{ 1 };       // number of streams defined
Stream* pAltInput[1]{ &Serial };        // Justina input streams                                                  
Print* pAltOutput[1]{ &Serial };        // Justina output streams                                                     


// define the size of program memory in bytes, which depends on the available RAM 
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32)
long progMemSize = 1 << 16;             // for ESP32 and RP2040, the maximum size is 64 kByte
#else
long progMemSize = 2000;
#endif

// create Justina_interpreter object
// the last argument (0) indicates (among others) that an SD card is not present 
Justina_interpreter justina(pAltInput, pAltOutput, terminalCount, progMemSize, 0);


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);

    justina.begin();                          // run interpreter (control will stay there until you quit Justina)
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
