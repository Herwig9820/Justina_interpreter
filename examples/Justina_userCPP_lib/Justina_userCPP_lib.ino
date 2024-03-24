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

#include "Justina.h"
#include "Justina_userCPPlibrary.h"

/*
    This sketch demonstrates how to write user c++ functions that can be called directly from the Justina interpreter.
    The user c++ functions are contained in a .cpp file, that can be considered a Justina user c++ 'library' file.  
    
    Built-in Justina functionality can be extended by writing specific functions in c++. Such functions may include 
    time-critical user routines, functions targeting specific hardware, functions extending functionality in a specific 
    domain, etc. These functions must then be 'registered' with Justina and given a 'Justina function name' (an alias).
    From then onward, these C++ functions can be called just like any other Justina function, with the same syntax, 
    using the alias as function name and passing scalar or array variables as arguments.

    A detailed description of the 3 steps to follow is contained in the USER MANUAL.
*/


// define between 1 and 4 alternative input streams and alternative output streams. 
// the first stream will function as the default Justina console.
constexpr int terminalCount{ 1 };                   // number of streams defined
Stream* pExtInput[terminalCount]{ &Serial };        // Justina input streams                                                  
Print* pExtOutput[terminalCount]{ &Serial };        // Justina output streams                                                     


// create Justina object
Justina justina(pExtInput, pExtOutput, terminalCount);


// ------------------------------------------------------------------------
// STEP 1. Forward declaration of user c++ functions defined in a .cpp file
// DETAILED INFORMATION: see USER MANUAL, available on GitHub
// ------------------------------------------------------------------------

bool JustinaComplex::cmplxAdd(void** const pdata, const char* const valueType, const int argCount, int& execError);
bool JustinaComplex::cmplxCtoP(void** const pdata, const char* const valueType, const int argCount, int& execError);


//-----------------------------------------------------------
// STEP 2. Define records with function attributes 
// DETAILED INFORMATION: see USER MANUAL, available on GitHub
// ----------------------------------------------------------

// arrays with a function attributes record (alias, function, min. and max. arguments allowed) for each user c++ function defined above
// ------------------------------------------------------------------------------------------------------------------------------------
Justina::CppBoolFunction const cppBoolFunctions[]{                          // user functions returning boolean value (bool)
    {"cmplxAdd", JustinaComplex::cmplxAdd, 3, 3},                           // - complex number addition                   
    {"cmplxCtoP", JustinaComplex::cmplxCtoP, 2, 2 }                         // - Cartesian to polar coordinates                   
};
// no user c++ functions defined with these return types: 
// OK to comment out or remove next lines 
Justina::CppFloatFunction const cppFloatFunctions[]{};                      // user c++ functions returning a floating point number (float)
Justina::Cpp_pCharFunction const cpp_pCharFunctions[]{};                    // user c++ functions returning a char* 
Justina::CppCharFunction const cppCharFunctions[]{ };                       // user c++ functions returning a character (char)
Justina::CppIntFunction const cppIntFunctions[]{ };                         // user c++ functions returning an integer (int)
Justina::CppLongFunction const cppLongFunctions[]{ };                       // user c++ functions returning a long integer (long)
Justina::CppVoidFunction  const cppVoidFunctions[]{};                       // user c++ functions returning nothing (void)


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);

    // ----------------------------------------------------------------
    // STEP 3. Register user functions: BEFORE starting the interpreter 
    // DETAILED INFORMATION: see USER MANUAL, available on GitHub
    // ----------------------------------------------------------------

    // for each return type: pass address of arrays containing user function attributes AND user function count
    justina.registerBoolUserCppFunctions(cppBoolFunctions, 2);              // user functions returning boolean value (bool)

    // no user c++ functions defined with these return types: OK to pass addresses with COUNT 0 -OR- to comment out or remove next lines 
    justina.registerFloatUserCppFunctions(cppFloatFunctions, 0);            // user c++ functions returning a floating point number (float)
    justina.register_pCharUserCppFunctions(cpp_pCharFunctions, 0);          // user c++ functions returning a char* 
    justina.registerCharUserCppFunctions(cppCharFunctions, 0);              // user c++ functions returning a character (char)
    justina.registerIntUserCppFunctions(cppIntFunctions, 0);                // user c++ functions returning an integer (int)
    justina.registerLongUserCppFunctions(cppLongFunctions, 0);              // user c++ functions returning a long integer (long)
    justina.registerVoidUserCppFunctions(cppVoidFunctions, 0);              // user c++ functions returning nothing (void)
                                                                                     
    // run interpreter (control will stay there until you quit Justina)
    justina.begin();
}

// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
