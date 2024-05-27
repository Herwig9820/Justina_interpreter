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

/*
    Example code demonstrating how to write user c++ functions for Justina
    ----------------------------------------------------------------------
    Built-in Justina functionality can be extended by writing specific functions in c++ ('user cpp functions). Such functions
    may include time-critical user routines, functions targeting specific hardware, functions extending functionality in a
    specific domain, etc. These functions must then be 'registered' with Justina and given a 'Justina function name' (an alias).
    From then onward, these C++ functions can be called just like any other Justina function, with the same syntax,
    using the alias as function name and passing scalar or array variables as arguments.

    This sketch demonstrates how to write user c++ functions and shows how to call these user c++ functions from Justina
    (from the Justina command line or from a Justina program).

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/


// create Justina_interpreter object with default values: IO via Serial only, SD card allowed, default SD card CS pin
Justina justina;


// ------------------------------------------------------
// STEP 1. Write user c++ functions
// MORE INFORMATION: see USER MANUAL, available on GitHub
// ------------------------------------------------------


// ------------------------------------------------------------------------------------
// *   example: a function not using any of its arguments and not returning a value   *
// ------------------------------------------------------------------------------------

void doSomething(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered as in this example):
    ------------------------------------------------------------
    cpp_doSomething();                                                      // although this function returns nothing, a zero will be returned to Justina

*/

    Serial.println("Justina was here");
}


// -------------------------------------------------------------------------------------------------------------
// *   example: user function to add all floats together and return the total, SKIPPING integers and strings   *
// --------------------------------------------------------------------------------------------------------------

float addAllFloats(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered as in this example):
    ------------------------------------------------------------
    cpp_addAllFloats(1.3, 2.5, 999, "hello");                               // result returned: 3.8 (only floats are added)

*/

    // all floating point arguments will be added up; other values will be discarded 
    float total = 0.;
    for (int i = 0; i < argCount; i++) {
        if ((valueType[i] & Justina::value_typeMask) == Justina::value_isFloat) { total += *(float*)pdata[i]; }
    }
    return total;
}


// ---------------------------------------------------------------------------------------------------
// *   example: for all Justina arguments provided, check argument type and change argument value    *
// *   this function doesn't return anything, but changes the values of the operands                 *
// ---------------------------------------------------------------------------------------------------

void changeAllArgValues(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call (if function is registered as in this example):
    ------------------------------------------------------------
    var longVar = 123, floatVar = 123., stringVar = "123";                  // create variables
    cpp_changeAllArgValues(longVar, floatVar, stringVar);                   // although this function returns nothing, a zero will be returned to Justina
*/

    for (int i = 0; i < argCount; i++) {                                    // for each argument...
         // get value type and variable / constant info
        bool isLong = ((valueType[i] & Justina::value_typeMask) == Justina::value_isLong);
        bool isFloat = ((valueType[i] & Justina::value_typeMask) == Justina::value_isFloat);
        bool isString = ((valueType[i] & Justina::value_typeMask) == Justina::value_isString);
        bool isVariable = (valueType[i] & 0x80);                // (not used in this example)

        // get values or pointers to values
        if (isLong) {
            long arg = *(long*)pdata[i];                                    // long argument 
            long* pArg = (long*)pdata[i];                                   // retrieve pointer, and add 1 to the argument (this will only have effect if the argument is a Justina variable)
            (*pArg)++;
        }
        else if (isFloat) {
            float arg = *(float*)pdata[i];                                  // floating point argument 
            float* pArg = (float*)pdata[i];                                 // retrieve pointer, multiply the argument by 10. (this will only have effect if the argument is a Justina variable)
            (*pArg) *= 10.;
        }
        // ...
        else if (isString) {
            char char1 = *(char*)pdata[i];                                  // character array argument, first character ('\0' if empty string)
            char* pChar = (char*)pdata[i];                                  // pointer to character array (char*)              
            if (strlen(pChar) >= 3) { strcpy(pChar, "abc"); }               // if character array has sufficient length, change first three characters
        }
    }
}


// -------------------------------------------------------------------------------
// *   example: user function returning a char* OR a returning a Justina error   *
// --------------------------------------------------------------------------------

char* returnFirstArg(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call (if function is registered as in this example):
    ------------------------------------------------------------
    cpp_returnFirstArg("Christian");                                        // result returned: "Christian"
*/

    // no need to test argument count (fixed to '1' - see STEP 2., below)  
    if ((valueType[0] & Justina::value_typeMask) != Justina::value_isString) {   // argument is not a string ?
        execError = 3103;                                                   // will raise a Justina error (see list of error codes in user documentation)
        return "";                                                          // because of the error, the function returned will be lost (even with error trapping)
    }

    /*
    Do NOT return a char* pointing to a local char array, unless it's declared as static (local variables
    exist on the stack until you leave the procedure, and the pointer returned may point to garbage).
    If you return a char* object created on the heap (NEW), make sure to save the pointer value
    (e.g., in a static variable) because you will have to DELETE the object later(also from a user c++ procedure)
    You can return a string literal, because string literals are stored in static memory
    */

    // static char name[10] = "John";  return name;     // OK
    // char* name = (char*)(pdata[0]); return name;     // OK
    return "Peter";                                     // OK     
}


//---------------------------------------------------------
// STEP 2. Define records with user c++ function attributes 
// MORE INFORMATION: see USER MANUAL, available on GitHub
// --------------------------------------------------------

// arrays with a function attributes record (alias, function, min. and max. arguments allowed) for each user c++ function defined above
// ------------------------------------------------------------------------------------------------------------------------------------
Justina::CppFloatFunction const cppFloatFunctions[]{                        // user c++ functions returning a floating point number (float) 
    {"cpp_addAllFloats", addAllFloats, 0, 8}
};

Justina::Cpp_pCharFunction const cpp_pCharFunctions[]{                      // user c++ functions returning a char*
    {"cpp_returnFirstArg", returnFirstArg, 1, 1 }
};

Justina::CppVoidFunction  const cppVoidFunctions[]{                         // user c++ functions returning nothing
    {"cpp_doSomething", doSomething, 0, 0},
    {"cpp_changeAllArgValues", changeAllArgValues, 0, 8},
};

// no user c++ functions defined with these return types: 
// OK to comment out or remove next lines 
Justina::CppBoolFunction const cppBoolFunctions[]{ };                       // user c++ functions returning a boolean value (bool) 
Justina::CppCharFunction const cppCharFunctions[]{ };                       // user c++ functions returning a character (char) 
Justina::CppIntFunction const cppIntFunctions[]{ };                         // user c++ functions returning an integer (int) 
Justina::CppLongFunction const cppLongFunctions[]{ };                       // user c++ functions returning a long integer (long) 


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
    justina.registerFloatUserCppFunctions(cppFloatFunctions, 1);            // user c++ functions returning a floating point number (float)
    justina.register_pCharUserCppFunctions(cpp_pCharFunctions, 1);          // user c++ functions returning a char* 
    justina.registerVoidUserCppFunctions(cppVoidFunctions, 2);              // user c++ functions returning nothing (void)

    // no user c++ functions defined with these return types: OK to                      
    // pass addresses with COUNT 0 -OR- to comment out or remove next lines              
    justina.registerBoolUserCppFunctions(cppBoolFunctions, 0);              // user c++ functions returning a boolean value (bool)
    justina.registerCharUserCppFunctions(cppCharFunctions, 0);              // user c++ functions returning a character (char)
    justina.registerIntUserCppFunctions(cppIntFunctions, 0);                // user c++ functions returning an integer (int)
    justina.registerLongUserCppFunctions(cppLongFunctions, 0);              // user c++ functions returning a long integer (long)

    // run interpreter (control will stay there until you quit Justina)
    justina.begin();
}

// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
