/*************************************************************************************************************************
*   Example Arduino sketch demonstrating Justina interpreter functionality												 *
*                                                                                                                        *
*   The Justina interpreter library is licensed under the terms of the GNU General Public License v3.0 as published      *
*   by the Free Software Foundation (https://www.gnu.org/licenses).                                                      *
*   Refer to GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter            *
*                                                                                                                        *
*   This example code is in the public domain                                                                            *
*                                                                                                                        *
*   2024, Herwig Taveirne                                                                                                *
*************************************************************************************************************************/

#include "Justina.h"
#include "Justina_userCPPlibrary.h"

/*
    Example code demonstrating how to write a user c++ function library for use by the Justina interpreter
	------------------------------------------------------------------------------------------------------
	Built-in Justina functionality can be extended by writing specific functions in c++. Such functions may include 
    time-critical user routines, functions targeting specific hardware, functions extending functionality in a specific 
    domain, etc. These functions must then be 'registered' with Justina and given a 'Justina function name' (an alias).
    From then onward, these C++ functions can be called just like any other Justina function, with the same syntax, 
    using the alias as function name and passing scalar or array variables as arguments.

    This sketch demonstrates how to create a Justina user c++ 'library' file. The c++ functions it contains can then  
    be called from Justina (from the Justina command line or from a Justina program).
	It also shows how to pass arrays (by reference) to a user c++ function.

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
	*/


// create Justina_interpreter object with default values: IO via Serial only, SD card allowed, default SD card CS pin
Justina justina;


// ------------------------------------------------------------------------
// STEP 1. Forward declaration of user c++ functions defined in a .cpp file
// MORE INFORMATION: see USER MANUAL, available on GitHub
// ------------------------------------------------------------------------

bool JustinaComplex::cmplxAdd(void** const pdata, const char* const valueType, const int argCount, int& execError);
bool JustinaComplex::cmplxCtoP(void** const pdata, const char* const valueType, const int argCount, int& execError);


//-------------------------------------------------------
// STEP 2. Define records with function attributes 
// MORE INFORMATION: see USER MANUAL, available on GitHub
// ------------------------------------------------------

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
    // MORE INFORMATION: see USER MANUAL, available on GitHub
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
