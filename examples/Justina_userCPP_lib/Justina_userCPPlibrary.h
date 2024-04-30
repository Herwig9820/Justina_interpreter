/*************************************************************************************************************************
*   Example code demonstrating how to write a user c++ function library for use by the Justina interpreter               *
*                                                                                                                        *
*   The Justina interpreter library is licensed under the terms of the GNU General Public License v3.0 as published      *
*   by the Free Software Foundation (https://www.gnu.org/licenses).                                                      *
*   Refer to GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter            *
*                                                                                                                        *
*   This example code is in the public domain                                                                            *
*                                                                                                                        *
*   2024, Herwig Taveirne                                                                                                *
*************************************************************************************************************************/

#ifndef _JUSTINA_USERCPP_h
#define _JUSTINA_USERCPP_h

#include "Arduino.h"
#include "Justina.h"

// define a name space for the Justina user c++ function 'library' functions

namespace JustinaComplex {
    // prototype for c++ function 'library' functions
    bool cmplxAdd(void** const pdata, const char* const valueType, const int argCount, int& execError);
    bool cmplxCtoP(void** const pdata, const char* const valueType, const int argCount, int& execError);
}
#endif
