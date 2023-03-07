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
        Serial.print(", list elem address: "); Serial.println((uint32_t)p, HEX);
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
    Serial.print(", list elem address: "); Serial.println((uint32_t)pElem, HEX);
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

    // programs and functions
    // ----------------------
    {"program",         cmdcod_program,         cmd_onlyProgramTop | cmd_skipDuringExec,            0,0,    cmdPar_103,     cmdBlockNone},        //// non-block commands: cmdBlockNone ?
    {"function",        cmdcod_function,        cmd_onlyInProgram | cmd_skipDuringExec,             0,0,    cmdPar_108,     cmdBlockExtFunction},

    {"receiveProg",     cmdcod_receiveProg,     cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},

    // declare variables
    // -----------------
    {"var",             cmdcod_var,             cmd_noRestrictions | cmd_skipDuringExec,            0,0,    cmdPar_111,     cmdBlockNone},
    {"const",           cmdcod_constVar,        cmd_noRestrictions | cmd_skipDuringExec,            0,0,    cmdPar_111,     cmdBlockNone},
    {"static",          cmdcod_static,          cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_111,     cmdBlockNone},

    {"delVar",          cmdcod_deleteVar,       cmd_onlyImmediateOutsideBlock | cmd_skipDuringExec, 0,0,    cmdPar_110,     cmdBlockNone},
    {"clearAll",        cmdcod_clearAll,        cmd_onlyImmediateOutsideBlock | cmd_skipDuringExec, 0,0,    cmdPar_102,     cmdBlockNone},
    {"clearProg",       cmdcod_clearProg,       cmd_onlyImmediateOutsideBlock | cmd_skipDuringExec, 0,0,    cmdPar_102,     cmdBlockNone},

    // print system info
    // -----------------
    {"vars",            cmdcod_printVars,       cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"callStack",       cmdcod_printCallSt,     cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},

    // flow control commands
    // ---------------------
    {"for",             cmdcod_for,             cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_109,     cmdBlockFor},
    {"while",           cmdcod_while,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockWhile},
    {"if",              cmdcod_if,              cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockIf},
    {"elseif",          cmdcod_elseif,          cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockIf_elseIf},
    {"else",            cmdcod_else,            cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockIf_else},
    {"end",             cmdcod_end,             cmd_noRestrictions,                                 0,0,    cmdPar_102,     cmdBlockGenEnd},                // closes inner open command block

    {"break",           cmdcod_break,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockOpenBlock_loop},        // allowed if at least one open loop block (any level) 
    {"continue",        cmdcod_continue,        cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockOpenBlock_loop },       // allowed if at least one open loop block (any level) 
    {"return",          cmdcod_return,          cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_106,     cmdBlockOpenBlock_function},    // allowed if currently an open function definition block 

    // input and output commands
    // -------------------------
    {"info",            cmdcod_info,            cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_114,     cmdBlockNone},
    {"input",           cmdcod_input,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_113,     cmdBlockNone},
    {"print",           cmdcod_print,           cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_107,     cmdBlockNone},
    {"printLine",       cmdcod_printLine,       cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_107,     cmdBlockNone},
    {"printTo",         cmdcod_printTo,         cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_112,     cmdBlockNone},
    {"printLineTo",     cmdcod_printLineTo,     cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_112,     cmdBlockNone},
    {"dispFmt",         cmdcod_dispfmt,         cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_112,     cmdBlockNone},
    {"dispMod",         cmdcod_dispmod,         cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_105,     cmdBlockNone},
    {"pause",           cmdcod_pause,           cmd_onlyInFunctionBlock,                            0,0,    cmdPar_106,     cmdBlockNone},
    {"halt",            cmdcod_halt,            cmd_onlyInFunctionBlock,                            0,0,    cmdPar_102,     cmdBlockNone},
    {"initSD",          cmdcod_initSD,          cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"ejectSD",         cmdcod_ejectSD,         cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"listFiles",       cmdcod_listFiles,       cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockNone},


    // debugging commands
    // ------------------
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

    // user callback functions
    // -----------------------

    {"declareCB",       cmdcod_declCB,          cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,  0,0,    cmdPar_110,     cmdBlockNone},
    {"clearCB",         cmdcod_clearCB,         cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,  0,0,    cmdPar_102,     cmdBlockNone},
    {"callcpp",         cmdcod_callback,        cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_101,     cmdBlockNone}
};


// internal (intrinsic) Justina functions
// --------------------------------------

// the 8 array pattern bits indicate the order of arrays and scalars; bit b0 to bit b7 refer to parameter 1 to 8, if a bit is set, an array is expected as argument
// if more than 8 arguments are supplied, only arguments 1 to 8 can be set as array arguments
// maximum number of parameters should be no more than 16

const Justina_interpreter::FuncDef Justina_interpreter::_functions[]{
    //  name                    id code                         #par    array pattern
    //  ----                    -------                         ----    -------------   

    // logical functions
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
#if !defined(ARDUINO_ARCH_RP2040)                                                                                               // Arduino RP2040: prevent linker error
    {"analogReference",         fnccod_analogReference,         1,1,    0b0},
#endif
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

    // Arduino SD card library
    { "open",                    fnccod_open,                   1,2,    0b0 },
    { "close",                   fnccod_close,                  1,1,    0b0 },
    { "read",                    fnccod_read,                   1,2,    0b0 },
    { "readBytes",               fnccod_readBytes,              2,2,    0b0 },
    { "readBytesUntil",          fnccod_readBytesUntil,         3,3,    0b0 },
    { "readLine",                fnccod_readLine,               2,2,    0b0 },
    { "find",                    fnccod_find,                   2,2,    0b0 },
    { "findUntil",               fnccod_findUntil,              3,3,    0b0 },
    { "peek",                    fnccod_peek,                   1,1,    0b0 },
    { "position",                fnccod_position,               1,1,    0b0 },
    { "size",                    fnccod_size,                   1,1,    0b0 },
    { "available",               fnccod_available,              1,1,    0b0 },
    { "name",                    fnccod_name,                   1,1,    0b0 },
    { "flush",                   fnccod_flush,                  1,1,    0b0 },
    { "seek",                    fnccod_seek,                   2,2,    0b0 },
    { "setTimeout",              fnccod_setTimeout,             2,2,    0b0 },
    { "isDirectory",             fnccod_isDirectory,            1,1,    0b0 },
    { "rewindDirectory",         fnccod_rewindDirectory,        1,1,    0b0 },
    { "openNextFile",            fnccod_openNextFile,           1,1,    0b0 },
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
    {term_bitShRightAssign, termcod_bitShRightAssign,   0x00,               0x01 | op_RtoL | op_long,   0x00}
};


// -------------------
// *   constructor   *
// -------------------

Justina_interpreter::Justina_interpreter(Stream* const pConsole, long progMemSize, int _SDcardChipSelectPin) : 
    _pConsole(pConsole), _progMemorySize(progMemSize), _SDcardChipSelectPin(_SDcardChipSelectPin) {

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

    _progMemorySize = progMemSize;

    _programStorage = new char[_progMemorySize + IMM_MEM_SIZE];

    initInterpreterVariables(true);
};


// ---------------------
// *   deconstructor   *
// ---------------------

Justina_interpreter::~Justina_interpreter() {
    if (!_keepInMemory) {
        resetMachine(true);             // delete all objects created on the heap: with = with user variables and FiFo stack
        _housekeepingCallback = nullptr;
        delete[] _programStorage;
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
    char c{};



    //// start temp test
    /*
    uint8_t* testptr = (uint8_t*)0x20000000;

    typedef union {
        struct {
            uint32_t bits1_0 : 2;
            uint32_t bits10_3 : 8;
        } bit;
        uint32_t reg;
    } ABC_type;

    ABC_type abc;
    abc.reg = 0xFFFFFFFF;
    Serial.print("word: "); Serial.println(abc.reg, HEX);

    abc.bit.bits1_0 = 1;
    abc.bit.bits10_3 = 3;

    Serial.print("word: "); Serial.println(abc.reg, HEX);
    Serial.print("bits: "); Serial.println(abc.bit.bits10_3, HEX);

    Serial.println("\r\nregisters: ");
    testptr = (uint8_t*)(&abc);
    testptr[0] = 'a';
    testptr[1] = 'b';
    testptr[2] = 'c';
    testptr[3] = 'd';

    Serial.println(testptr[0], HEX);
    Serial.println(testptr[1], HEX);
    Serial.println(testptr[2], HEX);
    Serial.println(testptr[3], HEX);

    uint32_t* tstp = (uint32_t*)(testptr);
    Serial.println(*tstp, HEX);
    Serial.print("word: "); Serial.println(abc.reg, HEX);

    testptr = (uint8_t*)0x20000000;
    testptr[1];

    uint8_t* REG8_PORT_DIRCLR0 = (uint8_t*)(&REG_PORT_DIRCLR0);        // 8 bit port register acces
    REG8_PORT_DIRCLR0[2];
    */
    //// end temp test


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
    _programCounter = _programStorage + _progMemorySize;
    *(_programStorage + _progMemorySize) = tok_no_token;                                      //  current end of program (immediate mode)
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

        // get a key (character from console) if available and perform a regular housekeeping callback as well
        c = getKey(kill, allowTimeOut);     // while parsing a program, set allowTimeOut to true to make sure the program is received completely              
        if (kill) { break; }                // return true if kill request received from calling program

        if (c < 0xFF) { _initiateProgramLoad = false; }                     // reset _initiateProgramLoad after each character received
        bool programOrStatementRead = _programMode ? ((c == 0xFF) && allowTimeOut) : (c == '\n');
        if ((c == 0xFF) && !programOrStatementRead) { continue; }                // no character (except when program or imm. mode line is read): start next loop

        quitNow = false;

        // if no character added: nothing to do, wait for next
        bool bufferOverrun{ false };                                        // buffer where statement characters are assembled for parsing
        bool noCharAdded = !addCharacterToInput(lastCharWasSemiColon, withinString, withinStringEscSequence, within1LineComment, withinMultiLineComment, redundantSemiColon, programOrStatementRead,
            bufferOverrun, flushAllUntilEOF, lineCount, statementCharCount, c);

        do {        // one loop only
            if (bufferOverrun) { result = result_statementTooLong; }
            else if (noCharAdded) { break; }               // start a new outer loop (read a character if available, etc.)

            // if a statement is complete (terminated by a semicolon or end of input), parse it
            // --------------------------------------------------------------------------------
            bool isStatementSeparator = (!withinString) && (!within1LineComment) && (!withinMultiLineComment) && (c == ';') && !redundantSemiColon;
            isStatementSeparator = isStatementSeparator || (withinString && (c == '\n'));  // new line sent to parser as well

            bool statementComplete = !bufferOverrun && (isStatementSeparator || (programOrStatementRead && (statementCharCount > 0)));

            int clearIndicator{ 0 };                                    // 1 = clear program cmd, 2 = clear all cmd
            if (statementComplete && !_quitJustina) {                   // if quitting anyway, just skip                                               
                _appFlags &= ~appFlag_errorConditionBit;              // clear error condition flag 
                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_parsing;     // status 'parsing'

                _statement[statementCharCount] = '\0';                            // add string terminator

                char* pStatement = _statement;                                                 // because passed by reference 
                char* pDummy{};
                _parsingExecutingTraceString = false; _parsingEvalString = false;
                result = parseStatement(pStatement, pDummy, clearIndicator = 0);          // parse ONE statement only 
                pErrorPos = pStatement;                                                      // in case of error

                if (result != result_tokenFound) { flushAllUntilEOF = true; }
                if (result == result_parse_kill) { kill = true; _quitJustina = true; }     // flushAllUntilEOF is true already (flush buffer before quitting)

                // reset after each statement read 
                statementCharCount = 0;
                withinString = false; withinStringEscSequence = false; within1LineComment = false; withinMultiLineComment = false;
                lastCharWasSemiColon = false;
            }

            if (programOrStatementRead) {       // program mode: complete program read and parsed / imm. mode: 1 statement read and parsed (with or without error)
                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_idle;     // status 'idle'

                quitNow = processAndExec(result, kill, lineCount, pErrorPos, clearIndicator);  // return value: quit Justina now

                // parsing error occured ? reset input controlling variables
                if (result != result_tokenFound) {
                    statementCharCount = 0;
                    withinString = false; withinStringEscSequence = false; within1LineComment = false; withinMultiLineComment = false;
                    lastCharWasSemiColon = false;
                }

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
    
    ejectSD() ;         // safety (in case an SD card is present: close all files and stop SD

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

bool Justina_interpreter::processAndExec(parseTokenResult_type result, bool& kill, int lineCount, char* pErrorPos, int clearIndicator) {

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
    else { printParsingResult(result, funcNotDefIndex, _statement, lineCount, pErrorPos); }                 // parsing error occured: print error message



    // if not in program mode and no parsing error: execute
    // ----------------------------------------------------
    execResult_type execResult{ result_execOK };
    if (!_programMode && (result == result_tokenFound)) {
        execResult = exec(_programStorage + _progMemorySize);                                             // execute parsed user statements

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
    else if (clearIndicator != 0) {                     // 1 = clear program cmd, 2 = clear all cmd 
        do {
            char s[50];
            sprintf(s, "===== Clear %s ? (please answer Y or N) =====", ((clearIndicator == 2) ? "memory" : "program"));
            _pConsole->println(s);

            // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
            // return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
            bool doAbort{ false }, doStop{ false }, doCancel{ false }, doDefault{ false };      // not used but mandatory
            int length{ 0 };
            char input[MAX_USER_INPUT_LEN + 1] = "";                                                                          // init: empty string
            if (readText(doAbort, doStop, doCancel, doDefault, input, length)) { return result_kill; }  // kill request from caller ?

            bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
            if (validAnswer) {
                if (tolower(input[0]) == 'y') { _pConsole->println((clearIndicator == 2) ? "clearing memory" : "clearing program"); resetMachine(clearIndicator == 2); }       // 1 = clear program, 2 = clear all (including user variables)
                break;
            }
        } while (true);
    }

    // no program error (could be immmediate mode error however): only reset a couple of items here 
    else {
        parsingStack.deleteList();
        _blockLevel = 0;
        _extFunctionBlockOpen = false;
    }


    // execution finished (not stopping in debug mode), with or without error: delete parsed strings in imm mode command : they are on the heap and not needed any more. Identifiers must stay avaialble
    // -> if stopping a program for debug, do not delete parsed strings (in imm. mode command), because that command line has now been pushed on  ...
     // the parsed command line stack and included parsed constants will be deleted later (resetMachine routine)
    if (execResult != result_stopForDebug) { deleteConstStringObjects(_programStorage + _progMemorySize); } // always


    // finalize
    // --------
    _programMode = false;
    _programCounter = _programStorage + _progMemorySize;                 // start of 'immediate mode' program area
    *(_programStorage + _progMemorySize) = tok_no_token;                                      //  current end of program (immediate mode)

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

    // parsing error occured ? wait until no more characters received (important if received from Serial)
    if ((result != result_tokenFound) && (_pConsole->available() > 0)) {
        char c{};
        do {
            if (_quitJustina) { break; };       // could be set before loop starts
            c = getKey(_quitJustina, true);     // set allowWaitTime to true: wait a little before deciding no more characters come in

        } while (c != 0xFF);        // assignment, not a comparison operator
    }

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


//--------------------------------------
// execute regular housekeeping callback
// -------------------------------------

void Justina_interpreter::checkTimeAndExecHousekeeping(bool& killNow) {
    // do a housekeeping callback at regular intervals (if callback function defined)
    killNow = false;
    if (_housekeepingCallback != nullptr) {
        _currenttime = millis();
        _previousTime = _currenttime;
        // note: also handles millis() overflow after about 47 days
        if ((_lastCallBackTime + callbackPeriod < _currenttime) || (_currenttime < _previousTime)) {            // while executing, limit calls to housekeeping callback routine 
            _lastCallBackTime = _currenttime;
            _housekeepingCallback(killNow, _appFlags);                                                           // execute housekeeping callback
            if (killNow) {
                while (_pConsole->available() > 0) { _pConsole->read(); }                                        // flush buffer and flag 'kill' (request from Justina caller)
            }
        }
    }
}


// -------------------------------------------------------------------------------------------------
// *   read character from keyboard, if available, and regularly perfoem a housekeeping callback   *
// -------------------------------------------------------------------------------------------------

char Justina_interpreter::getKey(bool& killNow, bool allowWaitTime) {     // default: no time out

    // enable time out = false: only check once for a character
    //                   true: allow a certain time for the character to arrive   

    checkTimeAndExecHousekeeping(killNow);
    if (killNow) { while (_pConsole->available() > 0) { _pConsole->read(); } return 0xFF; }     // empty buffer before quitting
    
    // read a character, if available in buffer
    char c = 0xFF;                                                                                              // init: no character read

    // read a character, if available in buffer
    long startWaitForReadTime = millis();
    bool readCharWindowExpired{};

    do {
        if (_pConsole->available() > 0) { c = _pConsole->read(); break; }
        // try to read character only once or keep trying until timeout occurs ?
        readCharWindowExpired = (!allowWaitTime || (startWaitForReadTime + GETCHAR_TIMEOUT < millis()));
    } while (!readCharWindowExpired);

    return c;

}

// ---------------------------------------------------------
// *   read text from keyboard and store in c++ variable   *
// ---------------------------------------------------------

// read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
// return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
// return value 'true' indicates kill request from Justina caller

bool Justina_interpreter::readText(bool& doAbort, bool& doStop, bool& doCancel, bool& doDefault, char* input, int& length) {
    bool backslashFound{ false }, quitNow{ false };

    length = 0;  // init
    do {                                                                                                            // until new line character encountered
        // read a character, if available in buffer
        char c{ };                                                           // init: no character available
        bool kill{ false };
        c = getKey(kill);               // get a key (character from console) if available and perform a regular housekeeping callback as well
        if (kill) { return true; }      // return value true: kill Justina interpreter (buffer is now flushed until next line character)

        if (c != 0xFF) {                                                                           // terminal character available for reading ?
            if (c == '\n') { break; }                                                                               // read until new line character
            else if (c < ' ') { continue; }                                                                         // skip control-chars except new line (ESC is skipped here as well - flag already set)

            // Check for Justina ESCAPE sequence (sent by terminal as individual characters) and cancel input, or use default value, if indicated
            // Note: if Justina ESCAPE sequence is not recognized, then backslash character is simply discarded
            if (c == '\\') {                                                                                        // backslash character found
                backslashFound = !backslashFound;
                if (backslashFound) { continue; }                                                                   // first backslash in a sequence: note and do nothing
            }

            else if (tolower(c) == 'a') {                                                                    // part of a Justina ESCAPE sequence ? Abort evaluation phase 
                if (backslashFound) { backslashFound = false;  doAbort = true;  continue; }
            }
            else if (tolower(c) == 's') {                                                                    // part of a Justina ESCAPE sequence ? Stop and enter debug mode 
                if (backslashFound) { backslashFound = false;  doStop = true;  continue; }
            }
            else if (tolower(c) == 'c') {                                                                    // part of a Justina ESCAPE sequence ? Cancel if allowed 
                if (backslashFound) { backslashFound = false;  doCancel = true;  continue; }
            }
            else if (tolower(c) == 'd') {                                                                    // part of a Justina ESCAPE sequence ? Use default value if provided
                if (backslashFound) { backslashFound = false; doDefault = true;  continue; }
            }

            if (length >= MAX_USER_INPUT_LEN) { continue; }                                                           // max. input length exceeded: drop character
            input[length] = c; input[++length] = '\0';
        }
    } while (true);

    return false;
}


// ---------------------------------------------------------------------------------------------
// print a list of global program variables and user variables with name, type, qualifier, value
// ---------------------------------------------------------------------------------------------

// user variables only: indicate whether they are used in the currently parsed program (if any)
// arrays: indicate dimensions and number of elements

void Justina_interpreter::printVariables(bool userVars) {

    // print table header
    char line[MAX_IDENT_NAME_LEN + 30];     // sufficient length for all line elements except the variable value itself
    sprintf(line, ("%-*s %-2c%-8s%-7svalue"), MAX_IDENT_NAME_LEN, (userVars ? "user variable       " : "global prog variable"), (userVars ? 'U' : ' '), "type", "qual");
    _pConsole->println(line);
    sprintf(line, "%-*s %-2c%-8s%-7s-----", MAX_IDENT_NAME_LEN, (userVars ? "-------------" : "--------------------"), (userVars ? '-' : ' '), "----", "----");
    _pConsole->println(line);

    // print table
    int varCount = userVars ? _userVarCount : _programVarNameCount;
    char** varName = userVars ? userVarNames : programVarNames;
    char* varType = userVars ? userVarType : globalVarType;
    Val* varValues = userVars ? userVarValues : globalVarValues;
    bool userVarUsedInProgram{};
    bool  varNameHasGlobalValue{};
    bool linesPrinted{ false };

    for (int q = 0; q <= 1; q++) {
        bool lookForConst = q == 0;
        for (int i = 0; i < varCount; i++) {
            varNameHasGlobalValue = userVars ? true : varType[i] & var_nameHasGlobalValue;
            if (varNameHasGlobalValue) {
                bool isConst = (varType[i] & var_isConstantVar);
                if (lookForConst == isConst) {
                    int valueType = (varType[i] & value_typeMask);
                    userVars ? userVarUsedInProgram = (varType[i] & var_userVarUsedByProgram) : false;
                    bool isLong = (valueType == value_isLong);
                    bool isFloat = (valueType == value_isFloat);
                    bool isString = (valueType == value_isStringPointer);
                    bool isArray = (varType[i] & var_isArray);

                    char type[10];
                    strcpy(type, isLong ? "long" : isFloat ? "float" : isString ? "string" : "????");

                    sprintf(line, "%-*s %-2c%-8s%-7s", MAX_IDENT_NAME_LEN, *(varName + i), (userVarUsedInProgram ? 'x' : ' '), type, (isConst ? "const  " : "       "));
                    _pConsole->print(line);

                    if (isArray) {
                        uint8_t* dims = (uint8_t*)varValues[i].pArray;
                        int dimCount = dims[3];
                        char arrayText[40] = "";
                        sprintf(arrayText, "(array %d", dims[0]);
                        if (dimCount >= 2) { sprintf(arrayText, "%sx%d", arrayText, dims[1]); }
                        if (dimCount == 3) { sprintf(arrayText, "%sx%d", arrayText, dims[2]); }
                        if (dimCount >= 2) { sprintf(arrayText, "%s = %d", arrayText, int(dims[0]) * int(dims[1]) * int(dimCount == 3 ? dims[2] : 1)); }
                        strcat(arrayText, " elem)");
                        _pConsole->println(arrayText);
                    }

                    else if (isLong) { _pConsole->println(varValues[i].longConst); }
                    else if (isFloat) { _pConsole->println(varValues[i].floatConst); }
                    else if (isString) {
                        char* pString = varValues[i].pStringConst;
                        expandStringBackslashSequences(pString);        // creates new string: DELETE it immediately after printing
                        _pConsole->println(pString);
                        delete[] pString;
                    }
                    else { _pConsole->println("????"); }

                    linesPrinted = true;
                }
            }
        }
    }
    if (!linesPrinted) { _pConsole->println("    (none)"); }
    _pConsole->println();
}

// -------------------------------
// delete a list of user variables
// -------------------------------

Justina_interpreter::parseTokenResult_type Justina_interpreter::deleteUserVariable(char* userVarName) {

    bool deleteLastVar = (userVarName == nullptr);

    bool varDeleted{ false };
    for (int index = (deleteLastVar ? _userVarCount - 1 : 0); index < _userVarCount; index++) {
        if (!deleteLastVar) {
            if (strcmp(userVarNames[index], userVarName) != 0) { continue; }     // no match yet: continue looking for it (if it exists)
        }

        bool userVarUsedInProgram = (userVarType[index] & var_userVarUsedByProgram);
        if (userVarUsedInProgram) { return result_varUsedInProgram; }        // match, but cannot delete (variable used in program)

        int valueType = (userVarType[index] & value_typeMask);
        bool isLong = (valueType == value_isLong);
        bool isFloat = (valueType == value_isFloat);
        bool isString = (valueType == value_isStringPointer);
        bool isArray = (userVarType[index] & var_isArray);


        // 1. delete variable name object
        // ------------------------------
    #if printCreateDeleteListHeapObjects
        Serial.print("----- (usrvar name) "); Serial.println((uint32_t) * (pIdentNameArray + index), HEX);
    #endif
        delete[] * (userVarNames + index);
        _userVarNameStringObjectCount--;

        // 2. if variable is an array of strings: delete all non-empty strings in array
        // ----------------------------------------------------------------------------
        if (isArray && isString) { deleteOneArrayVarStringObjects(userVarValues, index, true, false); }

        // 3. if variable is an array: delete the array storage
        // ----------------------------------------------------
        //    NOTE: do this before checking for strings (if both 'var_isArray' and 'value_isStringPointer' bits are set: array of strings, with strings already deleted)
        if (isArray) {       // variable is an array: delete array storage          
        #if printCreateDeleteListHeapObjects
            Serial.print(isUserVar ? "----- (usr ar stor) " : isLocalVar ? "----- (loc ar stor) " : "----- (array stor ) "); Serial.println((uint32_t)varValues[index].pStringConst, HEX);
        #endif
            delete[]  userVarValues[index].pArray;
            _userArrayObjectCount--;
    }

        // 4. if variable is a scalar string value: delete string
        // ------------------------------------------------------
        else if (isString) {       // variable is a scalar containing a string
            if (userVarValues[index].pStringConst != nullptr) {
            #if printCreateDeleteListHeapObjects
                Serial.print(isUserVar ? "----- (usr var str) " : isLocalVar ? "----- (loc var str)" : "----- (var string ) "); Serial.println((uint32_t)varValues[index].pStringConst, HEX);
            #endif
                delete[]  userVarValues[index].pStringConst;
                _userVarStringObjectCount--;
}
}

        // 5. move up next user variables one place
        //    if a user variable is used in currently loaded program: adapt index in program storage
        // -----------------------------------------------------------------------------------------
        for (int i = index; i < _userVarCount - 1; i++) {
            userVarNames[i] = userVarNames[i + 1];
            userVarValues[i] = userVarValues[i + 1];
            userVarType[i] = userVarType[i + 1];

            userVarUsedInProgram = (userVarType[i + 1] & var_userVarUsedByProgram);
            if (userVarUsedInProgram) {

                char* programStep = _programStorage;
                int tokenType{};
                do {
                    tokenType = findTokenStep(programStep, tok_isVariable, var_isUser, i + 1);
                    if (tokenType == '\0') { break; }
                    --((TokenIsVariable*)programStep)->identValueIndex;
                } while (true);
            }
        }

        _userVarCount--;
        varDeleted = true;
    }

    if (!varDeleted) { return result_varNotDeclared; }

    return result_tokenFound;
}