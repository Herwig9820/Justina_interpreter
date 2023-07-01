/***************************************************************************************
    Justina interpreter library for Arduino Nano 33 IoT and Arduino RP2040.

    Version:    v1.00 - xx/xx/2022
    Author:     Herwig Taveirne

    Justina is an interpreter which does NOT require you to use an IDE to write
    and compile programs. Programs are written on the PC using any text processor
    and transferred to the Arduino using any terminal capable of sending files.
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

#define PRINT_HEAP_OBJ_CREA_DEL 0
#define PRINT_DEBUG_INFO 0
#define PRINT_OBJECT_COUNT_ERRORS 0


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
const char Justina_interpreter::cmdPar_108[4]{ cmdPar_extFunction,                            cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
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
    //  name            id code                 where allowed                  padding (boundary alignment)     param key      control info
    //  ----            -------                 -------------                  ----------------------------     ---------      ------------   

    // declare and delete variables
    // ----------------------------
    {"var",             cmdcod_var,             cmd_noRestrictions | cmd_skipDuringExec,                0,0,    cmdPar_111,     cmdBlockNone},
    {"const",           cmdcod_constVar,        cmd_noRestrictions | cmd_skipDuringExec,                0,0,    cmdPar_111,     cmdBlockNone},
    {"static",          cmdcod_static,          cmd_onlyInFunctionBlock | cmd_skipDuringExec,           0,0,    cmdPar_111,     cmdBlockNone},

    {"delete",          cmdcod_deleteVar,       cmd_onlyImmediate | cmd_skipDuringExec,                 0,0,    cmdPar_110,     cmdBlockNone},                  // can only delete user variables (imm. mode)

    {"clearAll",        cmdcod_clearAll,        cmd_onlyImmediate | cmd_skipDuringExec,                 0,0,    cmdPar_102,     cmdBlockNone},                  // executed AFTER execution phase ends
    {"clearProg",       cmdcod_clearProg,       cmd_onlyImmediate | cmd_skipDuringExec,                 0,0,    cmdPar_102,     cmdBlockNone},                  // executed AFTER execution phase ends

    // program and flow control commands
    // ---------------------------------
    {"loadProg",        cmdcod_loadProg,        cmd_onlyImmediate,                                      0,0,    cmdPar_106,     cmdBlockNone},

    {"program",         cmdcod_program,         cmd_onlyProgramTop | cmd_skipDuringExec,                0,0,    cmdPar_103,     cmdBlockNone},
    {"function",        cmdcod_function,        cmd_onlyInProgram | cmd_skipDuringExec,                 0,0,    cmdPar_108,     cmdBlockExtFunction},

    {"for",             cmdcod_for,             cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_109,     cmdBlockFor},
    {"while",           cmdcod_while,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockWhile},
    {"if",              cmdcod_if,              cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockIf},
    {"elseif",          cmdcod_elseif,          cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockIf_elseIf},
    {"else",            cmdcod_else,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockIf_else},
    {"end",             cmdcod_end,             cmd_noRestrictions,                                     0,0,    cmdPar_102,     cmdBlockGenEnd},                // closes inner open command block

    {"break",           cmdcod_break,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockOpenBlock_loop},        // allowed if at least one open loop block (any level) 
    {"continue",        cmdcod_continue,        cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockOpenBlock_loop },       // allowed if at least one open loop block (any level) 
    {"return",          cmdcod_return,          cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_106,     cmdBlockOpenBlock_function},    // allowed if currently an open function definition block 

    {"pause",           cmdcod_pause,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_106,     cmdBlockNone},
    {"halt",            cmdcod_halt,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},

    // debugging commands
    // ------------------
    {"stop",            cmdcod_stop,            cmd_onlyInFunctionBlock,                                0,0,    cmdPar_102,     cmdBlockNone},
    {"nop",             cmdcod_nop,             cmd_onlyInFunctionBlock | cmd_skipDuringExec,           0,0,    cmdPar_102,     cmdBlockNone},                  // insert two bytes in program, do nothing

    {"go",              cmdcod_go,              cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"step",            cmdcod_step,            cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"stepOut",         cmdcod_stepOut,         cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"stepOver",        cmdcod_stepOver,        cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"blockStepOut",    cmdcod_stepOutOfBlock,  cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"blockStepEnd",    cmdcod_stepToBlockEnd,  cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"skip",            cmdcod_skip,            cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},

    {"trace",           cmdcod_trace,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockNone},

    {"abort",           cmdcod_abort,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"debug",           cmdcod_debug,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"quit",            cmdcod_quit,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_106,     cmdBlockNone},

    // settings
    // --------
    {"dispFmt",         cmdcod_dispfmt,         cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},
    {"dispMode",        cmdcod_dispmod,         cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_105,     cmdBlockNone},
    {"tabSize",         cmdcod_tabSize,         cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockNone},
    {"angleMode",       cmdcod_angle,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockNone},

    // input and output commands
    // -------------------------

    {"setConsole",      cmdcod_setConsole,      cmd_onlyImmediate,                                      0,0,    cmdPar_104,     cmdBlockNone},
    {"setConsoleIn",    cmdcod_setConsIn,       cmd_onlyImmediate,                                      0,0,    cmdPar_104,     cmdBlockNone},
    {"setConsoleOut",   cmdcod_setConsOut,      cmd_onlyImmediate,                                      0,0,    cmdPar_104,     cmdBlockNone},
    {"setDebugOut",     cmdcod_setDebugOut,     cmd_onlyImmediate,                                      0,0,    cmdPar_104,     cmdBlockNone},

    {"info",            cmdcod_info,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_114,     cmdBlockNone},
    {"input",           cmdcod_input,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_113,     cmdBlockNone},

    {"startSD",         cmdcod_startSD,         cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},
    {"stopSD",          cmdcod_stopSD,          cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},

    {"receiveFile",     cmdcod_receiveFile,     cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},
    {"sendFile",        cmdcod_sendFile,        cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},
    {"copy",            cmdcod_copyFile,        cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_107,     cmdBlockNone},

    {"dbout",           cmdcod_dbout,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},
    {"dboutLine",       cmdcod_dboutLine,       cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_107,     cmdBlockNone},

    {"cout",            cmdcod_cout,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},
    {"coutLine",        cmdcod_coutLine,        cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_107,     cmdBlockNone},
    {"coutList",        cmdcod_coutList,        cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},

    {"print",           cmdcod_print,           cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_116,     cmdBlockNone},
    {"printLine",       cmdcod_printLine,       cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},
    {"printList",       cmdcod_printList,       cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_116,     cmdBlockNone},

    {"vprint",          cmdcod_printToVar,      cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_116,     cmdBlockNone},
    {"vprintLine",      cmdcod_printLineToVar,  cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_112,     cmdBlockNone},
    {"vprintList",      cmdcod_printListToVar,  cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_116,     cmdBlockNone},

    {"listVars",        cmdcod_printVars,       cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_106,     cmdBlockNone},
    {"listCallSt",      cmdcod_printCallSt,     cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_106,     cmdBlockNone},
    {"listFilesToSerial",cmdcod_listFilesToSer, cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},
    {"listFiles",       cmdcod_listFiles,       cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_106,     cmdBlockNone},

    // user callback functions
    // -----------------------
    {"declareCB",       cmdcod_declCB,          cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,      0,0,    cmdPar_110,     cmdBlockNone},
    {"clearCB",         cmdcod_clearCB,         cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,      0,0,    cmdPar_102,     cmdBlockNone},
    {"callcpp",         cmdcod_callback,        cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_101,     cmdBlockNone}
};


// internal (intrinsic) Justina functions: returning a result
// ----------------------------------------------------------

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
    {"fmt",                     fnccod_format,                  1,6,    0b0},               // short label for 'system value'
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
    {"tab",                     fnccod_tab,                     0,1,    0b0},
    {"col",                     fnccod_gotoColumn,              1,1,    0b0},
    {"pos",                     fnccod_getColumnPos,            0,0,    0b0},
    {"repChar",                 fnccod_repchar,                 2,2,    0b0},
    {"findInStr",               fnccod_findsubstr,              2,3,    0b0},
    {"substInStr",              fnccod_replacesubstr,           3,4,    0b0},
    {"strCmp",                  fnccod_strcmp,                  2,2,    0b0},
    {"strCaseCmp",              fnccod_strcasecmp,              2,2,    0b0},
    {"strHex",                  fnccod_strhex,                  1,1,    0b0},
    {"quote",                   fnccod_quote,                   1,1,    0b0},

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

    // based upon Arduino SD card library functions
    { "open",                    fnccod_open,                   1,2,    0b0 },
    { "close",                   fnccod_close,                  1,1,    0b0 },

    { "cin",                     fnccod_cin,                    0,2,    0b0 },
    { "cinLine",                 fnccod_cinLine,                0,0,    0b0 },
    { "cinList",                 fnccod_cinParseList,           1,16,   0b0 },
    { "read",                    fnccod_read,                   1,3,    0b0 },
    { "readLine",                fnccod_readLine,               1,1,    0b0 },
    { "readList",                fnccod_parseList,              2,16,   0b0 },

    { "vreadList",               fnccod_parseListFromVar,       2,16,   0b0 },

    { "find",                    fnccod_find,                   2,2,    0b0 },
    { "findUntil",               fnccod_findUntil,              3,3,    0b0 },
    { "peek",                    fnccod_peek,                   0,1,    0b0 },
    { "available",               fnccod_available,              0,1,    0b0 },
    { "position",                fnccod_position,               1,1,    0b0 },
    { "size",                    fnccod_size,                   1,1,    0b0 },
    { "name",                    fnccod_name,                   1,1,    0b0 },
    { "fullName",                fnccod_fullName,               1,1,    0b0 },
    { "flush",                   fnccod_flush,                  1,1,    0b0 },
    { "seek",                    fnccod_seek,                   2,2,    0b0 },
    { "setTimeout",              fnccod_setTimeout,             2,2,    0b0 },
    { "getTimeout",              fnccod_getTimeout,             1,1,    0b0 },
    { "availableForWrite",       fnccod_availableForWrite,      1,1,    0b0 },
    { "getWriteError",           fnccod_getWriteError,          1,1,    0b0 },
    { "clearWriteError",         fnccod_clearWriteError,        1,1,    0b0 },
    { "isDirectory",             fnccod_isDirectory,            1,1,    0b0 },
    { "rewindDirectory",         fnccod_rewindDirectory,        1,1,    0b0 },
    { "openNext",                fnccod_openNextFile,           1,2,    0b0 },
    { "exists",                  fnccod_exists,                 1,1,    0b0 },
    { "createDirectory",         fnccod_mkdir,                  1,1,    0b0 },
    { "removeDirectory",         fnccod_rmdir,                  1,1,    0b0 },
    { "remove",                  fnccod_remove,                 1,1,    0b0 },

    { "fileNum",                 fnccod_fileNumber,             1,1,    0b0 },
    { "isInUse",                 fnccod_isOpenFile,             1,1,    0b0 },
    { "closeAll",                fnccod_closeAll,               0,0,    0b0 },
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

    // non-operator terminals: ONE character only, character should NOT appear in operator names

    {term_comma,            termcod_comma,              0x00,               0x00,                       0x00},
    {term_semicolon,        termcod_semicolon,          0x00,               0x00,                       0x00},
    {term_rightPar,         termcod_rightPar,           0x00,               0x00,                       0x00},
    {term_leftPar,          termcod_leftPar,            0x00,               0x10,                       0x00},

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

    {"EULER",               "2.7182818284590452354",    value_isFloat},     // base of natural logarithm
    {"PI",                  "3.14159265358979323846",   value_isFloat},     // PI
    {"HALF_PI",             "1.57079632679489661923",   value_isFloat},     // PI / 2
    {"QUART_PI",            "0.78539816339744830962",   value_isFloat},     // PI / 4
    {"TWO_PI",              "6.2831853071795864769",    value_isFloat},      // 2 * PI 

    {"DEG_TO_RAD",          "0.01745329251994329577",   value_isFloat},     // conversion factor: degrees to radians
    {"RAD_TO_DEG",          "57.2957795130823208768",   value_isFloat},     // radians to degrrees

    {"DEGREES",             "0",                        value_isLong},
    {"RADIANS",             "1",                        value_isLong},

    {"FALSE",               "0",                        value_isLong},      // value for boolean 'false'
    {"TRUE",                "1",                        value_isLong},      // value for boolean 'true'

    {"LONG_TYP",            "1",                        value_isLong},      // value type of a long value
    {"FLOAT_TYP",           "2",                        value_isLong},      // value type of a float value
    {"STRING_TYP",          "3",                        value_isLong},      // value type of a string value

    {"LOW",                 "0",                        value_isLong},      // standard ARduino constants for digital I/O
    {"HIGH",                "1",                        value_isLong},

    {"INPUT",               "0x0",                      value_isLong},      // standard ARduino constants for digital I/O
    {"OUTPUT",              "0x1",                      value_isLong},
    {"INPUT_PULLUP",        "0x2",                      value_isLong},
    {"INPUT_PULLDOWN",      "0x3",                      value_isLong},

    {"NO_PROMPT",           "0",                        value_isLong},      // do not print prompt and do not echo user input
    {"PROMPT",              "1",                        value_isLong},      // print prompt but no not echo user input
    {"ECHO",                "2",                        value_isLong},      // print prompt and echo user input

    {"NO_LAST",             "0",                        value_isLong},      // do not print last result
    {"PRINT_LAST",          "1",                        value_isLong},      // print last result
    {"QUOTE_LAST",          "2",                        value_isLong},      // print last result, quote string results 

    {"LEFT",                "0x1",                      value_isLong},      // left justify
    {"SIGN",                "0x2",                      value_isLong},      // force sign
    {"SPACE_IF_POS",        "0x4",                      value_isLong},      // insert a space if no sign
    {"DEC_POINT",           "0x8",                      value_isLong},      // used with 'F', 'E', 'G' specifiers: always add a decimal point, even if no digits follow
    {"HEX_0X",              "0x8",                      value_isLong},      // used with 'X' (hex) specifier: preceed non-zero numbers with '0x'
    {"PAD_ZERO",            "0x10",                     value_isLong},      // pad with zeros

    {"INFO_ENTER",          "0",                        value_isLong},      // confirmation required by pressing ENTER (any preceding characters are skipped)
    {"INFO_ENTER_CANC",     "1",                        value_isLong},      // idem, but if '\c' encountered in input stream the operation is canceled by user 
    {"INFO_YN",             "2",                        value_isLong},      // only yes or no answer allowed, by pressing 'y' or 'n' followed by ENTER   
    {"INFO_YN_CANC",        "3",                        value_isLong},      // idem, but if '\c' encountered in input stream the operation is canceled by user 

    {"INPUT_NO_DEF",        "0",                        value_isLong},      // '\d' sequences ('default') in the input stream are ignored
    {"INPUT_ALLOW_DEF",     "1",                        value_isLong},      // if '\d' sequence is encountered in the input stream, default value is returned

    {"USER_CANCELED",       "0",                        value_isLong},      // operation was canceled by user (\c sequence encountered)
    {"USER_SUCCESS",        "1",                        value_isLong},      // operation was NOT canceled by user

    {"KEEP_MEM",            "0",                        value_isLong},      // keep Justina in memory on quitting
    {"RELEASE_MEM",         "1",                        value_isLong},      // release memory on quitting

    {"CONSOLE",             "0",                        value_isLong},      // IO: read from / print to console
    {"EXT_IO_1",            "-1",                       value_isLong},      // IO: read from / print to alternative I/O port 1 (if defined)
    {"EXT_IO_2",            "-2",                       value_isLong},      // IO: read from / print to alternative I/O port 2 (if defined)
    {"EXT_IO_3",            "-3",                       value_isLong},      // IO: read from / print to alternative I/O port 3 (if defined)
    {"FILE_1",              "1",                        value_isLong},      // IO: read from / print to open SD file 1
    {"FILE_2",              "2",                        value_isLong},      // IO: read from / print to open SD file 2 
    {"FILE_3",              "3",                        value_isLong},      // IO: read from / print to open SD file 3 
    {"FILE_4",              "4",                        value_isLong},      // IO: read from / print to open SD file 4 
    {"FILE_5",              "5",                        value_isLong},      // IO: read from / print to open SD file 5 

    {"READ",                "0x1",                      value_isLong},      // open SD file for read access
    {"WRITE",               "0x2",                      value_isLong},      // open SD file for write access
    {"RDWR",                "0x3",                      value_isLong},      // open SD file for r/w access

    {"APPEND",              "0x4",                      value_isLong},      // writes will occur at end of file
    {"SYNC",                "0x8",                      value_isLong},      //  
    {"CREATE_OK",           "0x10",                     value_isLong},      // create new file if non-existent
    {"EXCL",                "0x20",                     value_isLong},      // --> use together with flag 0x10 
    {"CREATE_ONLY",         "0x30",                     value_isLong},      // create new file only - do not open an existing file
    {"TRUNC",               "0x40",                     value_isLong},      // truncate file to zero bytes on open (NOT if file is opened for read access only)
};


// -------------------
// *   constructor   *
// -------------------

Justina_interpreter::Justina_interpreter(Stream** const pAltInputStreams, int altIOstreamCount,
    long progMemSize, int JustinaConstraints, int SDcardChipSelectPin) :
    _pExternIOstreams(pAltInputStreams), _externIOstreamCount(altIOstreamCount), _progMemorySize(progMemSize), _JustinaConstraints(JustinaConstraints), _SDcardChipSelectPin(SDcardChipSelectPin) {

    // settings to be initialized when cold starting interpreter only
    // --------------------------------------------------------------

    _coldStart = true;

    _housekeepingCallback = nullptr;
    for (int i = 0; i < _userCBarrayDepth; i++) { _callbackUserProcStart[i] = nullptr; }
    _userCBprocStartSet_count = 0;

    _resWordCount = (sizeof(_resWords)) / sizeof(_resWords[0]);
    _functionCount = (sizeof(_functions)) / sizeof(_functions[0]);
    _termTokenCount = (sizeof(_terminals)) / sizeof(_terminals[0]);
    _symbvalueCount = (sizeof(_symbNumConsts)) / sizeof(_symbNumConsts[0]);

    _lastPrintedIsPrompt = false;

    _programMode = false;
    _currenttime = millis();
    _previousTime = _currenttime;
    _lastCallBackTime = _currenttime;

    parsingStack.setListName("parsing ");
    evalStack.setListName("eval    ");
    flowCtrlStack.setListName("flowCtrl");
    parsedCommandLineStack.setListName("cmd line");

    if (_progMemorySize + IMM_MEM_SIZE > pow(2, 16)) { _progMemorySize = pow(2, 16) - IMM_MEM_SIZE; }
    _programStorage = new char[_progMemorySize + IMM_MEM_SIZE];

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

    // particular stream is a TCP stream ? Retrigger TCP keep alive timer at each character read (communicated to Justina via application flags)
    int TCP_externIOStreamIndex = ((_JustinaConstraints & 0xf0) >> 4) - 1;
    _pTCPstream = (TCP_externIOStreamIndex == -1) ? nullptr : _pExternIOstreams[TCP_externIOStreamIndex];

    initInterpreterVariables(true);
};


// ---------------------
// *   deconstructor   *
// ---------------------

Justina_interpreter::~Justina_interpreter() {
    if (!_keepInMemory) {
        resetMachine(true);                                                                             // delete all objects created on the heap: with = with user variables and FiFo stack
        _housekeepingCallback = nullptr;
        delete[] _programStorage;
        delete[] _pIOprintColumns;
    }

    printlnTo(0, "\r\nJustina: bye\r\n");
    for (int i = 0; i < 48; i++) { printTo(0, "="); } printlnTo(0, "\r\n");

};


// ------------------------------
// *   set call back functons   *
// ------------------------------

bool Justina_interpreter::setMainLoopCallback(void (*func)(long& appFlags)) {

    // set the address of an optional 'user callback' function
    // Justina will call this user routine at specific time intervals, allowing  the user...
    // ...to execute a specific routine regularly (e.g. to maintain a TCP connection, to implement a heartbeat, ...)

    _housekeepingCallback = func;
    return true;
}

bool Justina_interpreter::setUserFcnCallback(void(*func) (const void** data, const char* valueType, const int argCount)) {

    // set the address of an optional 'user callback' function
    // this mechanism allows to call user c++ procedures using aliases

    if (_userCBprocStartSet_count > +_userCBarrayDepth) { return false; }                               // throw away if callback array full
    _callbackUserProcStart[_userCBprocStartSet_count++] = func;
    return true; // success
}


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
    parseTokenResult_type result{ result_tokenFound };                                                          // init

    _appFlags = 0x0000L;                                                                                        // init application flags (for communication with Justina caller, using callbacks)

    printlnTo(0);
    for (int i = 0; i < 13; i++) { printTo(0, "*"); } printTo(0, "____");
    for (int i = 0; i < 4; i++) { printTo(0, "*"); } printTo(0, "__");
    for (int i = 0; i < 14; i++) { printTo(0, "*"); } printTo(0, "_");
    for (int i = 0; i < 10; i++) { printTo(0, "*"); }printlnTo(0);

    printTo(0, "    "); printlnTo(0, J_productName);
    printTo(0, "    "); printlnTo(0, J_legalCopyright);
    printTo(0, "    Version: "); printTo(0, J_productVersion); printTo(0, " ("); printTo(0, J_buildDate); printlnTo(0, ")");
    for (int i = 0; i < 48; i++) { printTo(0, "*"); } printlnTo(0);

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
    // 0 = no card reader, 1 = card reader present, do not yet initialise, 2 = initialise card now, 3 = init card & run start.txt function start() now
    if ((_JustinaConstraints & 0b0011) >= 2) {
        printTo(0, "\r\nLooking for an SD card...\r\n");
        execResult_type execResult = startSD();
        printTo(0, _SDinitOK ? "SD card found\r\n" : "SD card error: SD card NOT found\r\n");
    }

    if ((_JustinaConstraints & 0b0011) == 3) {
        // open startup file and retrieve file number (which would be one, normally)
        _initiateProgramLoad = _SDinitOK;
        if (_initiateProgramLoad) {
            printlnTo(0, "Looking for 'start.txt' program...");
            if (!SD.exists("start.txt")) { _initiateProgramLoad = false; printlnTo(0, "'start.txt' program NOT found"); }
        }

        if (_initiateProgramLoad) {
            execResult_type execResult = SD_open(_loadProgFromStreamNo, "start.txt", O_READ);                   // this performs a few card & file checks as well
            _initiateProgramLoad = (execResult == result_execOK);
            if (!_initiateProgramLoad) { printTo(0, "Could not open 'start.txt' program - error "); printlnTo(0, execResult); }
        }

        if (_initiateProgramLoad) {                                                                             // !!! second 'if(_initiateProgramLoad)'
            resetMachine(false);                                                                                // if 'warm' start, previous program (with its variables) may still exist
            _programMode = true;
            _programCounter = _programStorage;
            loadingStartupProgram = true;
            startJustinaWithoutAutostart = false;
            streamNumber = _loadProgFromStreamNo;                                                               // autostart step 1: temporarily switch from console input to startup file (opening the file here) 
            setStream(streamNumber, pStatementInputStream);                                                     // error checking done while opening file
            printTo(0, "Loading program 'start.txt'...\r\n");
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
        }

        do {        // one loop only
            if (bufferOverrun) { result = result_statementTooLong; }
            if (kill) { quitNow = true;  result = result_parse_kill; break; }
            if (forcedAbort) { result = result_parse_abort; }
            if (stdConsole && !_programMode) { result = result_parse_stdConsole; }
            if (noCharAdded) { break; }               // start a new outer loop (read a character if available, etc.)

            // if a statement is complete (terminated by a semicolon or end of input), parse it
            // --------------------------------------------------------------------------------
            bool isStatementSeparator = (!withinString) && (!within1LineComment) && (!withinMultiLineComment) && (c == ';') && !redundantSemiColon;
            isStatementSeparator = isStatementSeparator || (withinString && (c == '\n'));  // a new line character within a string is sent to parser as well

            bool statementReadyForParsing = !bufferOverrun && !forcedAbort && !stdConsole && !kill && (isStatementSeparator || (allCharsReceived && (statementCharCount > 0)));

            if (statementReadyForParsing) {                                                                     // if quitting anyway, just skip                                               
                _appFlags &= ~appFlag_errorConditionBit;                                                        // clear error condition flag 
                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_parsing;                                // status 'parsing'

                _statement[statementCharCount] = '\0';                                                          // add string terminator

                char* pStatement = _statement;                                                                  // because passed by reference 
                char* pDummy{};
                _parsingExecutingTraceString = false; _parsingEvalString = false;

                result = parseStatement(pStatement, pDummy, clearCmdIndicator);                                 // parse ONE statement only 
                if ((++parsedStatementCount & 0x3f) == 0) {
                    printTo(0, '.');                                                                            // print a dot each 64 parsed lines
                    if ((parsedStatementCount & 0x0fff) == 0) { printlnTo(0); }                                 // print a crlf each 64 dots
                }
                pErrorPos = pStatement;                                                                         // in case of error

                if (result != result_tokenFound) { flushAllUntilEOF = true; }

                // reset after each statement read 
                statementCharCount = 0;
                withinString = false; withinStringEscSequence = false; within1LineComment = false; withinMultiLineComment = false;
                lastCharWasSemiColon = false;
            }

            // program mode: complete program read and parsed   /   imm. mode: all statements in command line read and parsed ?
            if (allCharsReceived || (result != result_tokenFound)) {                                            // note: if all statements have been read, they also have been parsed
                if (kill) { quitNow = true; }
                else { quitNow = processAndExec(result, kill, lineCount, pErrorPos, clearCmdIndicator, pStatementInputStream, streamNumber); }  // return value: quit Justina now


                // parsing error occured ? reset input controlling variables
                if (result == result_tokenFound) {
                    if (loadingStartupProgram) { launchingStartFunction = true; }
                }
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

                clearCmdIndicator = 0;          // reset
                result = result_tokenFound;

                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_idle;                                   // status 'idle'
            }
        } while (false);

        if (quitNow) { break; }                                                                                 // user gave quit command


    } while (true);

    // returning control to Justina caller
    _appFlags = 0x0000L;                                                                                        // clear all application flags
    _housekeepingCallback(_appFlags);                                                                           // pass application flags to caller immediately

    if (kill) { _keepInMemory = false; printlnTo(0, "\r\n\r\n>>>>> Justina: kill request received from calling program <<<<<"); }

    SD_closeAllFiles();                                                                                         // safety (in case an SD card is present: close all files 
    _SDinitOK = false;
    SD.end();                                                                                                   // stop SD card
    while (_pConsoleIn->available() > 0) { readFrom(0); }                                                       //  empty console buffer before quitting

    if (_keepInMemory) {                                                                                        // NOTE: if remove from memory: message given in destructor
        printlnTo(0, "\r\nJustina: bye\r\n");
        for (int i = 0; i < 48; i++) { printTo(0, "="); } printlnTo(0, "\r\n");
    }

    return _keepInMemory;                                                                                       // return to calling program
}


// ----------------------------------------------------------------------------------
// *   add a character received from the input stream to the parsing input buffer   *
// ----------------------------------------------------------------------------------

bool Justina_interpreter::addCharacterToInput(bool& lastCharWasSemiColon, bool& withinString, bool& withinStringEscSequence, bool& within1LineComment, bool& withinMultiLineComment,
    bool& redundantSemiColon, bool ImmModeLineOrProgramRead, bool& bufferOverrun, bool _flushAllUntilEOF, int& _lineCount, int& _statementCharCount, char c) {

    const char commentOuterDelim = '/'; // twice: single line comment, followed by inner del.: start of multi-line comment, preceded by inner delimiter: end of multi-line comment 
    const char commentInnerDelim = '*';

    static bool lastCharWasWhiteSpace{ false };
    static char lastCommentChar{ '\0' };                                                                        // init: none

    bool redundantSpaces = false;                                                                               // init

    bufferOverrun = false;
    if ((c < ' ') && (c != '\n')) { return false; }                                                             // skip control-chars except new line and EOF character

    // when a imm. mode line or program is completely read and the last character (part of the last statement) received from input stream is not a semicolon, add it
    if (ImmModeLineOrProgramRead) {
        if (_statementCharCount > 0) {
            if (_statement[_statementCharCount - 1] != ';') {
                if (_statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }
                _statement[_statementCharCount] = ';';                                                          // still room: add character
                _statementCharCount++;
            }
        }

        within1LineComment = false;
        withinMultiLineComment = false;
    }

    // not at end of program or imm. mode line: process character   
    else {
        if (_flushAllUntilEOF) { return false; }                                                                // discard characters (after parsing error)

        if (c == '\n') { _lineCount++; }                                                                        // line number used when while reading program in input file

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
                        _statement[_statementCharCount] = '\0';                                                 // add string terminator

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
        if (redundantSpaces || redundantSemiColon || within1LineComment || withinMultiLineComment) { return false; }    // no character added
        if (_statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }

        // add character  
        _statement[_statementCharCount] = c;                                                                    // still room: add character
        ++_statementCharCount;
    }

    return true;
}


// ----------------------------------------------------------------------------------------------------------------------------
// *   finalise parsing; execute if no errors; if in debug mode, trace and print debug info; re-init machine state and exit   *
// ----------------------------------------------------------------------------------------------------------------------------

bool Justina_interpreter::processAndExec(parseTokenResult_type result, bool& kill, int lineCount, char* pErrorPos, int& clearIndicator,
    Stream*& pStatementInputStream, int& statementInputStreamNumber) {

    bool quitJustina{ false };

    // all statements (in program or imm. mode line) have been parsed: finalise
    // ------------------------------------------------------------------------

    int funcNotDefIndex;
    if (result == result_tokenFound) {
        // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
        if (_programMode && (!allExternalFunctionsDefined(funcNotDefIndex))) { result = result_function_undefinedFunctionOrArray; }
        if (_blockLevel > 0) { result = result_block_noBlockEnd; }
    }

    (_programMode ? _lastProgramStep : _lastUserCmdStep) = _programCounter;

    if (result == result_tokenFound) {
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
        // if parsing a program from console or other external I/O stream, provide feedback immediately after user pressed abort button and process remainder of input file (flush)
        if (_loadProgFromStreamNo <= 0) {
            if (_programMode) {
                if (result == result_parse_abort) { printTo(0, "\r\nAbort: "); }                                // not for other parsing errors
                else { printTo(0, "\r\nParsing error: "); }
                if (result != result_tokenFound) { printlnTo(0, "processing remainder of input file... please wait"); }
            }

            // process (flush) remainder of input file
            long byteInCount{ 0 };
            char c{};
            do {                                                                                                // process remainder of input file (flush)
                // NOTE: forcedStop and forcedAbort are dummy arguments here and will be ignored because already flushing input file after error, abort or kill
                bool forcedStop{ false }, forcedAbort{ false }, stdConsDummy{ false };                          // dummy arguments (not needed here)
                c = getCharacter(kill, forcedStop, forcedAbort, stdConsDummy, _programMode);
                if (kill) { result = result_parse_kill; break; }                                                // kill while processing remainder of file
                if ((++byteInCount & 0x0fff) == 0) {
                    printTo(0, '.');
                    if ((byteInCount & 0x03ffff) == 0) { printlnTo(0); }                                        // print a dot each 4096 lines, a crlf each 64 dots
                }
            } while (c != 0xFF);
        }

        if (result == result_parse_abort) {
            printlnTo(0, "\r\n+++ Abort: parsing terminated +++");                                              // abort: display error message 
        }
        else if (result == result_parse_stdConsole) {
            printlnTo(0, "\r\n+++ console reset +++");
            _consoleIn_sourceStreamNumber = _consoleOut_sourceStreamNumber = -1;
            _pConsoleIn = _pConsoleOut = _pExternIOstreams[0];                                                  // set console to stream -1 (NOT debug out)
            _pConsolePrintColumn = &_pIOprintColumns[0];
            *_pConsolePrintColumn = 0;

        }
        else if (result == result_parse_kill) { quitJustina = true; }
        else { printParsingResult(result, funcNotDefIndex, _statement, lineCount, pErrorPos); }                 // parsing error occured: print error message

    }


    // if not in program mode and no parsing error: execute
    // ----------------------------------------------------
    execResult_type execResult{ result_execOK };
    if (!_programMode && (result == result_tokenFound)) {
        execResult = exec(_programStorage + _progMemorySize);                                                   // execute parsed user statements
        if (execResult == result_kill) { kill = true; }
        if (kill || (execResult == result_quit)) { printlnTo(0); quitJustina = true; }                          // make sure Justina prompt will be printed on a new line
    }

    // if in debug mode, trace expressions (if defined) and print debug info 
    // ---------------------------------------------------------------------
    if ((_openDebugLevels > 0) && (execResult != result_kill) && (execResult != result_quit) && (execResult != result_initiateProgramLoad)) { traceAndPrintDebugInfo(); }

    // re-init or reset interpreter state 
    // ----------------------------------

    // if program parsing error: reset machine, because variable storage might not be consistent with program any more
    if ((_programMode) && (result != result_tokenFound)) { resetMachine(false); }

    // before loadng a program, clear memory except user variables
    else if (execResult == result_initiateProgramLoad) { resetMachine(false); }

    // no program error (could be immmediate mode error however), not initiating program load: only reset a couple of items here 
    else {
        parsingStack.deleteList();
        _blockLevel = 0;
        _extFunctionBlockOpen = false;
    }

    // the clear memory / clear all command is executed AFTER the execution phase
    // --------------------------------------------------------------------------

    // first check there were no parsing or execution errors
    if ((result == result_tokenFound) && (execResult == result_execOK)) {
        if (clearIndicator != 0) {                     // 1 = clear program cmd, 2 = clear all cmd 
            while (_pConsoleIn->available() > 0) { readFrom(0); }                                               // empty console buffer first (to allow the user to start with an empty line)
            do {
                char s[50];
                sprintf(s, "===== Clear %s ? (please answer Y or N) =====", ((clearIndicator == 2) ? "memory" : "program"));
                printlnTo(0, s);

                // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                // return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
                bool doStop{ false }, doAbort{ false }, doCancel{ false }, doDefault{ false };      // not used but mandatory
                int length{ 1 };
                char input[1 + 1] = "";                                                                         // init: empty string. Provide room for 1 character + terminating '\0'
                // NOTE: stop, cancel land default arguments have no function here (execution has ended already), but abort and kill do
                if (getConsoleCharacters(doStop, doAbort, doCancel, doDefault, input, length, '\n')) { kill = true; quitJustina = true; break; }  // kill request from caller ?

                if (doAbort) { break; }        // avoid a next loop (getConsoleCharacters exits immediately when abort request received, not waiting for any characters)
                bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                if (validAnswer) {
                    // 1 = clear program, 2 = clear all (including user variables)
                    if (tolower(input[0]) == 'y') { printlnTo(0, (clearIndicator == 2) ? "clearing memory" : "clearing program"); resetMachine(clearIndicator == 2); }
                    break;
                }
            } while (true);
        }
    }

    // execution finished (not stopping in debug mode), with or without error: delete parsed strings in imm mode command : they are on the heap and not needed any more. Identifiers must stay avaialble
    // -> if stopping a program for debug, do not delete parsed strings (in imm. mode command), because that command line has now been pushed on  ...
     // the parsed command line stack and included parsed constants will be deleted later (resetMachine routine)
    if (execResult != result_stopForDebug) { deleteConstStringObjects(_programStorage + _progMemorySize); } // always

    // finalize: last actions before 'ready' mode (prompt displayed depending on settings)
    // -----------------------------------------------------------------------------------
    _programMode = false;
    _programCounter = _programStorage + _progMemorySize;                                                        // start of 'immediate mode' program area
    *(_programStorage + _progMemorySize) = tok_no_token;                                                        //  current end of program (immediate mode)

    if (execResult == result_initiateProgramLoad) {                                                             // initiate program load 
        _programMode = true;
        _programCounter = _programStorage;

        if (_lastPrintedIsPrompt) { printlnTo(0); }                                                             // print new line if last printed was a prompt
        printTo(0, (_loadProgFromStreamNo > 0) ? "Loading program...\r\n" : "Loading program... please wait\r\n");
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

    // has an error occured ? (exclude 'events' reported as an error)
    bool isError = (result != result_tokenFound) || ((execResult != result_execOK) && (execResult < result_startOfEvents));
    isError ? (_appFlags |= appFlag_errorConditionBit) : (_appFlags &= ~appFlag_errorConditionBit);             // set or clear error condition flag 
    (_appFlags &= ~appFlag_statusMask);
    (_openDebugLevels > 0) ? (_appFlags |= appFlag_stoppedInDebug) : (_appFlags |= appFlag_idle);               // status 'debug mode' or 'idle'

    // print new prompt and exit
    // -------------------------
    _lastPrintedIsPrompt = false;
    if ((_promptAndEcho != 0) && (execResult != result_initiateProgramLoad)) { printTo(0, "Justina> "); _lastPrintedIsPrompt = true; }

    return quitJustina;
}

// -------------------------------------------------------------------------
// *   trace expressions as defined in Trace statement, print debug info   *
// -------------------------------------------------------------------------

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
    do {                                                                                                        // there is at least one open function in the call stack
        blockType = *(char*)pFlowCtrlStackLvl;
        if (blockType == block_extFunction) { break; }
        pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
    } while (true);

    pDeepestOpenFunction = (OpenFunctionData*)pFlowCtrlStackLvl;                                                // deepest level of nested functions
    nextStatementPointer = pDeepestOpenFunction->pNextStep;

    printlnTo(0); for (int i = 1; i <= _dispWidth; i++) { printTo(0, "-"); } printlnTo(0);
    parseAndExecTraceString();                                                                                  // trace string may not contain keywords, external functions, generic names
    char msg[150] = "";
    sprintf(msg, "DEBUG ==>> NEXT [%s: ", extFunctionNames[pDeepestOpenFunction->functionIndex]);
    printTo(0, msg);
    prettyPrintStatements(10, nextStatementPointer);

    if (_openDebugLevels > 1) {
        sprintf(msg, "*** this + %d other programs STOPPED ***", _openDebugLevels - 1);
        printlnTo(0, msg);
    }
}


// -----------------------------------------------
// *   parse and exec trace string expressions   *
// -----------------------------------------------

// trace string may not contain keywords, external functions, generic names

void Justina_interpreter::parseAndExecTraceString() {
    char* pNextParseStatement{};

    if (_pTraceString == nullptr) { return; }                                                                   // no trace string: nothing to trace

    // trace string expressions will be parsed and executed from immediate mode program storage: 
    // before overwriting user statements that were just parsed and executed, delete parsed strings
    deleteConstStringObjects(_programStorage + _progMemorySize);

    bool valuePrinted{ false };
    char* pTraceParsingInput = _pTraceString;                                                                   // copy pointer to start of trace string
    _parsingExecutingTraceString = true;

    printTo(0, "TRACE ==>> ");
    do {
        // init
        *(_programStorage + _progMemorySize) = tok_no_token;                                                    // in case no valid tokens will be stored
        _programCounter = _programStorage + _progMemorySize;                                                    // start of 'immediate mode' program area

        // skip any spaces and semi-colons in the input stream
        while ((pTraceParsingInput[0] == ' ') || (pTraceParsingInput[0] == term_semicolon[0])) { pTraceParsingInput++; }
        if (*pTraceParsingInput == '\0') { break; }                                                             // could occur if semicolons skipped

        // parse ONE trace string expression only
        if (valuePrinted) { printTo(0, ", "); }                                                                 // separate values (if more than one)

        // note: application flags are not adapted (would not be passed to caller immediately)
        int dummy{};
        parseTokenResult_type result = parseStatement(pTraceParsingInput, pNextParseStatement, dummy);
        if (result == result_tokenFound) {
            // do NOT pretty print if parsing error, to avoid bad-looking partially printed statements (even if there will be an execution error later)
            prettyPrintStatements(0);         
            printTo(0, ": ");                                                                                   // resulting value will follow
            pTraceParsingInput = pNextParseStatement;
        }
        else {
            char  errStr[12];                                                                                   // includes place for terminating '\0'
            // if parsing error, print error instead of value AND CONTINUE with next trace expression (if any)
            sprintf(errStr, "<ErrP%d>", (int)result);                                                           
            printTo(0, errStr);
            // pNextParseStatement not yet correctly positioned: set to next statement
            while ((pTraceParsingInput[0] != term_semicolon[0]) && (pTraceParsingInput[0] != '\0')) { ++pTraceParsingInput; }
            if (pTraceParsingInput[0] == term_semicolon[0]) { ++pTraceParsingInput; }
        }

        // if parsing went OK: execute ONE parsed expression (just parsed now)
        execResult_type execResult{ result_execOK };
        if (result == result_tokenFound) {
            execResult = exec(_programStorage + _progMemorySize);                                               // note: value or exec. error is printed from inside exec()
        }

        valuePrinted = true;

        // execution finished: delete parsed strings in imm mode command (they are on the heap and not needed any more)
        deleteConstStringObjects(_programStorage + _progMemorySize);                                            // always
        *(_programStorage + _progMemorySize) = tok_no_token;                                                    //  current end of program (immediate mode)

    } while (*pTraceParsingInput != '\0');                                                                      // exit loop if all expressions handled

    _parsingExecutingTraceString = false;
    println(0);       // go to next output line

    return;
}


// --------------------------------------------------------------
// *   check if all external functions referenced are defined   *
// --------------------------------------------------------------

bool Justina_interpreter::allExternalFunctionsDefined(int& index) {
    index = 0;
    while (index < _extFunctionCount) {                                                                         // points to variable in use
        if (extFunctionData[index].pExtFunctionStartToken == nullptr) { return false; }
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

            _appFlags &= ~(appFlag_dataInOut | appFlag_TCPkeepAlive);                                           // reset 'external IO' flags 
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

    // delete identifier name objects on the heap (variable names, external function names) 
    deleteIdentifierNameObjects(programVarNames, _programVarNameCount);
    deleteIdentifierNameObjects(extFunctionNames, _extFunctionCount);
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
            _pDebugOut->print("----- (system var str) "); _pDebugOut->println((uint32_t)_pTraceString, HEX);
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
        _pDebugOut->print("*** Variable / function name objects cleanup error. Remaining: "); _pDebugOut->println(_identifierNameStringObjectCount);
    #endif
        _identifierNameStringObjectErrors += abs(_identifierNameStringObjectCount);
    }

    if (_parsedStringConstObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("*** Parsed constant string objects cleanup error. Remaining: "); _pDebugOut->println(_parsedStringConstObjectCount);
    #endif
        _parsedStringConstObjectErrors += abs(_parsedStringConstObjectCount);
    }

    if (_globalStaticVarStringObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("*** Variable string objects cleanup error. Remaining: "); _pDebugOut->println(_globalStaticVarStringObjectCount);
    #endif
        _globalStaticVarStringObjectErrors += abs(_globalStaticVarStringObjectCount);
    }

    if (_globalStaticArrayObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("*** Array objects cleanup error. Remaining: "); _pDebugOut->println(_globalStaticArrayObjectCount);
    #endif
        _globalStaticArrayObjectErrors += abs(_globalStaticArrayObjectCount);
    }

#if PRINT_DEBUG_INFO
    _pDebugOut->print("\r\n** Reset stats\r\n    parsed strings "); _pDebugOut->print(_parsedStringConstObjectCount);

    _pDebugOut->print(", prog name strings "); _pDebugOut->print(_identifierNameStringObjectCount);
    _pDebugOut->print(", prog var strings "); _pDebugOut->print(_globalStaticVarStringObjectCount);
    _pDebugOut->print(", prog arrays "); _pDebugOut->print(_globalStaticArrayObjectCount);
#endif

    if (withUserVariables) {
        if (_userVarNameStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** User variable name objects cleanup error. Remaining: "); _pDebugOut->println(_userVarNameStringObjectCount);
        #endif
            _userVarNameStringObjectErrors += abs(_userVarNameStringObjectCount);
        }

        if (_userVarStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** User variable string objects cleanup error. Remaining: "); _pDebugOut->println(_userVarStringObjectCount);
        #endif
            _userVarStringObjectErrors += abs(_userVarStringObjectCount);
        }

        if (_userArrayObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** User array objects cleanup error. Remaining: "); _pDebugOut->println(_userArrayObjectCount);
        #endif
            _userArrayObjectErrors += abs(_userArrayObjectCount);
        }

        if (_systemVarStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** System variable string objects cleanup error. Remaining: "); _pDebugOut->println(_systemVarStringObjectCount);
        #endif
            _systemVarStringObjectErrors += abs(_systemVarStringObjectCount);
        }

        if (_lastValuesStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** Last value FiFo string objects cleanup error. Remaining: "); _pDebugOut->print(_lastValuesStringObjectCount);
        #endif
            _lastValuesStringObjectErrors += abs(_lastValuesStringObjectCount);
        }

    #if PRINT_DEBUG_INFO
        _pDebugOut->print(", user var names "); _pDebugOut->print(_userVarNameStringObjectCount);
        _pDebugOut->print(", user var strings "); _pDebugOut->print(_userVarStringObjectCount);
        _pDebugOut->print(", user arrays "); _pDebugOut->print(_userArrayObjectCount);

        _pDebugOut->print(", last value strings "); _pDebugOut->print(_lastValuesStringObjectCount);
    #endif
    }
}


// --------------------------------------------
// *   initialise interpreter object fields   *
// --------------------------------------------

void Justina_interpreter::initInterpreterVariables(bool fullReset) {

    // intialised at cold start AND each time the interpreter is reset

    _blockLevel = 0;
    _extFunctionCount = 0;
    _paramOnlyCountInFunction = 0;
    _localVarCountInFunction = 0;
    _localVarCount = 0;
    _staticVarCountInFunction = 0;
    _staticVarCount = 0;
    _extFunctionBlockOpen = false;

    _programVarNameCount = 0;
    if (fullReset) { _userVarCount = 0; }
    else {
        int index = 0;                                                                                          // clear user variable flag 'variable is used by program'
        while (index++ < _userVarCount) { userVarType[index] = userVarType[index] & ~var_userVarUsedByProgram; }
    }
    _userCBprocAliasSet_count = 0;                                                                              // note: _userCBprocStartSet_count: only reset when starting interpreter

    *_programStorage = tok_no_token;                                                                            //  set as current end of program 
    *(_programStorage + _progMemorySize) = tok_no_token;                                                        //  set as current end of program (immediate mode)
    _programCounter = _programStorage + _progMemorySize;                                                        // start of 'immediate mode' program area

    _programName[0] = '\0';

    _pEvalStackTop = nullptr;   _pEvalStackMinus2 = nullptr; _pEvalStackMinus1 = nullptr;
    _pFlowCtrlStackTop = nullptr;   _pFlowCtrlStackMinus2 = nullptr; _pFlowCtrlStackMinus1 = nullptr;
    _pImmediateCmdStackTop = nullptr;

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

    // calculation result print format
    _dispWidth = DEFAULT_CALC_RESULT_PRINT_WIDTH; _dispNumPrecision = DEFAULT_NUM_PRECISION;
    _dispCharsToPrint = DEFAULT_STRCHAR_TO_PRINT; _dispFmtFlags = _defaultPrintFlags;
    _dispNumSpecifier[0] = 'G'; _dispNumSpecifier[1] = '\0';
    _dispIsIntFmt = false;
    makeFormatString(_dispFmtFlags, false, _dispNumSpecifier, _dispNumberFmtString);                // for numbers
    strcpy(_dispStringFmtString, "%*.*s%n");                                                        // for strings

    // print command argument format
    _printWidth = DEFAULT_PRINT_WIDTH; _printNumPrecision = DEFAULT_NUM_PRECISION;
    _printCharsToPrint = DEFAULT_STRCHAR_TO_PRINT; _printFmtFlags = _defaultPrintFlags;
    _printNumSpecifier[0] = 'G'; _printNumSpecifier[1] = '\0';

    // display output settings
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
        _pDebugOut->print(isUserVar ? "----- (usrvar name) " : "----- (ident name ) "); _pDebugOut->println((uint32_t) * (pIdentNameArray + index), HEX);
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
            _pDebugOut->print(isUserVar ? "----- (usr arr str) " : isLocalVar ? "-----(loc arr str)" : "----- (arr string ) "); _pDebugOut->println((uint32_t)pString, HEX);     // applicable to string and array (same pointer)
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
                _pDebugOut->print(isUserVar ? "----- (usr ar stor) " : isLocalVar ? "----- (loc ar stor) " : "----- (array stor ) "); _pDebugOut->println((uint32_t)varValues[index].pArray, HEX);
            #endif
                isUserVar ? _userArrayObjectCount-- : isLocalVar ? _localArrayObjectCount-- : _globalStaticArrayObjectCount--;
                delete[]  varValues[index].pArray;
            }
            else if ((varType[index] & value_typeMask) == value_isStringPointer) {                                                  // variable is a scalar containing a string
                if (varValues[index].pStringConst != nullptr) {
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print(isUserVar ? "----- (usr var str) " : isLocalVar ? "----- (loc var str)" : "----- (var string ) "); _pDebugOut->println((uint32_t)varValues[index].pStringConst, HEX);
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
            _pDebugOut->print("----- (FiFo string) "); _pDebugOut->println((uint32_t)lastResultValueFiFo[i].pStringConst, HEX);
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
                _pDebugOut->print("----- (parsed str ) ");   _pDebugOut->println((uint32_t)pAnum, HEX);
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
    _pDebugOut->print((varScope == var_isUser) ? "----- (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
    _pDebugOut->println((uint32_t)*pStackLvl->varOrConst.value.ppStringConst, HEX);
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
    _pDebugOut->print("----- (Intermd str) ");   _pDebugOut->println((uint32_t)_pEvalStackTop->varOrConst.value.pStringConst, HEX);
#endif
    _intermediateStringObjectCount--;
    delete[] pStackLvl->varOrConst.value.pStringConst;

    return result_execOK;
}


