/************************************************************************************************************
*    Justina interpreter library for Arduino boards with 32 bit SAMD microconrollers                        *
*        Version:    v1.01 - 12/07/2023                                                                     *
*        Author:     Herwig Taveirne, 2021-2023                                                             *
*                                                                                                           *
*    Tested with Nano 33 IoT and Arduino RP2040                                                             *
*                                                                                                           *
*                                                                                                           *
*    Justina is an interpreter which does NOT require you to use an IDE to write and compile programs.      *
*    Programs are written on the PC using any text processor and transferred to the Arduino using any       *
*    Serial or TCP Terminal program capable of sending files.                                               *
*    Justina can store and retrieve programs and other data on an SD card as well.                          *
*                                                                                                           *
*    See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                           *
*    This program is free software: you can redistribute it and/or modify                                   *
*    it under the terms of the GNU General Public License as published by                                   *
*    the Free Software Foundation, either version 3 of the License, or                                      *
*    (at your option) any later version.                                                                    *
*                                                                                                           *
*    This program is distributed in the hope that it will be useful,                                        *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of                                         *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                           *
*    GNU General Public License for more details.                                                           *
*                                                                                                           *
*    You should have received a copy of the GNU General Public License                                      *
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.                                  *
************************************************************************************************************/

/************************************************************************************************************
***                       Example program: calling the Justina Interpreter                                ***
************************************************************************************************************/

// In this example, we won't bother about system and call back routines, SD memory cards, multiple streams etc.
// Only purpose is to call the interpreter and verify that it works.

#include <ESP_AT_Lib.h>
#include "Justina.h"

Stream* pExternal_IO[1]{ &Serial };         // external streams, available for Justina
constexpr int terminalCount{ 1 };           // only one

// progMemSize defines the size of Justina program memory in bytes,...
// ...which depends on the available RAM 
#if defined (ARDUINO_ARCH_RP2040)
long progMemSize = pow(2, 16);
#else
long progMemSize = 2000;
#endif 

// create Justina_interpreter object. The last argument informs Justina that an SD card
// reader is not present, among others  
Justina_interpreter justina(pExternal_IO, terminalCount, progMemSize, 0);

// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(1000000);
    delay(5000);
    Serial.println(sizeof(justina));

    justina.run();                          // run interpreter
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}