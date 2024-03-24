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

/*
    This sketch demonstrates how to write user c++ functions that can be called directly from the Justina interpreter.

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


// ----------------------------------------------------------
// STEP 1. Write user c++ functions
// DETAILED INFORMATION: see USER MANUAL, available on GitHub
// ----------------------------------------------------------


// ------------------------------------------------------------------------------------
// *   example: a function not using any of its arguments and not returning a value   *
// ------------------------------------------------------------------------------------

void doSomething(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered with same Justina name)
    ------------
    doSomething();       // although this function returns nothing, a zero will be returned to Justina

*/

    pExtOutput[0]->println("Justina was here");                 // pExtOutput[0] is the default console in Justina
}


// -------------------------------------------------------------------------------------------------------------
// *   example: user function to add all floats together and return the total, SKIPPING integers and strings   *
// --------------------------------------------------------------------------------------------------------------
float addAllFloats(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered with same Justina name)
    ------------
    addAllFloats(1.3, 2.5, 999, "hello"); -> result returned: 3.8 (only floats are added)

*/

    // all floating point arguments will be added up; other values will be discarded 
    float total = 0.;
    for (int i = 0; i < argCount; i++) {
        if ((valueType[i] & justina.value_typeMask) == justina.value_isFloat) { total += *(float*)pdata[i]; }
    }
    return total;
}


// ---------------------------------------------------------------------------------------------------
// *   example: for all Justina arguments provided, check argument type and change argument value    *
// *   this function doesn't return anything, but changes the values of the operands                 *
// ---------------------------------------------------------------------------------------------------

void changeAllArgValues(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call (if function is registered with same Justina name)
    ------------
    var longVar = 123, floatVar = 123., stringVar = "123";
    changeAllArgValues(longVar, floatVar, stringVar);       // although this function returns nothing, a zero will be returned to Justina
*/

    for (int i = 0; i < argCount; i++) {                        // for each argument...
         // get value type and variable / constant info
        bool isLong = ((valueType[i] & justina.value_typeMask) == justina.value_isLong);
        bool isFloat = ((valueType[i] & justina.value_typeMask) == justina.value_isFloat);
        bool isString = ((valueType[i] & justina.value_typeMask) == justina.value_isStringPointer);
        bool isVariable = (valueType[i] & 0x80);                // (not used in this example)

        // get values or pointers to values
        if (isLong) {
            long arg = *(long*)pdata[i];                        // long argument 
            long* pArg = (long*)pdata[i];                       // retrieve pointer, and add 1 to the argument (this will only have effect if the argument is a Justina variable)
            (*pArg)++;
        }
        else if (isFloat) {
            float arg = *(float*)pdata[i];                      // floating point argument 
            float* pArg = (float*)pdata[i];                     // retrieve pointer, multiply the argument by 10. (this will only have effect if the argument is a Justina variable)
            (*pArg) *= 10.;
        }
        // ...
        else if (isString) {
            char char1 = *(char*)pdata[i];                      // character array argument, first character ('\0' if empty string)
            char* pChar = (char*)pdata[i];                      // pointer to character array (char*)              
            if (strlen(pChar) >= 3) { strcpy(pChar, "abc"); }   // if character array has sufficient length, change first three characters
        }
    }
}


// -------------------------------------------------------------------------------
// *   example: user function returning a char* OR a returning a Justina error   *
// --------------------------------------------------------------------------------

char* returnFirstArg(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call (if function is registered with same Justina name)
    ------------
    returnFirstArg("Christian");    // result returned: "John"
*/

    // no need to test argument count (fixed to '1' - see STEP 2., below)  
    if ((valueType[0] & justina.value_typeMask) != justina.value_isStringPointer) {     // argument is not a string ?
        execError = 3103;                                                               // will raise a Justina error (see list of error codes in user documentation)
        return "";                                                                      // because of the error, the function returned will be lost (even with error trapping)
    }

    /*
    Do NOT return a char* pointing to a local char array, unless it's declared as static (local variables
    exist on the stack until you leave the procedure, and the pointer returned may point to garbage).
    If you return a char* object created on the heap (NEW), make sure to save the pointer value
    (e.g., in a static variable) because you will have to DELETE the object later(also from a user c++ procedure)
    You can return a string literal, because string literals are stored in static memory

        static char name[10] = "John";  return name;    // OK
        char* name = (char*)(pdata[0]); return name;    // OK
    */

    return "Peter";                                     // OK     
}


//-----------------------------------------------------------
// STEP 2. Define records with function attributes 
// DETAILED INFORMATION: see USER MANUAL, available on GitHub
// ----------------------------------------------------------

// arrays with a function attributes record (alias, function, min. and max. arguments allowed) for each user c++ function defined above
// ------------------------------------------------------------------------------------------------------------------------------------
Justina::CppFloatFunction const cppFloatFunctions[]{                            // user c++ functions returning a floating point number (float) 
    {"addAllFloats", addAllFloats, 0, 8}
};

Justina::Cpp_pCharFunction const cpp_pCharFunctions[]{                          // user c++ functions returning a char*
    { "returnFirstArg", returnFirstArg, 1, 1 }
};

Justina::CppVoidFunction  const cppVoidFunctions[]{                             // user c++ functions returning nothing
    {"doSomething", doSomething, 0, 0},
    {"changeAllArgValues", changeAllArgValues, 0, 8},
};

// no user c++ functions defined with these return types: 
// OK to comment out or remove next lines 
Justina::CppBoolFunction const cppBoolFunctions[]{ };                           // user c++ functions returning a boolean value (bool) 
Justina::CppCharFunction const cppCharFunctions[]{ };                           // user c++ functions returning a character (char) 
Justina::CppIntFunction const cppIntFunctions[]{ };                             // user c++ functions returning an integer (int) 
Justina::CppLongFunction const cppLongFunctions[]{ };                           // user c++ functions returning a long integer (long) 


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
    justina.registerFloatUserCppFunctions(cppFloatFunctions, 1);                // user c++ functions returning a floating point number (float)
    justina.register_pCharUserCppFunctions(cpp_pCharFunctions, 1);              // user c++ functions returning a char* 
    justina.registerVoidUserCppFunctions(cppVoidFunctions, 2);                  // user c++ functions returning nothing (void)

    // no user c++ functions defined with these return types: OK to                      
    // pass addresses with COUNT 0 -OR- to comment out or remove next lines              
    justina.registerBoolUserCppFunctions(cppBoolFunctions, 0);                  // user c++ functions returning a boolean value (bool)
    justina.registerCharUserCppFunctions(cppCharFunctions, 0);                  // user c++ functions returning a character (char)
    justina.registerIntUserCppFunctions(cppIntFunctions, 0);                    // user c++ functions returning an integer (int)
    justina.registerLongUserCppFunctions(cppLongFunctions, 0);                  // user c++ functions returning a long integer (long)

    // run interpreter (control will stay there until you quit Justina)
    justina.begin();
}

// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
