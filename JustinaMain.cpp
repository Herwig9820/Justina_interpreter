/***************************************************************************************
    Justina interpreter library for Arduino Nano 33 IoT and Arduino RP2040.

    Version:    v1.00 - xx/xx/2022
    Author:     Herwig Taveirne

    Justina is an interpreter which does NOT require you to use an IDE to write
    and compile programs. Programs are written on the PC using any text processor
    and transferred to the Arduino using any serial terminal capable of sending files.
    Justina can store and retrieve programs and other data on an SD card as well.

    See GitHub for more information and documentation: //// <links>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************************/


#include "Justina.h"
////#include "math.h"
////#include <avr/dtostrf.h>  & dotostrf() functie toevoegen (bug in sprintf)        

#define printCreateDeleteListHeapObjects 0
#define debugPrint 0


// *****************************************************************
// ***            class LinkedList - implementation              ***
// *****************************************************************

// ---------------------------------------------
// *   initialisation of static class member   *
// ---------------------------------------------

int LinkedList::_listIDcounter = 0;

// -------------------
// *   constructor   *
// -------------------

LinkedList::LinkedList() {
    _listID = _listIDcounter;
    _listIDcounter++;       // static variable
    _pFirstElement = nullptr;
    _pLastElement = nullptr;
}


// ---------------------
// *   deconstructor   *
// ---------------------

LinkedList::~LinkedList() {
    _listIDcounter--;       // static variable
}


// --------------------------------------------------
// *   append a list element to the end of a list   *
// --------------------------------------------------

char* LinkedList::appendListElement(int size) {
    ListElemHead* p = (ListElemHead*)(new char[sizeof(ListElemHead) + size]);       // create list object with payload of specified size in bytes

    if (_pFirstElement == nullptr) {                                                  // not yet any elements
        _pFirstElement = p;
        p->pPrev = nullptr;                                                             // is first element in list: no previous element
    }
    else {
        _pLastElement->pNext = p;
        p->pPrev = _pLastElement;
    }
    _pLastElement = p;
    p->pNext = nullptr;                                                                 // because p is now last element
    _listElementCount++;

#if printCreateDeleteListHeapObjects
    Serial.print("(LIST) Create elem # "); Serial.print(_listElementCount);
    Serial.print(", list ID "); Serial.print(_listID);
    Serial.print(", stack: "); Serial.print(_listName);
    if (p == nullptr) { Serial.println("- list elem adres: nullptr"); }
    else {
        Serial.print(", list elem address: "); Serial.println((uint32_t)p - RAMSTART);
    }
#endif
    return (char*)(p + 1);                                          // pointer to payload of newly created element
}


// -----------------------------------------------------
// *   delete a heap object and remove it from list    *
// -----------------------------------------------------

char* LinkedList::deleteListElement(void* pPayload) {                              // input: pointer to payload of a list element

    ListElemHead* pElem = (ListElemHead*)pPayload;                                     // still points to payload: check if nullptr
    if (pElem == nullptr) { pElem = _pLastElement; }                                  // nullptr: delete last element in list (if it exists)
    else { pElem = pElem - 1; }                                                         // pointer to list element header

    if (pElem == nullptr) { return nullptr; }                                         // still nullptr: return (list is empty)

    ListElemHead* p = pElem->pNext;                                                     // remember return value

#if printCreateDeleteListHeapObjects
    // determine list element # by counting from the list start
    ListElemHead* q = _pFirstElement;
    int i{};
    for (i = 1; i <= _listElementCount; ++i) {
        if (q == pElem) { break; }            // always a match
        q = q->pNext;
    }

    Serial.print("(LIST) Delete elem # "); Serial.print(i); Serial.print(" (new # "); Serial.print(_listElementCount - 1);
    Serial.print("), list ID "); Serial.print(_listID);
    Serial.print(", stack: "); Serial.print(_listName);
    Serial.print(", list elem address: "); Serial.println((uint32_t)pElem - RAMSTART);
#endif

    // before deleting object, remove from list:
    // change pointers from previous element (or _pFirstPointer, if no previous element) and next element (or _pLastPointer, if no next element)
    ((pElem->pPrev == nullptr) ? _pFirstElement : pElem->pPrev->pNext) = pElem->pNext;
    ((pElem->pNext == nullptr) ? _pLastElement : pElem->pNext->pPrev) = pElem->pPrev;

    _listElementCount--;
    delete[]pElem;

    if (p == nullptr) { return nullptr; }
    else { return (char*)(p + 1); }                                           // pointer to payload of next element in list, or nullptr if last element deleted
}


// ------------------------------------------
// *   delete all list elements in a list   *
// ------------------------------------------

void LinkedList::deleteList() {
    if (_pFirstElement == nullptr) return;

    ListElemHead* pHead = _pFirstElement;
    while (true) {
        char* pNextPayload = deleteListElement((char*)(pHead + 1));
        if (pNextPayload == nullptr) { return; }
        pHead = ((ListElemHead*)pNextPayload) - 1;                                     // points to list element header 
    }
}


// ----------------------------------------------------
// *   get a pointer to the first element in a list   *
// ----------------------------------------------------

char* LinkedList::getFirstListElement() {
    return (char*)(_pFirstElement + (_pFirstElement == nullptr ? 0 : 1)); // add one header length
}


//----------------------------------------------------
// *   get a pointer to the last element in a list   *
//----------------------------------------------------

char* LinkedList::getLastListElement() {

    return (char*)(_pLastElement + (_pLastElement == nullptr ? 0 : 1)); // add one header length
}


// -------------------------------------------------------
// *   get a pointer to the previous element in a list   *
// -------------------------------------------------------

char* LinkedList::getPrevListElement(void* pPayload) {                                 // input: pointer to payload of a list element  
    if (pPayload == nullptr) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*)pPayload) - 1;                                     // points to list element header
    if (pElem->pPrev == nullptr) { return nullptr; }
    return (char*)(pElem->pPrev + 1);                                                      // points to payload of previous element
}


//----------------------------------------------------
// *   get a pointer to the next element in a list   *
//----------------------------------------------------

char* LinkedList::getNextListElement(void* pPayload) {
    if (pPayload == nullptr) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*)pPayload) - 1;                                     // points to list element header
    if (pElem->pNext == nullptr) { return nullptr; }
    return (char*)(pElem->pNext + 1);                                                      // points to payload of previous element
}


//-------------------------------------------------------------
// *   get the list ID (depends on the order of creation !)   *
//-------------------------------------------------------------

int LinkedList::getListID() {
    return _listID;
}


//--------------------------
// *   set the list name   *
//--------------------------

void LinkedList::setListName(char* listName) {
    strncpy(_listName, listName, listNameSize - 1);
    _listName[listNameSize - 1] = '\0';
    return;
}


//--------------------------
// *   get the list name   *
//--------------------------

char* LinkedList::getListName() {
    return _listName;
}


//-------------------------------
// *   get list element count   *
//-------------------------------

int LinkedList::getElementCount() {
    return _listElementCount;
}



/***********************************************************
*                 class Justina_interpreter                *
***********************************************************/

// -------------------------------------------------
// *   // initialisation of static class members   *
// -------------------------------------------------


// commands (FUNCTION, FOR, ...): 'allowed command parameter' keys
// ---------------------------------------------------------------

// command parameter spec name                 param type and flags                           param type and flags                            param type and flags                             param type and flags
// ---------------------------                 --------------------                           --------------------                            --------------------                             --------------------

const char Justina_interpreter::cmdPar_100[4]{ cmdPar_ident | cmdPar_multipleFlag,            cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_101[4]{ cmdPar_ident,                                  cmdPar_expression | cmdPar_multipleFlag,         cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_102[4]{ cmdPar_none,                                   cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_103[4]{ cmdPar_ident,                                  cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_104[4]{ cmdPar_expression,                             cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_105[4]{ cmdPar_expression,                             cmdPar_expression,                               cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_106[4]{ cmdPar_expression | cmdPar_optionalFlag,       cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_107[4]{ cmdPar_expression | cmdPar_multipleFlag,       cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_108[4]{ cmdPar_extFunction,                            cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_109[4]{ cmdPar_varOptAssignment,                       cmdPar_expression,                               cmdPar_expression | cmdPar_optionalFlag,        cmdPar_none };
const char Justina_interpreter::cmdPar_110[4]{ cmdPar_ident,                                  cmdPar_ident | cmdPar_multipleFlag,              cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_111[4]{ cmdPar_varOptAssignment,                       cmdPar_varOptAssignment | cmdPar_multipleFlag,   cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_112[4]{ cmdPar_expression,                             cmdPar_expression | cmdPar_multipleFlag,         cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_113[4]{ cmdPar_expression,                             cmdPar_varOptAssignment,                         cmdPar_varOptAssignment,                        cmdPar_none };
const char Justina_interpreter::cmdPar_114[4]{ cmdPar_expression,                             cmdPar_varOptAssignment | cmdPar_optionalFlag,   cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_999[4]{ cmdPar_varNoAssignment,                        cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };////test var no assignment


// commands: keywords with attributes
// ----------------------------------

const Justina_interpreter::ResWordDef Justina_interpreter::_resWords[]{
    //  name            id code                 where allowed              padding (boundary alignment)     param key      control info
    //  ----            -------                 -------------              ----------------------------     ---------      ------------   

    /* programs and functions */
    /* ---------------------- */

    {"program",         cmdcod_program,         cmd_onlyProgramTop | cmd_skipDuringExec,            0,0,    cmdPar_103,     cmdProgram},        //// non-block commands: cmdBlockNone ?
    {"function",        cmdcod_function,        cmd_onlyInProgram | cmd_skipDuringExec,             0,0,    cmdPar_108,     cmdBlockExtFunction},


    /* declare variables */
    /* ----------------- */

    {"var",             cmdcod_var,             cmd_noRestrictions | cmd_skipDuringExec,            0,0,    cmdPar_111,     cmdGlobalVar},
    {"static",          cmdcod_static,          cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_111,     cmdStaticVar},
    ////{"local",           cmdcod_local,           cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_111,     cmdLocalVar},

    //// to do
    //// -----

    {"delVar",          cmdcod_delete,          cmd_onlyImmediate | cmd_skipDuringExec,             0,0,    cmdPar_110,     cmdDeleteVar},
    {"clearVars",       cmdcod_clear,           cmd_onlyImmediate | cmd_skipDuringExec,             0,0,    cmdPar_102,     cmdBlockNone},
    {"test",            cmdcod_test,            cmd_onlyImmediate | cmd_skipDuringExec,/* temp */   0,0,    cmdPar_999,     cmdBlockNone},//// test var no assignment

    {"printVars",       cmdcod_vars,            cmd_onlyImmediate | cmd_skipDuringExec,/* temp */   0,0,    cmdPar_102,     cmdBlockNone},
    {"printCBs",        cmdcod_printCB,         cmd_onlyImmediate | cmd_skipDuringExec,/* temp */   0,0,    cmdPar_102,     cmdBlockNone},
    {"printProg",       cmdcod_printProg,       cmd_onlyImmediate | cmd_skipDuringExec,/* temp */   0,0,    cmdPar_102,     cmdBlockNone},


    // 
    // --------------

    {"printCallstack",  cmdcod_printCallSt,     cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},

    {"receiveProg",     cmdcod_receiveProg,     cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},


    /* flow control commands */
    /* --------------------- */

    {"for",             cmdcod_for,             cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_109,     cmdBlockFor},
    {"while",           cmdcod_while,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockWhile},
    {"if",              cmdcod_if,              cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockIf},
    {"elseif",          cmdcod_elseif,          cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockIf_elseIf},
    {"else",            cmdcod_else,            cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockIf_else},
    {"end",             cmdcod_end,             cmd_noRestrictions,                                 0,0,    cmdPar_102,     cmdBlockGenEnd},                // closes inner open command block

    {"break",           cmdcod_break,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockOpenBlock_loop},        // allowed if at least one open loop block (any level) 
    {"continue",        cmdcod_continue,        cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockOpenBlock_loop },       // allowed if at least one open loop block (any level) 
    {"return",          cmdcod_return,          cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_106,     cmdBlockOpenBlock_function},    // allowed if currently an open function definition block 


    /* input and output commands */
    /* ------------------------- */

    {"info",            cmdcod_info,            cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_114,     cmdBlockNone},
    {"input",           cmdcod_input,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_113,     cmdBlockNone},
    {"print",           cmdcod_print,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_107,     cmdBlockNone},
    {"dispFmt",         cmdcod_dispfmt,         cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_112,     cmdBlockNone},
    {"dispMod",         cmdcod_dispmod,         cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_105,     cmdBlockNone},
    {"pause",           cmdcod_pause,           cmd_onlyInFunctionBlock,                            0,0,    cmdPar_106,     cmdBlockNone},
    {"halt",            cmdcod_halt,            cmd_onlyInFunctionBlock,                            0,0,    cmdPar_102,     cmdBlockNone},


    /* debugging commands */
    /* ------------------ */

    {"stop",            cmdcod_stop,            cmd_onlyInFunctionBlock,                            0,0,    cmdPar_102,     cmdBlockNone},
    {"nop",             cmdcod_nop,             cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_102,     cmdBlockNone},                  // insert two bytes in program, do nothing

    {"go",              cmdcod_go,              cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"step",            cmdcod_step,            cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"stepOut",         cmdcod_stepOut,         cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"stepOver",        cmdcod_stepOver,        cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"blockStepOut",    cmdcod_stepOutOfBlock,  cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"blockStepEnd",    cmdcod_stepToBlockEnd,  cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"skip",            cmdcod_skip,            cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},

    {"trace",           cmdcod_trace,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockNone},

    {"abort",           cmdcod_abort,           cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"debug",           cmdcod_debug,           cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"quit",            cmdcod_quit,            cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_106,     cmdBlockNone},


    /* user callback functions */
    /* ----------------------- */

    {"declareCB",       cmdcod_declCB,          cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,  0,0,    cmdPar_110,     cmdBlockNone},
    {"clearCB",         cmdcod_clearCB,         cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,  0,0,    cmdPar_102,     cmdBlockNone},
    {"callCpp",         cmdcod_callback,        cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_101,     cmdBlockNone},
};


// internal (intrinsic) Justina functions
// --------------------------------------

// the 8 array pattern bits indicate the order of arrays and scalars; bit b0 to bit b7 refer to parameter 1 to 8, if a bit is set, an array is expected as argument
// if more than 8 arguments are supplied, only arguments 1 to 8 can be set as array arguments
// maximum number of parameters should be no more than 16

const Justina_interpreter::FuncDef Justina_interpreter::_functions[]{
    //  name                    id code                         #par    array pattern
    //  ----                    -------                         ----    -------------   

    //// logical functions
    {"ifte",                    fnccod_ifte,                    3,16,   0b0},
    {"switch",                  fnccod_switch,                  3,16,   0b0},
    {"index",                   fnccod_index,                   3,16,   0b0},
    {"choose",                  fnccod_choose,                  3,16,   0b0},

    // other functions
    {"eval",                    fnccod_eval,                    1,1,    0b0},
    {"ubound",                  fnccod_ubound,                  2,2,    0b00000001},        // first parameter is array (LSB)
    {"dims",                    fnccod_dims,                    1,1,    0b00000001},
    {"type",                    fnccod_valueType,               1,1,    0b0},
    {"r",                       fnccod_last,                    0,1,    0b0},               // short label for 'last result'
    {"ft",                      fnccod_format,                  1,6,    0b0},               // short label for 'system value'
    {"sysval",                  fnccod_sysVal,                  1,1,    0b0},

    // math functions
    {"sqrt",                    fnccod_sqrt,                    1,1,    0b0},
    {"sin",                     fnccod_sin,                     1,1,    0b0},
    {"cos",                     fnccod_cos,                     1,1,    0b0},
    {"tan",                     fnccod_tan,                     1,1,    0b0},
    {"asin",                    fnccod_asin,                    1,1,    0b0},
    {"acos",                    fnccod_acos,                    1,1,    0b0},
    {"atan",                    fnccod_atan,                    1,1,    0b0},
    {"ln",                      fnccod_ln,                      1,1,    0b0},
    {"lnp1",                    fnccod_lnp1,                    1,1,    0b0},
    {"log10",                   fnccod_log10,                   1,1,    0b0},
    {"exp",                     fnccod_exp,                     1,1,    0b0},
    {"expm1",                   fnccod_expm1,                   1,1,    0b0},

    {"round",                   fnccod_round,                   1,1,    0b0},
    {"ceil",                    fnccod_ceil,                    1,1,    0b0},
    {"floor",                   fnccod_floor,                   1,1,    0b0},
    {"trunc",                   fnccod_trunc,                   1,1,    0b0},

    {"min",                     fnccod_min,                     2,2,    0b0},
    {"max",                     fnccod_max,                     2,2,    0b0},
    {"abs",                     fnccod_abs,                     1,1,    0b0},
    {"sign",                    fnccod_sign,                    1,1,    0b0},
    {"fmod",                    fnccod_fmod,                    2,2,    0b0},

    // conversion functions
    {"cInt",                    fnccod_cint,                    1,1,    0b0},
    {"cFloat",                  fnccod_cfloat,                  1,1,    0b0},
    {"cStr",                    fnccod_cstr,                    1,1,    0b0},

    // Arduino digital I/O, timing and other functions
    {"millis",                  fnccod_millis,                  0,0,    0b0},
    {"micros",                  fnccod_micros,                  0,0,    0b0},
    {"delay",                   fnccod_delay,                   1,1,    0b0},
    {"delayMicroseconds",       fnccod_delayMicroseconds,       1,1,    0b0},
    {"digitalRead",             fnccod_digitalRead,             1,1,    0b0},
    {"digitalWrite",            fnccod_digitalWrite,            2,2,    0b0},
    {"pinMode",                 fnccod_pinMode,                 2,2,    0b0},
    {"analogRead",              fnccod_analogRead,              1,1,    0b0},
    {"analogReference",         fnccod_analogReference,         1,1,    0b0},
    {"analogWrite",             fnccod_analogWrite,             2,2,    0b0},
    {"analogReadResolution",    fnccod_analogReadResolution,    1,1,    0b0},
    {"analogWriteResolution",   fnccod_analogWriteResolution,   1,1,    0b0},
    {"noTone",                  fnccod_noTone,                  1,1,    0b0},
    {"pulseIn",                 fnccod_pulseIn,                 2,3,    0b0},
    {"shiftIn",                 fnccod_shiftIn,                 3,3,    0b0},
    {"shiftOut",                fnccod_shiftOut,                4,4,    0b0},
    {"tone",                    fnccod_tone,                    2,3,    0b0},
    {"random",                  fnccod_random,                  1,2,    0b0},
    {"randomSeed",              fnccod_randomSeed,              1,1,    0b0},

    // bit and byte manipulation functions
    {"bit",                     fnccod_bit,                     1,1,    0b0},
    {"bitRead",                 fnccod_bitRead,                 2,2,    0b0},
    {"bitClear",                fnccod_bitClear,                2,2,    0b0},
    {"bitSet",                  fnccod_bitSet,                  2,2,    0b0},
    {"bitWrite",                fnccod_bitWrite,                3,3,    0b0},
    {"maskedBitRead",           fnccod_bitsMaskedRead,          2,2,    0b0},
    {"maskedBitClear",          fnccod_bitsMaskedClear,         2,2,    0b0},
    {"maskedBitSet",            fnccod_bitsMaskedSet,           2,2,    0b0},
    {"maskedBitWrite",          fnccod_bitsMaskedWrite,         3,3,    0b0},
    {"byteRead",                fnccod_byteRead,                2,2,    0b0},
    {"byteWrite",               fnccod_byteWrite,               3,3,    0b0},
    {"reg32Read",               fnccod_reg32Read,               1,1,    0b0},
    {"reg8Read",                fnccod_reg8Read,                2,2,    0b0},
    {"reg32Write",              fnccod_reg32Write,              2,2,    0b0},
    {"reg8Write",               fnccod_reg8Write,               3,3,    0b0},

    // string and 'character' functions
    {"char",                    fnccod_char,                    1,1,    0b0},
    {"len",                     fnccod_len,                     1,1,    0b0},
    {"nl",                      fnccod_nl,                      0,0,    0b0},
    {"asc",                     fnccod_asc,                     1,2,    0b0},
    {"rtrim",                   fnccod_rtrim,                   1,1,    0b0},
    {"ltrim",                   fnccod_ltrim,                   1,1,    0b0},
    {"trim",                    fnccod_trim,                    1,1,    0b0},
    {"left",                    fnccod_left,                    2,2,    0b0},
    {"mid",                     fnccod_mid,                     3,3,    0b0},
    {"right",                   fnccod_right,                   2,2,    0b0},
    {"toUpper",                 fnccod_toupper,                 1,3,    0b0},
    {"toLower",                 fnccod_tolower,                 1,3,    0b0},
    {"space",                   fnccod_space,                   1,1,    0b0},
    {"repChar",                 fnccod_repchar,                 2,2,    0b0},
    {"strStr",                  fnccod_strstr,                  2,3,    0b0},
    {"strCmp",                  fnccod_strcmp,                  2,2,    0b0},

    {"isAlpha",                 fnccod_isAlpha,                 1,2,    0b0},
    {"isAlphaNumeric",          fnccod_isAlphaNumeric,          1,2,    0b0},
    {"isAscii",                 fnccod_isAscii,                 1,2,    0b0},
    {"isControl",               fnccod_isControl,               1,2,    0b0},
    {"isDigit",                 fnccod_isDigit,                 1,2,    0b0},
    {"isGraph",                 fnccod_isGraph,                 1,2,    0b0},
    {"isHexadecimalDigit",      fnccod_isHexadecimalDigit,      1,2,    0b0},
    {"isLowerCase",             fnccod_isLowerCase,             1,2,    0b0},
    {"isUpperCase",             fnccod_isUpperCase,             1,2,    0b0},
    {"isPrintable",             fnccod_isPrintable,             1,2,    0b0},
    {"isPunct",                 fnccod_isPunct,                 1,2,    0b0},
    {"isSpace",                 fnccod_isSpace,                 1,2,    0b0},
    {"isWhitespace",            fnccod_isWhitespace,            1,2,    0b0},
};


// terminal tokens 
// ---------------

// priority: bits b43210 define priority if used as prefix, infix, postfix operator, respectively (0x1 = lowest, 0x1F = highest) 
// priority 0 means operator not available for use as use as postfix, prefix, infix operator
// bit b7 defines associativity for infix operators (bit set indicates 'right-to-left').
// prefix operators: always right-to-left. postfix operators: always left-to-right
// NOTE: table entries with names starting with same characters: shortest entries should come BEFORE longest (e.g. '!' before '!=', '&' before '&&')
// postfix operator names can only be shared with prefix operator names

const Justina_interpreter::TerminalDef Justina_interpreter::_terminals[]{

    //  name            id code                 prefix prio                 infix prio          postfix prio         
    //  ----            -------                 -----------                 ----------          ------------   

    // non-operator terminals

    {term_comma,            termcod_comma,              0x00,               0x00,                       0x00},
    {term_semicolon,        termcod_semicolon,          0x00,               0x00,                       0x00},
    {term_rightPar,         termcod_rightPar,           0x00,               0x00,                       0x00},
    {term_leftPar,          termcod_leftPar,            0x00,               0x10,                       0x00},

    // operators (0x00 -> operator not available, 0x01 -> pure or compound assignment)
    // op_long: operands must be long, a long is returned (e.g. 'bitand' operator)
    // res_long: operands can be float or long, a long is returned (e.g. 'and' operator)
    // op_RtoL: operator has right-to-left associativity
    // prefix operators: always right-to-left associativity; not added to the operator definition table below

    {term_assign,           termcod_assign,             0x00,               0x01 | op_RtoL,             0x00},

    {term_bitAnd,           termcod_bitAnd,             0x00,               0x06 | op_long,             0x00},
    {term_bitXor,           termcod_bitXor,             0x00,               0x05 | op_long,             0x00},
    {term_bitOr,            termcod_bitOr,              0x00,               0x04 | op_long,             0x00},

    {term_and,              termcod_and,                0x00,               0x03 | res_long,            0x00},
    {term_or,               termcod_or,                 0x00,               0x02 | res_long,            0x00},
    {term_not,              termcod_not,                0x0C | res_long,    0x00,                       0x00},
    {term_bitCompl,         termcod_bitCompl,           0x0C | op_long,     0x00,                       0x00},

    {term_eq,               termcod_eq,                 0x00,               0x07 | res_long,            0x00},
    {term_neq,              termcod_ne,                 0x00,               0x07 | res_long,            0x00},
    {term_lt,               termcod_lt,                 0x00,               0x08 | res_long,            0x00},
    {term_gt,               termcod_gt,                 0x00,               0x08 | res_long,            0x00},
    {term_ltoe,             termcod_ltoe,               0x00,               0x08 | res_long,            0x00},
    {term_gtoe,             termcod_gtoe,               0x00,               0x08 | res_long,            0x00},

    {term_bitShLeft,        termcod_bitShLeft,          0x00,               0x09 | op_long,             0x00},
    {term_bitShRight,       termcod_bitShRight,         0x00,               0x09 | op_long,             0x00},

    {term_plus,             termcod_plus,               0x0C,               0x0A,                       0x00},      // note: for strings, concatenate
    {term_minus,            termcod_minus,              0x0C,               0x0A,                       0x00},
    {term_mult,             termcod_mult,               0x00,               0x0B,                       0x00},
    {term_div,              termcod_div,                0x00,               0x0B,                       0x00},
    {term_mod,              termcod_mod,                0x00,               0x0B | op_long,             0x00},
    {term_pow,              termcod_pow,                0x00,               0x0D | op_RtoL,             0x00},

    {term_incr,             termcod_incr,               0x0E,               0x00,                       0x0F},
    {term_decr,             termcod_decr,               0x0E,               0x00,                       0x0F},

    {term_plusAssign,       termcod_plusAssign,         0x00,               0x01 | op_RtoL,             0x00},
    {term_minusAssign,      termcod_minusAssign,        0x00,               0x01 | op_RtoL,             0x00},
    {term_multAssign,       termcod_multAssign,         0x00,               0x01 | op_RtoL,             0x00},
    {term_divAssign,        termcod_divAssign,          0x00,               0x01 | op_RtoL,             0x00},
    {term_modAssign,        termcod_modAssign,          0x00,               0x01 | op_RtoL,             0x00},

    {term_bitAndAssign,     termcod_bitAndAssign,       0x00,               0x01 | op_RtoL | op_long,   0x00},
    {term_bitOrAssign,      termcod_bitOrAssign,        0x00,               0x01 | op_RtoL | op_long,   0x00},
    {term_bitXorAssign,     termcod_bitXorAssign,       0x00,               0x01 | op_RtoL | op_long,   0x00},

    {term_bitShLeftAssign,  termcod_bitShLeftAssign,    0x00,               0x01 | op_RtoL | op_long,   0x00},
    {term_bitShRightAssign, termcod_bitShRightAssign,   0x00,               0x01 | op_RtoL | op_long,   0x00},
};


// -------------------
// *   constructor   *
// -------------------

Justina_interpreter::Justina_interpreter(Stream* const pConsole) : _pConsole(pConsole) {

    // settings to be initialized when cold starting interpreter only
    // --------------------------------------------------------------

    _coldStart = true;

    _housekeepingCallback = nullptr;
    for (int i = 0; i < _userCBarrayDepth; i++) { _callbackUserProcStart[i] = nullptr; }
    _userCBprocStartSet_count = 0;

    _resWordCount = (sizeof(_resWords)) / sizeof(_resWords[0]);
    _functionCount = (sizeof(_functions)) / sizeof(_functions[0]);
    _terminalCount = (sizeof(_terminals)) / sizeof(_terminals[0]);

    _quitJustina = false;
    _isPrompt = false;

    _programMode = false;
    _currenttime = millis();
    _previousTime = _currenttime;
    _lastCallBackTime = _currenttime;

    parsingStack.setListName("parsing ");
    evalStack.setListName("eval    ");
    flowCtrlStack.setListName("flowCtrl");
    immModeCommandStack.setListName("cmd line");

    // initialize interpreter object fields
    // ------------------------------------

    initInterpreterVariables(true);
};


// ---------------------
// *   deconstructor   *
// ---------------------

Justina_interpreter::~Justina_interpreter() {
    if (!_keepInMemory) {
        resetMachine(true);             // delete all objects created on the heap
        _housekeepingCallback = nullptr;
    }
    _pConsole->println("\r\nJustina: bye\r\n");
};


// ------------------------------
// *   set call back functons   *
// ------------------------------

bool Justina_interpreter::setMainLoopCallback(void (*func)(bool& requestQuit, long& appFlags)) {

    // a call from the user program initializes the address of a 'user callback' function.
    // Justina will call this user routine repeatedly and automatically, allowing  the user...
    // ...to execute a specific routine regularly (e.g. to maintain a TCP connection, to implement a heartbeat, ...)
    _housekeepingCallback = func;
    return true;
}

bool Justina_interpreter::setUserFcnCallback(void(*func) (const void** data, const char* valueType, const int argCount)) {

    // each call from the user program initializes a next 'user callback' function address in an array of function addresses 
    if (_userCBprocStartSet_count > +_userCBarrayDepth) { return false; }      // throw away if callback array full
    _callbackUserProcStart[_userCBprocStartSet_count++] = func;
    return true; // success
}



// ----------------------------
// *   interpreter main loop   *
// ----------------------------

bool Justina_interpreter::run(Stream* const pConsole, Stream** const pTerminal, int definedTerms) {

    bool withinStringEscSequence{ false };
    bool lastCharWasSemiColon{ false };
    bool within1LineComment{ false };
    bool withinMultiLineComment{ false };
    bool withinString{ false };
    static bool flushAllUntilEOF{ false };

    int lineCount{ 0 };
    int statementCharCount{ 0 };
    char* pErrorPos{};
    parseTokenResult_type result{ result_tokenFound };    // init

    // local variables 
    bool kill{ false };                                       // kill is true: request from caller, kill is false: quit command executed
    bool quitNow{ false };
    char c;

    bool redundantSemiColon = false;
    bool isCommentStartChar = (c == '$');                               // character can also be part of comment

    _pConsole->println();
    for (int i = 0; i < 13; i++) { _pConsole->print("*"); } _pConsole->print("____");
    for (int i = 0; i < 4; i++) { _pConsole->print("*"); } _pConsole->print("__");
    for (int i = 0; i < 14; i++) { _pConsole->print("*"); } _pConsole->print("_");
    for (int i = 0; i < 10; i++) { _pConsole->print("*"); }_pConsole->println();

    _pConsole->print("    "); _pConsole->println(ProductName);
    _pConsole->print("    "); _pConsole->println(LegalCopyright);
    _pConsole->print("    Version: "); _pConsole->print(ProductVersion); _pConsole->print(" ("); _pConsole->print(BuildDate); _pConsole->println(")");
    for (int i = 0; i < 48; i++) { _pConsole->print("*"); } _pConsole->println();

    _appFlags = 0x0000L;                            // init application flags (for communication with Justina caller, using callbacks)

    _programMode = false;
    _programCounter = _programStorage + PROG_MEM_SIZE;
    *(_programStorage + PROG_MEM_SIZE) = tok_no_token;                                      //  current end of program (immediate mode)
    _pConsole = pConsole;
    _isPrompt = false;                 // end of parsing
    _pTerminal = pTerminal;
    _definedTerminals = definedTerms;

    _coldStart = false;             // can be used if needed in this procedure, to determine whether this was a cold or warm start

    do {
        // when loading a program, as soon as first printable character of a PROGRAM is read, each subsequent character needs to follow after the previous one within a fixed time delay, handled by getKey().
        // program reading ends when no character is read within this time window.
        // when processing immediate mode statements (single line), reading ends when a New Line terminating character is received
        bool allowTimeOut = _programMode && !_initiateProgramLoad;          // _initiateProgramLoad is set during execution of the command to read a program source file from the console
        if (getKey(c, allowTimeOut)) { kill = true; break; }                // return true if kill request received from calling program
        if (c < 0xFF) { _initiateProgramLoad = false; }                     // reset _initiateProgramLoad after each printable character received
        bool programOrLineRead = _programMode ? ((c == 0xFF) && allowTimeOut) : (c == '\n');
        if ((c == 0xFF) && !programOrLineRead) { continue; }                // no character (except when program or imm. mode line is read): start next loop

        quitNow = false;

        // if no character added: nothing to do, wait for next
        bool bufferOverrun{ false };
        bool noCharAdded = !addCharacterToInput(lastCharWasSemiColon, withinString, withinStringEscSequence, within1LineComment, withinMultiLineComment, redundantSemiColon, programOrLineRead,
            bufferOverrun, flushAllUntilEOF, lineCount, statementCharCount, c);

        do {        // one loop only
            if (bufferOverrun) { result = result_statementTooLong; }
            else if (noCharAdded) { break; }               // start a new outer loop (read a character if available, etc.)

            // if a statement is complete (terminated by a semicolon or end of input), parse it
            // --------------------------------------------------------------------------------
            bool isStatementSeparator = (!withinString) && (!within1LineComment) && (!withinMultiLineComment) && (c == ';') && !redundantSemiColon;
            isStatementSeparator = isStatementSeparator || (withinString && (c == '\n'));  // new line sent to parser as well

            bool statementComplete = !bufferOverrun && (isStatementSeparator || (programOrLineRead && (statementCharCount > 0)));

            if (statementComplete && !_quitJustina) {                   // if quitting anyway, just skip                                               
                _appFlags &= ~appFlag_errorConditionBit;              // clear error condition flag 
                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_parsing;     // status 'parsing'

                _statement[statementCharCount] = '\0';                            // add string terminator

                char* pStatement = _statement;                                                 // because passed by reference 
                char* pDummy{};
                _parsingExecutingTraceString = false; _parsingEvalString = false;
                result = parseStatement(pStatement, pDummy);                                 // parse ONE statement only 
                pErrorPos = pStatement;                                                      // in case of error

                if (result != result_tokenFound) { flushAllUntilEOF = true; }
                if (result == result_parse_kill) { kill = true; _quitJustina = true; }     // flushAllUntilEOF is true already (flush buffer before quitting)

                // reset after each statement read 
                statementCharCount = 0;
                withinString = false; withinStringEscSequence = false; within1LineComment = false; withinMultiLineComment = false;
                lastCharWasSemiColon = false;
            }

            if (programOrLineRead) {       // program mode: complete program read and parsed / imm. mode: 1 statement read and parsed (with or without error)
                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_idle;     // status 'idle'

                quitNow = processAndExec(result, kill, lineCount, pErrorPos);  // return value: quit Justina now

                // reset after program (or imm. mode line) is read
                lineCount = 0;
                flushAllUntilEOF = false;

                _statement[statementCharCount] = '\0';                            // add string terminator
                result = result_tokenFound;
            }
        } while (false);

        if (quitNow) { break; }                        // user gave quit command


    } while (true);

    // returning control to Justina caller
    _appFlags = 0x0000L;                            // clear all application flags
    _housekeepingCallback(quitNow, _appFlags);      // pass application flags to caller immediately

    if (kill) { _keepInMemory = false; _pConsole->println("\r\n\r\n>>>>> Justina: kill request received from calling program <<<<<"); }
    if (_keepInMemory) { _pConsole->println("\r\nJustina: bye\r\n"); }        // if remove from memory: message given in destructor
    _quitJustina = false;         // if interpreter stays in memory: re-init

    return _keepInMemory;           // return to calling program
}


// ------------------------------------------------------------------------------
// * add a character received from the input stream to the parsing input buffer *
// ------------------------------------------------------------------------------

bool Justina_interpreter::addCharacterToInput(bool& lastCharWasSemiColon, bool& withinString, bool& withinStringEscSequence, bool& within1LineComment, bool& withinMultiLineComment,
    bool& redundantSemiColon, bool ImmModeLineOrProgramRead, bool& bufferOverrun, bool _flushAllUntilEOF, int& _lineCount, int& _statementCharCount, char c) {

    const char commentOuterDelim = '/'; // twice: single line comment, followed by inner del.: start of multi-line comment, preceded by inner delimiter: end of multi-line comment 
    const char commentInnerDelim = '*';

    static bool lastCharWasWhiteSpace{ false };
    static char lastCommentChar{ '\0' };                                  // init: none

    bool redundantSpaces = false;                                       // init

    bufferOverrun = false;
    if ((c < ' ') && (c != '\n')) { return false; }          // skip control-chars except new line and EOF character

    // when a program or imm. mode line is completely read and the last character (part of the last statement) received from input stream is not a semicolon, add it
    if (ImmModeLineOrProgramRead) {
        if (_statementCharCount > 0) {
            if (_statement[_statementCharCount - 1] != ';') {
                if (_statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }
                _statement[_statementCharCount] = ';';                               // still room: add character
                _statementCharCount++;
            }
        }

        within1LineComment = false;
        withinMultiLineComment = false;
    }

    // not at end of program or imm. mode line: process character   
    else {
        if (_flushAllUntilEOF) { return false; }                       // discard characters (after parsing error)

        if (c == '\n') { _lineCount++; }                           // line number used when while reading program in input file

        // currently within a string or within a comment ? check for ending delimiter, check for in-string backslash sequences
        if (withinString) {
            if (c == '\\') { withinStringEscSequence = !withinStringEscSequence; }
            else if (c == '\"') { withinString = withinStringEscSequence; withinStringEscSequence = false; }
            else { withinStringEscSequence = false; }                 // any other character within string
            lastCharWasWhiteSpace = false;
            lastCharWasSemiColon = false;
        }

        // within a single-line comment ? check for end of comment 
        else if (within1LineComment) {
            if (c == '\n') { within1LineComment = false; return false; }                // comment stops at end of line
        }

        // within a multi-line comment ? check for end of comment 
        else if (withinMultiLineComment) {
            if ((c == commentOuterDelim) && (lastCommentChar == commentInnerDelim)) { withinMultiLineComment = false; return false; }
            lastCommentChar = c;                // a discarded character within a comment
        }

        // NOT within a string or (single-or multi-) line comment ?
        else {
            bool leadingWhiteSpace = (((c == ' ') || (c == '\n')) && (_statementCharCount == 0));
            if (leadingWhiteSpace) { return false; };

            // start of string ?
            if (c == '\"') { withinString = true; }

            // start of (single-or multi-) line comment ?
            else if ((c == commentOuterDelim) || (c == commentInnerDelim)) {  // if previous character = same, then remove it from input buffer. It's the start of a single line comment
                if (_statementCharCount > 0) {
                    if (_statement[_statementCharCount - 1] == commentOuterDelim) {
                        lastCommentChar = '\0';         // reset
                        --_statementCharCount;
                        _statement[_statementCharCount] = '\0';                            // add string terminator

                        ((c == commentOuterDelim) ? within1LineComment : withinMultiLineComment) = true; return false;
                    }
                }
            }

            // white space in multi-line statements: replace a new line with a space (program only)
            else if (c == '\n') { c = ' '; }

            // check last character 
            redundantSpaces = (_statementCharCount > 0) && (c == ' ') && lastCharWasWhiteSpace;
            redundantSemiColon = (c == ';') && lastCharWasSemiColon;
            lastCharWasWhiteSpace = (c == ' ');                     // remember
            lastCharWasSemiColon = (c == ';');
        }

        // do NOT add character to parsing input buffer if specific conditions are met
        if (redundantSpaces || redundantSemiColon || within1LineComment || withinMultiLineComment) { return false; }            // no character added
        if (_statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }

        // add character  
        _statement[_statementCharCount] = c;                               // still room: add character
        ++_statementCharCount;
    }

    return true;
}


// ------------------------------------------------------------------------------------------------------------------------
// * finalise parsing, execute if no errors, if in debug mode, trace and print debug info, re-init machine state and exit *
// ------------------------------------------------------------------------------------------------------------------------

bool Justina_interpreter::processAndExec(parseTokenResult_type result, bool& kill, int lineCount, char* pErrorPos) {

    // all statements (in program or imm. mode line) have been parsed: finalise
    // ------------------------------------------------------------------------
    int funcNotDefIndex;
    if (result == result_tokenFound) {
        // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
        if (_programMode && (!allExternalFunctionsDefined(funcNotDefIndex))) { result = result_undefinedFunctionOrArray; }
        if (_blockLevel > 0) { result = result_noBlockEnd; }
    }

    (_programMode ? _lastProgramStep : _lastUserCmdStep) = _programCounter;

    if (result == result_tokenFound) {
        if (_programMode) {
            // parsing OK message (program mode only - no message in immediate mode)  
            printParsingResult(result, funcNotDefIndex, _statement, lineCount, pErrorPos);
        }
        else {
            // evaluation comes here
            if (_promptAndEcho == 2) { prettyPrintStatements(0); _pConsole->println(); }                    // immediate mode and result OK: pretty print input line
            else if (_promptAndEcho == 1) { _pConsole->println(); _isPrompt = false; }
        }
    }
    else { printParsingResult(result, funcNotDefIndex, _statement, lineCount, pErrorPos); }                   // error message


    // if not in program mode and no parsing error: execute
    // ----------------------------------------------------
    execResult_type execResult{ result_execOK };
    if (!_programMode && (result == result_tokenFound)) {
        execResult = exec(_programStorage + PROG_MEM_SIZE);                                             // execute parsed user statements

        if ((execResult == result_kill) || (execResult == result_quit)) { _quitJustina = true; }
        if (execResult == result_kill) { kill = true; }
    }


    // if in debug mode, trace expressions (if defined) and print debug info 
    // ---------------------------------------------------------------------
    if ((_openDebugLevels > 0) && (execResult != result_kill) && (execResult != result_quit) && (execResult != result_initiateProgramLoad)) { traceAndPrintDebugInfo(); }


    // re-init or reset interpreter state 
    // ----------------------------------
    // if program parsing error: reset machine, because variable storage might not be consistent with program any more
    if ((_programMode) && (result != result_tokenFound)) { resetMachine(false); }
    else if (execResult == result_initiateProgramLoad) { resetMachine(false); }

    // no program error (could be immmediate mode error however): only reset a couple of items here 
    else {
        parsingStack.deleteList();
        _blockLevel = 0;
        _extFunctionBlockOpen = false;
    }


    // execution finished (not stopping in debug mode), with or without error: delete parsed strings in imm mode command : they are on the heap and not needed any more. Identifiers must stay avaialble
    // -> if stopping a program for debug, do not delete parsed strings (in imm. mode command), because that command line has now been pushed on  ...
     // the parsed command line stack and included parsed constants will be deleted later (resetMachine routine)
    if (execResult != result_stopForDebug) { deleteConstStringObjects(_programStorage + PROG_MEM_SIZE); } // always


    // finalize
    // --------
    _programMode = false;
    _programCounter = _programStorage + PROG_MEM_SIZE;                 // start of 'immediate mode' program area
    *(_programStorage + PROG_MEM_SIZE) = tok_no_token;                                      //  current end of program (immediate mode)

    if (execResult == result_initiateProgramLoad) {
        _programMode = true;
        _programCounter = _programStorage;

        if (_isPrompt) { _pConsole->println(); }
        _pConsole->print("Waiting for program...\r\n");
        _isPrompt = false;

        _initiateProgramLoad = true;
    }

    // has an error occured ? (exclude 'events' reported as an error)
    bool isError = (result != result_tokenFound) || ((execResult != result_execOK) && (execResult < result_startOfEvents));
    isError ? (_appFlags |= appFlag_errorConditionBit) : (_appFlags &= ~appFlag_errorConditionBit);              // set or clear error condition flag 
    (_appFlags &= ~appFlag_statusMask);
    (_openDebugLevels > 0) ? (_appFlags |= appFlag_stoppedInDebug) : (_appFlags |= appFlag_idle);     // status 'debug mode' or 'idle'


    // print new prompt and exit
    // -------------------------
    if ((_promptAndEcho != 0) && (execResult != result_initiateProgramLoad)) { _pConsole->print("Justina> "); _isPrompt = true; }
    return _quitJustina;
}

// ---------------------------------------------------------------------
// * trace expressions as defined in Trace statement, print debug info *
// ---------------------------------------------------------------------

void Justina_interpreter::traceAndPrintDebugInfo() {
    // count of programs in debug:
    // - if an error occured in a RUNNING program, the program is terminated and the number of STOPPED programs ('in debug mode') does not change.
    // - if an error occured while executing a command line, then this count is not changed either
    // flow control stack:
    // - at this point, structure '_activeFunctionData' always contains flow control data for the main program level (command line - in debug mode if the count of open programs is not zero)
    // - the flow control stack maintains data about open block commands, open functions and eval() strings in execution (call stack)
    // => skip stack elements for any command line open block commands or eval() strings in execution, and fetch the data for the function where control will resume when started again

    char* nextStatementPointer = _programCounter;
    OpenFunctionData* pDeepestOpenFunction = &_activeFunctionData;

    void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;
    int blockType = block_none;
    do {                                                                // there is at least one open function in the call stack
        blockType = *(char*)pFlowCtrlStackLvl;
        if (blockType != block_extFunction) {
            pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
            continue;
        };
        break;
    } while (true);
    pDeepestOpenFunction = (OpenFunctionData*)pFlowCtrlStackLvl;        // deepest level of nested functions
    nextStatementPointer = pDeepestOpenFunction->pNextStep;

    _pConsole->println(); for (int i = 1; i <= _dispWidth; i++) { _pConsole->print("-"); } _pConsole->println();
    parseAndExecTraceString();     // trace string may not contain keywords, external functions, generic names
    char msg[150] = "";
    sprintf(msg, "DEBUG ==>> NEXT [%s: ", extFunctionNames[pDeepestOpenFunction->functionIndex]);
    _pConsole->print(msg);
    prettyPrintStatements(10, nextStatementPointer);

    if (_openDebugLevels > 1) {
        sprintf(msg, "*** this + %d other programs STOPPED ***", _openDebugLevels - 1);
        _pConsole->println(msg);
    }
}