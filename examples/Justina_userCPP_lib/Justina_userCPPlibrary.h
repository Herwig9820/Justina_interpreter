/***********************************************************************************************************
*   Sample Arduino program, demonstrating how to write user c++ functions for use in a Justina program.    *
*   User c++ functions may provide extra functionality to Justina or they may execute specific hardware    *
*   related tasks such as setting or reading a timer value, enabling a specific interrupt etc.             *
*                                                                                                          *
*   Copyright 2024, Herwig Taveirne                                                                        *
*                                                                                                          *
*   This file is part of the Justina Interpreter library.                                                  *
*   The Justina interpreter library is free software: you can redistribute it and/or modify it under       *
*   the terms of the GNU General Public License as published by the Free Software Foundation, either       *
*   version 3 of the License, or (at your option) any later version.                                       *
*                                                                                                          *
*   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;              *
*   without even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.             *
*   See the GNU General Public License for more details.                                                   *
*                                                                                                          *
*   You should have received a copy of the GNU General Public License along with this program. If not,     *
*   see <https://www.gnu.org/licenses/>.                                                                   *
*                                                                                                          *
*   The library is intended to work with 32 bit boards using the SAMD architecture ,                       *
*   the Arduino nano RP2040 and Arduino nano ESP32 boards.                                                 *
*                                                                                                          *
*   See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                          *
***********************************************************************************************************/

#ifndef _JUSTINA_USERCPP_h
#define _JUSTINA_USERCPP_h

#include "Arduino.h"
#include "Justina.h"

// define a name space for the Justina user c++ function 'library' functions

namespace JustinaComplex {
    bool cmplxAdd(void** const pdata, const char* const valueType, const int argCount, int& execError);
    bool cmplxCtoP(void** const pdata, const char* const valueType, const int argCount, int& execError);
}
#endif
