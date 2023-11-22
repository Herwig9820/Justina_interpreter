/************************************************************************************************************
*    Justina interpreter library for Arduino boards with 32 bit SAMD microconrollers                        *
*                                                                                                           *
*    Tested with Nano 33 IoT and Arduino RP2040                                                             *
*                                                                                                           *
*    Version:    v1.01 - 12/07/2023                                                                         *
*    Author:     Herwig Taveirne, 2021-2023                                                                 *
*                                                                                                           *
*    Justina is an interpreter which does NOT require you to use an IDE to write and compile programs.      *
*    Programs are written on the PC using any text processor and transferred to the Arduino using any       *
*    Serial or TCP Terminal program capable of sending files.                                               *
*    Justina can store and retrieve programs and other data on an SD card as well.                          *
*                                                                                                           *
*    See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                           *
*    This program is free software: you can redistribute it and/or modify                                   *
*    it under the terms of the GNU General Public License as published by                                   *
*    the Free Software Foundation, either version 3 of the License, or                                      *
*    (at your option) any later version.                                                                    *
*                                                                                                           *
*    This program is distributed in the hope that it will be useful,                                        *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of                                         *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                           *
*    GNU General Public License for more details.                                                           *
*                                                                                                           *
*    You should have received a copy of the GNU General Public License                                      *
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.                                  *
************************************************************************************************************/


#include "Justina.h"

#define PRINT_HEAP_OBJ_CREA_DEL 0
#define PRINT_DEBUG_INFO 0
#define PRINT_OBJECT_COUNT_ERRORS 0

// progMemSize defines the size of Justina program memory in bytes, which depends on the available RAM 
#if !defined (ARDUINO_ARCH_RP2040) && !defined (ARDUINO_ARCH_SAMD)
#error Justina library does not support boards with this processor.
#endif 


// *****************************************************************
// ***        class Justina_interpreter - implementation         ***
// *****************************************************************

// ----------------------------------------------
// *   initialisation of static class members   *
// ----------------------------------------------


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
const char Justina_interpreter::cmdPar_108[4]{ cmdPar_JustinaFunction,                        cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_109[4]{ cmdPar_varOptAssignment,                       cmdPar_expression,                               cmdPar_expression | cmdPar_optionalFlag,        cmdPar_none };
const char Justina_interpreter::cmdPar_110[4]{ cmdPar_ident,                                  cmdPar_ident | cmdPar_multipleFlag,              cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_111[4]{ cmdPar_varOptAssignment,                       cmdPar_varOptAssignment | cmdPar_multipleFlag,   cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_112[4]{ cmdPar_expression,                             cmdPar_expression | cmdPar_multipleFlag,         cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_113[4]{ cmdPar_expression,                             cmdPar_varOptAssignment,                         cmdPar_varOptAssignment,                        cmdPar_none };
const char Justina_interpreter::cmdPar_114[4]{ cmdPar_expression,                             cmdPar_varOptAssignment | cmdPar_optionalFlag,   cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_115[4]{ cmdPar_expression,                             cmdPar_expression | cmdPar_optionalFlag,         cmdPar_none,                                    cmdPar_none };
const char Justina_interpreter::cmdPar_116[4]{ cmdPar_expression,                             cmdPar_expression,                               cmdPar_expression | cmdPar_multipleFlag,        cmdPar_none };
const char Justina_interpreter::cmdPar_117[4]{ cmdPar_expression,                             cmdPar_expression,                               cmdPar_expression | cmdPar_optionalFlag,        cmdPar_none };


// commands: keywords with attributes
// ----------------------------------

const Justina_interpreter::ResWordDef Justina_interpreter::_resWords[]{
    //  name            id code                 where allowed                                          #arg     param key       control info
    //  ----            -------                 -------------                                          ----     ---------       ------------   

    // declare and delete variables
    // ----------------------------
    {"var",             cmdcod_var,             cmd_noRestrictions | cmd_skipDuringExec,                1,15,   cmdPar_111,     cmdBlockNone},
    {"const",           cmdcod_constVar,        cmd_noRestrictions | cmd_skipDuringExec,                1,15,   cmdPar_111,     cmdBlockNone},
    {"static",          cmdcod_static,          cmd_onlyInFunctionBlock | cmd_skipDuringExec,           1,15,   cmdPar_111,     cmdBlockNone},

    {"delete",          cmdcod_deleteVar,       cmd_onlyImmModeTop | cmd_skipDuringExec,                1,15,   cmdPar_110,     cmdBlockNone},                  // can only delete user variables (imm. mode)

    {"clearMem",        cmdcod_clearAll,        cmd_onlyImmediate | cmd_skipDuringExec,                 0,0,    cmdPar_102,     cmdBlockNone},                  // executed AFTER execution phase ends
    {"clearProg",       cmdcod_clearProg,       cmd_onlyImmediate | cmd_skipDuringExec,                 0,0,    cmdPar_102,     cmdBlockNone},                  // executed AFTER execution phase ends

    // program and flow control commands
    // ---------------------------------
    {"loadProg",        cmdcod_loadProg,        cmd_onlyImmediate,                                      0,1,    cmdPar_106,     cmdBlockNone},

    {"program",         cmdcod_program,         cmd_onlyProgramTop | cmd_skipDuringExec,                1,1,    cmdPar_103,     cmdBlockNone},
    {"function",        cmdcod_function,        cmd_onlyInProgram | cmd_skipDuringExec,                 1,1,    cmdPar_108,     cmdBlockJustinaFunction},

    {"for",             cmdcod_for,             cmd_onlyImmOrInsideFuncBlock,                           1,3,    cmdPar_109,     cmdBlockFor},
    {"while",           cmdcod_while,           cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockWhile},
    {"if",              cmdcod_if,              cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockIf},
    {"elseif",          cmdcod_elseif,          cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockIf_elseIf},
    {"else",            cmdcod_else,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockIf_else},
    {"end",             cmdcod_end,             cmd_noRestrictions,                                     0,0,    cmdPar_102,     cmdBlockGenEnd},            // closes inner open command block

    {"break",           cmdcod_break,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockOpenBlock_loop},        // allowed if at least one open loop block (any level) 
    {"continue",        cmdcod_continue,        cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockOpenBlock_loop },       // allowed if at least one open loop block (any level) 
    {"return",          cmdcod_return,          cmd_onlyImmOrInsideFuncBlock,                           0,1,    cmdPar_106,     cmdBlockOpenBlock_function},    // allowed if currently an open function definition block 

    {"pause",           cmdcod_pause,           cmd_onlyImmOrInsideFuncBlock,                           0,1,    cmdPar_106,     cmdBlockNone},
    {"halt",            cmdcod_halt,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},

    // debugging commands
    // ------------------
    {"stop",            cmdcod_stop,            cmd_onlyInFunctionBlock,                                0,0,    cmdPar_102,     cmdBlockNone},
    {"nop",             cmdcod_nop,             cmd_onlyInFunctionBlock | cmd_skipDuringExec,           0,0,    cmdPar_102,     cmdBlockNone},                  // insert two bytes in program, do nothing

    {"go",              cmdcod_go,              cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"step",            cmdcod_step,            cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"stepOut",         cmdcod_stepOut,         cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"stepOver",        cmdcod_stepOver,        cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"bStepOut",        cmdcod_stepOutOfBlock,  cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"loop",            cmdcod_stepToBlockEnd,  cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"setNextLine",     cmdcod_setNextLine,     cmd_onlyImmediate,                                      1,1,    cmdPar_104,     cmdBlockNone},

    {"trace",           cmdcod_trace,           cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},

    {"abort",           cmdcod_abort,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"debug",           cmdcod_debug,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},

    {"BPon",            cmdcod_BPon,            cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"BPoff",           cmdcod_BPoff,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"debug",           cmdcod_debug,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"setBP",           cmdcod_setBP,           cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},
    {"clearBP",         cmdcod_clearBP,         cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},
    {"enableBP",        cmdcod_enableBP,        cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},
    {"disableBP",       cmdcod_disableBP,       cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},

    {"raiseError",      cmdcod_raiseError,      cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},
    {"trapErrors",      cmdcod_trapErrors,      cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},
    {"clearError",      cmdcod_clearError,      cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockNone},
    {"quit",            cmdcod_quit,            cmd_onlyImmOrInsideFuncBlock,                           0,1,    cmdPar_106,     cmdBlockNone},

    // settings
    // --------
    {"dispWidth",       cmdcod_dispwidth,       cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},
    {"floatFmt",        cmdcod_floatfmt,        cmd_onlyImmOrInsideFuncBlock,                           1,3,    cmdPar_112,     cmdBlockNone},
    {"intFmt",          cmdcod_intfmt,          cmd_onlyImmOrInsideFuncBlock,                           1,3,    cmdPar_112,     cmdBlockNone},
    {"dispMode",        cmdcod_dispmod,         cmd_onlyImmOrInsideFuncBlock,                           2,2,    cmdPar_105,     cmdBlockNone},
    {"tabSize",         cmdcod_tabSize,         cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},
    {"angleMode",       cmdcod_angle,           cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},

    // input and output commands
    // -------------------------

    {"setConsole",      cmdcod_setConsole,      cmd_onlyImmediate,                                      1,1,    cmdPar_104,     cmdBlockNone},
    {"setConsoleIn",    cmdcod_setConsIn,       cmd_onlyImmediate,                                      1,1,    cmdPar_104,     cmdBlockNone},
    {"setConsoleOut",   cmdcod_setConsOut,      cmd_onlyImmediate,                                      1,1,    cmdPar_104,     cmdBlockNone},
    {"setDebugOut",     cmdcod_setDebugOut,     cmd_onlyImmediate,                                      1,1,    cmdPar_104,     cmdBlockNone},

    {"info",            cmdcod_info,            cmd_onlyImmOrInsideFuncBlock,                           1,2,    cmdPar_114,     cmdBlockNone},
    {"input",           cmdcod_input,           cmd_onlyImmOrInsideFuncBlock,                           3,3,    cmdPar_113,     cmdBlockNone},

    {"startSD",         cmdcod_startSD,         cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},
    {"stopSD",          cmdcod_stopSD,          cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},

    {"receiveFile",     cmdcod_receiveFile,     cmd_onlyImmOrInsideFuncBlock,                           1,3,    cmdPar_112,     cmdBlockNone},
    {"sendFile",        cmdcod_sendFile,        cmd_onlyImmOrInsideFuncBlock,                           1,3,    cmdPar_112,     cmdBlockNone},
    {"copy",            cmdcod_copyFile,        cmd_onlyImmOrInsideFuncBlock,                           2,3,    cmdPar_107,     cmdBlockNone},

    {"dbout",           cmdcod_dbout,           cmd_onlyImmOrInsideFuncBlock,                           1,15,   cmdPar_112,     cmdBlockNone},      // values (expressions) to print to debug out
    {"dboutLine",       cmdcod_dboutLine,       cmd_onlyImmOrInsideFuncBlock,                           0,15,   cmdPar_107,     cmdBlockNone},

    {"cout",            cmdcod_cout,            cmd_onlyImmOrInsideFuncBlock,                           1,15,   cmdPar_112,     cmdBlockNone},      // values (expressions) to print to console
    {"coutLine",        cmdcod_coutLine,        cmd_onlyImmOrInsideFuncBlock,                           0,15,   cmdPar_107,     cmdBlockNone},
    {"coutList",        cmdcod_coutList,        cmd_onlyImmOrInsideFuncBlock,                           1,15,   cmdPar_112,     cmdBlockNone},

    {"print",           cmdcod_print,           cmd_onlyImmOrInsideFuncBlock,                           2,16,   cmdPar_116,     cmdBlockNone},      // stream, values (expressions) to print to stream
    {"printLine",       cmdcod_printLine,       cmd_onlyImmOrInsideFuncBlock,                           1,16,   cmdPar_112,     cmdBlockNone},
    {"printList",       cmdcod_printList,       cmd_onlyImmOrInsideFuncBlock,                           2,16,   cmdPar_116,     cmdBlockNone},

    {"vprint",          cmdcod_printToVar,      cmd_onlyImmOrInsideFuncBlock,                           2,16,   cmdPar_116,     cmdBlockNone},      // variable, values (expressions) to print to variable
    {"vprintLine",      cmdcod_printLineToVar,  cmd_onlyImmOrInsideFuncBlock,                           1,16,   cmdPar_112,     cmdBlockNone},
    {"vprintList",      cmdcod_printListToVar,  cmd_onlyImmOrInsideFuncBlock,                           2,16,   cmdPar_116,     cmdBlockNone},

    {"listCallStack",   cmdcod_printCallSt,     cmd_onlyImmOrInsideFuncBlock,                           0,1,    cmdPar_106,     cmdBlockNone},      // print call stack to stream (default is console)
    {"listBP",          cmdcod_printBP,         cmd_onlyImmOrInsideFuncBlock,                           0,1,    cmdPar_106,     cmdBlockNone},      // list breakpoints
    {"listVars",        cmdcod_printVars,       cmd_onlyImmOrInsideFuncBlock,                           0,1,    cmdPar_106,     cmdBlockNone},      // list variables "         "         "         "
    {"listFiles",       cmdcod_listFiles,       cmd_onlyImmOrInsideFuncBlock,                           0,1,    cmdPar_106,     cmdBlockNone},      // list files     "         "         "         "
    {"listFilesToSerial",cmdcod_listFilesToSer, cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},      // list files to Serial with modification dates (SD library fixed)
};


// internal cpp Justina functions: returning a result
// --------------------------------------------------

// the 8 array pattern bits indicate the order of arrays and scalars; bit b0 to bit b7 refer to parameter 1 to 8, if a bit is set, an array is expected as argument
// if more than 8 arguments are supplied, only arguments 1 to 8 can be set as array arguments
// maximum number of parameters should be no more than 16

const Justina_interpreter::InternCppFuncDef Justina_interpreter::_internCppFunctions[]{
    //  name                    id code                         #arg    array pattern
    //  ----                    -------                         ----    -------------   

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
    {"signBit",                 fnccod_sign,                    1,1,    0b0},
    {"fmod",                    fnccod_fmod,                    2,2,    0b0},

    // lookup functions
    {"ifte",                    fnccod_ifte,                    3,15,   0b0},
    {"switch",                  fnccod_switch,                  3,15,   0b0},
    {"index",                   fnccod_index,                   3,15,   0b0},
    {"choose",                  fnccod_choose,                  3,15,   0b0},

    // conversion functions
    {"cInt",                    fnccod_cint,                    1,1,    0b0},
    {"cFloat",                  fnccod_cfloat,                  1,1,    0b0},
    {"cStr",                    fnccod_cstr,                    1,1,    0b0},

    // Arduino digital I/O, timing and other functions
    {"millis",                  fnccod_millis,                  0,0,    0b0},
    {"micros",                  fnccod_micros,                  0,0,    0b0},
    {"delay",                   fnccod_delay,                   1,1,    0b0},               // delay microseconds: doesn't make sense, because execution is not fast enough (interpreter)
    {"digitalRead",             fnccod_digitalRead,             1,1,    0b0},
    {"digitalWrite",            fnccod_digitalWrite,            2,2,    0b0},
    {"pinMode",                 fnccod_pinMode,                 2,2,    0b0},
    {"analogRead",              fnccod_analogRead,              1,1,    0b0},
#if !defined(ARDUINO_ARCH_RP2040)                                                           // Arduino RP2040: prevent linker error
    {"analogReference",         fnccod_analogReference,         1,1,    0b0},
#endif
    {"analogWrite",             fnccod_analogWrite,             2,2,    0b0},
    {"analogReadResolution",    fnccod_analogReadResolution,    1,1,    0b0},
    {"analogWriteResolution",   fnccod_analogWriteResolution,   1,1,    0b0},
    {"noTone",                  fnccod_noTone,                  1,1,    0b0},
    {"tone",                    fnccod_tone,                    2,3,    0b0},
    {"pulseIn",                 fnccod_pulseIn,                 2,3,    0b0},
    {"shiftIn",                 fnccod_shiftIn,                 3,3,    0b0},
    {"shiftOut",                fnccod_shiftOut,                4,4,    0b0},
    {"random",                  fnccod_random,                  1,2,    0b0},
    {"randomSeed",              fnccod_randomSeed,              1,1,    0b0},

    // Arduino bit and byte manipulation functions
    {"bit",                     fnccod_bit,                     1,1,    0b0},
    {"bitRead",                 fnccod_bitRead,                 2,2,    0b0},
    {"bitClear",                fnccod_bitClear,                2,2,    0b0},
    {"bitSet",                  fnccod_bitSet,                  2,2,    0b0},
    {"bitWrite",                fnccod_bitWrite,                3,3,    0b0},
    {"byteRead",                fnccod_byteRead,                2,2,    0b0},
    {"byteWrite",               fnccod_byteWrite,               3,3,    0b0},
    {"maskedWordBitRead",       fnccod_wordMaskedRead,          2,2,    0b0},
    {"maskedWordClear",         fnccod_wordMaskedClear,         2,2,    0b0},
    {"maskedWordSet",           fnccod_wordMaskedSet,           2,2,    0b0},
    {"maskedWordWrite",         fnccod_wordMaskedWrite,         3,3,    0b0},

    {"mem32Read",               fnccod_mem32Read,               1,1,    0b0},
    {"mem32Write",              fnccod_mem32Write,              2,2,    0b0},
    {"mem8Read",                fnccod_mem8Read,                2,2,    0b0},
    {"mem8Write",               fnccod_mem8Write,               3,3,    0b0},

    // string and 'character' functions
    {"char",                    fnccod_char,                    1,1,    0b0},
    {"len",                     fnccod_len,                     1,1,    0b0},
    {"line",                    fnccod_nl,                      0,0,    0b0},
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
    {"findInStr",               fnccod_findsubstr,              2,3,    0b0},
    {"substInStr",              fnccod_replacesubstr,           3,4,    0b0},
    {"strCmp",                  fnccod_strcmp,                  2,2,    0b0},
    {"strCaseCmp",              fnccod_strcasecmp,              2,2,    0b0},
    {"strToHexStr",             fnccod_strhex,                  1,1,    0b0},
    {"quote",                   fnccod_quote,                   1,1,    0b0},

    {"isAlpha",                 fnccod_isAlpha,                 1,2,    0b0},
    {"isAlphaNumeric",          fnccod_isAlphaNumeric,          1,2,    0b0},
    {"isDigit",                 fnccod_isDigit,                 1,2,    0b0},
    {"isHexDigit",              fnccod_isHexadecimalDigit,      1,2,    0b0},
    {"isControl",               fnccod_isControl,               1,2,    0b0},
    {"isGraph",                 fnccod_isGraph,                 1,2,    0b0},
    {"isPrintable",             fnccod_isPrintable,             1,2,    0b0},
    {"isPunct",                 fnccod_isPunct,                 1,2,    0b0},
    {"isWhitespace",            fnccod_isWhitespace,            1,2,    0b0},
    {"isAscii",                 fnccod_isAscii,                 1,2,    0b0},
    {"isLowerCase",             fnccod_isLowerCase,             1,2,    0b0},
    {"isUpperCase",             fnccod_isUpperCase,             1,2,    0b0},

    // other functions
    {"eval",                    fnccod_eval,                    1,1,    0b0},
    {"ubound",                  fnccod_ubound,                  2,2,    0b00000001},        // first parameter is array (LSB)
    {"dims",                    fnccod_dims,                    1,1,    0b00000001},
    {"type",                    fnccod_valueType,               1,1,    0b0},
    { "r",                      fnccod_last,                    0,1,    0b0 },               // short label for 'last result'
    { "err",                    fnccod_getTrappedErr,           0,0,    0b0 },
    {"sysval",                  fnccod_sysVal,                  1,1,    0b0},

    // input and output functions
    {"cin",                     fnccod_cin,                    0,2,    0b0 },
    {"cinLine",                 fnccod_cinLine,                0,0,    0b0 },
    {"cinList",                 fnccod_cinParseList,           1,15,   0b0 },
    {"read",                    fnccod_read,                   1,3,    0b0 },
    {"readLine",                fnccod_readLine,               1,1,    0b0 },
    {"readList",                fnccod_parseList,              2,16,   0b0 },

    {"vreadList",               fnccod_parseListFromVar,       2,16,   0b0 },

    {"find",                    fnccod_find,                   2,2,    0b0 },
    {"findUntil",               fnccod_findUntil,              3,3,    0b0 },
    {"peek",                    fnccod_peek,                   0,1,    0b0 },
    {"available",               fnccod_available,              0,1,    0b0 },
    {"flush",                   fnccod_flush,                  1,1,    0b0 },
    {"setTimeout",              fnccod_setTimeout,             2,2,    0b0 },
    {"getTimeout",              fnccod_getTimeout,             1,1,    0b0 },
    {"availableForWrite",       fnccod_availableForWrite,      1,1,    0b0 },
    {"getWriteError",           fnccod_getWriteError,          1,1,    0b0 },
    {"clearWriteError",         fnccod_clearWriteError,        1,1,    0b0 },

    {"fmt",                     fnccod_format,                  1,6,    0b0},               // short label for 'system value'
    {"tab",                     fnccod_tab,                     0,1,    0b0},
    {"col",                     fnccod_gotoColumn,              1,1,    0b0},
    {"pos",                     fnccod_getColumnPos,            0,0,    0b0},

    // SD card only (based upon Arduino SD card library functions)
    {"open",                    fnccod_open,                   1,2,    0b0 },
    {"close",                   fnccod_close,                  1,1,    0b0 },
    {"position",                fnccod_position,               1,1,    0b0 },
    {"size",                    fnccod_size,                   1,1,    0b0 },
    {"seek",                    fnccod_seek,                   2,2,    0b0 },
    {"name",                    fnccod_name,                   1,1,    0b0 },
    {"fullName",                fnccod_fullName,               1,1,    0b0 },
    {"isDirectory",             fnccod_isDirectory,            1,1,    0b0 },
    {"rewindDirectory",         fnccod_rewindDirectory,        1,1,    0b0 },
    {"openNext",                fnccod_openNextFile,           1,2,    0b0 },
    {"exists",                  fnccod_exists,                 1,1,    0b0 },
    {"createDirectory",         fnccod_mkdir,                  1,1,    0b0 },
    {"removeDirectory",         fnccod_rmdir,                  1,1,    0b0 },
    {"remove",                  fnccod_remove,                 1,1,    0b0 },
    {"fileNum",                 fnccod_fileNumber,             1,1,    0b0 },
    {"isInUse",                 fnccod_isOpenFile,             1,1,    0b0 },
    {"closeAll",                fnccod_closeAll,               0,0,    0b0 },
};


// terminal tokens 
// ---------------

// priority: bits b43210 define priority if used as prefix, infix, postfix operator, respectively (0x1 = lowest, 0x1F = highest) 
// priority 0 means operator not available for use as use as postfix, prefix, infix operator
// bit b7 defines associativity for infix operators (bit set indicates 'right-to-left').
// prefix operators: always right-to-left. postfix operators: always left-to-right
// NOTE: !!!!! table entries with names starting with same characters: shortest entries should come BEFORE longest (e.g. '!' before '!=', '&' before '&&') !!!!!
// postfix operator names can only be shared with prefix operator names

const Justina_interpreter::TerminalDef Justina_interpreter::_terminals[]{

    //  name                id code                 prefix prio          infix prio                 postfix prio         
    //  ----                -------                 -----------          ----------                 ------------   

    // non-operator terminals: ONE character only, character should NOT appear in operator names

    {term_semicolon,        termcod_semicolon_BPset,    0x00,               0x00,                       0x00},
    {term_semicolon,        termcod_semicolon_BPallowed,0x00,               0x00,                       0x00},
    {term_semicolon,        termcod_semicolon,          0x00,               0x00,                       0x00},      // MUST follow previous entries
    {term_comma,            termcod_comma,              0x00,               0x00,                       0x00},
    {term_leftPar,          termcod_leftPar,            0x00,               0x10,                       0x00},
    {term_rightPar,         termcod_rightPar,           0x00,               0x00,                       0x00},

    // operators (0x00 -> operator not available, 0x01 -> pure or compound assignment)
    // op_long: operands must be long, a long is returned (e.g. 'bitand' operator)
    // res_long: operands can be float or long, a long is returned (e.g. 'and' operator)
    // op_RtoL: operator has right-to-left associativity
    // prefix operators: always right-to-left associativity; not added to the operator definition table below

    // assignment operator: ONE character only, character should NOT appear in any other operator name, except compound operator names (but NOT as first character)
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


// symbolic constants
// -------------------

// these symbolic names can be used in Justina programs instead of the values themselves

const Justina_interpreter::SymbNumConsts Justina_interpreter::_symbNumConsts[]{

    // name                 // value                    // value type
    // ----                 --------                    // ----------

    // math: floating point constants (with more precision than what will actually be used)
    {"e",                   "2.7182818284590452354",    value_isFloat},     // base of natural logarithm
    {"PI",                  "3.14159265358979323846",   value_isFloat},     // PI
    {"HALF_PI",             "1.57079632679489661923",   value_isFloat},     // PI / 2
    {"QUART_PI",            "0.78539816339744830962",   value_isFloat},     // PI / 4
    {"TWO_PI",              "6.2831853071795864769",    value_isFloat},      // 2 * PI 

    {"DEG_TO_RAD",          "0.01745329251994329577",   value_isFloat},     // conversion factor: degrees to radians
    {"RAD_TO_DEG",          "57.2957795130823208768",   value_isFloat},     // radians to degrrees

    // angle mode
    {"DEGREES",             "0",                        value_isLong},
    {"RADIANS",             "1",                        value_isLong},

    // boolean values
    {"FALSE",               "0",                        value_isLong},      // value for boolean 'false'
    {"TRUE",                "1",                        value_isLong},      // value for boolean 'true'

    // data types
    {"LONG_TYP",            "1",                        value_isLong},      // value type of a long value
    {"FLOAT_TYP",           "2",                        value_isLong},      // value type of a float value
    {"STRING_TYP",          "3",                        value_isLong},      // value type of a string value

    // digital I/O
    {"LOW",                 "0",                        value_isLong},      // standard ARduino constants for digital I/O
    {"HIGH",                "1",                        value_isLong},
    {"INPUT",               "0x0",                      value_isLong},      // standard ARduino constants for digital I/O
    {"OUTPUT",              "0x1",                      value_isLong},
    {"INPUT_PULLUP",        "0x2",                      value_isLong},
    {"INPUT_PULLDOWN",      "0x3",                      value_isLong},
    {"LSBFIRST",            "0x0",                      value_isLong},      // standard ARduino constants for digital I/O
    {"MSBFIRST",            "0x1",                      value_isLong},

    // display mode command first argument: prompt and echo display
    {"NO_PROMPT",           "0",                        value_isLong},      // do not print prompt and do not echo user input
    {"PROMPT",              "1",                        value_isLong},      // print prompt but no not echo user input
    {"ECHO",                "2",                        value_isLong},      // print prompt and echo user input

    // display mode command second argument: last result format
    {"NO_LAST",             "0",                        value_isLong},      // do not print last result
    {"PRINT_LAST",          "1",                        value_isLong},      // print last result
    {"QUOTE_LAST",          "2",                        value_isLong},      // print last result, quote string results 

    // info command: type of confirmation required (argument 2, must be a variable)
    {"CONF_ENTER",          "0",                        value_isLong},      // confirmation required by pressing ENTER (any preceding characters are skipped)
    {"CONF_ENT_CANC",       "1",                        value_isLong},      // idem, but if '\c' encountered in input stream the operation is canceled by user 
    {"CONF_YN",             "2",                        value_isLong},      // only yes or no answer allowed, by pressing 'y' or 'n' followed by ENTER   
    {"CONF_YN_CANC",        "3",                        value_isLong},      // idem, but if '\c' encountered in input stream the operation is canceled by user 

    // input command: default allowed  
    {"INP_NO_DEF",          "0",                        value_isLong},      // '\d' sequences ('default') in the input stream are ignored
    {"INP_ALLOW_DEF",       "1",                        value_isLong},      // if '\d' sequence is encountered in the input stream, default value is returned

    // input and info command: flag 'user canceled' (input argument 3 / info argument 2 return value - argument must be a variable)
    {"USR_CANCELED",        "0",                        value_isLong},      // operation was canceled by user (\c sequence encountered)
    {"USR_SUCCESS",         "1",                        value_isLong},      // operation was NOT canceled by user

    // quit command
    {"QUIT_KEEP_",          "0",                        value_isLong},      // keep Justina in memory on quitting
    {"QUIT_RELEASE",        "1",                        value_isLong},      // release memory on quitting

    // input / output streams
    {"CONSOLE",             "0",                        value_isLong},      // IO: read from / print to console
    {"IO_1",                "-1",                       value_isLong},      // IO: read from / print to alternative I/O port 1 (if defined)
    {"IO_2",                "-2",                       value_isLong},      // IO: read from / print to alternative I/O port 2 (if defined)
    {"IO_3",                "-3",                       value_isLong},      // IO: read from / print to alternative I/O port 3 (if defined)
    {"IO_4",                "-4",                       value_isLong},      // IO: read from / print to alternative I/O port 4 (if defined)
    {"IO_5",                "-5",                       value_isLong},      // IO: read from / print to alternative I/O port 5 (if defined)
    {"FILE_1",              "1",                        value_isLong},      // IO: read from / print to open SD file 1
    {"FILE_2",              "2",                        value_isLong},      // IO: read from / print to open SD file 2 
    {"FILE_3",              "3",                        value_isLong},      // IO: read from / print to open SD file 3 
    {"FILE_4",              "4",                        value_isLong},      // IO: read from / print to open SD file 4 
    {"FILE_5",              "5",                        value_isLong},      // IO: read from / print to open SD file 5 

    // file access type on open
    {"READ",                "0x1",                      value_isLong},      // open SD file for read access
    {"WRITE",               "0x2",                      value_isLong},      // open SD file for write access
    {"RDWR",                "0x3",                      value_isLong},      // open SD file for r/w access
    {"APPEND",              "0x4",                      value_isLong},      // writes will occur at end of file
    {"SYNC",                "0x8",                      value_isLong},      //  
    {"NEW_OK",              "0x10",                     value_isLong},      // creating new files if non-existent is allowed, open existing files
    {"EXCL",                "0x20",                     value_isLong},      // "exclusive": use with 'NEW_OK' to create new files 'exclusively' 
    {"NEW_ONLY",            "0x30",                     value_isLong},      // create new file only - do not open an existing file
    {"TRUNC",               "0x40",                     value_isLong},      // truncate file to zero bytes on open (NOT if file is opened for read access only)

    {"RW_APP",              "0x07",                     value_isLong},      // open for read write access; writes at the end

    // formatting flags
    {"FMT_LEFT",            "0x01",                     value_isLong},      // align output left within the print field 
    {"FMT_SIGN",            "0x02",                     value_isLong},      // always add a sign (- or +) preceding the value
    {"FMT_SPACE",           "0x04",                     value_isLong},      // precede the value with a space if no sign is written 
    {"FMT_FPSEP",           "0x08",                     value_isLong},      // if used with 'F', 'E', 'G' specifiers: add decimal point, even if no digits after decimal point  
    {"FMT_0X",              "0x08",                     value_isLong},      // if used with 'hex output'X' specifier: precede non-zero values with 0x  
    {"FMT_PAD0",            "0x10",                     value_isLong},      // if used with 'F', 'E', 'G' specifiers: pad with zeros 
    {"FMT_NONE",            "0x00",                     value_isLong},      // no flags 
};


// -------------------
// *   constructor   *
// -------------------

Justina_interpreter::Justina_interpreter(Stream** const pAltInputStreams, int altIOstreamCount,
    long progMemSize, int JustinaConstraints, int SDcardChipSelectPin) :
    _pExternIOstreams(pAltInputStreams), _externIOstreamCount(altIOstreamCount), _progMemorySize(progMemSize), _justinaConstraints(JustinaConstraints), _SDcardChipSelectPin(SDcardChipSelectPin)

{

    // settings to be initialized when cold starting interpreter only
    // --------------------------------------------------------------

    _coldStart = true;

    _housekeepingCallback = nullptr;

    _lastPrintedIsPrompt = false;

    _programMode = false;
    _currenttime = millis();
    _previousTime = _currenttime;
    _lastCallBackTime = _currenttime;

    parsingStack.setListName("parsing ");
    evalStack.setListName("eval    ");
    flowCtrlStack.setListName("flowCtrl");
    parsedCommandLineStack.setListName("cmd line");

    // current print column is maintened for each stream separately: init
    _pIOprintColumns = new int[_externIOstreamCount];
    for (int i = 0; i < _externIOstreamCount; i++) {
        _pExternIOstreams[i]->setTimeout(DEFAULT_READ_TIMEOUT);                                         // NOTE: will only have effect for existing connections (e.g. TCP)
        _pIOprintColumns[i] = 0;
    }

    // by default, console and debug out are first element in _pExternIOstreams[]
    _consoleIn_sourceStreamNumber = _consoleOut_sourceStreamNumber = _debug_sourceStreamNumber = -1;
    _pConsoleIn = _pConsoleOut = _pDebugOut = _pExternIOstreams[0];
    _pConsolePrintColumn = _pDebugPrintColumn = _pIOprintColumns;                                       //  point to its current print column
    _pLastPrintColumn = _pIOprintColumns;

    // set linked list debug printing. Pointer to debug out stream pointer: will follow if debug stream is changed
    parsingStack.setDebugOutStream(static_cast<Stream**> (&_pDebugOut));                                // for debug printing within linked list object

    if (_progMemorySize + IMM_MEM_SIZE > pow(2, 16)) { _progMemorySize = pow(2, 16) - IMM_MEM_SIZE; }
    _programStorage = new char[_progMemorySize + IMM_MEM_SIZE];

#if PRINT_HEAP_OBJ_CREA_DEL
    /*temp*/Serial.print("+++++ (ext IO streams) "); /*temp*/Serial.println((uint32_t)_pIOprintColumns, HEX);
    /*temp*/Serial.print("+++++ (program memory) "); /*temp*/Serial.println((uint32_t)_programStorage, HEX);
#endif

    initInterpreterVariables(true);
};


// ------------------
// *   destructor   *
// ------------------
/*
Justina_interpreter::~Justina_interpreter() {
};
*/

// --------------------------------------------
// *   set system (main) call back functons   *
// --------------------------------------------

bool Justina_interpreter::setMainLoopCallback(void (*func)(long& appFlags)) {

    // this function is directly called from the Arduino program starting Justina
    // it stores the address of an optional 'user callback' function
    // Justina will call this user routine at specific time intervals, allowing  the user...
    // ...to execute a specific routine regularly (e.g. to maintain a TCP connection, to implement a heartbeat, ...)

    _housekeepingCallback = func;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// *   sets pointers to the locations where the Arduino program stored information about user-defined (external) cpp functions (user callback functions)   *
// ---------------------------------------------------------------------------------------------------------------------------------------------------------

    // these functions are directly called from the Arduino program starting Justina
    // each function stores the starting address of an array with information about external (user callback) functions with a specific return type 
    // for instance, _pExtCppFunctions[0] stores the adress of the array containing information about cpp functions returning a boolean value
    // a null pointer indicates there are no functions of a specific type
    // return types are: 0 = bool, 1 = char, 2 = int, 3 = long, 4 = float, 5 = char*, 6 = void (but returns zero to Justina)

bool Justina_interpreter::setUserBoolCppFunctionsEntryPoint(const CppBoolFunction* const  pCppBoolFunctions, int cppBoolFunctionCount) {
    _pExtCppFunctions[0] = (CppBoolFunction*)pCppBoolFunctions;
    _ExtCppFunctionCounts[0] = cppBoolFunctionCount;
};

char Justina_interpreter::setUserCharCppFunctionsEntryPoint(const CppCharFunction* const  pCppCharFunctions, int cppCharFunctionCount) {
    _pExtCppFunctions[1] = (CppCharFunction*)pCppCharFunctions;
    _ExtCppFunctionCounts[1] = cppCharFunctionCount;
};

int Justina_interpreter::setUserIntCppFunctionsEntryPoint(const CppIntFunction* const  pCppIntFunctions, int cppIntFunctionCount) {
    _pExtCppFunctions[2] = (CppIntFunction*)pCppIntFunctions;
    _ExtCppFunctionCounts[2] = cppIntFunctionCount;
};

long Justina_interpreter::setUserLongCppFunctionsEntryPoint(const CppLongFunction* const pCppLongFunctions, int cppLongFunctionCount) {
    _pExtCppFunctions[3] = (CppLongFunction*)pCppLongFunctions;
    _ExtCppFunctionCounts[3] = cppLongFunctionCount;
};

float Justina_interpreter::setUserFloatCppFunctionsEntryPoint(const CppFloatFunction* const pCppFloatFunctions, int cppFloatFunctionCount) {
    _pExtCppFunctions[4] = (CppFloatFunction*)pCppFloatFunctions;
    _ExtCppFunctionCounts[4] = cppFloatFunctionCount;
};

char* Justina_interpreter::setUser_pCharCppFunctionsEntryPoint(const Cpp_pCharFunction* const pCpp_pCharFunctions, int cpp_pCharFunctionCount) {
    _pExtCppFunctions[5] = (Cpp_pCharFunction*)pCpp_pCharFunctions;
    _ExtCppFunctionCounts[5] = cpp_pCharFunctionCount;
};

void Justina_interpreter::setUserCppCommandsEntryPoint(const CppVoidFunction* const pCppVoidFunctions, int cppVoidFunctionCount) {
    _pExtCppFunctions[6] = (CppVoidFunction*)pCppVoidFunctions;
    _ExtCppFunctionCounts[6] = cppVoidFunctionCount;
};

// ----------------------------
// *   interpreter main loop   *
// ----------------------------

bool Justina_interpreter::run() {
    bool withinStringEscSequence{ false };
    bool lastCharWasSemiColon{ false };
    bool within1LineComment{ false };
    bool withinMultiLineComment{ false };
    bool withinString{ false };
    bool redundantSemiColon = false;

    static bool flushAllUntilEOF{ false };

    int lineCount{ 0 };
    long parsedStatementCount{ 0 };
    int statementCharCount{ 0 };
    char* pErrorPos{};
    parsingResult_type result{ result_parsing_OK };                                                          // init

    // variables for maintaining lines allowing breakpoints
    bool parsedStatementStartsOnNewLine{ false };
    bool parsedStatementStartLinesAdjacent{ false };
    long statementStartsAtLine{ 0 };
    long parsedStatementStartsAtLine{ 0 };
    long BPstartLine{ 0 }, BPendLine{ 0 };

    static long BPpreviousEndLine{ 0 };

    _appFlags = 0x0000L;                                                                                        // init application flags (for communication with Justina caller, using callbacks)

    printlnTo(0);
    for (int i = 0; i < 13; i++) { /*temp*/Serial.print("*"); } /*temp*/Serial.print("____");
    for (int i = 0; i < 4; i++) { /*temp*/Serial.print("*"); } /*temp*/Serial.print("__");
    for (int i = 0; i < 14; i++) { /*temp*/Serial.print("*"); } /*temp*/Serial.print("_");
    for (int i = 0; i < 10; i++) { /*temp*/Serial.print("*"); }printlnTo(0);

    /*temp*/Serial.print("    "); /*temp*/Serial.println( J_productName);
    /*temp*/Serial.print("    "); /*temp*/Serial.println( J_legalCopyright);
    /*temp*/Serial.print("    Version: "); /*temp*/Serial.print(J_productVersion); /*temp*/Serial.print(" ("); /*temp*/Serial.print(J_buildDate); /*temp*/Serial.println( ")");
    for (int i = 0; i < 48; i++) { /*temp*/Serial.print("*"); } printlnTo(0);

    // find token index for terminal token 'semicolon with breakpoint allowed' 
    int index{}, semicolonBPallowed_index{}, semicolonBPset_index{}, matches{};

    for (index = _termTokenCount - 1, matches = 0; index >= 0; index--) {      // for all defined terminals
        if (_terminals[index].terminalCode == termcod_semicolon_BPallowed) { semicolonBPallowed_index = index; matches; }                              // token corresponds to terminal code ? Then exit loop    
        if (_terminals[index].terminalCode == termcod_semicolon_BPset) { semicolonBPset_index = index; matches++; }                              // token corresponds to terminal code ? Then exit loop    
        if (matches == 2) { break; }
    }
    _semicolonBPallowed_token = (semicolonBPallowed_index <= 0x0F) ? tok_isTerminalGroup1 : (semicolonBPallowed_index <= 0x1F) ? tok_isTerminalGroup2 : tok_isTerminalGroup3;
    _semicolonBPallowed_token |= ((semicolonBPallowed_index & 0x0F) << 4);
    _semicolonBPset_token = (semicolonBPset_index <= 0x0F) ? tok_isTerminalGroup1 : (semicolonBPset_index <= 0x1F) ? tok_isTerminalGroup2 : tok_isTerminalGroup3;
    _semicolonBPset_token |= ((semicolonBPset_index & 0x0F) << 4);

    _programMode = false;
    _programCounter = _programStorage + _progMemorySize;
    *(_programStorage + _progMemorySize) = tok_no_token;                                                        //  current end of program (FIRST byte of immediate mode command line)
    _lastPrintedIsPrompt = false;

    _coldStart = false;                                                                                         // can be used if needed in this procedure, to determine whether this was a cold or warm start

    Stream* pStatementInputStream = static_cast<Stream*>(_pConsoleIn);                                          // init: load program from console
    int streamNumber{ 0 };
    setStream(0);                                                                                               // set _pStreamIn to console, for use by Justina methods

    int clearCmdIndicator{ 0 };                                                                                 // 1 = clear program cmd, 2 = clear all cmd
    char c{};
    bool kill{ false };
    bool loadingStartupProgram{ false }, launchingStartFunction{ false };
    bool startJustinaWithoutAutostart{ true };

    // initialise SD card now ?
    // 0 = no card reader, 1 = card reader present, do not yet initialise, 2 = initialise card now, 3 = init card & run start.jus function start() now
    if ((_justinaConstraints & 0b0011) >= 2) {
        /*temp*/Serial.print("\r\nLooking for an SD card...\r\n");
        execResult_type execResult = startSD();
        /*temp*/Serial.print(_SDinitOK ? "SD card found\r\n" : "SD card error: SD card NOT found\r\n");
    }

    if ((_justinaConstraints & 0b0011) == 3) {
        // open startup file and retrieve file number (which would be one, normally)
        _initiateProgramLoad = _SDinitOK;
        if (_initiateProgramLoad) {
            /*temp*/Serial.println( "Looking for 'start.jus' program file...");
            if (!SD.exists("start.jus")) { _initiateProgramLoad = false; /*temp*/Serial.println( "'start.jus' program NOT found"); }
        }

        if (_initiateProgramLoad) {
            execResult_type execResult = SD_open(_loadProgFromStreamNo, "start.jus", O_READ);                   // this performs a few card & file checks as well
            _initiateProgramLoad = (execResult == result_execOK);
            if (!_initiateProgramLoad) { /*temp*/Serial.print("Could not open 'start.jus' program - error "); /*temp*/Serial.println( execResult); }
        }

        if (_initiateProgramLoad) {                                                                             // !!! second 'if(_initiateProgramLoad)'
            resetMachine(false);                                                                                // if 'warm' start, previous program (with its variables) may still exist
            _programMode = true;
            _programCounter = _programStorage;
            loadingStartupProgram = true;
            startJustinaWithoutAutostart = false;

            parsedStatementStartsOnNewLine = false;
            parsedStatementStartLinesAdjacent = false;
            statementStartsAtLine = 0;
            parsedStatementStartsAtLine = 0;
            BPstartLine = 0;
            BPendLine = 0;
            BPpreviousEndLine = 0;

            streamNumber = _loadProgFromStreamNo;                                                               // autostart step 1: temporarily switch from console input to startup file (opening the file here) 
            setStream(streamNumber, pStatementInputStream);                                                     // error checking done while opening file
            /*temp*/Serial.print("Loading program 'start.jus'...\r\n");
        }
    }

    parsedStatementCount = 0;
    do {
        // when loading a program, as soon as first printable character of a PROGRAM is read, each subsequent character needs to follow after the previous one within a fixed time delay, handled by getCharacter().
        // program reading ends when no character is read within this time window.
        // when processing immediate mode statements (single line), reading ends when a New Line terminating character is received
        bool programCharsReceived = _programMode && !_initiateProgramLoad;                                      // _initiateProgramLoad is set during execution of the command to read a program source file from the console
        bool waitForFirstProgramCharacter = _initiateProgramLoad;

        // get a character if available and perform a regular housekeeping callback as well
        // NOTE: forcedStop is a  dummy argument here (no program is running)
        bool quitNow{ false }, forcedStop{ false }, forcedAbort{ false }, stdConsole{ false };                  // kill is true: request from caller, kill is false: quit command executed
        bool bufferOverrun{ false };                                                                            // buffer where statement characters are assembled for parsing
        bool noCharAdded{ false };
        bool allCharsReceived{ false };

        long currentSourceLine{ 0 };

        _initiateProgramLoad = false;

        if (startJustinaWithoutAutostart) { allCharsReceived = true; startJustinaWithoutAutostart = false; }
        else if (launchingStartFunction) {                                                                      // autostart step 2: launch function
            strcpy(_statement, "start()");                                                                      // do not read from console; instead insert characters here
            statementCharCount = strlen(_statement);
            allCharsReceived = true;                                                                            // ready for parsing
            launchingStartFunction = false;                                                                     // nothing to prepare any more
        }
        else {     // note: while waiting for first program character, allow a longer time out              
            c = getCharacter(kill, forcedStop, forcedAbort, stdConsole, true, waitForFirstProgramCharacter);    // forced stop has no effect here
            if (kill) { break; }
            // start processing input buffer when (1) in program mode: time out occurs and at least one character received, or (2) in immediate mode: when a new line character is detected
            allCharsReceived = _programMode ? ((c == 0xFF) && programCharsReceived) : (c == '\n');              // programCharsReceived: at least one program character received
            if ((c == 0xFF) && !allCharsReceived && !forcedAbort && !stdConsole) { continue; }                  // no character: keep waiting for input (except when program or imm. mode line is read)

            // if no character added: nothing to do, wait for next
            noCharAdded = !addCharacterToInput(lastCharWasSemiColon, withinString, withinStringEscSequence, within1LineComment, withinMultiLineComment, redundantSemiColon, allCharsReceived,
                bufferOverrun, flushAllUntilEOF, lineCount, statementCharCount, c);
            currentSourceLine = lineCount + 1;
        }

        do {        // one loop only
            if (bufferOverrun) { result = result_statementTooLong; }
            if (kill) { quitNow = true;  result = result_parse_kill; break; }
            if (forcedAbort) { result = result_parse_abort; }
            if (stdConsole && !_programMode) { result = result_parse_setStdConsole; }
            if (noCharAdded) { break; }               // start a new outer loop (read a character if available, etc.)


            // if a statement is complete (terminated by a semicolon or end of input), maintain breakpoint line ranges and parse statement
            // ---------------------------------------------------------------------------------------------------------------------------
            bool isStatementSeparator = (!withinString) && (!within1LineComment) && (!withinMultiLineComment) && (c == term_semicolon[0]) && !redundantSemiColon;
            isStatementSeparator = isStatementSeparator || (withinString && (c == '\n'));  // a new line character within a string is sent to parser as well

            bool statementReadyForParsing = !bufferOverrun && !forcedAbort && !stdConsole && !kill && (isStatementSeparator || (allCharsReceived && (statementCharCount > 0)));

            if (_programMode && (statementCharCount == 1) && !noCharAdded) { statementStartsAtLine = currentSourceLine; }                                    // first character of new statement

            if (statementReadyForParsing) {                                                                     // if quitting anyway, just skip                                               

                _appFlags &= ~appFlag_errorConditionBit;                                                        // clear error condition flag 
                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_parsing;                                // status 'parsing'
                _statement[statementCharCount] = '\0';                                                          // add string terminator

                char* pStatement = _statement;                                                                  // because passed by reference 
                char* pDummy{};
                _parsingExecutingTraceString = false;                       // init
                _parsingExecutingTriggerString = false;
                _parsingEvalString = false;

                // The user can set breakpoints for source lines having at least one statement starting on that line (given that the statement is not 'parsing only').
                // Procedure 'collectSourceLineRangePairs' stores necessary data to enable this functionality.
                ////result = _pBreakpoints->collectSourceLineRangePairs(_semicolonBPallowed_token, parsedStatementStartsOnNewLine, parsedStatementStartLinesAdjacent, statementStartsAtLine, parsedStatementStartsAtLine,
                ////    BPstartLine, BPendLine, BPpreviousEndLine);
                result = result_parsing_OK; //// temp
                if (result == result_parsing_OK) { result = parseStatement(pStatement, pDummy, clearCmdIndicator); }       // parse ONE statement only 

                if ((++parsedStatementCount & 0x3f) == 0) {
                    /*temp*/Serial.print('.');                                                                            // print a dot each 64 parsed lines
                    if ((parsedStatementCount & 0x0fff) == 0) { printlnTo(0); }                                 // print a crlf each 64 dots
                }
                pErrorPos = pStatement;                                                                         // in case of error

                if (result != result_parsing_OK) { flushAllUntilEOF = true; }

                // reset after each statement read 
                statementCharCount = 0;
                withinString = false; withinStringEscSequence = false; within1LineComment = false; withinMultiLineComment = false;
                lastCharWasSemiColon = false;
                parsedStatementStartsOnNewLine = false;         // reset flag (prepare for next statement)
            }

            // last 'gap' source line range and 'adjacent' source line "start of statement" range of source file
            if (_programMode && allCharsReceived) {
                ////result = _pBreakpoints->addOneSourceLineRangePair(BPstartLine - BPpreviousEndLine - 1, BPendLine - BPstartLine + 1);
                result = result_parsing_OK; //// temp
            }

            // program mode: complete program read and parsed   /   imm. mode: all statements in command line read and parsed OR parsing error ?
            if (allCharsReceived || (result != result_parsing_OK)) {                                            // note: if all statements have been read, they also have been parsed
                if (kill) { quitNow = true; }
                else {
                    quitNow = finaliseParsing(result, kill, lineCount, pErrorPos, allCharsReceived); // return value: quit Justina now

                    // if not in program mode and no parsing error: execute
                    execResult_type execResult{ result_execOK };
                    if (!_programMode && (result == result_parsing_OK)) {
                        execResult = exec(_programStorage + _progMemorySize);                                                   // execute parsed user statements
                        if (execResult == result_kill) { kill = true; }
                        if (kill || (execResult == result_quit)) { printlnTo(0); quitNow = true; }                          // make sure Justina prompt will be printed on a new line
                    }

                    quitNow = quitNow || prepareForIdleMode(result, execResult, kill, clearCmdIndicator, pStatementInputStream, streamNumber); // return value: quit Justina now
                }


                if (result == result_parsing_OK) {
                    if (loadingStartupProgram) { launchingStartFunction = true; }
                }
                // parsing error occurred ? reset input controlling variables
                else
                {
                    statementCharCount = 0;
                    withinString = false; withinStringEscSequence = false; within1LineComment = false; withinMultiLineComment = false;
                    lastCharWasSemiColon = false;
                }

                loadingStartupProgram = false;    // if this was a startup program load, then now it's aborted because of parsing error

                // reset after program (or imm. mode line) is read and processed
                lineCount = 0;
                parsedStatementCount = 0;
                flushAllUntilEOF = false;
                _statement[statementCharCount] = '\0';                                                          // add string terminator

                parsedStatementStartsOnNewLine = false;
                parsedStatementStartLinesAdjacent = false;
                statementStartsAtLine = 0;
                parsedStatementStartsAtLine = 0;
                BPstartLine = 0;
                BPendLine = 0;
                BPpreviousEndLine = 0;

                clearCmdIndicator = 0;          // reset
                result = result_parsing_OK;
            }
        } while (false);
        if (quitNow) { break; }                                                                                 // user gave quit command


    } while (true);

    // returning control to Justina caller
    _appFlags = 0x0000L;                                                                                        // clear all application flags
    _housekeepingCallback(_appFlags);  //// temp: quit Justina bug                                                                         // pass application flags to caller immediately

    if (kill) { _keepInMemory = false; /*temp*/Serial.println( "\r\n\r\n>>>>> Justina: kill request received from calling program <<<<<"); }

    SD_closeAllFiles();                                                                                         // safety (in case an SD card is present: close all files 
    _SDinitOK = false;
    SD.end();                                                                                                   // stop SD card

    while (_pConsoleIn->available() > 0) { readFrom(0); }                                                       //  empty console buffer before quitting
    /*temp*/Serial.println( "\r\nJustina: bye\r\n");
    for (int i = 0; i < 48; i++) { /*temp*/Serial.print("="); } /*temp*/Serial.println( "\r\n");

    if(!_keepInMemory){     //// NAAR DESTRUCTOR na oplossen 'Quit Justina' bug
        resetMachine(true);                                                                             // delete all objects created on the heap: with = with user variables and FiFo stack
        delete[] _programStorage;
        delete[] _pIOprintColumns;
    }
    return _keepInMemory;                                                                                       // return to calling program
}


// ----------------------------------------------------------------------------------
// *   add a character received from the input stream to the parsing input buffer   *
// ----------------------------------------------------------------------------------

bool Justina_interpreter::addCharacterToInput(bool& lastCharWasSemiColon, bool& withinString, bool& withinStringEscSequence, bool& within1LineComment, bool& withinMultiLineComment,
    bool& redundantSemiColon, bool ImmModeLineOrProgramRead, bool& bufferOverrun, bool flushAllUntilEOF, int& lineCount, int& statementCharCount, char c) {

    const char commentOuterDelim = '/'; // twice: single line comment, followed by inner del.: start of multi-line comment, preceded by inner delimiter: end of multi-line comment 
    const char commentInnerDelim = '*';

    static bool lastCharWasWhiteSpace{ false };
    static char lastCommentChar{ '\0' };                                                                        // init: none

    bool redundantSpaces = false;

    bufferOverrun = false;
    if ((c < ' ') && (c != '\n')) { return false; }                                                             // skip control-chars except new line and EOF character

    // when a imm. mode line or program is completely read and the last character (part of the last statement) received from input stream is not a semicolon, add it
    if (ImmModeLineOrProgramRead) {
        if (statementCharCount > 0) {
            if (_statement[statementCharCount - 1] != term_semicolon[0]) {
                if (statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }
                _statement[statementCharCount] = term_semicolon[0];                                                          // still room: add character
                statementCharCount++;
            }
        }

        within1LineComment = false;
        withinMultiLineComment = false;
    }

    // not at end of program or imm. mode line: process character   
    else {
        if (flushAllUntilEOF) { return false; }                                                                // discard characters (after parsing error)

        if (c == '\n') { lineCount++; }                                                                        // line number used when while reading program in input file

        // currently within a string or within a comment ? check for ending delimiter, check for in-string backslash sequences
        if (withinString) {
            if (c == '\\') { withinStringEscSequence = !withinStringEscSequence; }
            else if (c == '\"') { withinString = withinStringEscSequence; withinStringEscSequence = false; }
            else { withinStringEscSequence = false; }                                                           // any other character within string
            lastCharWasWhiteSpace = false;
            lastCharWasSemiColon = false;
        }

        // within a single-line comment ? check for end of comment 
        else if (within1LineComment) {
            if (c == '\n') { within1LineComment = false; return false; }                                        // comment stops at end of line
        }

        // within a multi-line comment ? check for end of comment 
        else if (withinMultiLineComment) {
            if ((c == commentOuterDelim) && (lastCommentChar == commentInnerDelim)) { withinMultiLineComment = false; return false; }
            lastCommentChar = c;                // a discarded character within a comment
        }

        // NOT within a string or (single-or multi-) line comment ?
        else {
            bool leadingWhiteSpace = (((c == ' ') || (c == '\n')) && (statementCharCount == 0));
            if (leadingWhiteSpace) { return false; };

            // start of string ?
            if (c == '\"') { withinString = true; }

            // start of (single-or multi-) line comment ?
            else if ((c == commentOuterDelim) || (c == commentInnerDelim)) {  // if previous character = same, then remove it from input buffer. It's the start of a single line comment
                if (statementCharCount > 0) {
                    if (_statement[statementCharCount - 1] == commentOuterDelim) {
                        lastCommentChar = '\0';         // reset
                        --statementCharCount;
                        _statement[statementCharCount] = '\0';                                                 // add string terminator

                        ((c == commentOuterDelim) ? within1LineComment : withinMultiLineComment) = true; return false;
                    }
                }
            }

            // white space in multi-line statements: replace a new line with a space (program only)
            else if (c == '\n') { c = ' '; }

            // check last character 
            redundantSpaces = (statementCharCount > 0) && (c == ' ') && lastCharWasWhiteSpace;
            redundantSemiColon = (c == term_semicolon[0]) && lastCharWasSemiColon;
            lastCharWasWhiteSpace = (c == ' ');                     // remember
            lastCharWasSemiColon = (c == term_semicolon[0]);
        }

        // do NOT add character to parsing input buffer if specific conditions are met
        if (redundantSpaces || redundantSemiColon || within1LineComment || withinMultiLineComment) { return false; }    // no character added
        if (statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }

        // add character  
        _statement[statementCharCount] = c;                                                                    // still room: add character
        ++statementCharCount;
    }

    return true;
}


// ----------------------------------------------------------------------------------------------------------------------------
// *   finalise parsing; execute if no errors; if in debug mode, trace and print debug info; re-init machine state and exit   *  ////
// ----------------------------------------------------------------------------------------------------------------------------

bool Justina_interpreter::finaliseParsing(parsingResult_type& result, bool& kill, int lineCount, char* pErrorPos, bool allCharsReceived) {

    bool quitJustina{ false };

    // all statements (in program or imm. mode line) have been parsed: finalise ////
    // ------------------------------------------------------------------------

    int funcNotDefIndex;
    if (result == result_parsing_OK) {
        // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
        if (_programMode && (!checkAllJustinaFunctionsDefined(funcNotDefIndex))) { result = result_function_undefinedFunctionOrArray; }
        if (_blockLevel > 0) { result = result_block_noBlockEnd; }
    }

    (_programMode ? _lastProgramStep : _lastUserCmdStep) = _programCounter;

    if (result == result_parsing_OK) {
        if (_programMode) {
            // parsing OK message (program mode only - no message in immediate mode)  
            printParsingResult(result, funcNotDefIndex, _statement, lineCount, pErrorPos);
        }
        else {
            if (_promptAndEcho == 2) { prettyPrintStatements(0); printlnTo(0); }                                // immediate mode and result OK: pretty print input line
            else if (_promptAndEcho == 1) { printlnTo(0); }
        }
    }
    else {          // parsing error, abort or kill during parsing
        if (_programMode && (_loadProgFromStreamNo <= 0)) {
            if (result == result_parse_abort) { /*temp*/Serial.print("\r\nAbort: "); }                                // not for other parsing errors
            else { /*temp*/Serial.print("\r\nParsing error: "); }
            if (result != result_parsing_OK) { /*temp*/Serial.println( "processing remainder of input file... please wait"); }
        }

        char c{};
        long byteInCount{ 0 };
        do {                                                                                                // process remainder of input file (flush)
            // NOTE: forcedStop and forcedAbort are dummy arguments here and will be ignored because already flushing input file after error, abort or kill
            bool forcedStop{ false }, forcedAbort{ false }, stdConsDummy{ false };                          // dummy arguments (not needed here)
            if (!_programMode && allCharsReceived) { break; }                                               // last character received before call was a newline character: complete user command line was read
            c = getCharacter(kill, forcedStop, forcedAbort, stdConsDummy, true);
            if (kill) { result = result_parse_kill; break; }                                                // kill while processing remainder of file
            if (!_programMode && (c == '\n')) { break; }                                                    // complete user command line was read
            else if (_programMode && ((++byteInCount & 0x0fff) == 0)) {
                /*temp*/Serial.print('.');
                if ((byteInCount & 0x03ffff) == 0) { printlnTo(0); }                                        // print a dot each 4096 lines, a crlf each 64 dots
            }
        } while (c != 0xFF);


        if (result == result_parse_abort) {
            /*temp*/Serial.println( _programMode ? "\r\n+++ Abort: parsing terminated +++" : "");                       // abort: display error message if aborting program parsing
        }
        else if (result == result_parse_setStdConsole) {
            /*temp*/Serial.println( "\r\n+++ console reset +++");
            _consoleIn_sourceStreamNumber = _consoleOut_sourceStreamNumber = -1;
            _pConsoleIn = _pConsoleOut = _pExternIOstreams[0];                                                  // set console to stream -1 (NOT debug out)
            _pConsolePrintColumn = &_pIOprintColumns[0];
            *_pConsolePrintColumn = 0;

        }
        else if (result == result_parse_kill) { quitJustina = true; }
        else { printParsingResult(result, funcNotDefIndex, _statement, lineCount, pErrorPos); }                 // parsing error occurred: print error message

    }
    return quitJustina;
}


// ---------------------------------------------
// *   finalise execution: prepare for idle mode
// ---------------------------------------------

bool Justina_interpreter::prepareForIdleMode(parsingResult_type result, execResult_type execResult, bool& kill, int& clearIndicator, Stream*& pStatementInputStream, int& statementInputStreamNumber) {

    bool quitJustina{ false };

    // if in debug mode, trace expressions (if defined) and print debug info 
    // ---------------------------------------------------------------------
    if ((_openDebugLevels > 0) && (execResult != result_kill) && (execResult != result_quit) && (execResult != result_initiateProgramLoad)) ; //// temp { traceAndPrintDebugInfo(execResult); }

    // re-init or reset interpreter state 
    // ----------------------------------

    // if program parsing error: reset machine, because variable storage might not be consistent with program any more
    if ((_programMode) && (result != result_parsing_OK)) { resetMachine(false); }

    // before loadng a program, clear memory except user variables
    else if (execResult == result_initiateProgramLoad) { resetMachine(false); }

    // no program error (could be immmediate mode error however), not initiating program load: only reset a couple of items here 
    else {
        parsingStack.deleteList();
        _blockLevel = 0;                                     // current number of open block commands (during parsing) - block level + parenthesis level = parsing stack depth
        _parenthesisLevel = 0;                                      // current number of open parentheses (during parsing)

        _justinaFunctionBlockOpen = false;
    }

    // execution finished (not stopping in debug mode), with or without error: delete parsed strings in imm mode command : they are on the heap and not needed any more. Identifiers must stay availalble
    // -> if stopping a program for debug, do not delete parsed strings (in imm. mode command), because that command line has now been pushed on  ...
     // the parsed command line stack and included parsed constants will be deleted later (resetMachine routine)
    if ((execResult != result_stopForDebug) && (execResult != result_stopForBreakpoint)) { deleteConstStringObjects(_programStorage + _progMemorySize); } // always

    // finalize: last actions before 'ready' mode (prompt displayed depending on settings)
    // -----------------------------------------------------------------------------------
    _programMode = false;
    _programCounter = _programStorage + _progMemorySize;                                                        // start of 'immediate mode' program area
    *(_programStorage + _progMemorySize) = tok_no_token;                                                        //  current end of program (immediate mode)

    if (execResult == result_initiateProgramLoad) {                                                             // initiate program load 
        _programMode = true;
        _programCounter = _programStorage;

        if (_lastPrintedIsPrompt) { printlnTo(0); }                                                             // print new line if last printed was a prompt
        /*temp*/Serial.print((_loadProgFromStreamNo > 0) ? "Loading program...\r\n" : "Loading program... please wait\r\n");
        _lastPrintedIsPrompt = false;

        statementInputStreamNumber = _loadProgFromStreamNo;
        setStream(statementInputStreamNumber, pStatementInputStream);

        // useful for remote terminals (characters sent to connect are flushed, this way)
        if (_loadProgFromStreamNo <= 0) { while (pStatementInputStream->available()) { readFrom(statementInputStreamNumber); } }

        _initiateProgramLoad = true;
    }
    else {      // with or without parsing or execution error
        statementInputStreamNumber = 0;
        setStream(statementInputStreamNumber, pStatementInputStream);
        if (_loadProgFromStreamNo > 0) { SD_closeFile(_loadProgFromStreamNo); _loadProgFromStreamNo = 0; }
    }


    while (_pConsoleIn->available()) { readFrom(0); }                                                           // empty console buffer first (to allow the user to start with an empty line)

    // has an error occurred ? (exclude 'events' reported as an error)
    bool isError = (result != result_parsing_OK) || ((execResult != result_execOK) && (execResult < result_startOfEvents));
    isError ? (_appFlags |= appFlag_errorConditionBit) : (_appFlags &= ~appFlag_errorConditionBit);             // set or clear error condition flag 
    (_appFlags &= ~appFlag_statusMask);
    (_openDebugLevels > 0) ? (_appFlags |= appFlag_stoppedInDebug) : (_appFlags |= appFlag_idle);               // status 'debug mode' or 'idle'

    // print new prompt and exit
    // -------------------------
    _lastPrintedIsPrompt = false;
    if ((_promptAndEcho != 0) && (execResult != result_initiateProgramLoad)) {
        /*temp*/Serial.print("Justina> "); _lastPrintedIsPrompt = true;
    }

    return quitJustina;
}

// -------------------------------------------------------------
// *   check if all Justina functions referenced are defined   *
// -------------------------------------------------------------

bool Justina_interpreter::checkAllJustinaFunctionsDefined(int& index) {
    index = 0;
    while (index < _justinaFunctionCount) {                                                                     // points to variable in use
        if (justinaFunctionData[index].pJustinaFunctionStartToken == nullptr) { return false; }
        index++;
    }
    return true;
}


//----------------------------------------------
// *   execute regular housekeeping callback   *
// ---------------------------------------------

// do a housekeeping callback at regular intervals (if callback function defined), but only
// - while waiting for input
// - while executing the Justina delay() function
// - when a statement has been executed 
// the callback function relays specific flags to the caller and upon return, reads certain flags set by the caller 

void Justina_interpreter::execPeriodicHousekeeping(bool* pKillNow, bool* pForcedStop, bool* pForcedAbort, bool* pSetStdConsole) {
    if (pKillNow != nullptr) { *pKillNow = false; }; if (pForcedStop != nullptr) { *pForcedStop = false; } if (pForcedAbort != nullptr) { *pForcedAbort = false; }    // init
    if (_housekeepingCallback != nullptr) {
        _currenttime = millis();
        _previousTime = _currenttime;
        // note: also handles millis() overflow after about 47 days
        if ((_lastCallBackTime + CALLBACK_INTERVAL < _currenttime) || (_currenttime < _previousTime)) {         // while executing, limit calls to housekeeping callback routine 
            _lastCallBackTime = _currenttime;
            _housekeepingCallback(_appFlags);                                                                   // execute housekeeping callback
            if ((_appFlags & appFlag_consoleRequestBit) && (pSetStdConsole != nullptr)) { *pSetStdConsole = true; }
            if ((_appFlags & appFlag_killRequestBit) && (pKillNow != nullptr)) { *pKillNow = true; }
            if ((_appFlags & appFlag_stopRequestBit) && (pForcedStop != nullptr)) { *pForcedStop = true; }
            if ((_appFlags & appFlag_abortRequestBit) && (pForcedAbort != nullptr)) { *pForcedAbort = true; }

            _appFlags &= ~(appFlag_dataInOut | appFlag_dataRecdFromStreamMask);                                           // reset 'external IO' flags 
        }
    }
}


// -------------------------
// *   reset interpreter   *
// -------------------------

void Justina_interpreter::resetMachine(bool withUserVariables) {

    // delete all objects created on the heap
    // --------------------------------------

    // note: objects living only during execution do not need to be deleted: they are all always deleted when the execution phase ends (even if with execution errors)
    // more in particular: evaluation stack, intermediate alphanumeric constants, local storage areas, local variable strings, local array objects

    // delete identifier name objects on the heap (variable names, Justina function names) 
    deleteIdentifierNameObjects(programVarNames, _programVarNameCount);
    deleteIdentifierNameObjects(JustinaFunctionNames, _justinaFunctionCount);
    if (withUserVariables) { deleteIdentifierNameObjects(userVarNames, _userVarCount, true); }

    // delete variable heap objects: array variable element string objects
    deleteStringArrayVarsStringObjects(globalVarValues, globalVarType, _programVarNameCount, 0, true);
    deleteStringArrayVarsStringObjects(staticVarValues, staticVarType, _staticVarCount, 0, false);
    if (withUserVariables) {
        deleteStringArrayVarsStringObjects(userVarValues, userVarType, _userVarCount, 0, false, true);
        deleteLastValueFiFoStringObjects();
    }

    // delete variable heap objects: scalar variable strings and array variable array storage 
    deleteVariableValueObjects(globalVarValues, globalVarType, _programVarNameCount, 0, true);
    deleteVariableValueObjects(staticVarValues, staticVarType, _staticVarCount, 0, false);
    if (withUserVariables) {
        deleteVariableValueObjects(userVarValues, userVarType, _userVarCount, 0, false, true);

        if (_pTraceString != nullptr) {        // internal trace 'variable'
        #if PRINT_HEAP_OBJ_CREA_DEL
            /*temp*/Serial.print("----- (system var str) "); /*temp*/Serial.println((uint32_t)_pTraceString, HEX);
        #endif
            _systemVarStringObjectCount--;
            delete[] _pTraceString;
            _pTraceString = nullptr;                                                                            // old trace string
        }
    }

    // delete all elements of the immediate mode parsed statements stack
    // (parsed immediate mode statements can be temporarily pushed on the immediate mode stack to be replaced either by parsed debug command lines or parsed eval() strings) 
    // also delete all parsed alphanumeric constants: (1) in the currently parsed program program, (2) in parsed immediate mode statements (including those on the imm.mode parsed statements stack)) 

    clearParsedCommandLineStack(parsedCommandLineStack.getElementCount());                                      // including parsed string constants
    deleteConstStringObjects(_programStorage);
    deleteConstStringObjects(_programStorage + _progMemorySize);

    // delete all elements of the flow control stack 
    // in the process, delete all local variable areas referenced in elements of the flow control stack referring to functions, including local variable string and array values
    int dummy{};
    clearFlowCtrlStack(dummy);

    // clear expression evaluation stack
    clearEvalStack();

    // delete parsing stack (keeps track of open parentheses and open command blocks during parsing)
    parsingStack.deleteList();

    // check that all heap objects are deleted (in fact only the count is checked)
    // ---------------------------------------------------------------------------
    danglingPointerCheckAndCount(withUserVariables);                                                            // check and count


    // initialize interpreter object variables
    // ---------------------------------------
    initInterpreterVariables(withUserVariables);


    printlnTo(0);
}


// ---------------------------------------------------------------------------------------
// *   perform consistency checks: verify that all objects created are destroyed again   *
// ---------------------------------------------------------------------------------------

void Justina_interpreter::danglingPointerCheckAndCount(bool withUserVariables) {

    // note: the evaluation stack, intermediate string objects, function local storage, and function local variable strings and arrays exist solely during execution
    //       relevant checks are performed each time execution terminates 

    // parsing stack: no need to check if any elements were left (the list has just been deleted)
    // note: this stack does not contain any pointers to heap objects

    // string and array heap objects: any objects left ?
    if (_identifierNameStringObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        /*temp*/Serial.print("*** Variable / function name objects cleanup error. Remaining: "); /*temp*/Serial.println(_identifierNameStringObjectCount);
    #endif
        _identifierNameStringObjectErrors += abs(_identifierNameStringObjectCount);
}

    if (_parsedStringConstObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        /*temp*/Serial.print("*** Parsed constant string objects cleanup error. Remaining: "); /*temp*/Serial.println(_parsedStringConstObjectCount);
    #endif
        _parsedStringConstObjectErrors += abs(_parsedStringConstObjectCount);
    }

    if (_globalStaticVarStringObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        /*temp*/Serial.print("*** Variable string objects cleanup error. Remaining: "); /*temp*/Serial.println(_globalStaticVarStringObjectCount);
    #endif
        _globalStaticVarStringObjectErrors += abs(_globalStaticVarStringObjectCount);
    }

    if (_globalStaticArrayObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        /*temp*/Serial.print("*** Array objects cleanup error. Remaining: "); /*temp*/Serial.println(_globalStaticArrayObjectCount);
    #endif
        _globalStaticArrayObjectErrors += abs(_globalStaticArrayObjectCount);
    }

#if PRINT_DEBUG_INFO
    /*temp*/Serial.print("\r\n** Reset stats\r\n    parsed strings "); /*temp*/Serial.print(_parsedStringConstObjectCount);

    /*temp*/Serial.print(", prog name strings "); /*temp*/Serial.print(_identifierNameStringObjectCount);
    /*temp*/Serial.print(", prog var strings "); /*temp*/Serial.print(_globalStaticVarStringObjectCount);
    /*temp*/Serial.print(", prog arrays "); /*temp*/Serial.print(_globalStaticArrayObjectCount);
#endif

    if (withUserVariables) {
        if (_userVarNameStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            /*temp*/Serial.print("*** User variable name objects cleanup error. Remaining: "); /*temp*/Serial.println(_userVarNameStringObjectCount);
        #endif
            _userVarNameStringObjectErrors += abs(_userVarNameStringObjectCount);
    }

        if (_userVarStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            /*temp*/Serial.print("*** User variable string objects cleanup error. Remaining: "); /*temp*/Serial.println(_userVarStringObjectCount);
        #endif
            _userVarStringObjectErrors += abs(_userVarStringObjectCount);
        }

        if (_userArrayObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            /*temp*/Serial.print("*** User array objects cleanup error. Remaining: "); /*temp*/Serial.println(_userArrayObjectCount);
        #endif
            _userArrayObjectErrors += abs(_userArrayObjectCount);
        }

        if (_systemVarStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            /*temp*/Serial.print("*** System variable string objects cleanup error. Remaining: "); /*temp*/Serial.println(_systemVarStringObjectCount);
        #endif
            _systemVarStringObjectErrors += abs(_systemVarStringObjectCount);
        }

        if (_lastValuesStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            /*temp*/Serial.print("*** Last value FiFo string objects cleanup error. Remaining: "); /*temp*/Serial.print(_lastValuesStringObjectCount);
        #endif
            _lastValuesStringObjectErrors += abs(_lastValuesStringObjectCount);
        }

    #if PRINT_DEBUG_INFO
        /*temp*/Serial.print(", user var names "); /*temp*/Serial.print(_userVarNameStringObjectCount);
        /*temp*/Serial.print(", user var strings "); /*temp*/Serial.print(_userVarStringObjectCount);
        /*temp*/Serial.print(", user arrays "); /*temp*/Serial.print(_userArrayObjectCount);

        /*temp*/Serial.print(", last value strings "); /*temp*/Serial.print(_lastValuesStringObjectCount);
    #endif
    }
}


// --------------------------------------------
// *   initialise interpreter object fields   *
// --------------------------------------------

void Justina_interpreter::initInterpreterVariables(bool fullReset) {

    // intialised at cold start AND each time the interpreter is reset

    _blockLevel = 0;                                     // current number of open block commands (during parsing) - block level + parenthesis level = parsing stack depth
    _justinaFunctionCount = 0;
    _paramOnlyCountInFunction = 0;
    _localVarCountInFunction = 0;
    _localVarCount = 0;
    _staticVarCountInFunction = 0;
    _staticVarCount = 0;
    _justinaFunctionBlockOpen = false;

    _programVarNameCount = 0;
    if (fullReset) { _userVarCount = 0; }
    else {
        int index = 0;                                                                                          // clear user variable flag 'variable is used by program'
        while (index++ < _userVarCount) { userVarType[index] = userVarType[index] & ~var_userVarUsedByProgram; }
    }
    *_programStorage = tok_no_token;                                                                            //  set as current end of program 
    *(_programStorage + _progMemorySize) = tok_no_token;                                                        //  set as current end of program (immediate mode)
    _programCounter = _programStorage + _progMemorySize;                                                        // start of 'immediate mode' program area

    _programName[0] = '\0';

    _pEvalStackTop = nullptr; _pEvalStackMinus1 = nullptr;   _pEvalStackMinus2 = nullptr;
    _pFlowCtrlStackTop = nullptr;
    _pParsedCommandLineStackTop = nullptr;

    _intermediateStringObjectCount = 0;      // reset at the start of execution
    _localVarValueAreaCount = 0;
    _localVarStringObjectCount = 0;
    _localArrayObjectCount = 0;

    _activeFunctionData.callerEvalStackLevels = 0;                                                  // this is the highest program level
    _callStackDepth = 0;                                                                            // equals flow control stack depth minus open loop (if, for, ...) blocks (= blocks being executed)
    _openDebugLevels = 0;                                                                           // equals imm mode cmd stack depth minus open eval() strings (= eval() strings being executed)


    // reset counters for heap objects
    // -------------------------------

    _identifierNameStringObjectCount = 0;
    _parsedStringConstObjectCount = 0;

    _globalStaticVarStringObjectCount = 0;
    _globalStaticArrayObjectCount = 0;

    if (fullReset) {
        _lastValuesCount = 0;                                                                       // current last result FiFo depth (values currently stored)

        _userVarNameStringObjectCount = 0;
        _userVarStringObjectCount = 0;
        _userArrayObjectCount = 0;
        _systemVarStringObjectCount = 0;

        _lastValuesStringObjectCount = 0;

        _openFileCount = 0;
    }


    // initialize format settings for numbers and strings (width, characters to print, flags, ...)
    // -------------------------------------------------------------------------------------------

    _dispWidth = DEFAULT_DISP_WIDTH;

    _dispFloatPrecision = DEFAULT_FLOAT_PRECISION;
    _dispIntegerPrecision = DEFAULT_INT_PRECISION;

    strcpy(_dispFloatSpecifier, DEFAULT_FLOAT_SPECIFIER);
    strcpy(_dispIntegerSpecifier, DEFAULT_INT_SPECIFIER);                                                                 // here without 'd' (long integer) : will be added  
    strcpy(_dispStringSpecifier, DEFAULT_STR_SPECIFIER);                                                                 // here without 'd' (long integer) : will be added  

    _dispFloatFmtFlags = DEFAULT_FLOAT_FLAGS;
    _dispIntegerFmtFlags = DEFAULT_INT_FLAGS;
    _dispStringFmtFlags = DEFAULT_STR_FLAGS;

    makeFormatString(_dispIntegerFmtFlags, true, _dispIntegerSpecifier, _dispIntegerFmtString);           // for integers
    makeFormatString(_dispFloatFmtFlags, false, _dispFloatSpecifier, _dispFloatFmtString);               // for floats
    makeFormatString(_dispStringFmtFlags, false, _dispStringSpecifier, _dispStringFmtString);               // for strings

    // fmt() function settings 
    // -----------------------
    _fmt_width = DEFAULT_FMT_WIDTH;                             // width

    _fmt_numPrecision = DEFAULT_FLOAT_PRECISION;                  // precision
    _fmt_strCharsToPrint = DEFAULT_STR_CHARS_TO_PRINT;

    strcpy(_fmt_numSpecifier, DEFAULT_FLOAT_SPECIFIER);         // specifier   
    strcpy(_fmt_stringSpecifier, DEFAULT_STR_SPECIFIER);

    _fmt_numFmtFlags = DEFAULT_FLOAT_FLAGS;             // flags
    _fmt_stringFmtFlags = DEFAULT_STR_FLAGS;             // flags


    // display output settings
    // -----------------------

    if (fullReset) {
        _promptAndEcho = 2, _printLastResult = 1;
    }
}


// -------------------------------------------------------------------------------------
// *   delete all identifier names (char strings)                                      *
// *   note: this excludes generic identifier names stored as alphanumeric constants   *
// -------------------------------------------------------------------------------------

void Justina_interpreter::deleteIdentifierNameObjects(char** pIdentNameArray, int identifiersInUse, bool isUserVar) {
    int index = 0;          // points to last variable in use
    while (index < identifiersInUse) {                       // points to variable in use
    #if PRINT_HEAP_OBJ_CREA_DEL
        /*temp*/Serial.print(isUserVar ? "----- (usrvar name) " : "----- (ident name ) "); /*temp*/Serial.println((uint32_t) * (pIdentNameArray + index), HEX);
    #endif
        isUserVar ? _userVarNameStringObjectCount-- : _identifierNameStringObjectCount--;
        delete[] * (pIdentNameArray + index);
        index++;
    }
}


// --------------------------------------------------------------------------------------------------------------
// *   all string array variables of a specified scope: delete array element character strings (heap objects)   *
// --------------------------------------------------------------------------------------------------------------

void Justina_interpreter::deleteStringArrayVarsStringObjects(Justina_interpreter::Val* varValues, char* sourceVarScopeAndFlags, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar, bool isLocalVar) {
    int index = paramOnlyCount;                    // skip parameters (if function, otherwise must be zero) - if parameter variables are arrays, they are always a reference variable 
    while (index < varNameCount) {
        if (!checkIfGlobalValue || (sourceVarScopeAndFlags[index] & (var_nameHasGlobalValue))) {                                    // if only for global values: is it a global value ?

            if ((sourceVarScopeAndFlags[index] & (var_isArray | value_typeMask)) == (var_isArray | value_isStringPointer)) {        // array of strings
                deleteOneArrayVarStringObjects(varValues, index, isUserVar, isLocalVar);
            }
        }
        index++;
    }
}


// ---------------------------------------------------------------------------------------------
// *   delete variable heap objects: array variable character strings for ONE array variable   *
// ---------------------------------------------------------------------------------------------

// no checks are made - make sure the variable is an array variable storing strings

void Justina_interpreter::deleteOneArrayVarStringObjects(Justina_interpreter::Val* varValues, int index, bool isUserVar, bool isLocalVar) {
    void* pArrayStorage = varValues[index].pArray;                                                                                  // void pointer to an array of string pointers; element 0 contains dimensions and dimension count
    int dimensions = (((char*)pArrayStorage)[3]);                                                                                   // can range from 1 to MAX_ARRAY_DIMS
    int arrayElements = 1;                                                                                                          // determine array size
    for (int dimCnt = 0; dimCnt < dimensions; dimCnt++) { arrayElements *= (int)((((char*)pArrayStorage)[dimCnt])); }

    // delete non-empty strings
    for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) {                                                              // array element 0 contains dimensions and count
        char* pString = ((char**)pArrayStorage)[arrayElem];
        uint32_t stringPointerAddress = (uint32_t) & (((char**)pArrayStorage)[arrayElem]);
        if (pString != nullptr) {
        #if PRINT_HEAP_OBJ_CREA_DEL
            /*temp*/Serial.print(isUserVar ? "----- (usr arr str) " : isLocalVar ? "-----(loc arr str)" : "----- (arr string ) "); /*temp*/Serial.println((uint32_t)pString, HEX);     // applicable to string and array (same pointer)
        #endif
            isUserVar ? _userVarStringObjectCount-- : isLocalVar ? _localVarStringObjectCount-- : _globalStaticVarStringObjectCount--;
            delete[]  pString;                                                                                                      // applicable to string and array (same pointer)
        }
    }
}


// ----------------------------------------------------------------------------------------------
// *   delete variable heap objects: scalar variable strings and array variable array storage   *
// ----------------------------------------------------------------------------------------------

// note: make sure array variable element string objects have been deleted prior to calling this routine

void Justina_interpreter::deleteVariableValueObjects(Justina_interpreter::Val* varValues, char* varType, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar, bool isLocalVar) {

    int index = 0;
    // do NOT skip parameters if deleting function variables: with constant args, a local copy is created (always scalar) and must be deleted if non-empty string
    while (index < varNameCount) {
        if (!checkIfGlobalValue || (varType[index] & (var_nameHasGlobalValue))) {                                                   // global value ?
            // check for arrays before checking for strings (if both 'var_isArray' and 'value_isStringPointer' bits are set: array of strings, with strings already deleted)
            if (((varType[index] & value_typeMask) != value_isVarRef) && (varType[index] & var_isArray)) {                          // variable is an array: delete array storage          
            #if PRINT_HEAP_OBJ_CREA_DEL
                /*temp*/Serial.print(isUserVar ? "----- (usr ar stor) " : isLocalVar ? "----- (loc ar stor) " : "----- (array stor ) "); /*temp*/Serial.println((uint32_t)varValues[index].pArray, HEX);
            #endif
                isUserVar ? _userArrayObjectCount-- : isLocalVar ? _localArrayObjectCount-- : _globalStaticArrayObjectCount--;
                delete[]  varValues[index].pArray;
            }
            else if ((varType[index] & value_typeMask) == value_isStringPointer) {                                                  // variable is a scalar containing a string
                if (varValues[index].pStringConst != nullptr) {
                #if PRINT_HEAP_OBJ_CREA_DEL
                    /*temp*/Serial.print(isUserVar ? "----- (usr var str) " : isLocalVar ? "----- (loc var str)" : "----- (var string ) "); /*temp*/Serial.println((uint32_t)varValues[index].pStringConst, HEX);
                #endif
                    isUserVar ? _userVarStringObjectCount-- : isLocalVar ? _localVarStringObjectCount-- : _globalStaticVarStringObjectCount--;
                    delete[]  varValues[index].pStringConst;
                }
            }
        }
        index++;
    }
}


// --------------------------------------------------------------------
// *   delete variable heap objects: last value Fifo string objects   *
// --------------------------------------------------------------------

void Justina_interpreter::deleteLastValueFiFoStringObjects() {
    if (_lastValuesCount == 0) return;

    for (int i = 0; i < _lastValuesCount; i++) {
        bool isNonEmptyString = (lastResultTypeFiFo[i] == value_isStringPointer) ? (lastResultValueFiFo[i].pStringConst != nullptr) : false;
        if (isNonEmptyString) {
        #if PRINT_HEAP_OBJ_CREA_DEL
            /*temp*/Serial.print("----- (FiFo string) "); /*temp*/Serial.println((uint32_t)lastResultValueFiFo[i].pStringConst, HEX);
        #endif
            _lastValuesStringObjectCount--;
            delete[] lastResultValueFiFo[i].pStringConst;
        }
    }
}


// -----------------------------------------------------------------------------------------
// *   delete all parsed alphanumeric constant value heap objects                          *
// *   note: this includes UNQUALIFIED identifier names stored as alphanumeric constants   *
// -----------------------------------------------------------------------------------------

// must be called before deleting tokens (list elements) 

void Justina_interpreter::deleteConstStringObjects(char* pFirstToken) {
    char* pAnum;
    TokenPointer prgmCnt;

    prgmCnt.pTokenChars = pFirstToken;
    uint8_t tokenType = *prgmCnt.pTokenChars & 0x0F;
    while (tokenType != tok_no_token) {                                                                                             // for all tokens in token list
        bool isStringConst = (tokenType == tok_isConstant) ? (((*prgmCnt.pTokenChars >> 4) & value_typeMask) == value_isStringPointer) : false;

        if (isStringConst || (tokenType == tok_isGenericName)) {
            memcpy(&pAnum, prgmCnt.pCstToken->cstValue.pStringConst, sizeof(pAnum));                                                // pointer not necessarily aligned with word size: copy memory instead
            if (pAnum != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                /*temp*/Serial.print("----- (parsed str ) ");   /*temp*/Serial.println((uint32_t)pAnum, HEX);
            #endif
                _parsedStringConstObjectCount--;
                delete[] pAnum;
            }
        }
        uint8_t tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
            (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*prgmCnt.pTokenChars >> 4) & 0x0F;
        prgmCnt.pTokenChars += tokenLength;
        tokenType = *prgmCnt.pTokenChars & 0x0F;
    }
}


// ---------------------------------------------------------------------------------
// *   delete a variable string object referenced in an evaluation stack element   *
// ---------------------------------------------------------------------------------

// if not a string, then do nothing. If not a string, or string is empty, then exit WITH error

Justina_interpreter::execResult_type Justina_interpreter::deleteVarStringObject(LE_evalStack* pStackLvl) {
    if (pStackLvl->varOrConst.tokenType != tok_isVariable) { return result_execOK; };                                               // not a variable
    if ((*pStackLvl->varOrConst.varTypeAddress & value_typeMask) != value_isStringPointer) { return result_execOK; }                // not a string object
    if (*pStackLvl->varOrConst.value.ppStringConst == nullptr) { return result_execOK; }

    char varScope = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);

    // delete variable string object
#if PRINT_HEAP_OBJ_CREA_DEL
    /*temp*/Serial.print((varScope == var_isUser) ? "----- (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
    /*temp*/Serial.println((uint32_t)*pStackLvl->varOrConst.value.ppStringConst, HEX);
#endif
    (varScope == var_isUser) ? _userVarStringObjectCount-- : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount-- : _localVarStringObjectCount--;
    delete[] * pStackLvl->varOrConst.value.ppStringConst;
    return result_execOK;
}


// --------------------------------------------------------------------------------------
// *   delete an intermediate string object referenced in an evaluation stack element   *
// --------------------------------------------------------------------------------------

// if not a string, then do nothing. If not an intermediate string object, then exit WITHOUT error

Justina_interpreter::execResult_type Justina_interpreter::deleteIntermStringObject(LE_evalStack* pStackLvl) {

    if ((pStackLvl->varOrConst.valueAttributes & constIsIntermediate) != constIsIntermediate) { return result_execOK; }             // not an intermediate constant
    if (pStackLvl->varOrConst.valueType != value_isStringPointer) { return result_execOK; }                                         // not a string object
    if (pStackLvl->varOrConst.value.pStringConst == nullptr) { return result_execOK; }
#if PRINT_HEAP_OBJ_CREA_DEL
    /*temp*/Serial.print("----- (Intermd str) ");   /*temp*/Serial.println((uint32_t)_pEvalStackTop->varOrConst.value.pStringConst, HEX);
#endif
    _intermediateStringObjectCount--;
    delete[] pStackLvl->varOrConst.value.pStringConst;

    return result_execOK;
}


