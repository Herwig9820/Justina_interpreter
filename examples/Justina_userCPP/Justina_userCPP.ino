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
    Example code demonstrating how to write user C++ functions and user C++ commands for Justina
    --------------------------------------------------------------------------------------------
    Built-in Justina functionality can be extended by writing specific C++ functions. In Justina, these C++ functions will become available as
    'external' Justina functions or commands.
    User-written C++ functions may include time-critical user routines, functions targeting specific hardware, functions extending functionality in
    a specific domain, etc. These C++ functions must be 'registered' with Justina and given a Justina function name or command name (an alias).
    From then onward, these external Justina functions and commands can be called just like any other Justina function or command, with the same syntax,
    using the alias as function name and passing scalar or array variables as arguments (external commands: argument list not enclosed in parentheses).

    This sketch demonstrates how to write user C++ functions and register these functions with Justina, either as function or command.

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/


// create Justina_interpreter object with default values: IO via Serial only, SD card allowed, default SD card CS pin
Justina justina;


// ------------------------------------------------------
// STEP 1. Write C++ functions
// MORE INFORMATION: see USER MANUAL, available on GitHub
// ------------------------------------------------------


// -------------------------------------------------------------------------------------------------------------
// *   example: C++ function implementing an external Justina COMMAND, changing the values of the arguments    *
// -------------------------------------------------------------------------------------------------------------

void changeAllArgValues(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call (if function is registered as a command, as in this example):
    --------------------------------------------------------------------------
    var longVar = 123, floatVar = 123., stringVar = "123";                  // create variables
    usrc_changeAllArgValues longVar, floatVar, stringVar;                    // command: arguments NOT enclosed in parentheses
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


// ----------------------------------------------------------------------------------
// *   example: C++ function implementing an external Justina function.             *
// *   the function does not use any of its arguments and will not return a value   *
// ----------------------------------------------------------------------------------

void doSomething(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered as in this example):
    ------------------------------------------------------------
    usrf_doSomething();                                                     // although this function returns nothing, a zero will be returned to Justina
*/

    Serial.println("Justina was here");
}


// ---------------------------------------------------------------------------------------------------------------------------
// *   example: C++ function implementing an external Justina function to add up all float arguments, returning the total.   *
// *   strings are skipped, while the presence of an integer value type will create a Justina error                          *
// ---------------------------------------------------------------------------------------------------------------------------

float addAllFloats(void** const pdata, const char* const valueType, const int argCount, int& execError) {

/*
    Justina call (if function is registered as in this example):
    ------------------------------------------------------------
    usrf_addAllFloats(1.3, 2.5, 20., "hello");                              // result returned: 23.8
    usrf_addAllFloats(1.3, 2.5, 20, "hello");                               // raises a Justina error (integer type encountered)
*/

    // all floating point arguments will be added up; other values will be discarded 
    float total = 0.;
    for (int i = 0; i < argCount; i++) {
        if ((valueType[i] & Justina::value_typeMask) == Justina::value_isLong) { execError = Justina::execResult_type::result_arg_floatTypeExpected; break; }
        else if ((valueType[i] & Justina::value_typeMask) == Justina::value_isFloat) { total += *(float*)pdata[i]; }
    }
    return total;
}


// -------------------------------------------------------------------------------------------------------------------
// *   example: C++ function implementing an external Justina function returning its (single) argument as a char*;   *
// *   if the argument is not a string (char*), the function returns a Justina error                                 *
// -------------------------------------------------------------------------------------------------------------------

char* returnFirstArg(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call (if function is registered as in this example):
    ------------------------------------------------------------
    usrf_returnFirstArg("Christian");                                           // result returned: "Christian"
*/

    // no need to test argument count (fixed to '1' - see STEP 2., below)  
    if ((valueType[0] & Justina::value_typeMask) != Justina::value_isString) {  // argument is not a string ?
        execError = Justina::execResult_type::result_arg_stringExpected;        // will raise a Justina error (see list of error codes in user documentation)
        return "";                                                              // because of the error, the value returned will be lost (even with error trapping)
    }

    /*
    Do NOT return a char* pointing to a local char array, unless it's declared as static (local variables
    exist on the stack until you leave the procedure, and the pointer returned may point to garbage).
    If you return a char* object created on the heap (NEW), make sure to save the pointer value
    (e.g., in a static variable) because you will have to DELETE the object later(also from a user C++ procedure)
    You can return a string literal, because string literals are stored in static memory
    */

    char* name = (char*)(pdata[0]); return name;        // OK
    // static char name[10] = "John";  return name;     // OK (valid return statement as well)
    // return "Peter";                                  // OK (valid return statement as well)
}


//------------------------------------------------------------------------------
// STEP 2. Define records with external Justina function (or command) attributes 
// MORE INFORMATION: see USER MANUAL, available on GitHub
// -----------------------------------------------------------------------------

// external Justina COMMANDS: arrays with Justina command attribute records.
// each record contains an alias, function pointer, absolute minimum and maximum number of arguments,
// and, OPTIONAL, usage restriction and argument sequence restriction (all checked during parsing)
// --------------------------------------------------------------------------------------------------

Justina::CppCommand const cppCommands[]{
    {"usrc_changeAllArgValues", changeAllArgValues, 0, 8, Justina::userCmd_noRestriction, Justina::argSeq_expressions}
};


// external Justina FUNCTIONS: arrays with function attributes records.
// each record contains an alias, function pointer, absolute minimum and maximum number of arguments (checked during parsing)
// --------------------------------------------------------------------------------------------------------------------------

Justina::CppVoidFunction  const cppVoidFunctions[]{                         // user C++ functions returning nothing
    {"usrf_doSomething", doSomething, 0, 0}
};

Justina::CppFloatFunction const cppFloatFunctions[]{                        // user C++ functions returning a floating point number (float) 
    {"usrf_addAllFloats", addAllFloats, 0, 8}
};

Justina::Cpp_pCharFunction const cpp_pCharFunctions[]{                      // user C++ functions returning a char*
    {"usrf_returnFirstArg", returnFirstArg, 1, 1 }
};

// no user C++ functions defined with these return types: 
// OK to comment out or remove next lines 
Justina::CppBoolFunction const cppBoolFunctions[]{ };                       // user C++ functions returning a boolean value (bool) 
Justina::CppCharFunction const cppCharFunctions[]{ };                       // user C++ functions returning a character (char) 
Justina::CppIntFunction const cppIntFunctions[]{ };                         // user C++ functions returning an integer (int) 
Justina::CppLongFunction const cppLongFunctions[]{ };                       // user C++ functions returning a long integer (long) 


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);

    // -----------------------------------------------------------------------------------------
    // STEP 3. Register external Justina functions and commands: BEFORE starting the interpreter 
    // MORE INFORMATION: see USER MANUAL, available on GitHub
    // -----------------------------------------------------------------------------------------

    // external Justina commands: pass address of arrays containing command attributes AND argument count
    justina.registerUserCommands(cppCommands, 1);

    // external Justina functions: for each C++ function return type: pass address of arrays containing function attributes AND argument count
    justina.registerFloatUserCppFunctions(cppFloatFunctions, 1);            // user C++ functions returning a floating point number (float)
    justina.register_pCharUserCppFunctions(cpp_pCharFunctions, 1);         // user C++ functions returning a char* 
    justina.registerVoidUserCppFunctions(cppVoidFunctions, 1);              // user C++ functions returning nothing (void)

    // no user C++ functions defined with these return types: OK to pass addresses with COUNT 0 -OR- to comment out or remove next lines              
    justina.registerBoolUserCppFunctions(cppBoolFunctions, 0);              // user C++ functions returning a boolean value (bool)
    justina.registerCharUserCppFunctions(cppCharFunctions, 0);              // user C++ functions returning a character (char)
    justina.registerIntUserCppFunctions(cppIntFunctions, 0);                // user C++ functions returning an integer (int)
    justina.registerLongUserCppFunctions(cppLongFunctions, 0);              // user C++ functions returning a long integer (long)


    // run interpreter (control will stay there until you quit Justina)
    justina.begin();
}

// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
