/***********************************************************************************************************
*   Sample program, demonstrating how to write user c++ functions for use in a Justina program.            *
*   User c++ functions may provide extra functonality to Justina or they may execute specific hardware     *
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

// define between 1 and 4 alternative input streams and alternative output streams. 
// the first stream will function as the default Justina console.
constexpr int terminalCount{ 1 };           // number of streams defined
Stream* pAltInput[1]{ &Serial };            // Justina input streams                                                  
Print* pAltOutput[1]{ &Serial };            // Justina output streams                                                     


// define the size of program memory in bytes, which depends on the available RAM 
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32)
long progMemSize = 1 << 16;                 // for ESP32 and RP2040, the maximum size is 64 kByte
#else
long progMemSize = 2000;
#endif

// create Justina_interpreter object
// the last argument (0) indicates (among others) that an SD card is not present 
Justina_interpreter justina(pAltInput, pAltOutput, terminalCount, progMemSize, 0);

// sample string
const int myTextLength = 5;
char myText[myTextLength + 1]{ "abcde" };   // including terminator character '0'


// -----------------------------
// 1. Writing user c++ functions
// -----------------------------

/*
*   Justina can call functions written in c++ (named 'user c++ functions') using the SAME syntax as it uses for calling any internal Justina function,
*   passing between 0 and 8 (eight) function arguments back and forth (values are passed by reference) and returning a function result, provided
*   that the user c++ functions utilise the interfacing mechanism described below.
*
*   user c++ functions are implemented as callback functions, allowing Justina to call procedures in the user program.
*
*   No matter how many arguments a Justina function provides, the c++ implmementation of that user function always has 4 (four) parameters.
*
*   parameter 1 (void** const pdata) is a pointer to an array containing void pointers to (maximum eight) arguments, passed by reference by Justina.
*
*   parameter 2 (const char* const valueType) is a pointer to an array indicating the value types (long, float or char*) of the respective arguments, and
*   whether these arguments are Justina variables or constants.
*   - value types: apply public Justina constant 'value_typeMask' before checking a value type (using public Justina constants 'value_isLong', 'value_isFloat' and 'value_isStringPointer')
*   - variable or constant: bit 7 of a value type indicates that the corresponding pointer to an argument points to a Justina variable
*
*   parameter 3 (const int argCount) contains the number of supplied Justina arguments, from 0 to 8 (EIGHT).
*
*   parameter 4 (int& execError) can return an error code to Justina, which will handle this error as it handles all other errors:
*   Justina will stop execution, unless the error is catched by the Justina trapErrors command.
*   The valid range of error codes is from 3000 to 4999. Outside this range, error codes will be discarded.
*
*   All Justina arguments are passed to by reference: Justina sets a pointer to the respective arguments (integer, float or text (char*) before calling the user c++ function.
*   if an argument passed by Justina is not a variable but the result of a Justina expression or a constant, Justina actually passes a pointer to a COPY of the value:
*   this ensures that the user c++ procedure cannot inadvertantly change the original Justina value while keeping it simple for the c++ programmer.
*
*   in case the address pointed to is an ARRAY element, the user actually has access to the complete array by setting a pointer to subsequent or preceding array elements.
*
*   user c++ functions can return a c++ boolean, char, int, long, float, char* as a result, or nothing (void).
*   - Justina will convert c++ boolean, char and int return values to long upon return
*   - Justina will return zero if a user c++ function was called that doesn't return anything
*   - note the special considerations to take into account when returning a char* in the example provided below
*
*   whithin a user c++ procedure:
*   - NEVER change the value type (float, character string)
*   - you can change the characters in a string but NEVER INCREASE the length of strings
*     -> empty strings cannot be changed at all (this would increase the length of the string)
*   - it is allowed to DECREASE the length of a string (with a '\0' terminating character), but keep in mind that it will still occupy the same amount of memory
*     -> exception: if you change a string to an empty string (writing a '\0' terminating character in the first position), memory will be released
*        (in Justina, an empty string is just a null pointer)
*   do NOT change any of the pointers (argument 1); ONLY change the (0 to 8) Justina arguments pointed to by c++ function argument 1.
*
*   further information is contained in the Justina user manual.
*/


// example: a function not using any of its arguments and not returning a value
// ----------------------------------------------------------------------------
void doSomething(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    // ... (do something)
    pAltOutput[0]->println("Justina was here");         // pAltOutput[0] is the default console in Justina
}


// example: user function to add 2 floats together and return the total
// --------------------------------------------------------------------
float addAllFloats(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    // all floating point arguments will be added up; other values will be discarded 
    float total = 0.;
    for (int i = 0; i < argCount; i++) {
        if ((valueType[i] & justina.value_typeMask) == justina.value_isFloat) { total += *(float*)pdata[i]; }
    }
    return total;
}


// example: for all Justina arguments provided, check argument type and change argument value 
// ------------------------------------------------------------------------------------------
void changeAllArgValues(void** const pdata, const char* const valueType, const int argCount, int& execError) {

    for (int i = 0; i < argCount; i++) {                // for each argument...
         // get value type and variable / constant info
        bool isLong = ((valueType[i] & justina.value_typeMask) == justina.value_isLong);
        bool isFloat = ((valueType[i] & justina.value_typeMask) == justina.value_isFloat);
        bool isString = ((valueType[i] & justina.value_typeMask) == justina.value_isStringPointer);
        bool isVariable = (valueType[i] & 0x80);        // (not used in this example)

        // get values or pointers to values
        if (isLong) {
            long arg = *(long*)pdata[i];                // long argument 
            long* pArg = (long*)pdata[i];               // retrieve pointer, and add 1 to the argument (this will only have effect if the argument is a Justina variable)
            (*pArg)++;
        }
        else if (isFloat) {
            float arg = *(float*)pdata[i];              // floating point argument 
            float* pArg = (float*)pdata[i];             // retrieve pointer, multiply the argument by 10. (this will only have effect if the argument is a Justina variable)
            (*pArg) *= 10.;
        }
        // ...
        else if (isString) {
            char char1 = *(char*)pdata[i];              // character array argument, first vharacter ('\0' if empty string)
            char* pChar = (char*)pdata[i];              // pointer to character array (char*)              
            if (strlen(pChar) >= 3) { strcpy(pChar, "abc"); } // if character array has sufficient length, change first three characters
        }
    }
}


// example: user function returning a char* (!!! read important note below !!!), user function producing a Justina error
// ---------------------------------------------------------------------------------------------------------------------
char* returnFirstArg(void** const pdata, const char* const valueType, const int argCount, int& execError) {

    // NOTE: Do NOT return a local char* unless you declare it static, because otherwise it will go out of scope upon return, freeing the memory occupied by the dynamic char array.
    //       Same if you create a NEW char* object and return it, make sure you DELETE it later (also in a c++ user routine): keep the pointer value preserved in between NEW and DELETE. 

    // no need to test argument count; it's fixed to one (see step 2., below)  
    if ((valueType[0] & justina.value_typeMask) != justina.value_isStringPointer) {     // argument is not a string ?
        execError = 3103;                               // error: string expected (see list of error codes in user documentation)
        return myText;                                  // unchanged (because of the error, value will not be used anyway)
    }

    pAltOutput[0]->print("\r\nmy text before change: "); pAltOutput[0]->println(myText);
    // Always respect the initial length of a char* : you may shorten the string stored in it, but you can't make it longer than the initial string
    if (strlen((char*)pdata[0]) <= myTextLength) { strcpy(myText, (char*)pdata[0]); }   // take into account size of mytext array
    pAltOutput[0]->print("my text after change : "); pAltOutput[0]->println(myText);
    return myText;
}


//-------------------------------------------
// 2. Define records with function attributes 
//-------------------------------------------

/*
* For each user c++ function, create a 'function attributes' record in an array, with
* - the Justina name (adhering to the Justina name convention) that will be used for calling the function (char array)
* - the c++ function name (function pointer)
* - minimum and maximum number of arguments allowed (both values between 0 and 8). Justina will check these limits during parsing.
*
* Each function return type (boolean, char, int, long, float, char* as a result, void) has its own array.
* Each array must contain at least 1 entry. If no user c++ functions are defined with a specific return type, add this entry: {"", nullptr, 0, 0} .
* Entries with invalid Justina names or with a null pointer as c++ function pointer will be skipped.
*
* The Justina function name does not have to be the same name as the user c++ function name, the two names are unrelated.
*/

// arrays with function attributes for the user c++ functions defined above
// ------------------------------------------------------------------------
Justina_interpreter::CppFloatFunction const cppFloatFunctions[]{
    {"addAllFloats", addAllFloats, 0, 8}
};

Justina_interpreter::Cpp_pCharFunction const cpp_pCharFunctions[]{
    { "returnFirstArg", returnFirstArg, 1, 1 }
};

Justina_interpreter::CppVoidFunction  const cppVoidFunctions[]{
    {"doSomething", doSomething, 0, 0},
    {"changeAllArgValues", changeAllArgValues, 0, 8},
};

// no user c++ functions defined with these return types: OK to comment out or remove next lines 
Justina_interpreter::CppBoolFunction const cppBoolFunctions[]{ };
Justina_interpreter::CppCharFunction const cppCharFunctions[]{ };
Justina_interpreter::CppIntFunction const cppIntFunctions[]{ };
Justina_interpreter::CppLongFunction const cppLongFunctions[]{ };


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);

    // -----------------------------------------------------------
    // 3. Register user functions: BEFORE starting the interpreter 
    // -----------------------------------------------------------

    justina.registerFloatUserCppFunctions(cppFloatFunctions, sizeof(cppFloatFunctions) / sizeof(cppFloatFunctions[0]));
    justina.register_pCharUserCppFunctions(cpp_pCharFunctions, sizeof(cpp_pCharFunctions) / sizeof(cpp_pCharFunctions[0]));
    justina.registerVoidUserCppFunctions(cppVoidFunctions, sizeof(cppVoidFunctions) / sizeof(cppVoidFunctions[0]));

    // no user c++ functions defined with these return types: OK to comment out or remove next lines 
    justina.registerBoolUserCppFunctions(cppBoolFunctions, sizeof(cppBoolFunctions) / sizeof(cppBoolFunctions[0]));
    justina.registerCharUserCppFunctions(cppCharFunctions, sizeof(cppCharFunctions) / sizeof(cppCharFunctions[0]));
    justina.registerIntUserCppFunctions(cppIntFunctions, sizeof(cppIntFunctions) / sizeof(cppIntFunctions[0]));
    justina.registerLongUserCppFunctions(cppLongFunctions, sizeof(cppLongFunctions) / sizeof(cppLongFunctions[0]));

    
    // run interpreter (control will stay there until you quit Justina)
    justina.begin();                          
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
