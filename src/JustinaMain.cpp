/***********************************************************************************************************
*   Justina interpreter library                                                                            *
*                                                                                                          *
*   Copyright 2024, Herwig Taveirne                                                                        *
*                                                                                                          *
*   This file is part of the Justina Interpreter library.                                                  *
*   The Justina interpreter library is free software: you can redistribute it and/or modify it under       *
*   the terms of the GNU General Public License as published by the Free Software Foundation, either       *
*   version 3 of the License, or (at your option) any later version.                                       *
*                                                                                                          *
*   This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;              *
*   without even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.             *
*   See the GNU General Public License for more details.                                                   *
*                                                                                                          *
*   You should have received a copy of the GNU General Public License along with this program. If not,     *
*   see https://www.gnu.org/licenses.                                                                      *
*                                                                                                          *
*   The library is intended to work with 32 bit boards using the SAMD architecture ,                       *
*   the Arduino nano RP2040 and Arduino nano ESP32 boards.                                                 *
*                                                                                                          *
*   See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                          *
***********************************************************************************************************/


#include "Justina.h"

#define PRINT_HEAP_OBJ_CREA_DEL 0
#define PRINT_DEBUG_INFO 0
#define PRINT_OBJECT_COUNT_ERRORS 0


// *****************************************************************
// ***        class Justina - implementation         ***
// *****************************************************************

// ----------------------------------------------
// *   initialization of static class members   *
// ----------------------------------------------


// commands: keywords with attributes
// ----------------------------------


const Justina::ResWordDef Justina::_resWords[]{
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

    {"for",             cmdcod_for,             cmd_onlyImmOrInsideFuncBlock,                           2,3,    cmdPar_109,     cmdBlockFor},
    {"while",           cmdcod_while,           cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockWhile},
    {"if",              cmdcod_if,              cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockIf},
    {"elseif",          cmdcod_elseif,          cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockIf_elseIf},
    {"else",            cmdcod_else,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockIf_else},
    {"end",             cmdcod_end,             cmd_noRestrictions,                                     0,0,    cmdPar_102,     cmdBlockGenEnd},                // closes inner open command block

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

    {"abort",           cmdcod_abort,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"debug",           cmdcod_debug,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},

    {"trace",           cmdcod_trace,           cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},
    {"viewExprOn",      cmdcod_traceExprOn,     cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},
    {"viewExprOff",     cmdcod_traceExprOff,    cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_102,     cmdBlockNone},

    {"BPon",            cmdcod_BPon,            cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"BPoff",           cmdcod_BPoff,           cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"BPactivate",      cmdcod_BPactivate,      cmd_onlyImmediate,                                      0,0,    cmdPar_102,     cmdBlockNone},
    {"setBP",           cmdcod_setBP,           cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},
    {"clearBP",         cmdcod_clearBP,         cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},
    {"enableBP",        cmdcod_enableBP,        cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},
    {"disableBP",       cmdcod_disableBP,       cmd_onlyImmediate,                                      1,9,    cmdPar_112,     cmdBlockNone},
    {"moveBP",          cmdcod_moveBP,          cmd_onlyImmediate,                                      2,2,    cmdPar_105,     cmdBlockNone},

    {"raiseError",      cmdcod_raiseError,      cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},
    {"trapErrors",      cmdcod_trapErrors,      cmd_onlyImmOrInsideFuncBlock,                           1,1,    cmdPar_104,     cmdBlockNone},
    {"clearError",      cmdcod_clearError,      cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_104,     cmdBlockNone},
    {"quit",            cmdcod_quit,            cmd_onlyImmOrInsideFuncBlock,                           0,0,    cmdPar_106,     cmdBlockNone},

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
    {"copyFile",        cmdcod_copyFile,        cmd_onlyImmOrInsideFuncBlock,                           2,3,    cmdPar_107,     cmdBlockNone},

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

const Justina::InternCppFuncDef Justina::_internCppFunctions[]{
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
    {"wait",                    fnccod_delay,                   1,1,    0b0},               // delay microseconds: doesn't make sense, because execution is not fast enough (interpreter)
    {"pinMode",                 fnccod_pinMode,                 2,2,    0b0},
    {"digitalRead",             fnccod_digitalRead,             1,1,    0b0},
    {"digitalWrite",            fnccod_digitalWrite,            2,2,    0b0},
    {"analogRead",              fnccod_analogRead,              1,1,    0b0},
    {"analogReference",         fnccod_analogReference,         1,1,    0b0},
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
    {"maskedWordRead",          fnccod_wordMaskedRead,          2,2,    0b0},
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
    {"repeatChar",              fnccod_repeatchar,              2,2,    0b0},
    {"replaceChar",             fnccod_replaceChar,             2,3,    0b0},
    {"findStr",                 fnccod_findsubstr,              2,3,    0b0},
    {"replaceStr",              fnccod_replacesubstr,           3,4,    0b0},
    {"strCmp",                  fnccod_strcmp,                  2,2,    0b0},
    {"strCaseCmp",              fnccod_strcasecmp,              2,2,    0b0},
    {"ascToHexStr",             fnccod_ascToHexString,          1,1,    0b0},
    {"hexStrToAsc",             fnccod_hexStringToAsc,          1,2,    0b0},
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
    {"r",                       fnccod_last,                    0,1,    0b0 },              // short label for 'last result'
    {"err",                     fnccod_getTrappedErr,           0,1,    0b0 },
    {"isColdStart",             fnccod_isColdStart,             0,0,    0b0 },
    {"sysVal",                  fnccod_sysVal,                  1,1,    0b0},

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

    {"fmt",                     fnccod_format,                 1,6,    0b0},                // short label for 'system value'
    {"tab",                     fnccod_tab,                    0,1,    0b0},
    {"col",                     fnccod_gotoColumn,             1,1,    0b0},
    {"pos",                     fnccod_getColumnPos,           0,0,    0b0},

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
    {"isInUse",                 fnccod_hasOpenFile,            1,1,    0b0 },
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

const Justina::TerminalDef Justina::_terminals[]{

    //  name                id code                 prefix prio          infix prio                 postfix prio         
    //  ----                -------                 -----------          ----------                 ------------   

    // non-operator terminals: ONE character only, character should NOT appear in operator names

    {term_semicolon,        termcod_semicolon_BPset,    0x00,               0x00,                       0x00},
    {term_semicolon,        termcod_semicolon_BPallowed,0x00,               0x00,                       0x00},
    {term_semicolon,        termcod_semicolon,          0x00,               0x00,                       0x00},      // MUST directly follow two previous 'semicolon operator' entries
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

    {term_plus,             termcod_plus,               0x0C,               0x0A,                       0x00},      // note: for strings, means 'concatenate'
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


// predefined constants
// --------------------

// these symbolic names can be used in Justina programs instead of the values themselves
// symbolic names should not contain '\' or '#' characters and should have a valid (identifier name) length

const Justina::SymbNumConsts Justina::_symbNumConsts[]{

    // name                 // value                    // value type
    // ----                 --------                    // ----------

    // math: floating point constants (with more precision than what will actually be used)
    {"e",                   "2.7182818284590452354",    value_isFloat},         // base of natural logarithm (more digits then actually needed for float)
    {"PI",                  "3.14159265358979323846",   value_isFloat},         // PI (more digits then actually needed for float)
    {"HALF_PI",             "1.57079632679489661923",   value_isFloat},         // PI / 2
    {"QUART_PI",            "0.78539816339744830962",   value_isFloat},         // PI / 4
    {"TWO_PI",              "6.2831853071795864769",    value_isFloat},         // 2 * PI 

    {"DEG_TO_RAD",          "0.01745329251994329577",   value_isFloat},         // conversion factor: degrees to radians
    {"RAD_TO_DEG",          "57.2957795130823208768",   value_isFloat},         // radians to degrees

    // angle mode
    {"RADIANS",             "0",                        value_isLong},
    {"DEGREES",             "1",                        value_isLong},

    // boolean values
    {"FALSE",               "0",                        value_isLong},          // value for boolean 'false'
    {"TRUE",                "1",                        value_isLong},          // value for boolean 'true'

    {"OFF",                 "0",                        value_isLong},          // value for boolean 'false'
    {"ON",                  "1",                        value_isLong},          // value for boolean 'true'

    // data types
    {"INTEGER",             "1",                        value_isLong},          // value type of an integer value
    {"FLOAT",               "2",                        value_isLong},          // value type of a float value
    {"STRING",              "3",                        value_isLong},          // value type of a string value

    // digital I/O
    {"LOW",                 "0",                        value_isLong},          // standard ARduino constants for digital I/O
    {"HIGH",                "1",                        value_isLong},
    {"INPUT",               "0x1",                      value_isLong},          // standard ARduino constants for digital I/O
    {"OUTPUT",              "0x3",                      value_isLong},
    {"INPUT_PULLUP",        "0x5",                      value_isLong},
    {"INPUT_PULLDOWN",      "0x9",                      value_isLong},
#if (defined ARDUINO_ARCH_ESP32) 
    {"LED_BUILTIN",         "13",                       value_isLong},
    {"LED_RED",             "14",                       value_isLong},
    {"LED_GREEN",           "15",                       value_isLong},
    {"LED_BLUE",            "16",                       value_isLong},
#else
    {"LED_BUILTIN",         "13",                       value_isLong},
#endif
    {"LSBFIRST",            "0x0",                      value_isLong},          // standard ARduino constants for digital I/O
    {"MSBFIRST",            "0x1",                      value_isLong},

    // display mode command first argument: prompt and echo display
    {"NO_PROMPT",           "0",                        value_isLong},          // do not print prompt and do not echo user input
    {"PROMPT",              "1",                        value_isLong},          // print prompt but no not echo user input
    {"ECHO",                "2",                        value_isLong},          // print prompt and echo user input

    // display mode command second argument: last result format
    {"NO_RESULTS",          "0",                        value_isLong},          // do not print last result
    {"RESULTS",             "1",                        value_isLong},          // print last result
    {"QUOTE_RES",           "2",                        value_isLong},          // print last result, quote string results 

    // info command: type of confirmation required ("request answer yes/no, ...")
    {"ENTER",               "0",                        value_isLong},          // confirmation required by pressing ENTER (any preceding characters are skipped)
    {"ENTER_CANCEL",        "1",                        value_isLong},          // idem, but if '\c' encountered in input stream the operation is canceled by user 
    {"YES_NO",              "2",                        value_isLong},          // only yes or no answer allowed, by pressing 'y' or 'n' followed by ENTER   
    {"YN_CANCEL",           "3",                        value_isLong},          // idem, but if '\c' encountered in input stream the operation is canceled by user 

    // input command: default allowed  
    {"NO_DEFAULT",          "0",                        value_isLong},          // '\d' sequences ('default') in the input stream are ignored
    {"ALLOW_DEFAULT",       "1",                        value_isLong},          // if '\d' sequence is encountered in the input stream, default value is returned

    // input and info command: flag 'user canceled' (input argument 3 / info argument 2 return value - argument must be a variable)
    {"CANCELED",            "0",                        value_isLong},          // operation was canceled by user (\c sequence encountered)
    {"OK",                  "1",                        value_isLong},          // OK 
    {"NOK",                 "-1",                       value_isLong},          // NOT OK 

    // input / output streams
    {"CONSOLE",             "0",                        value_isLong},          // IO: read from / print to console
    {"IO1",                 "-1",                       value_isLong},          // IO: read from / print to alternative I/O port 1 (if defined)
    {"IO2",                 "-2",                       value_isLong},          // IO: read from / print to alternative I/O port 2 (if defined)
    {"IO3",                 "-3",                       value_isLong},          // IO: read from / print to alternative I/O port 3 (if defined)
    {"IO4",                 "-4",                       value_isLong},          // IO: read from / print to alternative I/O port 4 (if defined)
    {"FILE1",               "1",                        value_isLong},          // IO: read from / print to open SD file 1
    {"FILE2",               "2",                        value_isLong},          // IO: read from / print to open SD file 2 
    {"FILE3",               "3",                        value_isLong},          // IO: read from / print to open SD file 3 
    {"FILE4",               "4",                        value_isLong},          // IO: read from / print to open SD file 4 
    {"FILE5",               "5",                        value_isLong},          // IO: read from / print to open SD file 5 

    // file access type on open: constants can be bitwise 'or'ed
    // READ can be combined with WRITE or APPEND; APPEND automatically includes WRITE (but only possible to append)
    {"READ",                "0x1",                      value_isLong},          // open SD file for read access
    {"WRITE",               "0x2",                      value_isLong},          // open SD file for write access
    {"APPEND",              "0x6",                      value_isLong},          // open SD file for write access, writes will occur at end of file
    // NOTE: next 4 file access constants HAVE NO FUNCTION with nano ESP32 boards - they don't do anything
    {"SYNC",                "0x8",                      value_isLong},          // synchronous writes: send data physically to the card after each write 
    {"NEW_OK",              "0x10",                     value_isLong},          // creating new files if non-existent is allowed, open existing files
    {"NEW_ONLY",            "0x30",                     value_isLong},          // create new file only - do not open an existing file
    {"TRUNC",               "0x40",                     value_isLong},          // truncate file to zero bytes on open (NOT if file is opened for read access only)

    // file EOF 
    {"EOF",                 "-1",                       value_isLong},          // seek(n, EOF) is same as seek(n, size(n))

    // formatting: specifiers for floating point numbers
    {"FIXED",               "f",                        value_isStringPointer},     // fixed point notation
    {"EXP_U",               "E",                        value_isStringPointer},     // scientific notation, exponent: 'E'
    {"EXP",                 "e",                        value_isStringPointer},     // scientific notation, exponent: 'e' 
    {"SHORT_U",             "G",                        value_isStringPointer},     // shortest notation possible; if exponent: 'E' 
    {"SHORT",               "g",                        value_isStringPointer },    // shortest notation possible; if exponent: 'e'   

    // formatting: specifiers for integers
    {"DEC",                 "d",                        value_isStringPointer},     // base 10 (decimal)
    {"HEX_U",               "X",                        value_isStringPointer},     // base 16 (hex), digits A..F
    {"HEX",                 "x",                        value_isStringPointer},     // base 16 (hex), digits a..f

    // formatting: specifier for character strings 
    {"CHARS",               "s",                        value_isStringPointer },    // character string   

    // formatting: flags
    {"FMT_LEFT",            "0x01",                      value_isLong},         // align output left within the print field 
    {"FMT_SIGN",            "0x02",                      value_isLong},         // always add a sign (- or +) preceding the value
    {"FMT_SPACE",           "0x04",                      value_isLong},         // precede the value with a space if no sign is written 
    {"FMT_POINT",           "0x08",                      value_isLong},         // if used with 'F', 'E', 'G' specifiers: add decimal point, even if no digits after decimal point  
    {"FMT_0X",              "0x08",                      value_isLong},         // if used with hex output 'X' specifier: precede non-zero values with 0x  
    {"FMT_000",             "0x10",                      value_isLong},         // if used with 'F', 'E', 'G' specifiers: pad with zeros 
    {"FMT_NONE",            "0x00",                      value_isLong},         // no flags 

    // boards
    {"BOARD_OTHER",         "0",                         value_isLong },        // board architecture is undefined
    {"BOARD_SAMD",          "1",                         value_isLong },        // board architecture is SAMD
    {"BOARD_RP2040",        "2",                         value_isLong },        // board architecture is RP2040 
    {"BOARD_ESP32",         "3",                         value_isLong },        // board architecture is ESP32 

};


// -------------------
// *   constructor   *
// -------------------

Justina::Justina(int JustinaStartupOptions, int SDcardChipSelectPin) : _justinaStartupOptions(JustinaStartupOptions), _SDcardChipSelectPin(SDcardChipSelectPin), _externIOstreamCount(1) {
    _ppExternInputStreams = _pDefaultExternalInput;
    _ppExternOutputStreams = _pDefaultExternalOutput;

    constructorCommonPart();
}


Justina::Justina(Stream** const ppAltInputStreams, Print** const ppAltOutputStreams, int altIOstreamCount, int JustinaStartupOptions, int SDcardChipSelectPin) :
    _ppExternInputStreams(ppAltInputStreams), _ppExternOutputStreams(ppAltOutputStreams), _externIOstreamCount(altIOstreamCount),
    _justinaStartupOptions(JustinaStartupOptions), _SDcardChipSelectPin(SDcardChipSelectPin) {

    constructorCommonPart();
}


void Justina::constructorCommonPart() {

    // settings to be initialized when cold starting interpreter only
    // --------------------------------------------------------------

    // NOTE: count of objects created / deleted in constructors / destructors is not maintained

    _constructorInvoked = true;

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

    // create objects
    // --------------
    // current print column is maintained for each stream separately: init
    _pPrintColumns = new int[_externIOstreamCount];                                                 // maximum 4 external streams
    for (int i = 0; i < _externIOstreamCount; i++) {
        // NOTE: will only have effect for currently established connections (e.g. TCP)
        if (_ppExternInputStreams[i] != nullptr) { _ppExternInputStreams[i]->setTimeout(DEFAULT_READ_TIMEOUT); }
        _pPrintColumns[i] = 0;
    }

    // create a 'breakpoints' object, containing the breakpoints table, and responsible for handling breakpoints 
    _pBreakpoints = new Breakpoints(this, (_PROGRAM_MEMORY_SIZE * BP_LINE_RANGE_PROGMEM_STOR_RATIO) / 100, MAX_BP_COUNT);


    // by default, console in/out and debug out are first element in _ppExternInputStreams[], _ppExternOutputStreams[]
    _consoleIn_sourceStreamNumber = _consoleOut_sourceStreamNumber = _debug_sourceStreamNumber = -1;
    _pConsoleIn = _ppExternInputStreams[0];
    _pConsoleOut = _pDebugOut = _ppExternOutputStreams[0];
    _pConsolePrintColumn = _pDebugPrintColumn = _pPrintColumns;                                     //  point to its current print column
    _pLastPrintColumn = _pPrintColumns;

    // set linked list debug printing. Pointer to debug out stream pointer: will follow if debug stream is changed
    parsingStack.setDebugOutStream(&_pDebugOut);                                                    // for debug printing within linked list object

    initInterpreterVariables(true);                     // init internal variables 
};


// ------------------
// *   destructor   *
// ------------------

Justina::~Justina() {
    resetMachine(true, true);                           // delete all objects created on the heap, including user variables (+ FiFo stack), with trace expression and breakpoints

    // NOTE: object count of objects created / deleted in constructors / destructors is not maintained
    delete _pBreakpoints;                               // not an array: use 'delete'
    delete[] _pPrintColumns;
};


// ----------------------------
// *   interpreter main loop   *
// ----------------------------

void Justina::begin() {
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
    parsingResult_type result{ result_parsing_OK };                                         // init

    // variables for maintaining lines allowing breakpoints
    bool parsedStatementStartsOnNewLine{ false };
    bool parsedStatementStartLinesAdjacent{ false };
    long statementStartsAtLine{ 0 };
    long parsedStatementAllowingBPstartsAtLine{ 0 };
    long BPstartLine{ 0 }, BPendLine{ 0 };

    static long BPpreviousEndLine{ 0 };

    _appFlags = 0x0000L;                                                                    // init application flags (for communication with Justina caller, using callbacks)

    printlnTo(0);
    for (int i = 0; i < 13; i++) { printTo(0, "*"); } printTo(0, "____");
    for (int i = 0; i < 4; i++) { printTo(0, "*"); } printTo(0, "__");
    for (int i = 0; i < 14; i++) { printTo(0, "*"); } printTo(0, "_");
    for (int i = 0; i < 10; i++) { printTo(0, "*"); }printlnTo(0);

    printTo(0, "    "); printlnTo(0, J_productName);
    printTo(0, "    "); printlnTo(0, J_legalCopyright);
    printTo(0, "    Version: "); printlnTo(0, J_version);
    for (int i = 0; i < 48; i++) { printTo(0, "*"); } printlnTo(0);

#if PRINT_HEAP_OBJ_CREA_DEL
    int col{};
    _pDebugOut->println();
    _pDebugOut->print("+++++ (Justina object) at 0x"); col = 10 - _pDebugOut->print((uint32_t)this, HEX); _pDebugOut->print(", size "); _pDebugOut->println(sizeof(*this));
    _pDebugOut->print("+++++ (program memory) at 0x"); col = 10 - _pDebugOut->print((uint32_t)_programStorage, HEX); _pDebugOut->print(", size "); _pDebugOut->println(_PROGRAM_MEMORY_SIZE + IMM_MEM_SIZE);
#endif

    // find token index for terminal token 'semicolon with breakpoint allowed' 
    int index{}, semicolonBPallowed_index{}, semicolonBPset_index{}, matches{};

    for (index = _termTokenCount - 1, matches = 0; index >= 0; index--) {      // for all defined terminals
        if (_terminals[index].terminalCode == termcod_semicolon_BPallowed) { semicolonBPallowed_index = index; matches; }           // token corresponds to terminal code ? Then exit loop    
        if (_terminals[index].terminalCode == termcod_semicolon_BPset) { semicolonBPset_index = index; matches++; }                 // token corresponds to terminal code ? Then exit loop    
        if (matches == 2) { break; }
    }
    _semicolonBPallowed_token = (semicolonBPallowed_index <= 0x0F) ? tok_isTerminalGroup1 : (semicolonBPallowed_index <= 0x1F) ? tok_isTerminalGroup2 : tok_isTerminalGroup3;
    _semicolonBPallowed_token |= ((semicolonBPallowed_index & 0x0F) << 4);
    _semicolonBPset_token = (semicolonBPset_index <= 0x0F) ? tok_isTerminalGroup1 : (semicolonBPset_index <= 0x1F) ? tok_isTerminalGroup2 : tok_isTerminalGroup3;
    _semicolonBPset_token |= ((semicolonBPset_index & 0x0F) << 4);

    _programMode = false;
    _programCounter = _programStorage + _PROGRAM_MEMORY_SIZE;
    *(_programStorage + _PROGRAM_MEMORY_SIZE) = tok_no_token;                               //  current end of program (FIRST byte of immediate mode command line)
    _lastPrintedIsPrompt = false;

    _coldStart = _constructorInvoked;
    _constructorInvoked = false;                                                            // reset

    Stream* pStatementInputStream = _pConsoleIn;                                            // init: load program from console
    int streamNumber{ 0 };
    setStream(0);                                                                           // set _pStreamIn to console, for use by Justina methods

    int clearCmdIndicator{ 0 };                                                             // 1 = clear program cmd, 2 = clear all cmd
    char c{};
    bool kill{ false };
    bool loadingStartupProgram{ false }, launchingStartFunction{ false };
    bool startJustinaWithoutAutostart{ true };

   // initialize SD card now ?
    // 0 = no card reader, 1 = card reader present, do not yet initialize, 2 = initialize card now, 3 = init card & run /Justina/start.jus function start() now
    if ((_justinaStartupOptions & SD_mask) >= SD_init) {
        printTo(0, "\r\nLooking for an SD card...\r\n");
        execResult_type execResult = startSD();
        printTo(0, _SDinitOK ? "SD card found\r\n" : "SD card error: SD card NOT found\r\n");
    }

    if ((_justinaStartupOptions & SD_mask) == SD_runStart) {
        // open startup file and retrieve file number (which would be one, normally)
        _initiateProgramLoad = _SDinitOK;
        if (_initiateProgramLoad) {
            printlnTo(0, "Looking for 'start.jus' program file in /Justina folder...");
            if (!SD.exists("/Justina/start.jus")) { _initiateProgramLoad = false; printlnTo(0, "'start.jus' program NOT found in folder /Justina"); }      // ADD staring slash ! (required by ESP32 SD library)
        }

        if (_initiateProgramLoad) {
            execResult_type execResult = SD_open(_loadProgFromStreamNo, "/Justina/start.jus", READ_FILE);       // this performs a few card & file checks as well
            _initiateProgramLoad = (execResult == result_execOK);
            if (!_initiateProgramLoad) { printTo(0, "Could not open 'start.jus' program - error "); printlnTo(0, execResult); }
            if (openFiles[_loadProgFromStreamNo - 1].file.size() == 0) { _initiateProgramLoad = false;  printTo(0, "File '/Justina/start.jus' is empty - error "); printlnTo(0, result_SD_fileIsEmpty); }
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
            parsedStatementAllowingBPstartsAtLine = 0;
            BPstartLine = 0;
            BPendLine = 0;
            BPpreviousEndLine = 0;

            streamNumber = _loadProgFromStreamNo;                                                               // autostart step 1: temporarily switch from console input to startup file (opening the file here) 
            setStream(streamNumber, pStatementInputStream);                                                     // error checking done while opening file

            printTo(0, "Loading program '/Justina/start.jus'...\r\n");
        }
    }

    parsedStatementCount = 0;
    do {
        // when loading a program, as soon as first printable character of a PROGRAM is read, each subsequent character needs to follow after the previous one within a fixed time delay, handled by getCharacter().
        // program reading ends when no character is read within this time window.
        // when processing immediate mode statements (single line), reading ends when a New Line terminating character is received
        bool programCharsReceived = _programMode && !_initiateProgramLoad;                                      // _initiateProgramLoad is set during execution of the command to read a program source file from the console
        bool waitForFirstProgramCharacter = _initiateProgramLoad && (_loadProgFromStreamNo <= 0);

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
            // do not read from console; instead insert characters here
            strcpy(_statement, "start();");                                                                     // NOTE: ending ';' is important
            statementCharCount = strlen(_statement);
            allCharsReceived = true;                                                                            // ready for parsing
            launchingStartFunction = false;                                                                     // nothing to prepare any more
        }
        else {     // note: while waiting for first program character, allow a longer time out              
            bool charFetched{ false };
            c = getCharacter(charFetched, kill, forcedStop, forcedAbort, stdConsole, true, waitForFirstProgramCharacter);    // forced stop has no effect here
            if (charFetched) {
                if (waitForFirstProgramCharacter) { printlnTo(0, "Receiving and parsing program... please wait"); }
                _appFlags &= ~appFlag_errorConditionBit;                                                        // clear error condition flag 
                _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_parsing;                                // status 'parsing'
            }

            if (kill) { break; }
            // start processing input buffer when (1) in program mode: time out occurs and at least one character received, or (2) in immediate mode: when a new line character is detected
            allCharsReceived = _programMode ? (!charFetched && programCharsReceived) : (c == '\n');              // programCharsReceived: at least one program character received
            if (!charFetched && !allCharsReceived && !forcedAbort && !stdConsole) { continue; }                  // no character: keep waiting for input (except when program or imm. mode line is read)

            // if no character added: nothing to do, wait for next
            noCharAdded = !addCharacterToInput(lastCharWasSemiColon, withinString, withinStringEscSequence, within1LineComment, withinMultiLineComment, redundantSemiColon, allCharsReceived,
                bufferOverrun, flushAllUntilEOF, lineCount, statementCharCount, c);
            currentSourceLine = lineCount + 1;      // adjustment only
        }

        do {        // one loop only
            if (bufferOverrun) { result = result_statementTooLong; }
            if (kill) { quitNow = true;  result = result_parse_kill; break; }
            if (forcedAbort) { result = result_parse_abort; }
            if (stdConsole && !_programMode) { result = result_parse_setStdConsole; }


            // if a statement is complete (terminated by a semicolon or end of input), maintain breakpoint line ranges and parse statement
            // ---------------------------------------------------------------------------------------------------------------------------
            bool isStatementSeparator = (!withinString) && (!within1LineComment) && (!withinMultiLineComment) && (c == term_semicolon[0]) && !redundantSemiColon;
            isStatementSeparator = isStatementSeparator || (withinString && (c == '\n'));  // a new line character within a string is sent to parser as well

            bool statementReadyForParsing = !bufferOverrun && !forcedAbort && !stdConsole && !kill && (isStatementSeparator || (allCharsReceived && (statementCharCount > 0)));

            if (_programMode && (statementCharCount == 1) && !noCharAdded) { statementStartsAtLine = currentSourceLine; }      // first character of new statement

            if (statementReadyForParsing) {                                                                     // if quitting anyway, just skip                                               

                _statement[statementCharCount] = '\0';                                                          // add string terminator

                char* pStatement = _statement;                                                                  // because passed by reference 
                char* pDummy{};
                _parsingExecutingTraceString = false;                                                           // init
                _parsingExecutingTriggerString = false;
                _parsingEvalString = false;

                // The user can set breakpoints for source lines having at least one statement starting on that line (given that the statement is not 'parsing only').
                // Procedure 'collectSourceLineRangePairs' stores necessary data to enable this functionality.
                result = _pBreakpoints->collectSourceLineRangePairs(_semicolonBPallowed_token, parsedStatementStartsOnNewLine, parsedStatementStartLinesAdjacent,
                    statementStartsAtLine, parsedStatementAllowingBPstartsAtLine, BPstartLine, BPendLine, BPpreviousEndLine);

                if (result == result_parsing_OK) { result = parseStatement(pStatement, pDummy, clearCmdIndicator, parsedStatementStartsOnNewLine, parsedStatementAllowingBPstartsAtLine); }       // parse ONE statement only 

                if ((++parsedStatementCount & 0x0f) == 0) {
                    printTo(0, '.');                                                                            // print a dot each 64 parsed lines
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
                result = _pBreakpoints->addOneSourceLineRangePair(BPstartLine - BPpreviousEndLine - 1, BPendLine - BPstartLine + 1);
            }

            // program mode: complete program read and parsed   /   imm. mode: all statements in command line read and parsed OR parsing error ?
            if (allCharsReceived || (result != result_parsing_OK)) {                                            // note: if all statements have been read, they also have been parsed
                if (kill) { quitNow = true; }
                else {
                    quitNow = finaliseParsing(result, kill, lineCount, pErrorPos, allCharsReceived);            // return value: quit Justina now

                    // if not in program mode and no parsing error: execute
                    execResult_type execResult{ result_execOK };
                    if (!_programMode && (result == result_parsing_OK)) {
                        execResult = exec(_programStorage + _PROGRAM_MEMORY_SIZE);                              // execute parsed user statements
                        if (execResult == result_kill) { kill = true; }
                        if (kill || (execResult == result_quit)) { printlnTo(0); quitNow = true; }              // make sure Justina prompt will be printed on a new line
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
                parsedStatementAllowingBPstartsAtLine = 0;
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
    if (_housekeepingCallback != nullptr) { _housekeepingCallback(_appFlags); }                                 // pass application flags to caller immediately

    if (kill) { printlnTo(0, "\r\n\r\nProcessing kill request from calling program"); }

    SD_closeAllFiles();                                                                                         // safety (in case an SD card is present: close all files 
    _SDinitOK = false;
    SD.end();                                                                                                   // stop SD card

    // !!! if code is ever changed to clear memory when quitting: include next line
    /* resetMachine(true, true); */

    while (_pConsoleIn->available() > 0) { readFrom(0); }                                                       //  empty console buffer before quitting
    printlnTo(0, "\r\nJustina: bye\r\n");
    for (int i = 0; i < 48; i++) { printTo(0, "="); } printlnTo(0, "\r\n");

    // If data is NOT kept in memory, objects that will be deleted are: variable and function names; parsed, intermediate and variable string objects,...
    // ...array objects, stack entries, last values FiFo, open function data, breakpoint trigger and view strings, ...
    // Objects that are not deleted now, will be deleted when the Justina object is deleted (destructor).  
    // (program and variable memory itself is only freed when the Justina object itself is deleted).
    return;                                                                                                     // return to calling program
}


// ----------------------------------------------------------------------------------
// *   add a character received from the input stream to the parsing input buffer   *
// ----------------------------------------------------------------------------------

bool Justina::addCharacterToInput(bool& lastCharWasSemiColon, bool& withinString, bool& withinStringEscSequence, bool& within1LineComment, bool& withinMultiLineComment,
    bool& redundantSemiColon, bool ImmModeLineOrProgramRead, bool& bufferOverrun, bool flushAllUntilEOF, int& lineCount, int& statementCharCount, char c) {

    const char commentOuterDelim = '/'; // twice: single line comment; followed by inner delimiter: start of multi-line comment; preceded by inner delimiter: end of multi-line comment 
    const char commentInnerDelim = '*';

    static bool lastCharWasWhiteSpace{ false };
    static char lastCommentChar{ '\0' };                                                                        // init: none

    bool redundantSpaces = false;

    bufferOverrun = false;
    if (c == '\t') { c = ' '; };                                                                                // replace TAB characters by space characters
    if ((c < ' ') && (c != '\n')) { return false; }                                                             // skip all other control-chars except new line and EOF character

    // when a imm. mode line or program is completely read and the last character (part of the last statement) received from input stream is not a semicolon, add it
    if (ImmModeLineOrProgramRead) {
        if (statementCharCount > 0) {
            if (_statement[statementCharCount - 1] != term_semicolon[0]) {
                if (statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }
                _statement[statementCharCount] = term_semicolon[0];                                             // still room: add character
                statementCharCount++;
            }
        }

        within1LineComment = false;
        withinMultiLineComment = false;
    }

    // not at end of program or imm. mode line: process character   
    else {
        if (flushAllUntilEOF) { return false; }                                                                 // discard characters (after parsing error)

        if (c == '\n') { lineCount++; }                                                                         // line number used when while reading program in input file

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
                        _statement[statementCharCount] = '\0';                                                  // add string terminator

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
        _statement[statementCharCount] = c;                                                                     // still room: add character
        ++statementCharCount;
    }

    return true;
}


// ------------------------
// *   finalize parsing   *  
// ------------------------

bool Justina::finaliseParsing(parsingResult_type& result, bool& kill, int lineCount, char* pErrorPos, bool allCharsReceived) {

    bool quitJustina{ false };

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
            if (_promptAndEcho == 2) { prettyPrintStatements(0, 0); printlnTo(0); }                             // immediate mode and result OK: pretty print input line
            else if (_promptAndEcho == 1) { printlnTo(0); }
        }
    }
    else {          // parsing error, abort or kill during parsing
        if (_programMode && (_loadProgFromStreamNo <= 0)) {
            if (result == result_parse_abort) { printTo(0, "\r\nAbort: "); }                                    // not for other parsing errors
            else { printTo(0, "\r\nParsing error: "); }
            printlnTo(0, "processing remainder of input file... please wait");
        }

        if (result == result_parse_abort) {
            printlnTo(0, _programMode ? "\r\n+++ Abort: parsing terminated +++" : "");                          // abort: display error message if aborting program parsing
        }
        else if (result == result_parse_setStdConsole) {
            printlnTo(0, "\r\n+++ console reset +++");
            _consoleIn_sourceStreamNumber = _consoleOut_sourceStreamNumber = -1;
            _pConsoleIn = _ppExternInputStreams[0];                                                             // set console to stream -1 (NOT debug out)
            _pConsoleOut = _ppExternOutputStreams[0];                                                           // set console to stream -1 (NOT debug out)
            _pConsolePrintColumn = &_pPrintColumns[0];
            *_pConsolePrintColumn = 0;

        }
        else if (result == result_parse_kill) { quitJustina = true; }
        else {
            printParsingResult(result, funcNotDefIndex, _statement, lineCount, pErrorPos);                      // parsing error occurred: print error message
        }
    }
    return quitJustina;
}


// ---------------------------------------------
// *   finalize execution: prepare for idle mode
// ---------------------------------------------

bool Justina::prepareForIdleMode(parsingResult_type result, execResult_type execResult, bool& kill, int& clearIndicator, Stream*& pStatementInputStream, int& statementInputStreamNumber) {

    bool quitJustina{ false };

    // if in debug mode, trace expressions (if defined) and print debug info 
    // ---------------------------------------------------------------------
    if ((_openDebugLevels > 0) && (execResult != result_kill) && (execResult != result_quit) && (execResult != result_initiateProgramLoad)) { traceAndPrintDebugInfo(execResult); }

    // re-init or reset interpreter state 
    // ----------------------------------

    // if program parsing error: reset machine, because variable storage might not be consistent with program any more
    if ((_programMode) && (result != result_parsing_OK)) { resetMachine(false); }


#if PRINT_DEBUG_INFO  
    // NOTE !!! Also set PRINT_DEBUG_INFO to 1 in file Breakpoints !!!
    if (_programMode) {
        _pDebugOut->println();
        _pBreakpoints->printLineRangesToDebugOut(static_cast<Stream*>(_pDebugOut));
    }
#endif

    // before loading a program, clear memory except user variables
    else if (execResult == result_initiateProgramLoad) { resetMachine(false); }

    // no program error (could be immediate mode error however), not initiating program load: only reset a couple of items here 
    else {
        parsingStack.deleteList();
        _blockLevel = 0;                                        // current number of open block commands (during parsing) - block level + parenthesis level = parsing stack depth
        _parenthesisLevel = 0;                                  // current number of open parentheses (during parsing)

        _justinaFunctionBlockOpen = false;
    }

    // the clear memory / clear all command is executed AFTER the execution phase
    // --------------------------------------------------------------------------

    // first check there were no parsing or execution errors
    if ((result == result_parsing_OK) && (execResult == result_execOK)) {
        if (clearIndicator != 0) {                     // 1 = clear program cmd, 2 = clear all cmd 
            while (_pConsoleIn->available() > 0) { readFrom(0); }                                               // empty console buffer first (to allow the user to start with an empty line)
            do {
                char s[60];
                sprintf(s, "===== Clear %s ? (please answer Y or N) =====", ((clearIndicator == 2) ? "memory" : "program"));
                printlnTo(0, s);

                // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                // return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
                bool doCancel{ false }, doStop{ false }, doAbort{ false }, doDefault{ false };      // not used but mandatory
                int length{ 2 };                                                                                // detects input > 1 character
                char input[2 + 1] = "";                                                                         // init: empty string. Provide room for 1 character + terminating '\0'
                // NOTE: stop, cancel and default arguments have no function here (execution has ended already), but abort and kill do
                if (getConsoleCharacters(doStop, doAbort, doCancel, doDefault, input, length, '\n')) { kill = true; quitJustina = true; break; }  // kill request from caller ?

                if (doAbort) { break; }        // avoid a next loop (getConsoleCharacters exits immediately when abort request received, not waiting for any characters)
                bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                if (validAnswer) {
                    // 1 = clear program, 2 = clear all (including user variables)
                    if (tolower(input[0]) == 'y') {
                        printlnTo(0, (clearIndicator == 2) ? "clearing memory" : "clearing program");
                        resetMachine(clearIndicator == 2, clearIndicator == 2);
                    }
                    break;
                }
            } while (true);
        }
    }

    // execution finished (not stopping in debug mode), with or without error: delete parsed strings in imm mode command : they are on the heap and not needed any more. 
    // Identifiers must stay available.
    // -> if stopping a program for debug, do not delete parsed strings (in imm. mode command), because that command line has now been pushed on...
     // ...the parsed command line stack and included parsed constants will be deleted later (resetMachine routine).
    if ((execResult != result_stopForDebug) && (execResult != result_stopForBreakpoint)) { deleteConstStringObjects(_programStorage + _PROGRAM_MEMORY_SIZE); } // always

    // finalize: last actions before 'ready' mode (prompt displayed depending on settings)
    // -----------------------------------------------------------------------------------
    _programMode = false;
    _programCounter = _programStorage + _PROGRAM_MEMORY_SIZE;                                                   // start of 'immediate mode' program area
    *(_programStorage + _PROGRAM_MEMORY_SIZE) = tok_no_token;                                                   //  current end of program (immediate mode)

    if (execResult == result_initiateProgramLoad) {                                                             // initiate program load 
        _programMode = true;
        _programCounter = _programStorage;

        if (_lastPrintedIsPrompt) { printlnTo(0); }                                                             // print new line if last printed was a prompt
        printTo(0, (_loadProgFromStreamNo > 0) ? "Loading program...\r\n" : "Waiting for program...\r\n");
        _lastPrintedIsPrompt = false;

        statementInputStreamNumber = _loadProgFromStreamNo;
        setStream(statementInputStreamNumber, pStatementInputStream);           // set stream to program input stream (is a valid stream, already checked)

        // useful for remote terminals (characters sent to connect are flushed, this way)
        if (_loadProgFromStreamNo <= 0) { while (pStatementInputStream->available()) { readFrom(statementInputStreamNumber); } }

        _initiateProgramLoad = true;
    }
    else {      // with or without parsing or execution error
        // flush any remaining input characters:
        // - if a program was loaded now, then the stream is still the stream from where the program was loaded (only flush if an external IO stream)
        // - if not in program load mode, then the stream will be the console
        if ((statementInputStreamNumber <= 0) && (pStatementInputStream->available() > 0)) {  // skip if initially buffer is empty
            bool stop{ false }, abort{ false };     // dummy, as we are entering idle mode anyway
            setStream(statementInputStreamNumber); flushInputCharacters(stop, abort);
        }
        statementInputStreamNumber = 0;                                         // set stream for statement input back to console (if it wasn't)
        setStream(statementInputStreamNumber, pStatementInputStream);
        if (_loadProgFromStreamNo > 0) { SD_closeFile(_loadProgFromStreamNo); }
        _loadProgFromStreamNo = 0;
    }

    // has an error occurred ? (exclude 'events' reported as an error)
    bool isError = (result != result_parsing_OK) || ((execResult != result_execOK) && (execResult < result_startOfEvents));
    isError ? (_appFlags |= appFlag_errorConditionBit) : (_appFlags &= ~appFlag_errorConditionBit);             // set or clear error condition flag 
    // status 'idle in debug mode' or 'idle' 
    (_appFlags &= ~appFlag_statusMask);
    (_openDebugLevels > 0) ? (_appFlags |= appFlag_stoppedInDebug) : (_appFlags |= appFlag_idle);

    // print new prompt and exit
    // -------------------------
    _lastPrintedIsPrompt = false;
    if ((_promptAndEcho != 0) && (execResult != result_initiateProgramLoad)) {
        printTo(0, "Justina> "); _lastPrintedIsPrompt = true;
    }

    return quitJustina;
}

// -------------------------------------------------------------------------
// *   trace expressions as defined in trace statement, print debug info   *
// -------------------------------------------------------------------------

void Justina::traceAndPrintDebugInfo(execResult_type execResult) {
    // count of programs in debug:
    // - if an error occurred in a RUNNING program, the program is terminated and the number of STOPPED programs ('in debug mode') does not change.
    // - if an error occurred while executing a command line, then this count is not changed either
    // flow control stack:
    // - at this point, structure '_activeFunctionData' always contains flow control data for the main program level (command line - in debug mode if the count of open programs is not zero)
    // - the flow control stack maintains data about open block commands, open functions and eval() strings in execution (call stack)
    // => skip stack elements for any command line open block commands or eval() strings in execution, and fetch the data for the function where control will resume when started again

    char* nextStatementPointer = _programCounter;
    OpenFunctionData* pDeepestOpenFunction = &_activeFunctionData;

    void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;
    int blockType = block_none;
    do {                                                                                                        // there is at least one open function in the call stack
        blockType = ((openBlockGeneric*)pFlowCtrlStackLvl)->blockType;
        if (blockType == block_JustinaFunction) { break; }
        pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
    } while (true);

    pDeepestOpenFunction = (OpenFunctionData*)pFlowCtrlStackLvl;                                                // deepest level of nested functions
    nextStatementPointer = pDeepestOpenFunction->pNextStep;

    bool isBreakpointStop = (execResult == result_stopForBreakpoint);

    // print debug header ('STOP'or 'BREAK') line
    // ------------------------------------------
    int stopLineLength = max(_dispWidth, 30) + 2 + 2 + 1;                                                       // '2': starting and ending CRLF, '1': terminating \0, '30': STOP line minimum length (small display widths)
    char msg[max(stopLineLength, 20 + MAX_IDENT_NAME_LEN)];                                                     // '20': line number and function: sufficient length for fixed part

    int length = sprintf(msg, "%s", (isBreakpointStop ? "\r\n-- BREAK " : "\r\n-- STOP "));
    if (_openDebugLevels > 1) { length += sprintf(msg + length, "-- [%ld] ", _openDebugLevels); }
    int i{};
    for (i = length; i < (stopLineLength - 2 - 1); i++) { msg[i] = '-'; }
    strcpy(msg + i, "\r\n");                                                                                    // this adds terminating \0 as well
    printTo(_debug_sourceStreamNumber, msg);

    // print trace and breakpoint trace string, if any
    // -----------------------------------------------
    // if this is a breakpoint stop, parse and evaluate expression (if any) stored in BP view string
    // for all stops: parse and evaluate expression (if any) stored in overall trace string
    Breakpoints::BreakpointData* pBreakpointDataRow{ nullptr };
    int BPdataRow{};
    bool lineHasBPtableEntry = (*(nextStatementPointer - 1) == _semicolonBPset_token);                          // (note that BP can be disabled, hit count not yet reached or trigger result = false)
    if (lineHasBPtableEntry) {                                                                                  // check attributes in breakpoints table
        pBreakpointDataRow = _pBreakpoints->findBPtableRow(nextStatementPointer, BPdataRow);                    // find table entry
    }
    if (isBreakpointStop) { parseAndExecTraceOrBPviewString(BPdataRow); }                                       // BP view string: may not contain keywords, Justina functions, generic names
    parseAndExecTraceOrBPviewString();                                                                          // trace string: may not contain keywords, Justina functions, generic names


    // print the source line, function and statement 
    // ---------------------------------------------
    // if source line has an entry in breakpoint table: retrieve source line from there. If not,...
    // ...then calculate the source line number by counting parsed statements with a preceding 'breakpoint set' or 'breakpoint allowed' token.
    // note that the second method can be slow(-er) if program consists of a large number of statements
    long sourceLine = (lineHasBPtableEntry) ? pBreakpointDataRow->sourceLine : _pBreakpoints->findLineNumberForBPstatement(nextStatementPointer);
    sprintf(msg, "line %ld: [%s] ", sourceLine, JustinaFunctionNames[pDeepestOpenFunction->functionIndex]);
    printTo(_debug_sourceStreamNumber, msg);
    prettyPrintStatements(_debug_sourceStreamNumber, 1, nextStatementPointer);                                  // print statement
    printTo(_debug_sourceStreamNumber, "\r\n");
    return;
}


// -----------------------------------------------
// *   parse and exec trace string expressions   *
// -----------------------------------------------

// trace string may not contain keywords, user functions, generic names

void Justina::parseAndExecTraceOrBPviewString(int BPindex) {
    char* pNextParseStatement{};

    char* pTraceParsingInput = ((BPindex == -1) ? _pTraceString : _pBreakpoints->_pBreakpointData[BPindex].pView);      // copy pointer to start of trace string
    if (pTraceParsingInput == nullptr) { return; }                                                                      // no trace string: nothing to trace

    // trace string expressions will be parsed and executed from immediate mode program storage: 
    // before overwriting user statements that were just parsed and executed, delete parsed strings
    deleteConstStringObjects(_programStorage + _PROGRAM_MEMORY_SIZE);

    bool valuePrinted{ false };

    _parsingExecutingTraceString = true;

    printTo(_debug_sourceStreamNumber, (BPindex == -1) ? "<TRACE> " : "<BP TR> ");

    // in each loop, parse and execute ONE expression 
    do {
        // init
        *(_programStorage + _PROGRAM_MEMORY_SIZE) = tok_no_token;                                               // in case no valid tokens will be stored
        _programCounter = _programStorage + _PROGRAM_MEMORY_SIZE;                                               // start of 'immediate mode' program area

        // skip any spaces and semi-colons in the input stream
        while ((pTraceParsingInput[0] == ' ') || (pTraceParsingInput[0] == term_semicolon[0])) { pTraceParsingInput++; }
        if (*pTraceParsingInput == '\0') { break; }                                                             // could occur if semicolons skipped

        // parse multiple trace string expressions ? print a comma in between
        if (valuePrinted) { printTo(_debug_sourceStreamNumber, ", "); }                                         // separate values (if more than one)

        // note: application flags are not adapted (would not be passed to caller immediately)
        int dummy{};
        parsingResult_type result = parseStatement(pTraceParsingInput, pNextParseStatement, dummy);             // parse ONE statement
        if (result == result_parsing_OK) {
            if (!_printTraceValueOnly) {
                // do NOT pretty print if parsing error, to avoid bad-looking partially printed statements (even if there will be an execution error later)
                prettyPrintStatements(_debug_sourceStreamNumber, 0);
                printTo(_debug_sourceStreamNumber, ": ");                                                       // resulting value will follow
                pTraceParsingInput = pNextParseStatement;
            }
        }
        else {
            char  errStr[12];                                                                                   // includes place for terminating '\0'
            // if parsing error, print error instead of value AND CONTINUE with next trace expression (if any)
            sprintf(errStr, "<ErrP%d>", (int)result);
            printTo(_debug_sourceStreamNumber, errStr);
            // pNextParseStatement not yet correctly positioned: set to next statement
            while ((pTraceParsingInput[0] != term_semicolon[0]) && (pTraceParsingInput[0] != '\0')) { ++pTraceParsingInput; }
            if (pTraceParsingInput[0] == term_semicolon[0]) { ++pTraceParsingInput; }
        }

        // if parsing went OK: execute ONE parsed expression (just parsed now)
        execResult_type execResult{ result_execOK };
        if (result == result_parsing_OK) {
            execResult = exec(_programStorage + _PROGRAM_MEMORY_SIZE);                                          // note: value or exec. error is printed from inside exec()
        }

        valuePrinted = true;

        // execution finished: delete parsed strings in imm mode command (they are on the heap and not needed any more)
        deleteConstStringObjects(_programStorage + _PROGRAM_MEMORY_SIZE);                                       // always
        *(_programStorage + _PROGRAM_MEMORY_SIZE) = tok_no_token;                                               // current end of program (immediate mode)

    } while (*pTraceParsingInput != '\0');                                                                      // exit loop if all expressions handled

    _parsingExecutingTraceString = false;
    printlnTo(_debug_sourceStreamNumber);       // go to next output line

    return;
}


// -------------------------------------------------------------
// *   check if all Justina functions referenced are defined   *
// -------------------------------------------------------------

bool Justina::checkAllJustinaFunctionsDefined(int& index) const {
    index = 0;
    while (index < _justinaFunctionCount) {                                                                     // points to variable in use
        if (justinaFunctionData[index].pJustinaFunctionStartToken == nullptr) { return false; }
        index++;
    }
    return true;
}


// ----------------------------------------------------
// *   set system (housekeeping) call back function   *
// ----------------------------------------------------

void Justina::setSystemCallbackFunction(void (*func)(long& appFlags)) {

    // this function is directly called from the main Arduino program, before the Justina begin() method is called
    // it stores the address of an optional 'system callback' function
    // Justina will call this user routine at specific time intervals, allowing  the user...
    // ...to execute a specific routine regularly (e.g. to maintain a TCP connection, to implement a heartbeat, ...)

    _housekeepingCallback = func;
}


// ----------------------------------
// *   set RTC call back function   *
// ----------------------------------

void Justina::setRTCCallbackFunction(void (*func)(uint16_t* date, uint16_t* time)) {

    // this function is directly called from the main Arduino program, before the Justina begin() method is called
    // it stores the address of an optional 'RTC callback' function
    // Justina can than call this function when it needs the date or time )

    _dateTime = func;
}


//----------------------------------------------
// *   execute regular housekeeping callback   *
// ---------------------------------------------

// do a housekeeping callback at regular intervals (if callback function defined), but only
// - while waiting for input
// - while executing the Justina delay() function
// - when a statement has been executed 
// the callback function relays specific flags to the caller and upon return, reads certain flags set by the caller 

void Justina::execPeriodicHousekeeping(bool* pKillNow, bool* pForcedStop, bool* pForcedAbort, bool* pSetStdConsole) {
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

            _appFlags &= ~appFlag_dataInOut;                                                                    // reset flag
        }
    }
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// *   sets pointers to the locations where the Arduino program stored information about user-defined (external) cpp functions (user callback functions)   *
// ---------------------------------------------------------------------------------------------------------------------------------------------------------

    // the 'register...' functions are called from the main Arduino program, before the Justina begin() method is called
    // each function stores the starting address of an array with information about external (user callback) functions with a specific CPP return type 
    // for instance, _pExtCppFunctions[0] stores the address of the array containing information about cpp functions returning a boolean value
    // a null pointer indicates there are no functions of a specific type
    // cpp function return types are: 0 = bool, 1 = char, 2 = int, 3 = long, 4 = float, 5 = char*, 6 = void (but returns zero to Justina)

void Justina::registerBoolUserCppFunctions(const CppBoolFunction* const  pCppBoolFunctions, const int cppBoolFunctionCount) {
    _pExtCppFunctions[0] = (CppBoolFunction*)pCppBoolFunctions;
    _ExtCppFunctionCounts[0] = cppBoolFunctionCount;
};

void Justina::registerCharUserCppFunctions(const CppCharFunction* const  pCppCharFunctions, const int cppCharFunctionCount) {
    _pExtCppFunctions[1] = (CppCharFunction*)pCppCharFunctions;
    _ExtCppFunctionCounts[1] = cppCharFunctionCount;
};

void Justina::registerIntUserCppFunctions(const CppIntFunction* const  pCppIntFunctions, const int cppIntFunctionCount) {
    _pExtCppFunctions[2] = (CppIntFunction*)pCppIntFunctions;
    _ExtCppFunctionCounts[2] = cppIntFunctionCount;
};

void Justina::registerLongUserCppFunctions(const CppLongFunction* const pCppLongFunctions, const int cppLongFunctionCount) {
    _pExtCppFunctions[3] = (CppLongFunction*)pCppLongFunctions;
    _ExtCppFunctionCounts[3] = cppLongFunctionCount;
};

void Justina::registerFloatUserCppFunctions(const CppFloatFunction* const pCppFloatFunctions, const int cppFloatFunctionCount) {
    _pExtCppFunctions[4] = (CppFloatFunction*)pCppFloatFunctions;
    _ExtCppFunctionCounts[4] = cppFloatFunctionCount;
};

void Justina::register_pCharUserCppFunctions(const Cpp_pCharFunction* const pCpp_pCharFunctions, const int cpp_pCharFunctionCount) {
    _pExtCppFunctions[5] = (Cpp_pCharFunction*)pCpp_pCharFunctions;
    _ExtCppFunctionCounts[5] = cpp_pCharFunctionCount;
};

void Justina::registerVoidUserCppFunctions(const CppVoidFunction* const pCppVoidFunctions, const int cppVoidFunctionCount) {
    _pExtCppFunctions[6] = (CppVoidFunction*)pCppVoidFunctions;
    _ExtCppFunctionCounts[6] = cppVoidFunctionCount;
};


// -------------------------
// *   reset interpreter   *
// -------------------------

void Justina::resetMachine(bool withUserVariables, bool withBreakpoints) {

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
    }

    // delete all elements of the immediate mode parsed statements stack
    // (parsed immediate mode statements can be temporarily pushed on the immediate mode stack to be replaced either by parsed debug command lines or parsed eval() strings) 
    // also delete all parsed alphanumeric constants: (1) in the currently parsed program, (2) in parsed immediate mode statements (including those on the imm.mode parsed statements stack)) 

    clearParsedCommandLineStack(parsedCommandLineStack.getElementCount());                                      // including parsed string constants
    deleteConstStringObjects(_programStorage);
    deleteConstStringObjects(_programStorage + _PROGRAM_MEMORY_SIZE);

    // delete all elements of the flow control stack 
    // in the process, delete all local variable areas referenced in elements of the flow control stack referring to functions, including local variable string and array values
    int dummy{};
    clearFlowCtrlStack(dummy);

    // clear expression evaluation stack
    clearEvalStack();

    // delete parsing stack (keeps track of open parentheses and open command blocks during parsing)
    parsingStack.deleteList();

    // delete trace string and delete breakpoint view and trigger strings ?
    if (withBreakpoints) {
        if (_pTraceString != nullptr) {        // internal trace 'variable'
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n----- (system exp str) "); _pDebugOut->println((uint32_t)_pTraceString, HEX);
            _pDebugOut->print("reset mach.: trace str "); _pDebugOut->println(_pTraceString);
        #endif
            _systemStringObjectCount--;
            delete[] _pTraceString;
            _pTraceString = nullptr;                                                                            // old trace string
        }

        _pBreakpoints->resetBreakpointsState();                                                                 // '_breakpointsStatusDraft' is set false (breakpoint table empty) 
    }
    
    // if machine reset without clearing breakpoints, set breakpoints status to DRAFT then 
    else {
        bool wasDraft = _pBreakpoints->_breakpointsStatusDraft;
        _pBreakpoints->_breakpointsStatusDraft = (_pBreakpoints->_breakpointsUsed > 0);                         // '_breakpointsStatusDraft' set according to existence of entries in breakpoint table
        _pBreakpoints->_BPlineRangeStorageUsed = 0;

        if (!wasDraft && _pBreakpoints->_breakpointsStatusDraft) {
            printlnTo(0); for (int i = 1; i <= 40; i++) { printTo(0, '*'); }
            printlnTo(0, "\r\n** Breakpoint status now set to DRAFT **");                                       // because table not empty
            for (int i = 1; i <= 40; i++) { printTo(0, '*'); } printlnTo(0);
        }
    }

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

void Justina::danglingPointerCheckAndCount(bool withUserVariables) {

    // note: the evaluation stack, intermediate string objects, function local storage, and function local variable strings and arrays exist solely during execution
    //       relevant checks are performed each time execution terminates 

    // parsing stack: no need to check if any elements were left (the list has just been deleted)
    // note: this stack does not contain any pointers to heap objects

    // string and array heap objects: any objects left ?
    if (_identifierNameStringObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("**** Variable / function name objects cleanup error. Remaining: "); _pDebugOut->println(_identifierNameStringObjectCount);
    #endif
        _identifierNameStringObjectErrors += abs(_identifierNameStringObjectCount);
    }

    if (_parsedStringConstObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("**** Parsed constant string objects cleanup error. Remaining: "); _pDebugOut->println(_parsedStringConstObjectCount);
    #endif
        _parsedStringConstObjectErrors += abs(_parsedStringConstObjectCount);
    }

    if (_globalStaticVarStringObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("**** Variable string objects cleanup error. Remaining: "); _pDebugOut->println(_globalStaticVarStringObjectCount);
    #endif
        _globalStaticVarStringObjectErrors += abs(_globalStaticVarStringObjectCount);
    }

    if (_globalStaticArrayObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("**** Array objects cleanup error. Remaining: "); _pDebugOut->println(_globalStaticArrayObjectCount);
    #endif
        _globalStaticArrayObjectErrors += abs(_globalStaticArrayObjectCount);
    }

#if PRINT_DEBUG_INFO
    _pDebugOut->print("\r\n   Reset stats\r\n   parsed strings "); _pDebugOut->print(_parsedStringConstObjectCount);
    _pDebugOut->print(", prog name strings "); _pDebugOut->print(_identifierNameStringObjectCount);
    _pDebugOut->print(", prog var strings "); _pDebugOut->print(_globalStaticVarStringObjectCount);
    _pDebugOut->print(", prog arrays "); _pDebugOut->print(_globalStaticArrayObjectCount);
#endif

    if (withUserVariables) {
        if (_userVarNameStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("**** User variable name objects cleanup error. Remaining: "); _pDebugOut->println(_userVarNameStringObjectCount);
        #endif
            _userVarNameStringObjectErrors += abs(_userVarNameStringObjectCount);
        }

        if (_userVarStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("**** User variable string objects cleanup error. Remaining: "); _pDebugOut->println(_userVarStringObjectCount);
        #endif
            _userVarStringObjectErrors += abs(_userVarStringObjectCount);
        }

        if (_userArrayObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("**** User array objects cleanup error. Remaining: "); _pDebugOut->println(_userArrayObjectCount);
        #endif
            _userArrayObjectErrors += abs(_userArrayObjectCount);
        }

        if (_lastValuesStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("**** Last value FiFo string objects cleanup error. Remaining: "); _pDebugOut->print(_lastValuesStringObjectCount);
        #endif
            _lastValuesStringObjectErrors += abs(_lastValuesStringObjectCount);
        }

        if (_systemStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("**** System variable string objects cleanup error. Remaining: "); _pDebugOut->println(_systemStringObjectCount);
        #endif
            _systemStringObjectErrors += abs(_systemStringObjectCount);
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
// *   initialize interpreter object fields   *
// --------------------------------------------

void Justina::initInterpreterVariables(bool fullReset) {

    // initialized at cold start AND each time the interpreter is reset

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
    *(_programStorage + _PROGRAM_MEMORY_SIZE) = tok_no_token;                                                   //  set as current end of program (immediate mode)
    _programCounter = _programStorage + _PROGRAM_MEMORY_SIZE;                                                   // start of 'immediate mode' program area

    _programName[0] = '\0';

    _pEvalStackTop = nullptr; _pEvalStackMinus1 = nullptr;   _pEvalStackMinus2 = nullptr;
    _pFlowCtrlStackTop = nullptr;
    _pParsedCommandLineStackTop = nullptr;

    _localVarValueAreaCount = 0;
    _localVarStringObjectCount = 0;
    _localArrayObjectCount = 0;

    _activeFunctionData.callerEvalStackLevels = 0;                              // this is the highest program level
    _callStackDepth = 0;                                                        // equals flow control stack depth minus open loop (if, for, ...) blocks (= blocks being executed)
    _openDebugLevels = 0;                                                       // equals imm mode cmd stack depth minus open eval() strings (= eval() strings being executed)


    // reset counters for heap objects
    // -------------------------------

    _identifierNameStringObjectCount = 0;                                       // object count
    _parsedStringConstObjectCount = 0;
    _intermediateStringObjectCount = 0;      
    _globalStaticVarStringObjectCount = 0;
    _globalStaticArrayObjectCount = 0;

    if (fullReset) {
        _lastValuesCount = 0;                                                   // current last result FiFo depth (values currently stored)
        _openFileCount = 0;

        // reset counters for heap objects
        // -------------------------------

        _userVarNameStringObjectCount = 0;                                      
        _lastValuesStringObjectCount = 0;
        _userVarStringObjectCount = 0;
        _userArrayObjectCount = 0;
        _systemStringObjectCount = 0;


        // initialize format settings for numbers and strings (width, characters to print, flags, ...)
        // -------------------------------------------------------------------------------------------

        _dispWidth = DEFAULT_DISP_WIDTH;
        _tabSize = DEFAULT_TAB_SIZE;
        _dispFloatPrecision = DEFAULT_FLOAT_PRECISION;
        _dispIntegerPrecision = DEFAULT_INT_PRECISION;

        strcpy(_dispFloatSpecifier, DEFAULT_FLOAT_SPECIFIER);
        strcpy(_dispIntegerSpecifier, DEFAULT_INT_SPECIFIER);                       // here without 'd' (long integer) : will be added  
        strcpy(_dispStringSpecifier, DEFAULT_STR_SPECIFIER);                        // here without 'd' (long integer) : will be added  

        _dispFloatFmtFlags = DEFAULT_FLOAT_FLAGS;
        _dispIntegerFmtFlags = DEFAULT_INT_FLAGS;
        _dispStringFmtFlags = DEFAULT_STR_FLAGS;

        makeFormatString(_dispIntegerFmtFlags, true, _dispIntegerSpecifier, _dispIntegerFmtString);             // for integers
        makeFormatString(_dispFloatFmtFlags, false, _dispFloatSpecifier, _dispFloatFmtString);                  // for floats
        makeFormatString(_dispStringFmtFlags, false, _dispStringSpecifier, _dispStringFmtString);               // for strings


        // fmt() function settings 
        // -----------------------
        _fmt_width = DEFAULT_FMT_WIDTH;                                             // width

        _fmt_numPrecision = DEFAULT_FLOAT_PRECISION;                                // precision
        _fmt_strCharsToPrint = DEFAULT_STR_CHARS_TO_PRINT;

        strcpy(_fmt_numSpecifier, DEFAULT_FLOAT_SPECIFIER);                         // specifier   
        strcpy(_fmt_stringSpecifier, DEFAULT_STR_SPECIFIER);

        _fmt_numFmtFlags = DEFAULT_FLOAT_FLAGS;                                     // flags
        _fmt_stringFmtFlags = DEFAULT_STR_FLAGS;                                    // flags


        // display and other settings
        // --------------------------

        _promptAndEcho = 2, _printLastResult = 1;
        _angleMode = 0;

        _printTraceValueOnly = 0;                                                   // init: view expression texts and values during tracing
    }
}


// -------------------------------------------------------------------------------------
// *   delete all identifier names (char strings)                                      *
// *   note: this excludes generic identifier names stored as alphanumeric constants   *
// -------------------------------------------------------------------------------------

void Justina::deleteIdentifierNameObjects(char** pIdentNameArray, int identifiersInUse, bool isUserVar) {
    int index = 0;          // points to last variable in use
    while (index < identifiersInUse) {                       // points to variable in use
    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print(isUserVar ? "\r\n----- (usrvar name) " : "\r\n----- (ident name ) "); _pDebugOut->println((uint32_t) * (pIdentNameArray + index), HEX);
        _pDebugOut->print("       delete ident "); _pDebugOut->println(*(pIdentNameArray + index));
    #endif
        isUserVar ? _userVarNameStringObjectCount-- : _identifierNameStringObjectCount--;
        delete[] * (pIdentNameArray + index);
        index++;
    }
}


// --------------------------------------------------------------------------------------------------------------
// *   all string array variables of a specified scope: delete array element character strings (heap objects)   *
// --------------------------------------------------------------------------------------------------------------

void Justina::deleteStringArrayVarsStringObjects(Justina::Val* varValues, char* sourceVarScopeAndFlags, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar, bool isLocalVar) {
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

void Justina::deleteOneArrayVarStringObjects(Justina::Val* varValues, int index, bool isUserVar, bool isLocalVar) {
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
            _pDebugOut->print(isUserVar ? "\r\n----- (usr arr str) " : isLocalVar ? "\r\n-----(loc arr str)" : "\r\n----- (arr string ) "); _pDebugOut->println((uint32_t)pString, HEX);     // applicable to string and array (same pointer)
            _pDebugOut->print(" delete arr.var.str "); _pDebugOut->println(pString);
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

void Justina::deleteVariableValueObjects(Justina::Val* varValues, char* varType, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar, bool isLocalVar) {

    int index = 0;
    // do NOT skip parameters if deleting function variables: with constant args, a local copy is created (always scalar) and must be deleted if non-empty string
    while (index < varNameCount) {
        if (!checkIfGlobalValue || (varType[index] & (var_nameHasGlobalValue))) {                                                   // global value ?
            // check for arrays before checking for strings (if both 'var_isArray' and 'value_isStringPointer' bits are set: array of strings, with strings already deleted)
            if (((varType[index] & value_typeMask) != value_isVarRef) && (varType[index] & var_isArray)) {                          // variable is an array: delete array storage          
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print(isUserVar ? "\r\n----- (usr ar stor) " : isLocalVar ? "\r\n----- (loc ar stor) " : "\r\n----- (array stor ) "); _pDebugOut->println((uint32_t)varValues[index].pArray, HEX);
            #endif
                isUserVar ? _userArrayObjectCount-- : isLocalVar ? _localArrayObjectCount-- : _globalStaticArrayObjectCount--;
                delete[]  varValues[index].pArray;
            }
            else if ((varType[index] & value_typeMask) == value_isStringPointer) {                                                  // variable is a scalar containing a string
                if (varValues[index].pStringConst != nullptr) {
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print(isUserVar ? "\r\n----- (usr var str) " : isLocalVar ? "\r\n----- (loc var str)" : "\r\n----- (var string ) "); _pDebugOut->println((uint32_t)varValues[index].pStringConst, HEX);
                    _pDebugOut->print("     del.var.values "); _pDebugOut->println(varValues[index].pStringConst);
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
// *   delete variable heap objects: last value FIFO string objects   *
// --------------------------------------------------------------------

void Justina::deleteLastValueFiFoStringObjects() {
    if (_lastValuesCount == 0) return;

    for (int i = 0; i < _lastValuesCount; i++) {
        bool isNonEmptyString = (lastResultTypeFiFo[i] == value_isStringPointer) ? (lastResultValueFiFo[i].pStringConst != nullptr) : false;
        if (isNonEmptyString) {
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n----- (FiFo string) "); _pDebugOut->println((uint32_t)lastResultValueFiFo[i].pStringConst, HEX);
            _pDebugOut->print("del.last.v.fifo str "); _pDebugOut->println(lastResultValueFiFo[i].pStringConst);
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

void Justina::deleteConstStringObjects(char* pFirstToken) {
    char* pAnum;
    TokenPointer prgmCnt;

    prgmCnt.pTokenChars = pFirstToken;
    uint8_t tokenType = *prgmCnt.pTokenChars & 0x0F;
    while (tokenType != tok_no_token) {                                                                 // for all tokens in token list
        // not for predefined symbolic constants
        bool isStringConst = (tokenType == tok_isConstant) ? (((*prgmCnt.pTokenChars >> 4) & value_typeMask) == value_isStringPointer) : false;

        if (isStringConst || (tokenType == tok_isGenericName)) {
            memcpy(&pAnum, prgmCnt.pCstToken->cstValue.pStringConst, sizeof(pAnum));                    // pointer not necessarily aligned with word size: copy memory instead
            if (pAnum != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n----- (parsed str ) ");   _pDebugOut->println((uint32_t)pAnum, HEX);
                _pDebugOut->print("   del.const.string ");   _pDebugOut->println(pAnum);
            #endif
                _parsedStringConstObjectCount--;
                delete[] pAnum;
            }
        }
        uint8_t tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) : (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) :
            (tokenType == tok_isSymbolicConstant) ? sizeof(TokenIsSymbolicConstant) : (*prgmCnt.pTokenChars >> 4) & 0x0F;
        prgmCnt.pTokenChars += tokenLength;
        tokenType = *prgmCnt.pTokenChars & 0x0F;
    }
}


// ---------------------------------------------------------------------------------
// *   delete a variable string object referenced in an evaluation stack element   *
// ---------------------------------------------------------------------------------

// if not a string, then do nothing. If not a string, or string is empty, then exit WITH error

Justina::execResult_type Justina::deleteVarStringObject(LE_evalStack* pStackLvl) {
    if (pStackLvl->varOrConst.tokenType != tok_isVariable) { return result_execOK; };                                               // not a variable
    if ((*pStackLvl->varOrConst.varTypeAddress & value_typeMask) != value_isStringPointer) { return result_execOK; }                // not a string object
    if (*pStackLvl->varOrConst.value.ppStringConst == nullptr) { return result_execOK; }

    char varScope = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);

    // delete variable string object
#if PRINT_HEAP_OBJ_CREA_DEL
    _pDebugOut->print((varScope == var_isUser) ? "\r\n----- (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "\r\n----- (var string ) " : "\r\n----- (loc var str) ");
    _pDebugOut->println((uint32_t)*pStackLvl->varOrConst.value.ppStringConst, HEX);
    _pDebugOut->print("     del var string "); _pDebugOut->println(*pStackLvl->varOrConst.value.ppStringConst);
#endif
    (varScope == var_isUser) ? _userVarStringObjectCount-- : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount-- : _localVarStringObjectCount--;
    delete[] * pStackLvl->varOrConst.value.ppStringConst;
    return result_execOK;
}


// --------------------------------------------------------------------------------------
// *   delete an intermediate string object referenced in an evaluation stack element   *
// --------------------------------------------------------------------------------------

// if not a string, then do nothing. If not an intermediate string object, then exit WITHOUT error

Justina::execResult_type Justina::deleteIntermStringObject(LE_evalStack* pStackLvl) {

    if ((pStackLvl->varOrConst.valueAttributes & constIsIntermediate) != constIsIntermediate) { return result_execOK; }             // not an intermediate constant
    if (pStackLvl->varOrConst.valueType != value_isStringPointer) { return result_execOK; }                                         // not a string object
    if (pStackLvl->varOrConst.value.pStringConst == nullptr) { return result_execOK; }
#if PRINT_HEAP_OBJ_CREA_DEL
    _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)_pEvalStackTop->varOrConst.value.pStringConst, HEX);
    _pDebugOut->print("  del.interm.string ");   _pDebugOut->println(_pEvalStackTop->varOrConst.value.pStringConst);
#endif
    _intermediateStringObjectCount--;
    delete[] pStackLvl->varOrConst.value.pStringConst;

    return result_execOK;
}


