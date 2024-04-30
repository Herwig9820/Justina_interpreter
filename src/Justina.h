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
*   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;              *
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
************************************************************************************************************/


#ifndef _JUSTINA_h
#define _JUSTINA_h

#if !defined(ARDUINO_ARCH_SAMD) && !defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_ESP32)
#error “The Justina interpreter library only supports boards with a SAMD, RP2040 or ESP32 architecture.”
#endif

#include "Arduino.h"
#include <SD.h>
#include <SPI.h>

// if Justina constants file is found, include it
#if defined(__has_include)
#if __has_include ("../../Justina_constants/Justina_constants.h")
#include "../../Justina_constants/Justina_constants.h"
#endif
#endif

// default values in case "Justina_constants.h" is not found OR it doesn't contain a #define statement for a particular constant
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32)

#if !defined(PROGMEM_SIZE)
#define PROGMEM_SIZE 65536      // program memory, in bytes (INCLUDING immediate mode parsed statements size). Maximum: 2^16 = 65536. Don't go lower than 2000 
#endif
#if !defined(MAXVAR_USER)
#define MAXVAR_USER 255         // max. distinct user variables allowed. Absolute limit: 255
#endif
#if !defined(MAXVAR_PROG)
#define MAXVAR_PROG 255         // max. program variable NAMES allowed (distinct global, static, local/parameter variables may share the same name). Absolute limit: 255
#endif
#if !defined(MAXVAR_STAT)
#define MAXVAR_STAT 255         // max. distinct static variables allowed. Absolute limit: 255
#endif
#if !defined(MAXFUNC)
#define MAXFUNC 255             // max. Justina functions allowed. Absolute limit: 255
#endif

#else

#if !defined(PROGMEM_SIZE)
#define PROGMEM_SIZE 4000
#endif
#if !defined(MAXVAR_USER)
#define MAXVAR_USER 64
#endif
#if !defined(MAXVAR_PROG)
#define MAXVAR_PROG 64
#endif
#if !defined(MAXVAR_STAT)
#define MAXVAR_STAT 32
#endif
#if !defined(MAXFUNC)
#define MAXFUNC 32
#endif

#endif

// max. user variables allowed. Absolute limit: 255
// max. program variable NAMES allowed (independent global, static, local/parameter variables may share the same name). Absolute limit: 255
// max. static variable NAMES allowed (independent static variables in multiple functions may share the same name). Absolute limit: 255
// max. Justina functions allowed. Absolute limit: 255

#define J_productName "Justina: JUST an INterpreter for Arduino"
#define J_legalCopyright "Copyright 2024, Herwig Taveirne"
#define J_version "1.1.1"            
#define J_buildDate "March 18, 2024"


// ******************************************************************
// ***                      class LinkedList                      ***
// ******************************************************************

// store and retrieve data in linked lists

class LinkedList {
    friend class Justina;

    // --------------------
    // *   enumerations   *
    // --------------------

    static constexpr int listNameSize = 9;                              // including terminating '\0'


    // ------------------
    // *   structures   *
    // ------------------


    struct ListElemHead {                                               // list element structure (fixed length for all data types)
        ListElemHead* pNext;                                            // pointer to next list element
        ListElemHead* pPrev;                                            // pointer to previous list element (currently not used; needed if deleting other list elements than last) 
    };


    // -----------------
    // *   variables   *
    // -----------------


    static Print** _ppDebugOutStream;                                   // pointer to pointer to debug stream
    static int _listIDcounter;                                          // number of lists created
    static long _createdListObjectCounter;                              // count of created objects across lists

    long _listElementCount = 0;                                         // linked list length (number of objects in list)

    ListElemHead* _pFirstElement = nullptr;                             // pointers to first and last list element
    ListElemHead* _pLastElement = nullptr;

    char _listName[listNameSize] = "";                                  // includes terminating '\0'
    int _listID{ 0 };                                                   // list ID (in order of creation) 


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

    LinkedList();                                                       // constructor
    ~LinkedList();                                                      // destructor

    char* appendListElement(int size);
    char* deleteListElement(void* pPayload = nullptr);                  // pointer to payload of list element to be removed
    void deleteList();
    char* getFirstListElement();
    char* getLastListElement();
    char* getPrevListElement(void* pPayload);
    char* getNextListElement(void* pPayload);
    int getElementCount();
    int getListID();
    void setListName(char* listName);
    char* getListName();
    void setDebugOutStream(Print** pDebugOutStream);
    static long getCreatedObjectCount();
};


// *****************************************************************
// ***                      class Justina                        ***
// *****************************************************************

class Breakpoints;                                                      // forward declaration

class Justina {
    friend class Breakpoints;

    // --------------------
    // *   enumerations   *
    // --------------------

    // these values are used in a CmdBlockDef structure and are shared between multiple commands
    enum blockType_type {
        // value 1: block type
        block_none,                                                     // command is not a block command
        block_JustinaFunction,
        block_for,
        block_while,
        block_if,
        block_alterFlow,                                                // alter flow in specific open block types
        block_genericEnd,                                               // ends any type of open block

        block_eval,                                                     // execution only, signals execution of parsed eval() string 
        block_trigger,                                                  // execution only, signals execution of parsed trigger string

        // value 2, 3, 4: position in open block, min & max position of previous block command within same block level
        block_na,                                                       // not applicable for this block type
        block_startPos,                                                 // command starts an open block
        block_midPos1,                                                  // command only allowed in open block  
        block_midPos2,                                                  // command only allowed in open block
        block_endPos,                                                   // command ends an open block
        block_inOpenFunctionBlock,                                      // command can only occur if currently a function block is open
        block_inOpenLoopBlock,                                          // command can only occur if at least one loop block is open

        // alternative for value 2: type of command (only if block type = block_none)
        cmd_program,
        cmd_globalVar,
        cmd_localVar,
        cmd_staticVar,
        cmd_deleteVar
    };

    // unique identification code of a Justina command
    enum cmd_code {
        cmdcod_none,                                                    // no command being executed

        cmdcod_program,
        cmdcod_deleteVar,
        cmdcod_clearAll,
        cmdcod_clearProg,
        cmdcod_printVars,
        cmdcod_printCallSt,
        cmdcod_printBP,
        cmdcod_function,
        cmdcod_static,
        cmdcod_constVar,
        cmdcod_var,
        cmdcod_for,
        cmdcod_while,
        cmdcod_if,
        cmdcod_elseif,
        cmdcod_else,
        cmdcod_break,
        cmdcod_continue,
        cmdcod_return,
        cmdcod_end,
        cmdcod_pause,
        cmdcod_halt,
        cmdcod_stop,
        cmdcod_abort,
        cmdcod_go,
        cmdcod_step,
        cmdcod_stepOut,
        cmdcod_stepOver,
        cmdcod_stepOutOfBlock,
        cmdcod_stepToBlockEnd,
        cmdcod_setNextLine,
        cmdcod_trace,
        cmdcod_traceExprOn,
        cmdcod_traceExprOff,
        cmdcod_debug,
        cmdcod_BPon,
        cmdcod_BPoff,
        cmdcod_setBP,
        cmdcod_clearBP,
        cmdcod_enableBP,
        cmdcod_disableBP,
        cmdcod_nop,
        cmdcod_raiseError,
        cmdcod_trapErrors,
        cmdcod_clearError,
        cmdcod_quit,
        cmdcod_info,
        cmdcod_input,
        cmdcod_dbout,
        cmdcod_dboutLine,
        cmdcod_cout,
        cmdcod_coutLine,
        cmdcod_coutList,
        cmdcod_print,
        cmdcod_printLine,
        cmdcod_printList,
        cmdcod_printToVar,
        cmdcod_printLineToVar,
        cmdcod_printListToVar,
        cmdcod_dispwidth,
        cmdcod_floatfmt,
        cmdcod_intfmt,
        cmdcod_dispmod,
        cmdcod_tabSize,
        cmdcod_angle,
        cmdcod_declCB,
        cmdcod_loadProg,
        cmdcod_receiveFile,
        cmdcod_sendFile,
        cmdcod_copyFile,
        cmdcod_listFilesToSer,
        cmdcod_listFiles,
        cmdcod_startSD,
        cmdcod_stopSD,
        cmdcod_setConsole,
        cmdcod_setConsIn,
        cmdcod_setConsOut,
        cmdcod_setDebugOut
    };

    // unique identification code of an internal cpp function (= built into Justina)
    enum func_code {
        fnccod_ifte,
        fnccod_switch,
        fnccod_index,
        fnccod_choose,

        fnccod_sqrt,
        fnccod_sin,
        fnccod_cos,
        fnccod_tan,
        fnccod_asin,
        fnccod_acos,
        fnccod_atan,
        fnccod_ln,
        fnccod_lnp1,
        fnccod_log10,
        fnccod_exp,
        fnccod_expm1,
        fnccod_round,
        fnccod_ceil,
        fnccod_floor,
        fnccod_trunc,
        fnccod_min,
        fnccod_max,
        fnccod_abs,
        fnccod_sign,
        fnccod_fmod,

        fnccod_eval,
        fnccod_ubound,
        fnccod_dims,
        fnccod_valueType,
        fnccod_last,
        fnccod_getTrappedErr,
        fnccod_isColdStart,
        fnccod_asc,
        fnccod_replaceChar,
        fnccod_char,
        fnccod_len,
        fnccod_nl,
        fnccod_format,
        fnccod_sysVal,

        fnccod_ltrim,
        fnccod_rtrim,
        fnccod_trim,
        fnccod_left,
        fnccod_mid,
        fnccod_right,
        fnccod_toupper,
        fnccod_tolower,
        fnccod_space,
        fnccod_tab,
        fnccod_gotoColumn,
        fnccod_getColumnPos,
        fnccod_repeatchar,
        fnccod_findsubstr,
        fnccod_replacesubstr,
        fnccod_strcmp,
        fnccod_strcasecmp,
        fnccod_ascToHexString,
        fnccod_hexStringToAsc,
        fnccod_quote,

        fnccod_cint,
        fnccod_cfloat,
        fnccod_cstr,

        fnccod_millis,
        fnccod_micros,
        fnccod_delay,
        fnccod_pinMode,
        fnccod_digitalRead,
        fnccod_digitalWrite,
        fnccod_analogRead,
        fnccod_analogReference,
        fnccod_analogWrite,
        fnccod_analogReadResolution,
        fnccod_analogWriteResolution,
        fnccod_noTone,
        fnccod_pulseIn,
        fnccod_shiftIn,
        fnccod_shiftOut,
        fnccod_tone,
        fnccod_random,
        fnccod_randomSeed,

        fnccod_bit,
        fnccod_bitClear,
        fnccod_bitSet,
        fnccod_bitRead,
        fnccod_bitWrite,
        fnccod_wordMaskedClear,
        fnccod_wordMaskedSet,
        fnccod_wordMaskedRead,
        fnccod_wordMaskedWrite,
        fnccod_byteRead,
        fnccod_byteWrite,
        fnccod_mem32Read,
        fnccod_mem8Read,
        fnccod_mem32Write,
        fnccod_mem8Write,

        fnccod_isAlpha,
        fnccod_isAlphaNumeric,
        fnccod_isAscii,
        fnccod_isControl,
        fnccod_isDigit,
        fnccod_isGraph,
        fnccod_isHexadecimalDigit,
        fnccod_isLowerCase,
        fnccod_isPrintable,
        fnccod_isPunct,
        fnccod_isUpperCase,
        fnccod_isWhitespace,

        fnccod_open,
        fnccod_close,

        fnccod_cin,
        fnccod_cinLine,
        fnccod_cinParseList,
        fnccod_read,
        fnccod_readLine,
        fnccod_parseList,
        fnccod_parseListFromVar,

        fnccod_find,
        fnccod_findUntil,

        fnccod_position,
        fnccod_seek,
        fnccod_size,
        fnccod_name,
        fnccod_fullName,
        fnccod_available,
        fnccod_peek,
        fnccod_setTimeout,
        fnccod_getTimeout,
        fnccod_availableForWrite,
        fnccod_getWriteError,
        fnccod_clearWriteError,
        fnccod_flush,
        fnccod_isDirectory,
        fnccod_rewindDirectory,
        fnccod_openNextFile,
        fnccod_fileNumber,
        fnccod_hasOpenFile,
        fnccod_closeAll,
        fnccod_exists,
        fnccod_mkdir,
        fnccod_rmdir,
        fnccod_remove
    };

    // unique identification code of operators and other terminals
    enum termin_code {
        // operators
        termcod_assign = 0,
        termcod_plusAssign,
        termcod_minusAssign,
        termcod_multAssign,
        termcod_divAssign,
        termcod_modAssign,
        termcod_bitAndAssign,
        termcod_bitOrAssign,
        termcod_bitXorAssign,
        termcod_bitShLeftAssign,
        termcod_bitShRightAssign,

        termcod_lt,
        termcod_gt,
        termcod_ltoe,
        termcod_gtoe,
        termcod_ne,
        termcod_eq,

        termcod_plus,
        termcod_minus,
        termcod_mult,
        termcod_div,
        termcod_mod,
        termcod_pow,
        termcod_incr,
        termcod_decr,
        termcod_and,
        termcod_or,
        termcod_not,

        termcod_bitCompl,
        termcod_bitShLeft,
        termcod_bitShRight,
        termcod_bitAnd,
        termcod_bitOr,
        termcod_bitXor,

        termcod_opRangeEnd = termcod_bitXor,

        // other terminals
        termcod_comma = termcod_opRangeEnd + 1,
        termcod_semicolon,
        termcod_semicolon_BPallowed,                                    // breakpoint can be set because next statement is first statement in next source code line
        termcod_semicolon_BPset,                                        // breakpoint is set
        termcod_leftPar,
        termcod_rightPar
    };

    enum tokenType_type {                                               // token type
        tok_no_token,                                                   // no token to process
        tok_isReservedWord,
        tok_isInternCppFunction,
        tok_isExternCppFunction,
        tok_isJustinaFunction,
        tok_isConstant,
        tok_isSymbolicConstant,                                         // predefined Justina constant
        tok_isVariable,                                                 // including constant variables (CONST)
        tok_isGenericName,

        // all terminal tokens: at the end of the list ! (occupy only one character in program, combining token type and index)
        tok_isTerminalGroup1,                                           // if index in terminal table < 15 (too many operators to fit in 4 bits)
        tok_isTerminalGroup2,                                           // if index between 16 and 31
        tok_isTerminalGroup3,                                           // if index between 32 and 47

        tok_isEvalEnd,                                                  // execution only, signals end of parsed eval() statements and of trigger strings
    };

    // error codes for all PARSING errors
    enum parsingResult_type {                                           // token parsing result
        result_parsing_OK = 0,                                          // no error

        // incomplete expression errors
        result_statementTooLong = 1000,
        result_tokenNotFound,
        result_missingLeftParenthesis,
        result_missingRightParenthesis,

        // token not allowed errors
        result_separatorNotAllowedHere = 1100,
        result_operatorNotAllowedHere,
        result_prefixOperatorNotAllowedhere,
        result_invalidOperator,
        result_parenthesisNotAllowedHere,
        result_resWordNotAllowedHere,
        result_functionNotAllowedHere,
        result_variableNotAllowedHere,
        result_alphaConstNotAllowedHere,
        result_numConstNotAllowedHere,
        result_assignmNotAllowedHere,
        result_cannotChangeConstantValue,
        result_identifierNotAllowedHere,
        result_prefixCharNotAllowedHere,

        // token expected errors
        result_constantValueExpected = 1200,
        result_variableNameExpected,
        result_assignmentOrSeparatorExpected,
        result_separatorExpected,

        // used memory errors
        result_maxVariableNamesReached = 1300,
        result_maxLocalVariablesReached,
        result_maxStaticVariablesReached,
        result_maxJustinaFunctionsReached,
        result_progMemoryFull,

        // token errors
        result_identifierTooLong = 1400,
        result_spaceMissing,
        result_token_not_recognised,
        result_alphaConstTooLong,
        result_alphaConstInvalidEscSeq,
        result_alphaNoCtrlCharAllowed,
        result_alphaClosingQuoteMissing,
        result_numberInvalidFormat,
        result_parse_overflow,                                          // note: underflow conditions are not detected during parsing

        // function definition or call errors
        result_function_wrongArgCount = 1500,
        result_function_redefinitionNotAllowed,
        result_function_mandatoryArgFoundAfterOptionalArgs,
        result_function_maxArgsExceeded,
        result_function_prevCallsWrongArgCount,
        result_function_defsCannotBeNested,
        result_function_scalarAndArrayArgOrderNotConsistent,
        result_function_scalarArgExpected,
        result_function_arrayArgExpected,
        result_function_redefiningNotAllowed,
        result_function_undefinedFunctionOrArray,
        result_function_arrayParamMustHaveEmptyDims,
        result_function_needsParentheses,

        // variable errors
        result_var_nameInUseForFunction = 1600,
        result_var_notDeclared,
        result_var_redeclared,
        result_var_definedAsScalar,
        result_var_definedAsArray,
        result_var_constantArrayNotAllowed,
        result_var_constantVarNeedsAssignment,
        result_var_ControlVarInUse,
        result_var_controlVarIsConstant,
        result_var_illegalInDeclaration,
        result_var_illegalInProgram,
        result_var_usedInProgram,
        result_var_deleteSyntaxinvalid,

        // array errors
        result_arrayDef_noDims = 1700,
        result_arrayDef_negativeDim,
        result_arrayDef_dimTooLarge,
        result_arrayDef_maxDimsExceeded,
        result_arrayDef_maxElementsExceeded,
        result_arrayDef_emptyInitStringExpected,
        result_arrayDef_dimNotValid,
        result_arrayUse_noDims,
        result_arrayUse_wrongDimCount,

        // command errors and command argument errors
        result_cmd_programCmdMissing = 1800,
        result_cmd_onlyProgramStart,
        result_cmd_onlyImmediateMode,
        result_cmd_onlyImmModeFirstStatement,
        result_cmd_onlyInsideProgram,
        result_cmd_onlyInsideFunction,
        result_cmd_onlyOutsideFunction,
        result_cmd_onlyImmediateOrInFunction,
        result_cmd_onlyInProgOutsideFunction,
        result_cmd_onlyImmediateNotWithinBlock,

        result_cmd_resWordExpectedAsPar,
        result_cmd_expressionExpectedAsPar,
        result_cmd_varWithoutAssignmentExpectedAsPar,
        result_cmd_varWithOptionalAssignmentExpectedAsPar,
        result_cmd_variableExpectedAsPar,
        result_cmd_variableNameExpectedAsPar,
        result_cmd_identExpectedAsPar,
        result_cmd_argumentMissing,
        result_cmd_tooManyArguments,

        // user callback errors
        result_userCB_allAliasesSet = 1900,
        result_userCB_aliasRedeclared,

        // block command errors
        result_block_noBlockEnd = 2000,
        result_block_noOpenBlock,
        result_block_noOpenLoop,
        result_block_noOpenFunction,
        result_block_notAllowedInThisOpenBlock,
        result_block_wrongBlockSequence,

        // tracing, eval() and other PARSING errors during EXECUTION phase
        result_trace_eval_resWordNotAllowed = 2100,
        result_trace_eval_genericNameNotAllowed,
        result_trace_userFunctonNotAllowed,                             // tracing restriction only
        result_trace_evalFunctonNotAllowed,                             // tracing restriction only
        result_parseList_stringNotComplete,
        result_parseList_valueToParseExpected,

        // breakpoint errors
        result_BP_lineRangeTooLong,
        result_BP_lineTableMemoryFull,
        result_BP_emptyTriggerString,
        result_BP_triggerString_nothingToEvaluate,

        // other program errors
        result_parse_abort = 2200,
        result_parse_setStdConsole,
        result_parse_kill
    };

    // error codes for all EXECUTION errors
    enum execResult_type {
        result_execOK = 0,

        // start of valid exec error range (tested upon return of user cpp functions containing an error code)
        result_startOfExecErrorRange = 3000,

        // arrays
        result_array_subscriptOutsideBounds = 3000,
        result_array_subscriptNonInteger,
        result_array_subscriptNonNumeric,
        result_array_dimCountInvalid,
        result_array_valueTypeIsFixed,

        // cpp function arguments
        result_arg_outsideRange = 3100,
        result_arg_integerTypeExpected,
        result_arg_floatTypeExpected,
        result_arg_stringExpected,
        result_arg_numberExpected,
        result_arg_nonEmptyStringExpected,
        result_arg_stringTooShort,
        result_arg_invalid,
        result_arg_integerDimExpected,
        result_arg_dimNumberInvalid,
        result_arg_varExpected,
        result_arg_tooManyArgs,
        result_arg_wrongSpecifierForDataType,

        // numbers and strings
        result_integerTypeExpected = 3200,
        result_floatTypeExpected,
        result_numberExpected,
        result_operandsNumOrStringExpected,
        result_undefined,
        result_overflow,
        result_underflow,
        result_divByZero,
        result_testexpr_numberExpected,

        // abort, kill, quit
        result_noProgramStopped = 3300,                                 // 'go' command not allowed because not in debug mode

        // breakpoint errors
        result_BP_sourcelineNumberExpected = 3400,
        result_BP_notAllowedForSourceLine,
        result_BP_statementIsNonExecutable,
        result_BP_maxBPentriesReached,
        result_BP_wasNotSet,
        result_BP_hitcountNotWithinRange,
        result_BP_sourceLineNotInStoppedFunction,
        result_BP_cannotMoveIntoBlocks,

        // evaluation and list parsing function errors
        result_eval_emptyString = 3500,
        result_eval_nothingToEvaluate,
        result_eval_parsingError,
        result_list_parsingError,

        // SD card
        result_SD_noCardOrNotAllowed = 3600,
        result_SD_noCardOrCardError,
        result_SD_fileNotFound,
        result_SD_couldNotOpenFile,                                     // or file does not exist 
        result_SD_fileIsNotOpen,
        result_SD_fileAlreadyOpen,
        result_SD_invalidFileNumber,
        result_SD_fileIsEmpty,
        result_SD_maxOpenFilesReached,
        result_SD_fileSeekError,
        result_SD_directoryExpected,
        result_SD_directoryNotAllowed,
        result_SD_couldNotCreateFileDir,
        result_SD_directoryDoesNotExist,
        result_SD_pathIsNotValid,
        result_SD_sourceIsDestination,
        result_SD_fileNotAllowedHere,

        // IO streams
        result_IO_invalidStreamNumber = 3700,
        result_IO_noDeviceOrNotForInput,
        result_IO_noDeviceOrNotForOutput,

        // end of valid exec error range (tested upon return of user cpp functions containing an error code)
        result_endOfExecErrorRange = 4999,


        // *** MANDATORY =>LAST<= range of errors: events ***
        // *** ------------------------------------------ ***
        result_startOfEvents = 9000,

        // abort, kill, quit, stop EVENTS (first handled as errors - which they are not - initially following the same flow)
        result_stopForDebug = 9000,                                     // 'Stop' command executed (from inside a program only): this enters debug mode
        result_stopForBreakpoint,                                       // breakpoint encountered
        result_abort,                                                   // abort running program (return to Justina prompt)
        result_kill,                                                    // caller requested to exit Justina interpreter
        result_quit,                                                    // 'Quit' command executed (exit Justina interpreter)

        result_initiateProgramLoad                                      // command processed to start loading a program
    };

    // debug codes
    enum dbType_type {
        db_continue = 0,
        db_singleStep,
        db_stepOut,
        db_stepOver,
        db_stepOutOfBlock,
        db_stepToBlockEnd,
    };


    // ---------------------
    // *   constants (1)   *
    // ---------------------

    // ---------------------------------------------------------------
    // constants that may be changed freely within specific boundaries
    // ---------------------------------------------------------------

    static constexpr uint16_t IMM_MEM_SIZE{ 500 };                              // size, in bytes, of user command memory (stores parsed user statements entered from the keyboard)
    static constexpr uint16_t BP_LINE_RANGE_PROGMEM_STOR_RATIO{ 5 };            // breakpoints: source line range storage as a % of program storage 
    static constexpr uint16_t MAX_BP_COUNT{ 10 };                               // breakpoints: maximum number of set breakpoints 

    // user, program 
    static constexpr long _PROGRAM_MEMORY_SIZE{ PROGMEM_SIZE - IMM_MEM_SIZE };  // program memory, in bytes (excluding immediate mode parsed statements size). Maximum: 65536 (2^16). Don't go lower than 2000 
    static constexpr int MAX_USERVARNAMES{ MAXVAR_USER };                       // max. user variables allowed. Absolute parser limit: 255
    static constexpr int MAX_PROGVARNAMES{ MAXVAR_PROG };                       // max. program variable NAMES allowed (same name may be reused for global, static, local & parameter variables). Absolute limit: 255
    static constexpr int MAX_STATIC_VARIABLES{ MAXVAR_STAT };                   // max. static variables allowed across all parsed functions (only). Absolute limit: 255
    static constexpr int MAX_LOCAL_VARIABLES{ 255 };                            // max. local variables allowed across all parsed functions, including function parameters. Absolute limit: 255
    static constexpr int MAX_JUSTINA_FUNCTIONS{ MAXFUNC };                      // max. Justina functions allowed. Absolute limit: 255
    static constexpr int MAX_LOC_VARS_IN_FUNC{ 32 };                            // max. local and parameter variables allowed (only) in an INDIVIDUAL parsed function. Absolute limit: 255 
    static constexpr int MAX_ARRAY_DIMS{ 3 };                                   // max. array dimensions allowed. Absolute limit: 3 
    static constexpr int MAX_ARRAY_ELEM{ 1000 };                                // max. elements allowed in an array. Absolute limit: 2^15-1 = 32767. Individual dimensions are limited to a size of 255
    static constexpr int MAX_LAST_RESULT_DEPTH{ 10 };                           // max. depth of 'last results' FiFo

    static constexpr int MAX_IDENT_NAME_LEN{ 30 };                              // max length of identifier names, excluding terminating '\0'
    static constexpr int MAX_ALPHA_CONST_LEN{ 255 };                            // max length of character strings stored in variables, excluding terminating '\0',. Absolute limit: 255
    static constexpr int MAX_USER_INPUT_LEN{ 100 };                             // max. length of text a user can enter with an INPUT statement. Absolute limit: 255
    static constexpr int MAX_STATEMENT_LEN{ 500 };                              // max. length of a single user statement 

    static constexpr int MAX_OPEN_SD_FILES{ 5 };                                // SD card: max. concurrent open files

    static constexpr long LONG_WAIT_FOR_CHAR_TIMEOUT{ 10000 };                  // milliseconds
    static constexpr long DEFAULT_READ_TIMEOUT{ 500 };                          // milliseconds

    // ------------------------------------------------------------------------------------------------------
    // constants that should NOT be changed without carefully examining the impact on the Justina application
    // ------------------------------------------------------------------------------------------------------

    // (1) display and (2) fmt() settings
    // -> display: last values, command line echo, tracing, print commands
    // -------------------------------------------------------------------


    // maximum values
    static inline const int MIN_CONSOLE_PRINT_WIDTH = 16;               // min. width of console line. Width as in c++ printf 'format.width' sub-specifier (useful for small OLED displays)
    static inline const int MAX_CONSOLE_PRINT_WIDTH = 255;              // min. width of console line. Width as in c++ printf 'format.width' sub-specifier

    static inline const int MAX_PRINT_WIDTH = 255;                      // max. width of print field. Absolute limit: 255. Width as in c++ printf 'format.width' sub-specifier
    static inline const int MAX_INT_PRECISION = 10;                     // max. integer precision (2**31: 10 digits). Precision as defined as in c++ printf 'format.precision' sub-specifier for integers
    static inline const int MAX_FLOAT_PRECISION = 8;                    // max. floating-point precision. Precision as defined as in c++ printf 'format.precision' sub-specifier for floating-point numbers
    static inline const int MAX_STRCHAR_TO_PRINT = 255;                 // max. # of alphanumeric characters to print. Absolute limit: 255. Defined as in c++ printf 'format.precision' sub-specifier

    static inline const char DEFAULT_FLOAT_SPECIFIER[2]{ "f" };         // default specifier for floating point numbers. Arduino doesn't recognize uppercase "F"
    static inline const char DEFAULT_INT_SPECIFIER[2]{ "d" };           // default specifier for integers 
    static inline const char DEFAULT_STR_SPECIFIER[2]{ "s" };           // default specifier for integers 

    // separate defaults for display settings and fmt() function
    static constexpr int DEFAULT_DISP_WIDTH{ 64 };                      // display setting      : default width of the print field 
    static constexpr int DEFAULT_TAB_SIZE{ 8 };                         // display setting      : default width of the print field 
    static constexpr int DEFAULT_FMT_WIDTH{ 0 };                        // fmt() function output: default width of the print field 

    // shared defaults for display settings AND fmt() function settings    
    static constexpr int DEFAULT_FLOAT_PRECISION{ 2 };                  // default precision for floating point numbers
    static constexpr int DEFAULT_INT_PRECISION{ 1 };                    // default 'minimum digits to print' for integers 
    static constexpr int DEFAULT_STR_CHARS_TO_PRINT{ DEFAULT_DISP_WIDTH };  // default # alphanumeric characters to print

    static constexpr int DEFAULT_FLOAT_FLAGS{ 0X08 };                   // default for floating point numbers: always print decimal point
    static constexpr int DEFAULT_INT_FLAGS{ 0X00 };                     // default for integers: no flags
    static constexpr int DEFAULT_STR_FLAGS{ 0X00 };                     // default for strings: no flags



    // terminals (parsing)
    // -------------------

    //NOTE: terminal symbols should NOT start and end with an alphanumeric character or with an underscore

    // ASSIGNMENT operator: ONE character only, character should NOT appear in any other operator name, except compound operator names (but NOT as first character)
    static constexpr char* term_assign = "=";

    // compound assignment operators
    static constexpr char* term_plusAssign = "+=";
    static constexpr char* term_minusAssign = "-=";
    static constexpr char* term_multAssign = "*=";
    static constexpr char* term_divAssign = "/=";
    static constexpr char* term_modAssign = "%=";
    static constexpr char* term_bitShLeftAssign = "<<=";
    static constexpr char* term_bitShRightAssign = ">>=";
    static constexpr char* term_bitAndAssign = "&=";
    static constexpr char* term_bitOrAssign = "|=";
    static constexpr char* term_bitXorAssign = "^=";

    // prefix and postfix increment operators
    static constexpr char* term_incr = "++";
    static constexpr char* term_decr = "--";

    // relational operators
    static constexpr char* term_lt = "<";
    static constexpr char* term_gt = ">";
    static constexpr char* term_ltoe = "<=";
    static constexpr char* term_gtoe = ">=";
    static constexpr char* term_neq = "!=";
    static constexpr char* term_eq = "==";

    // arithmetic operators
    static constexpr char* term_plus = "+";
    static constexpr char* term_minus = "-";
    static constexpr char* term_mult = "*";
    static constexpr char* term_div = "/";
    static constexpr char* term_mod = "%";
    static constexpr char* term_pow = "**";

    // logical operators
    static constexpr char* term_and = "&&";
    static constexpr char* term_or = "||";
    static constexpr char* term_not = "!";

    // bitwise operators
    static constexpr char* term_bitShLeft = "<<";
    static constexpr char* term_bitShRight = ">>";
    static constexpr char* term_bitAnd = "&";
    static constexpr char* term_bitOr = "|";
    static constexpr char* term_bitXor = "^";
    static constexpr char* term_bitCompl = "~";

    // NON-operator terminals: ONE character only, character should NOT appear in operator names
    static constexpr char* term_semicolon = ";";                        // must be single character
    static constexpr char* term_comma = ",";                            // must be single character
    static constexpr char* term_leftPar = "(";                          // must be single character
    static constexpr char* term_rightPar = ")";                         // must be single character

    // infix operator associativity: bit b7 indicates right-to-left associativity
    // note: prefix operators always have right-to-left associativity; postfix operators: always left_to_right 
    static constexpr uint8_t op_RtoL = 0x80;                            // infix operator: right-to-left associativity 
    static constexpr uint8_t op_long = 0x40;                            // operators: operand(s) must be long (no casting), result is long 
    static constexpr uint8_t res_long = 0x20;                           // result is long (operators can be long or float)


    // railroad tracks (parsing): allowed sequence of tokens
    // -----------------------------------------------------

    // these constants are used to check to which token group (or group of token groups) a parsed token belongs
    static constexpr uint8_t lastTokenGroup_0 = 1 << 0;                 // operator
    static constexpr uint8_t lastTokenGroup_1 = 1 << 1;                 // comma
    static constexpr uint8_t lastTokenGroup_2 = 1 << 2;                 // (line start), semicolon, keyword, generic identifier
    static constexpr uint8_t lastTokenGroup_3 = 1 << 3;                 // number or alphanumeric (literal or symbolic) constant, right bracket
    static constexpr uint8_t lastTokenGroup_4 = 1 << 4;                 // internal cpp, external cpp or Justina function 
    static constexpr uint8_t lastTokenGroup_5 = 1 << 5;                 // left parenthesis
    static constexpr uint8_t lastTokenGroup_6 = 1 << 6;                 // variable, generic identifier

    // groups of token groups: combined token groups (for testing valid token sequences when next token will be parsed)
    static constexpr uint8_t lastTokenGroups_5_2_1_0 = lastTokenGroup_5 | lastTokenGroup_2 | lastTokenGroup_1 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_3 = lastTokenGroup_6 | lastTokenGroup_3;
    static constexpr uint8_t lastTokenGroups_6_3_0 = lastTokenGroup_6 | lastTokenGroup_3 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_3_2_0 = lastTokenGroup_6 | lastTokenGroup_3 | lastTokenGroup_2 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_5_3_0 = lastTokenGroup_6 | lastTokenGroup_5 | lastTokenGroup_3 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_5_4_2_1_0 = lastTokenGroup_6 | lastTokenGroup_5 | lastTokenGroup_4 | lastTokenGroup_2 | lastTokenGroup_1 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_5_3_2_1_0 = lastTokenGroup_6 | lastTokenGroup_5 | lastTokenGroup_3 | lastTokenGroup_2 | lastTokenGroup_1 | lastTokenGroup_0;


    // type & info about of a parenthesis level (parsing)
    // --------------------------------------------------
    static constexpr uint8_t JustinaFunctionBit{ B00000001 };
    static constexpr uint8_t internCppFunctionBit{ B00000010 };
    static constexpr uint8_t externCppFunctionBit{ B00000100 };         // is user-provided cpp function
    static constexpr uint8_t openParenthesisBit{ B00001000 };           // but not a function

    static constexpr uint8_t JustinaFunctionPrevDefinedBit{ B00010000 };
    static constexpr uint8_t arrayBit{ B00100000 };
    static constexpr uint8_t varAssignmentAllowedBit{ B01000000 };
    static constexpr uint8_t varHasPrefixIncrDecrBit{ B10000000 };

    static constexpr uint8_t varIsConstantBit{ B00000001 };


    // commands (FUNCTION, FOR, ...): allowed command parameters for commands with a specific key (parsing)
    // ----------------------------------------------------------------------------------------------------

    // commands parameters: number / type of parameters allowed for a group of commands
    static constexpr uint8_t cmdPar_none = 0;
    static constexpr uint8_t cmdPar_resWord = 1;                        // note: currently, reserved words are not used as command parameters, instead they start a command
    static constexpr uint8_t cmdPar_varNoAssignment = 2;                // and no operators
    static constexpr uint8_t cmdPar_varOptAssignment = 3;
    static constexpr uint8_t cmdPar_expression = 4;
    static constexpr uint8_t cmdPar_JustinaFunction = 5;
    static constexpr uint8_t cmdPar_numConstOnly = 6;
    static constexpr uint8_t cmdPar_ident = 7;

    // flags may be combined with value of one of the allowed types above
    static constexpr uint8_t cmdPar_flagMask = 0x18;                    // allowed 0 to n times. Only for last command parameter
    static constexpr uint8_t cmdPar_multipleFlag = 0x08;                // allowed 0 to n times. Only for last command parameter
    static constexpr uint8_t cmdPar_optionalFlag = 0x10;                // allowed 0 to 1 times. If parameter is present, next parameters do not have to be optional 

    // keys for grouping together commands with the same number / type of parameters

    // command parameter spec name          param type and flags                           param type and flags                            param type and flags                             param type and flags
    // ---------------------------          --------------------                           --------------------                            --------------------                             --------------------
    static inline const char cmdPar_100[4]{ cmdPar_ident | cmdPar_multipleFlag,            cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_101[4]{ cmdPar_ident,                                  cmdPar_expression | cmdPar_multipleFlag,         cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_102[4]{ cmdPar_none,                                   cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_103[4]{ cmdPar_ident,                                  cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_104[4]{ cmdPar_expression,                             cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_105[4]{ cmdPar_expression,                             cmdPar_expression,                               cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_106[4]{ cmdPar_expression | cmdPar_optionalFlag,       cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_107[4]{ cmdPar_expression | cmdPar_multipleFlag,       cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_108[4]{ cmdPar_JustinaFunction,                        cmdPar_none,                                     cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_109[4]{ cmdPar_varOptAssignment,                       cmdPar_expression,                               cmdPar_expression | cmdPar_optionalFlag,        cmdPar_none };
    static inline const char cmdPar_110[4]{ cmdPar_ident,                                  cmdPar_ident | cmdPar_multipleFlag,              cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_111[4]{ cmdPar_varOptAssignment,                       cmdPar_varOptAssignment | cmdPar_multipleFlag,   cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_112[4]{ cmdPar_expression,                             cmdPar_expression | cmdPar_multipleFlag,         cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_113[4]{ cmdPar_expression,                             cmdPar_varOptAssignment,                         cmdPar_varOptAssignment,                        cmdPar_none };
    static inline const char cmdPar_114[4]{ cmdPar_expression,                             cmdPar_varOptAssignment | cmdPar_optionalFlag,   cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_115[4]{ cmdPar_expression,                             cmdPar_expression | cmdPar_optionalFlag,         cmdPar_none,                                    cmdPar_none };
    static inline const char cmdPar_116[4]{ cmdPar_expression,                             cmdPar_expression,                               cmdPar_expression | cmdPar_multipleFlag,        cmdPar_none };
    static inline const char cmdPar_117[4]{ cmdPar_expression,                             cmdPar_expression,                               cmdPar_expression | cmdPar_optionalFlag,        cmdPar_none };


    // commands (FUNCTION, FOR, ...): usage restrictions for specific commands (parsing)
    // ---------------------------------------------------------------------------------

    // bits b3210: indicate command (not parameter) usage restrictions 
    static constexpr char cmd_usageRestrictionMask = 0x0F;              // mask

    static constexpr char cmd_noRestrictions = 0x00;                    // command has no usage restrictions 
    static constexpr char cmd_onlyInProgram = 0x01;                     // command is only allowed inside a program
    static constexpr char cmd_onlyInProgOutsideFunc = 0x02;             // command is only allowed inside a program
    static constexpr char cmd_onlyInFunctionBlock = 0x03;               // command is only allowed inside a function block
    static constexpr char cmd_onlyImmediate = 0x04;                     // command is only allowed in immediate mode
    static constexpr char cmd_onlyOutsideFunctionBlock = 0x05;          // command is only allowed outside a function block (so also in immediate mode)
    static constexpr char cmd_onlyImmOrInsideFuncBlock = 0x06;          // command is only allowed inside a function block or in immediate mode
    static constexpr char cmd_onlyProgramTop = 0x07;                    // only as first program statement
    static constexpr char cmd_onlyImmModeTop = 0x08;                    // only as first user command statement
    static constexpr char cmd_onlyImmediateNotWithinBlock = 0x09;       // command is only allowed in immediate mode, and only outside blocks

    // bit b7: skip command during execution
    static constexpr char cmd_skipDuringExec = 0x80;                    // command is parsed but not executed


    // variable scope and value type bits 
    // ----------------------------------

    // bit b7: program variable name has a global program variable associated with it. Only used during parsing, not stored in token
    //         user variables: user variable is used by program. Not stored in token 
    static constexpr uint8_t var_nameHasGlobalValue = 0x80;             // flag: global program variable attached to this variable NAME (note that meaning is different from 'var_isGlobal' constant)
    static constexpr uint8_t var_userVarUsedByProgram = 0x80;           // flag: user variable is used by program

    // bits b654: variable scope. Use: (1) during parsing: temporarily store the variable type associated with a particular reference of a variable name 
    // (2) stored in 'variable' token to indicate the variable type associated with a particular reference of a variable name 
    static constexpr uint8_t var_scopeMask = 0x70;                      // mask
    static constexpr uint8_t var_isUser = 5 << 4;                       // variable is a user variable, in or outside function
    static constexpr uint8_t var_isGlobal = 4 << 4;                     // variable is global, in or outside function
    static constexpr uint8_t var_isStaticInFunc = 3 << 4;               // variable is static in function
    static constexpr uint8_t var_isLocalInFunc = 2 << 4;                // variable is local (non-parameter) in function
    static constexpr uint8_t var_isParamInFunc = 1 << 4;                // variable is function parameter
    static constexpr uint8_t var_scopeToSpecify = 0 << 4;               // scope is not yet defined (temporary use during parsing; never stored in token)

    // bit b3: variable is an array (and not a scalar)
    static constexpr uint8_t var_isArray = 0x08;                        // stored with variable attributes and in 'variable' token. Value can not be changed during execution

    // bitb2: variable is a constant
    static constexpr uint8_t var_isConstantVar = 0x04;                  // stored with variable attributes and in 'variable' token. Variable value can not be changed at runtime

    // bit 0 (maintain in token only): 'forced function variable in debug mode' (for pretty printing only)
    static constexpr uint8_t var_isForcedFunctionVar = 1;

    // bits b10: value type 
    // - PARSED constants: value type bits are maintained in the 'constant' token (but not in same bit positions)
    // - INTERMEDIATE constants (execution only) and variables: value type is maintained together with variable / intermediate constant data (per variable, array or constant) 
    // Note: because the value type is not fixed for scalar variables (type can dynamically change at runtime), this info is not maintained in the parsed 'variable' token 

    static constexpr uint8_t value_isVarRef = 0x00;
public:
    static constexpr uint8_t value_typeMask = 0x03;                     // mask for value type 
    static constexpr uint8_t value_isLong = 0x01;
    static constexpr uint8_t value_isFloat = 0x02;
    static constexpr uint8_t value_isString = 0x03;
private:
    static constexpr uint8_t value_isStringPointer = 0x03;              // same as value_isString (for use by calling Arduino program)


    // application flag bits
    //----------------------
public:
    // bits 3-0: flags signaling specific Justina status conditions to the program that called Justina (periodic system (main) callbacks)
    static constexpr long appFlag_errorConditionBit = 0x01L;            // Justina parsing or execution error has occurred

    static constexpr long appFlag_statusMask = 0x06L;                   // status bits mask
    static constexpr long appFlag_statusAbit = 0x02L;                   // status A bit 
    static constexpr long appFlag_statusBbit = 0x04L;                   // status B bit

    static constexpr long appFlag_idle = 0x00L;                         // idle status
    static constexpr long appFlag_parsing = 0x02L;                      // parsing status
    static constexpr long appFlag_executing = 0x04L;                    // executing status
    static constexpr long appFlag_stoppedInDebug = 0x06L;               // stopped in debug status

    static constexpr long appFlag_dataInOut = 0x08L;                    // an external I/O stream transmitted or received data (not SD) 

    // bits 7-4: spare

    // bits 11-8: 4 flags signaling specific caller status conditions to Justina
    static constexpr long appFlag_requestMask = 0x0f00;                 // mask for request bits
    static constexpr long appFlag_consoleRequestBit = 0x0100L;          // request to reset console to default
    static constexpr long appFlag_killRequestBit = 0x0200L;             // request to kill Justina
    static constexpr long appFlag_stopRequestBit = 0x0400L;             // request to stop a running Justina program
    static constexpr long appFlag_abortRequestBit = 0x0800L;            // request to abort running Justina code (either a Justina program or immediate mode Justina statements)

    // bits 31-12: spare

private:


    // Justina startup options
    // -----------------------
    static constexpr long SD_mask = 0x3;                                // bits 1-0
public:
    static constexpr long SD_notAllowed = 0x0;                          // card reader not present or card operations not allowed (maybe exclusively in use by Arduino program)
    static constexpr long SD_allowed = 0x1;                             // card reader is allowed but do not initialize card (maybe no SD card inserted, no SD card board present). This is the default
    static constexpr long SD_init = 0x2;                                // init SD card upon Justina begin() 
    static constexpr long SD_runStart = 0x3;                            // init SD card upon Justina begin(); load program "start.jus(); execute start() 
private:

    // system callbacks: time interval
    // -------------------------------
    static constexpr unsigned long CALLBACK_INTERVAL = 100;             // in ms; should be considerably less than any heartbeat period defined in main program


    // user callback procedures
    // ------------------------
    static constexpr char passCopyToCallback = 0x40;                    // flag: a pointer to a copy of a constant value was passed to a user callback (external cpp function) 


    // other
    // -----
    static constexpr char c_JustinaFunctionFirstOccurFlag = 0x10;       // flag: min > max means not initialized
    static constexpr char c_JustinaFunctionMaxArgs = 0xF;               // must fit in 4 bits


    // block statements: status flags (execution)
   // ------------------------------------------

    static constexpr uint8_t withinIteration = 0x01;                    // flag is set at the start of each iteration and cleared at the end
    static constexpr uint8_t forLoopInit = 0x02;                        // flag signals start of first iteration of a FOR loop
    static constexpr uint8_t breakFromLoop = 0x04;                      // flag: break statement encountered
    static constexpr uint8_t testFail = 0x08;                           // flag: loop test failed


    // constants only stored within the evaluation stack, for (parsed and intermediate) constant and variable tokens (execution)
    // -------------------------------------------------------------------------------------------------------------------------

    // bit b0: intermediate constant (not a parsed constant, not a constant stored in a variable) 
    static constexpr uint8_t constIsIntermediate = 0x01;
    // bit b1: the address is the address of an array element. If this bit is zero, the address is the scalar or array variable base address 
    static constexpr uint8_t var_isArray_pendingSubscripts = 0x02;

    // bits b4..2: print tab request, set column request. Bits set by tab() resp. col() functions if the function result is an argument of a print command (e.g. cout)
    static constexpr uint8_t isPrintTabRequest = 0x04;
    static constexpr uint8_t isPrintColumnRequest = 0x08;


    // SD card
    // -------

#if defined ARDUINO_ARCH_ESP32
    static constexpr char READ_FILE{ 0x01 };                            // aligned with Arduino (non-ESP32) SD library constants
    static constexpr char WRITE_FILE{ 0x02 };
    static constexpr char APPEND_FILE{ 0x04 };
    static constexpr char CREATE_FILE{ 0x10 };
    static constexpr char EXCL_FILE{ 0x20 };
    static constexpr char TRUNC_FILE{ 0x00 };                           // zero: will do nothing (no function with ESP2 SDlibrary)
#else
    static constexpr char READ_FILE{ O_READ };                          // Arduino SD library constants                
    static constexpr char WRITE_FILE{ O_WRITE };
    static constexpr char APPEND_FILE{ O_APPEND };
    static constexpr char CREATE_FILE{ O_CREAT };
    static constexpr char EXCL_FILE{ O_EXCL };
    static constexpr char TRUNC_FILE{ O_TRUNC };
#endif


    // ------------------------------
    // *   unions, structures (1)   *
    // ------------------------------

    // structures for token storage in Justina program memory
    // ------------------------------------------------------

    // note: tokens are stored in a packed form in Justina 'program memory' (ram), only occupying the number of bytes needed   
    // to avoid boundary alignment issues of structure members, character placeholders of correct size are used for all structure members

    union CstValue {                                                    // UNION between cpp constants (long, float, char*), each occupying 4 bytes
        char longConst[4];
        char floatConst[4];
        char pStringConst[4];
    };

    struct TokenIsResWord {                                             // keyword token (command): length 2 or 4 (if not a block command, token step is not stored and length will be 2)
        char tokenType;                                                 // will be set to specific token type
        char tokenIndex;                                                // index into list of tokens of a specific type
        char toTokenStep[2];                                            // tokens for block commands (IF, FOR, BREAK, END, ...): step number of 'block start' token or next block token (uint16_t)
    };

    struct TokenIsConstant {                                            // token storage for a constant token: length 5
        char tokenType;                                                 // will be set to specific token type
        CstValue cstValue;
    };

    // NOTE: tokenType and cstValue members in same order as in TokenIsConstant struct 
    struct TokenIsSymbolicConstant {                                    // token storage for a SYMBOLIC (PREDEFINED) constant token: length 5
        char tokenType;                                                 // will be set to specific token type
        CstValue cstValue;
        char nameIndex;                                                 // index into table with predefined symbolic constants
    };

    struct TokenIsInternCppFunction {                                   // token storage for internal cpp function: length 2
        char tokenType;                                                 // will be set to specific token type
        char tokenIndex;                                                // index into list of tokens
    };

    struct TokenIsExternCppFunction {                                   // token storage for external (user-provided) cpp function: length 3
        char tokenType;                                                 // will be set to specific token type
        char returnValueType;                                           // 0 = bool, 1 = char, 2 = int, 3 = long, 4 = float, 5 = char*, 6 = void (but returns zero to Justina)
        char funcIndexInType;                                           // index into list of external functions with a specific return type
    };

    struct TokenIsJustinaFunction {                                     // token storage for Justina function: length 2
        char tokenType;                                                 // will be set to specific token type
        char identNameIndex;                                            // index into Justina function name and additional data storage 
    };

    struct TokenIsVariable {                                            // token storage for variable: length 4
        char tokenType;                                                 // will be set to specific token type
        char identInfo;                                                 // global, parameter, local, static variable; array or scalar
        char identNameIndex;                                            // index into variable name storage
        char identValueIndex;                                           // for global variables: equal to name index, for static and local variables: pointing to different storage areas 
    };

    struct TokenIsTerminal {                                            // operators, separators, parenthesis: length 1 (token type and index combined)
        char tokenTypeAndIndex;                                         // will be set to specific token type (operator, left parenthesis, ...), AND bits 7 to 4 are set to token index
    };


    union TokenPointer {                                                // UNION of pointers to variables of all defined value types (long, float, char*)
        char* pTokenChars;
        TokenIsResWord* pResW;
        TokenIsConstant* pCstToken;
        TokenIsSymbolicConstant* pSymbCstToken;
        TokenIsInternCppFunction* pInternCppFunc;
        TokenIsExternCppFunction* pExternCppFunc;
        TokenIsJustinaFunction* pJustinaFunc;
        TokenIsVariable* pVar;
        TokenIsTerminal* pTermTok;
    };


    // structure for command, internal cpp function, terminal / operator, predefined symbolic constant definitions
    // -----------------------------------------------------------------------------------------------------------

    struct CmdBlockDef {                                                // block commands
        char blockType;                                                 // block type ('for' block, 'if' block,...)
        char blockPosOrAction;                                          // position of command (keyword) in block (0=start, 1, 2 = mid, 3=end)
        char blockMinPredecessor;                                       // minimum position of previous command (keyword) for open block
        char blockMaxPredecessor;                                       // maximum position
    };

    struct ResWordDef {                                                 // keywords with pattern for parameters (if keyword is used as command, starting an instruction)
        const char* const _resWordName;
        const char resWordCode;
        const char restrictions;                                        // specifies where he use of a keyword is allowed (in a program, in a function, ...)
        const char minArgs;                                             // minimum & maximum number of arguments AND padding (boundary alignment)                                     
        const char maxArgs;
        const char* pCmdAllowedParTypes;
        const CmdBlockDef cmdBlockDef;                                  // block commands: position in command block and min, max required position of previous block command 
    };

    struct InternCppFuncDef {                                           // internal cpp function names and codes with min & max number of arguments allowed 
        const char* funcName;
        char functionCode;
        char minArgs;                                                   // internal cpp functions: min & max number of allowed arguments
        char maxArgs;
        char arrayPattern;                                              // order of arrays and scalars; bit b0 to bit b7 refer to parameter 1 to 8, if a bit is set, an array is expected as argument
    };

    struct TerminalDef {                                                // function names with min & max number of arguments allowed 
        const char* terminalName;
        char terminalCode;
        char prefix_priority;                                           // 0: not a prefix operator
        char infix_priority;                                            // 0: not an infix operator
        char postfix_priority;                                          // 0: not a postfix operator
        char associativityAnduse;
    };

    struct SymbNumConsts {
        const char* symbolName;
        const char* symbolValue;
        char valueType;                                                 // float or long
    };


    // ---------------------
    // *   constants (2)   *
    // ---------------------

    // block commands only (FOR, END, etc.): type of block, position in block, sequence check in block: allowed previous block commands 
    static constexpr CmdBlockDef cmdBlockJustinaFunction{ block_JustinaFunction, block_startPos, block_na, block_na };          // 'IF' block mid position 2, min & max previous position is block start & block position 1, resp.
    static constexpr CmdBlockDef cmdBlockWhile{ block_while, block_startPos, block_na, block_na };                              // 'WHILE' block start
    static constexpr CmdBlockDef cmdBlockFor{ block_for, block_startPos, block_na, block_na };                                  // 'FOR' block start
    static constexpr CmdBlockDef cmdBlockIf{ block_if, block_startPos, block_na, block_na };                                    // 'IF' block start
    static constexpr CmdBlockDef cmdBlockIf_elseIf{ block_if, block_midPos1, block_startPos, block_midPos1 };                   // 'IF' block mid position 1, min & max previous position is block start & block position 1, resp.
    static constexpr CmdBlockDef cmdBlockIf_else{ block_if, block_midPos2, block_startPos, block_midPos1 };                     // 'IF' block mid position 2, min & max previous position is block start & block position 1, resp.

    // 'alter flow' block commands require an open block of a specific type, NOT necessary in the current inner open block 
    // the second value ('position in block') is specified to indicate which type of open block is required (e.g. a RETURN command can only occur within a FUNCTION...END block)
    static constexpr CmdBlockDef cmdBlockOpenBlock_loop{ block_alterFlow, block_inOpenLoopBlock, block_na, block_na };          // only if an open FOR or WHILE block 
    static constexpr CmdBlockDef cmdBlockOpenBlock_function{ block_alterFlow, block_inOpenFunctionBlock, block_na, block_na };  // only if an open FUNCTION definition block 

    // used to close any type of currently open inner block
    static constexpr CmdBlockDef cmdBlockGenEnd{ block_genericEnd, block_endPos, block_na, block_endPos };                      // all block types: block end 

    // other commands: first value indicates it's not a block command, other positions not used
    static constexpr CmdBlockDef cmdBlockNone{ block_none, block_na, block_na, block_na };                                      // not a 'block' command

    // sizes MUST be specified AND must be exact
    static const ResWordDef _resWords[77];                                                                                      // keyword names
    static const InternCppFuncDef _internCppFunctions[141];                                                                     // internal cpp function names and codes with min & max arguments allowed
    static const TerminalDef _terminals[40];                                                                                    // terminals (including operators)
#if (defined ARDUINO_ARCH_ESP32) 
    static const SymbNumConsts _symbNumConsts[79];                                                                              // predefined constants
#else
    static const SymbNumConsts _symbNumConsts[74];                                                                              // predefined constants
#endif
    static constexpr int _resWordCount{ sizeof(_resWords) / sizeof(_resWords[0]) };                                             // count of keywords in keyword table 
    static constexpr int _internCppFunctionCount{ (sizeof(_internCppFunctions)) / sizeof(_internCppFunctions[0]) };             // count of internal cpp functions in functions table
    static constexpr int _termTokenCount{ sizeof(_terminals) / sizeof(_terminals[0]) };                                         // count of operators and other terminals in terminals table
    static constexpr int _symbvalueCount{ sizeof(_symbNumConsts) / sizeof(_symbNumConsts[0]) };


    // ------------------------------
    // *   unions, structures (2)   *
    // ------------------------------

    // parsing stack: structures to keep track of open block levels and open parenthesis levels
    // ----------------------------------------------------------------------------------------

    struct OpenParenthesesLvl {                                         // must fit in 8 bytes (2 words). If stack level is open parenthesis:

        // functions only : if definition already parsed, min & max number of arguments required
        // if not, then current state of min & max argument count found in COMPLETELY PARSED calls to function

        char minArgs;                                                   // note: 1 for parenthesis without function
        char maxArgs;                                                   // note: 1 for parenthesis without function

        // arrays only
        char arrayDimCount;                                             // previously defined array: dimension count. Zero if new array or if scalar variable.

        // functions and arrays
        char identifierIndex;                                           // internal cpp functions and variables: index to name pointer
        char variableScope;                                             // variables: scope (user, global, static, local, parameter)
        char actualArgsOrDims;                                          // actual number of arguments found (function) or dimension count (prev. defined array) 
        char flags;                                                     // if stack level is open parenthesis (not open block): other flags 
        char flags2;
    };

    struct OpenCmdBlockLvl {
        CmdBlockDef cmdBlockDef;                                        // storage for info about block commands
        char toTokenStep[2];                                            // block commands: step number of next block command, or to block start command, in open block
        char fcnBlock_functionIndex;                                    // function definition block only: function index
    };


    union LE_parsingStack {
        OpenParenthesesLvl openPar;
        OpenCmdBlockLvl openBlock;
    };

    // structure to collect data about Justina functions during parsing
    // ----------------------------------------------------------------

    struct JustinaFunctionData {
        char* pJustinaFunctionStartToken;                               // Justina function: pointer to start of parsed function (token)

        char paramOnlyCountInFunction;
        char localVarCountInFunction;                                   // needed to reserve run time storage for local variables 
        char staticVarCountInFunction;                                  // needed when in debugging mode only
        char spare;                                                     // boundary alignment

        char localVarNameRefs_startIndex;                               // not in function, but overall, needed when in debugging mode only
        char staticVarStartIndex;                                       // needed when in debugging mode only
        char paramIsArrayPattern[2];                                    // parameter pattern: b15 flag set when parsing function definition or first function call; b14-b0 flags set when corresponding parameter or argument is array      
    };


    // execution
    // ---------

    union Val {                                                         // UNION between cpp constants (long, float, char*), pointers to these constants, and void pointers for specific uses                                                  
        void* pBaseValue;                                               // address of a Justina variable value (which can be a long, float, a string pointer or a variable address itself)

        long longConst;
        float floatConst;
        char* pStringConst;
        void* pArray;                                                   // pointer to memory block reserved for array of any Justina type

        long* pLongConst;
        float* pFloatConst;
        char** ppStringConst;
        void** ppArray;                                                 // 'pointer to pointer' to memory block reserved for array of any Justina type                                        

        char bytes[4];
    };


    //  evaluation stack data (execution)
    // ----------------------------------

    // during execution, data is pushed to / popped from an evaluation stack, using the structures below

    struct GenericTokenLvl {                                            // only to determine token type and for finding source error position during unparsing (for printing)
        char tokenType;
        char spare[3];                                                  // boundary alignment
        char* tokenAddress;                                             // must be second 4-byte word
    };

    struct GenericNameLvl {                                             // stack level for generic identifiers 
        char tokenType;
        char spare[3];
        char* pStringConst;
        char* tokenAddress;                                             // must be second 4-byte word, only for finding source error position during unparsing (for printing)
    };

    struct VarOrConstLvl {                                              // stack level for constants and variables
        char tokenType;
        char valueType;
        char sourceVarScopeAndFlags;                                    // flags indicating 'is array'; 'is array element', is constant variable; combined with SOURCE variable scope
        char valueAttributes;
        char* tokenAddress;                                             // must be second 4-byte word, only for finding source error position during unparsing (for printing)
        Val value;                                                      // float or pointer (4 byte)
        char* varTypeAddress;                                           // variables only: pointer to variable value type
    };

    struct FunctionLvl {                                                // stack level for functions
        char tokenType;
        char index;
        char returnValueType;                                           // external cpp function only; 0 = bool, 1 = char, 2 = int, 3 = long, 4 = float, 5 = char*, 6 = void (but returns zero to Justina)
        char funcIndexInType;                                           // external cpp function only
        char* tokenAddress;                                             // must be second 4-byte word, only for finding source error position during unparsing (for printing)
    };

    struct TerminalTokenLvl {                                           // stack level for terminals / operators
        char tokenType;
        char index;
        char spare[2];                                                  // boundary alignment
        char* tokenAddress;                                             // must be second 4-byte word, only for finding source error position during unparsing (for printing)
    };

    union LE_evalStack {                                                // UNION of structures used for evaluation stack list elements 
        GenericTokenLvl genericToken;
        GenericNameLvl genericName;
        VarOrConstLvl varOrConst;
        FunctionLvl function;
        TerminalTokenLvl terminal;
    };


    // flow control stack data (execution)
    // -----------------------------------

    // each function called, EXCEPT the currently ACTIVE function (deepest call stack level), and all other block commands (e.g. while...end, etc.), use a flow control stack level
    // flow control data for the currently active function - or the main program level if no function is currently active - is stored in structure '_activeFunctionData' (NOT on the flow control stack)
    // -> if executing a command in immediate mode, and not within a called function or open block, the control flow stack has no elements
    // -> if executing a 'start block' command (like 'while', ...), a structure of type 'OpenBlockTestData' containing flow control data for that open block is pushed to the flow control stack,
    //    and structure '_activeFunctionData' still contains flow control data for the currently active function.
    //    if a block is ended, the corresponding flow control data will be popped from the stack
    // -> if calling a function, flow control data for what is now becoming the caller (stored in structure _activeFunctionData) is pushed to the flow control stack,
    //    and flow control data for the CALLED function will now be stored in structure '_activeFunctionData'  
    //    if a function is ended, the corresponding flow control data will be COPIED to structure '_activeFunctionData' again before it is popped from the stack

    struct openBlockGeneric {
        char blockType : 6;                                             // command block: will identify stack level as an if...end, for...end, ... block
        char spareFlags : 2;
    };

    struct OpenBlockTestData {
        char blockType : 6;                                             // command block: will identify stack level as an if...end, for...end, ... block
        char spareFlags : 2;
        char loopControl;                                               // flags: within iteration, request break from loop, test failed
        char testValueType;                                             // 'for' loop tests: value type used for loop tests
        char spare;                                                     // boundary alignment

        // FOR...END loop only
        char* pControlValueType;
        Val pControlVar;
        Val step;
        Val finalValue;
        char* nextTokenAddress;                                         // address of token directly following 'FOR...; statement
    };

    struct OpenFunctionData {                                           // data about all open functions (active + call stack)
        char blockType : 6;                                             // command block: will identify stack level as a function block
        char trapEnable : 1;                                            // enable error trapping
        char spareFlag : 1;
        char functionIndex;                                             // user function index 
        char callerEvalStackLevels;                                     // evaluation stack levels in use by caller(s) and main (call stack)

        // within a function, as in immediate mode, only one command can be active at a time (ended by semicolon), in contrast to command blocks, which can be nested, so command data can be stored here:
        // data is stored when a keyword is processed and it is cleared when the ending semicolon (ending the command) is processed
        char activeCmd_ResWordCode;                                     // keyword code (set to 'cmdcod_none' again when semicolon is processed)
        char* activeCmd_tokenAddress;                                   // address in program memory of parsed keyword token                                

        // value area pointers (note: a value is a long, a float or a pointer to a string or array, or (if reference): pointer to 'source' (referenced) variable))
        Val* pLocalVarValues;                                           // points to local variable value storage area
        char** ppSourceVarTypes;                                        // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        char* pVariableAttributes;                                      // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) and 'is array' and 'is constant var' flags

        char* pNextStep;                                                // next step to execute (look ahead)
        char* errorStatementStartStep;                                  // first token in statement where execution error occurs (error reporting)
        char* errorProgramCounter;                                      // token to point to in statement (^) if execution error occurs (error reporting)
    };


    // structure to maintain data about open files (SD card)
    // -----------------------------------------------------

    struct OpenFile {
        File file;
        char* filePath{ nullptr };                                      // including file name
        bool fileNumberInUse{ false };                                  // file number = position in structure (base 0) + 1
        int currentPrintColumn{ 0 };
    };


    // external cpp (user callback) functions: a structure for each return type (bool, char, int, long, float, char*, void)
    // --------------------------------------------------------------------------------------------------------------------

    // the Arduino program calling Justina must create an array variable for each return type that will be used  
    // for instance, if 2 cpp functions which return long values are present, the Arduino program must create an array of type 'CppLongType' with 2 elements  

    struct CppDummyVoidFunction {
        const char* cppFunctionName;                                                                    // function name
        void* func;                                                                                     // function pointer
        char minArgCount;
        char maxArgCount;
    };

public:
    struct CppBoolFunction {
        const char* cppFunctionName;                                                                    // function name
        bool (*func)(void** const pdata, const char* const valueType, const int argCount, int& execError);    // function pointer
        char minArgCount;
        char maxArgCount;
    };

    struct CppCharFunction {
        const char* cppFunctionName;
        char (*func)(void** const pdata, const char* const valueType, const int argCount, int& execError);
        char minArgCount;
        char maxArgCount;
    };

    struct CppIntFunction {
        const char* cppFunctionName;
        int (*func)(void** const pdata, const char* const valueType, const int argCount, int& execError);
        char minArgCount;
        char maxArgCount;
    };

    struct CppLongFunction {
        const char* cppFunctionName;
        long (*func)(void** const pdata, const char* const valueType, const int argCount, int& execError);
        char minArgCount;
        char maxArgCount;
    };

    struct CppFloatFunction {
        const char* cppFunctionName;
        float (*func)(void** const pdata, const char* const valueType, const int argCount, int& execError);
        char minArgCount;
        char maxArgCount;
    };

    struct Cpp_pCharFunction {
        const char* cppFunctionName;
        char* (*func)(void** const pdata, const char* const valueType, const int argCount, int& execError);
        char minArgCount;
        char maxArgCount;
    };

    struct CppVoidFunction {
        const char* cppFunctionName;
        void (*func)(void** const pdata, const char* const valueType, const int argCount, int& execError);
        char minArgCount;                                                                               // not used for external cpp (user) commands
        char maxArgCount;
    };
private:


    // -----------------
    // *   variables   *
    // -----------------

    // basic settings
    // --------------

    bool _constructorInvoked{};
    bool _coldStart{};                                              // is this a cold start (first call to Justina begin() method after Justina object creation) ? (this is unrelated to memory clear on quitting)
    int _justinaStartupOptions{ 0 };                                // see constants SD_notAllowed, SD_allowed, SD_init, SD_runStart
    char _programName[MAX_IDENT_NAME_LEN + 1];
    char* _lastProgramStep{ nullptr };
    char* _lastUserCmdStep{ nullptr };                              // location in Justine imm. mode program memory where final 'tok_no_token' token is placed


    // parsing 
    // -------

    bool _programMode{ false };
    char _statement[MAX_STATEMENT_LEN + 1] = "";                    // character buffer for one statement read from console, SD file or any other external IO channel, ready to be parsed

    LinkedList parsingStack;                                        // during parsing: parsing stack keeps track of open parentheses and open blocks
    LE_parsingStack* _pParsingStack;                                // stack used during parsing to keep track of open blocks (e.g. for...end) , functions, open parentheses

    LE_parsingStack* _pFunctionDefStack;

    int _blockLevel = 0;                                            // current number of open block commands (during parsing) - block level + parenthesis level = parsing stack depth
    int _parenthesisLevel = 0;                                      // current number of open parentheses (during parsing)

    int _userVarCount{ 0 };                                         // counts number of user variables (names and values) 
    int _programVarNameCount{ 0 };                                  // counts number of variable NAMES (global variables: also stores values) 
    int _localVarCountInFunction{ 0 };                              // counts number of local variables in a specific function (names only, values not used)
    int _paramOnlyCountInFunction{ 0 };
    int _staticVarCountInFunction{ 0 };
    int _localVarCount{ 0 };                                        // local variable count (across all functions)
    int _staticVarCount{ 0 };                                       // static variable count (across all functions)
    int _justinaFunctionCount{ 0 };                                 // Justina function count


    // parsing commands
    // ----------------

    bool _justinaFunctionBlockOpen = false;                         // commands within FUNCTION...END block are being parsed (excluding END command)
    bool _isCommand = false;                                        // a command is being parsed (instruction starting with a keyword)

    bool _isProgramCmd = false;                                     // flags: a specific command is being parsed (= starting with a reserved word)
    bool _isJustinaFunctionCmd = false;
    bool _isGlobalOrUserVarCmd = false;
    bool _isLocalVarCmd = false;
    bool _isStaticVarCmd = false;
    bool _isAnyVarCmd = false;
    bool _isConstVarCmd = false;
    bool _isDeleteVarCmd = false;
    bool _isClearProgCmd = false;
    bool _isClearAllCmd = false;
    bool _isForCommand = false;

    bool _userVarUnderConstruction = false;                         // user variable is created, but process is not terminated
    bool _leadingSpaceCheck{ false };

    bool _lvl0_withinExpression;                                    // currently parsing an expression
    bool _lvl0_isPurePrefixIncrDecr;                                // the prefix increment/decrement operator just parsed is the first token of a (sub-) expression
    bool _lvl0_isPureVariable;                                      // the variable token just parsed is the first token of a (sub-) expression (or the second token but only if preceded by a prefix incr/decr token)
    bool _lvl0_isVarWithAssignment;                                 // operator just parsed is a (compound or pure) assignment operator, preceded by a 'pure' variable (see preceding line)

    int _initVarOrParWithUnaryOp;                                   // commands declaring variables or function parameters: initializer unary operators only: -1 = minus, 1 = plus, 0 = no unary op 

    // allowed parameter types for a command (variables with optional assignment, any expression, generic identifier only,...)  
    const char* _pCmdAllowedParTypes;                               // (the pointer itself is not constant)

    int _cmdParSpecColumn{ 0 };
    int _cmdArgNo{ 0 };                                             // argument number within a command


    // parsing expressions 
    // -------------------

    char _minFunctionArgs{ 0 };                                     // if Justina function defined prior to call: min & max allowed arguments...
    char _maxFunctionArgs{ 0 };                                     // ...otherwise, counters to keep track of min & max actual arguments in previous calls 
    int _functionIndex{ 0 };                                        // index of Justina function or internal cpp function in definition table
    int _variableNameIndex{ 0 };                                    // index of variable NAME in variable NAME table (note that multiple variables can share the same name, if their scope is different)
    int _variableScope{ 0 };                                        // variable scope: user, global, parameter, local, static
    bool _varIsConstant{ 0 };                                       // identifier refers to symbolic constant
    char _arrayDimCount{ 0 };
    int _tokenIndex{ 0 };                                           // token index within reserved words or terminals definition table 


    // remember what token type was last parsed, or even the token before that
    // -----------------------------------------------------------------------

    uint16_t _lastTokenStep, _lastVariableTokenStep;
    uint16_t _blockCmdTokenStep, _blockStartCmdTokenStep;           // remember step number (in JUSTINA program memory) of keyword starting a block command                           

    tokenType_type _lastTokenType{ tok_no_token };                  // type of last token parsed
    tokenType_type _lastTokenType_hold{ tok_no_token };
    tokenType_type _previousTokenType{ tok_no_token };

    termin_code _lastTermCode{};                                    // type of last token parsed
    termin_code _lastTermCode_hold{};
    termin_code _previousTermCode{};

    bool _lastTokenIsString;                                        // last token parsed was a string
    bool _lastTokenIsTerminal;                                      // last token parsed was a terminal
    bool _lastTokenIsTerminal_hold;

    bool _lastTokenIsPrefixOp, _lastTokenIsPostfixOp;
    bool _lastTokenIsPrefixIncrDecr;


    // flags related to current parsing stack level
    // --------------------------------------------

    bool _thisLvl_lastIsVariable;                                   // variable parsed (for arrays: right parenthesis is now parsed)
    bool _thislvl_lastIsConstVar;                                   // symbolic constant has been parsed (scalars allowed only)
    bool _thisLvl_lastOpIsIncrDecr;                                 // last operation parsed was an increment or decrement operator
    bool _thisLvl_assignmentStillPossible;                          // an assignment operator is still allowed at this stage of parsing

    uint8_t _lastTokenGroup_sequenceCheck_bit = 0;                  // bits indicate which token group the last token parsed belongs to          


    // execution
    // ---------

    char* _programCounter{ nullptr };                               // pointer to token memory address (NOT token step number)

    bool _initiateProgramLoad = false;                              // waiting for first program instruction streamed from SD or external IO stream
    int _loadProgFromStreamNo = 0;                                  // 0: receive from console

    int _lastValuesCount{ 0 };                                      // number of values in 'last values' (last results) buffer
    bool _lastValueIsStored = false;
    bool _lastPrintedIsPrompt{ false };                             // was the last thing printed a prompt ?

    LinkedList evalStack;                                           // evaluation stack keeps intermediate results of an expression being evaluated (execution phase)
    LE_evalStack* _pEvalStackTop{ nullptr }, * _pEvalStackMinus1{ nullptr }, * _pEvalStackMinus2{ nullptr };    // pointers to evaluation stack top elements

    // if statements are currently being executed, structure _activeFunctionData maintains data about either the main program level (command line statements being executed), ...
    // ... the active Justina function (if function being executed), or the internal cpp eval("'string'") function (if expression contained in 'string' being evaluated)
    // _activeFunctionData pushes its current data to / pops it from the flow control stack when a new Justina function or eval() function is called terminates
    OpenFunctionData _activeFunctionData;

    // the flow control stack maintains data about all currently open callers (Justina functions, eval() functions or program main level) AND any open blocks (loops, ...)  
    // => entries from stack TOP (newest entries) to BOTTOM: 
    // - first, entries for any open block in the active function (or main program level, if code is currently executed from there)
    // - then, for each caller in the call stack:
    //    - first, an entry for the caller (could be another Justina function, an eval() function or the main program level) 
    //    - then, entries for any open block in the caller
    //
    // - if a program is currently stopped (debug mode), data about the stopped function is also pushed to the flow control stack (it has 'called' the debug level, so to speak) ...
    //   ... and _activeFunctionData now contains data about a 'new' main program level (any command line statements executed for debugging purposes) 
    //
    // if execution of a NEW program is started while in debug mode, the whole process as described above is repeated. So, you can have more than one program being suspended
    LinkedList flowCtrlStack;
    void* _pFlowCtrlStackTop{ nullptr };                                        // pointers to flow control stack top elements
    int _callStackDepth{ 0 };                                                   // number of currently open Justina functions + open eval() functions + count of stopped programs (in debug mode): ...
    // ...this equals flow ctrl stack depth MINUS open loops (if, for, ...)

// while at least one program is stopped (debug mode), the PARSED code of the original command line from where execution started is pushed to a separate stack, and popped again ...
// ...when the program resumes, so that execution can continue there. If multiple programs are currently stopped (see: flow control stack), this stack will contain multiple entries
    LinkedList parsedCommandLineStack;
    char* _pParsedCommandLineStackTop{ nullptr };
    int _openDebugLevels{ 0 };                                      // number of stopped programs: equals parsed command line stack depth minus open eval() strings (= eval() strings being executed)

    char _semicolonBPallowed_token{}, _semicolonBPset_token{};      // will be initialized when Justina starts up


    // console settings and output and print commands
    // ----------------------------------------------

    int _angleMode{ 0 };                                                            // 0 = radians, 1 = degrees
    int _promptAndEcho{ 2 };                                                        // print prompt and print input echo
    int _printLastResult{ 1 };                                                      // print last result: 0 = do not print, 1 = print, 2 = print and expand backslash sequences in string constants  


    // display settings (last values, command line echo, tracing, print commands
    // -------------------------------------------------------------------------

    int _dispWidth = DEFAULT_DISP_WIDTH;

    int _dispFloatPrecision = DEFAULT_FLOAT_PRECISION;
    int _dispIntegerPrecision = DEFAULT_INT_PRECISION;

    char _dispFloatSpecifier[2]{ "" };                                              // will be initialized in Justina constructor 
    char _dispIntegerSpecifier[2]{ "" };
    char _dispStringSpecifier[2]{ "" };

    int _dispFloatFmtFlags = DEFAULT_FLOAT_FLAGS;
    int _dispIntegerFmtFlags = DEFAULT_INT_FLAGS;
    int _dispStringFmtFlags = DEFAULT_STR_FLAGS;                                    // string format flags are fixed, cannot be changed

    char _dispFloatFmtString[20] = "";                                              // long enough to contain all format specifier parts; initialized during reset
    char _dispIntegerFmtString[15] = "";
    char _dispStringFmtString[20] = "";


    // fmt() function settings 
    // -----------------------

    // shared for values to format of all data types, EXCEPT precision  and specifier character: maintained separately for strings

    int _fmt_width = DEFAULT_FMT_WIDTH;

    int _fmt_numPrecision = DEFAULT_FLOAT_PRECISION;                                // all numeric types
    int _fmt_strCharsToPrint = DEFAULT_STR_CHARS_TO_PRINT;                          // string type

    char _fmt_numSpecifier[2]{ "" };                                                // will be initialized in Justina constructor                                              
    char _fmt_stringSpecifier[2]{ "" };

    int _fmt_numFmtFlags = DEFAULT_FLOAT_FLAGS;
    int _fmt_stringFmtFlags = DEFAULT_STR_FLAGS;


    int _tabSize{ DEFAULT_TAB_SIZE };                                                              // tab size, default value if not changed by tabSize command 


    // debugging
    // ---------

    int _stepCallStackLevel{ 0 };                                   // call stack levels at the moment of a step, ... debugging command 
    int _stepFlowCtrlStackLevels{ 0 };                              // total flow control stack levels at the moment of a step, ... debugging command
    int _stepCmdExecuted{ db_continue };                            // type of debugging command executed (step, ...)
    bool _debugCmdExecuted{ false };                                // a debug command was executed
    bool _pendingStopForDebug{ false };                             // remember to stop anyway if trigger string result (not yet calculated) is zero

    Breakpoints* _pBreakpoints{ nullptr };

    // error trapping
    // --------------

    int _trappedErrorNumber{ (int)result_execOK };


    // evaluation strings (eval("...")), trace and trigger strings
    // -----------------------------------------------------------

    bool _parsingEvalString{ false };
    long _evalParseErrorCode{ 0L };

    bool _parsingExecutingTraceString{ false };
    bool _printTraceValueOnly{ false };                             // do not print trace expression, only print trace evaluation result (value)
    char* _pTraceString{ nullptr };
    Val _traceResultValue{};
    char _traceResultValueType{};

    bool _parsingExecutingTriggerString{ false };


    // counting of heap objects
    // notes: heap objects created / destroyed in destructors are not counted 
    //        linked list element count is maintained within the linked list objects
    // -----------------------------------------------------------------------------

    // name strings for variables and functions
    int _identifierNameStringObjectCount = 0, _identifierNameStringObjectErrors = 0;
    int _userVarNameStringObjectCount = 0, _userVarNameStringObjectErrors = 0;

    // constant strings
    int _parsedStringConstObjectCount = 0, _parsedStringConstObjectErrors = 0;          // char arrays referenced in program memory
    int _intermediateStringObjectCount = 0, _intermediateStringObjectErrors = 0;        // char arrays referenced in the evaluation stack
    int _lastValuesStringObjectCount = 0, _lastValuesStringObjectErrors = 0;            // char arrays referenced in the last results FIFO

    // strings as value of variables
    int _globalStaticVarStringObjectCount = 0, _globalStaticVarStringObjectErrors = 0;  // char arrays referenced in Justina global or static variables
    int _userVarStringObjectCount = 0, _userVarStringObjectErrors = 0;                  // char arrays referenced in Justina user variables
    int _localVarStringObjectCount = 0, _localVarStringObjectErrors = 0;                // char arrays referenced in Justina local variables
    int _systemStringObjectCount = 0, _systemStringObjectErrors = 0;                    // temporary or system char arrays, NOT referenced in any of the above 

    // array storage 
    int _globalStaticArrayObjectCount = 0, _globalStaticArrayObjectErrors = 0;          // Justina global array objects
    int _userArrayObjectCount = 0, _userArrayObjectErrors = 0;                          // Justina user array objects
    int _localArrayObjectCount = 0, _localArrayObjectErrors = 0;                        // Justina local array objects

    // local variable storage areas
    int _localVarValueAreaCount = 0, _localVarValueAreaErrors = 0;                      // storage areas for local variables and attributes                      


    // system (main) callback
    // ----------------------

    void (*_housekeepingCallback)(long& appFlags);                  // pointer to callback function for heartbeat
    long _appFlags = 0x00L;                                         // bidirectional flags to transfer info / requests between main program and Justina library
    unsigned long _lastCallBackTime{ 0 }, _currenttime{ 0 }, _previousTime{ 0 };


    // user callbacks (external cpp functions)
    // ---------------------------------------

    // for bool, char, int, long,float, char*, void (returns zero to Justina) function return types; for commands (void return type)
    void* _pExtCppFunctions[7]{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    int _ExtCppFunctionCounts[7]{ 0,0,0,0,0,0,0 };


    // external IO, SD card and files
    // ------------------------------

    Stream** _ppExternInputStreams{ nullptr };                      // available external IO streams (set by Justina caller)
    Print** _ppExternOutputStreams{ nullptr };

    // for use by cout..., dbout, ... commands (without explicit stream indicated)
    Stream* _pConsoleIn{ nullptr };
    Print* _pConsoleOut{ nullptr }, * _pDebugOut{ nullptr };
    int _consoleIn_sourceStreamNumber{}, _consoleOut_sourceStreamNumber{}, _debug_sourceStreamNumber{};     // != 0: always originating stream (external or SD)

    int* _pPrintColumns{};                                          // maintains current print column per output stream ( points to array on the heap)
    int* _pConsolePrintColumn{ nullptr }, * _pDebugPrintColumn{ nullptr };
    int* _pLastPrintColumn{ nullptr };

    Stream* _pStreamIn{ nullptr };
    Print* _pStreamOut{ nullptr };
    int _streamNumberIn{ 0 }, _streamNumberOut{ 0 };

    int _externIOstreamCount = 0;                                   // maximum is 4

#if !defined ARDUINO_ARCH_ESP32
    Sd2Card _SDcard{};
#endif

    OpenFile openFiles[MAX_OPEN_SD_FILES];                          // open files: file paths and attributed file numbers
    int _openFileCount = 0;
    int _SDcardChipSelectPin{ 10 };
    bool _SDinitOK = false;


    // Justina program and variable storage
    // ------------------------------------

    // program storage
    char _programStorage[_PROGRAM_MEMORY_SIZE + IMM_MEM_SIZE];

    // variable scope: global (program variables and user variables), local within function (including function parameters), static within function     

    // global program and user variables bearing the same name: in a program, the program variable has priority; in immediate mode a user variable has priority

    // variable names: 
    // two storage areas, one for all program variables (global, local and static) and one for user variables
    // program variable names are attributed to each program variable (minimum one) bearing that name (0>1 global, 0>n local, 0>n static variables)
    // user variable names are 1:1 linked with user variables

    // variable values: 
    // separate storage areas for user, global and static variable values 
    // storage space for local function variable values (including function parameters) is only reserved during execution of a procedure (based on info collected during parsing) 
    // variable values: stored in a 4-byte word. Either a float, a long, a pointer to a character string or a pointer to the start of an array 

    // variable types:
    // separate storage areas for user, global and static variable types
    // storage space for local function variable types (including function parameters) is created and destroyed at runtime before / after execution of a procedure (based on info collected during parsing) 
    // the variable type is maintained in one byte for each variable (see relevant constant definitions): 
    // - bit 7: program variables: a global program variable is attached to this program variable NAME; user variables: flag - variable is in use by program 
    // - bit b6..4: temporary use during parsing 
    // - bit b3: variable is an array
    // - bit b2: variable is a symbolic constant
    // - bits b1..0: value type (long, float, ... see constant definitions) 

    // user variable storage
    char* userVarNames[MAX_USERVARNAMES];                           // store distinct user variable names: ONLY for user variables (same name as program variable is OK)
    Val userVarValues[MAX_USERVARNAMES];
    char userVarType[MAX_USERVARNAMES];

    // variable name storage                                     
    char* programVarNames[MAX_PROGVARNAMES];                        // store distinct variable names: COMMON NAME for all program variables (global, static, local)
    char programVarValueIndex[MAX_PROGVARNAMES]{ 0 };               // temporarily maintains index to variable storage during function parsing
    Val globalVarValues[MAX_PROGVARNAMES];                          // if variable name is in use for global variable: store global value (float, pointer to string, pointer to array of floats)
    char globalVarType[MAX_PROGVARNAMES]{ 0 };                      // stores value type (float, pointer to string) and 'is array' flag

    // static variable value storage
    Val staticVarValues[MAX_STATIC_VARIABLES];                      // store static variable values (float, pointer to string, pointer to array of floats) 
    char staticVarType[MAX_STATIC_VARIABLES]{ 0 };                  // stores value type (float, pointer to string) and 'is array' flag
    char staticVarNameRef[MAX_STATIC_VARIABLES]{ 0 };               // used while in DEBUGGING mode only: index of static variable NAME

    // function key data storage
    char* JustinaFunctionNames[MAX_JUSTINA_FUNCTIONS];
    JustinaFunctionData justinaFunctionData[MAX_JUSTINA_FUNCTIONS];

    // local variable value storage
    char localVarNameRef[MAX_LOCAL_VARIABLES]{ 0 };                 // used while in DEBUGGING mode only: index of local variable NAME

    // temporary local variable storage during function parsing (without values)
    char localVarType[MAX_LOC_VARS_IN_FUNC]{ 0 };                   // parameter, local variables: temporarily maintains array flag during function parsing (storage reused by functions during parsing)
    char localVarDims[MAX_LOC_VARS_IN_FUNC][4]{ 0 };                // LOCAL variables: temporarily maintains dimensions during function parsing (storage reused by functions during parsing)

    // storage for last evaluation results
    Val lastResultValueFiFo[MAX_LAST_RESULT_DEPTH];
    char lastResultTypeFiFo[MAX_LAST_RESULT_DEPTH]{  };

    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

public:
    // constructor/ destructor
    // -----------------------

#if defined ARDUINO_ARCH_ESP32
    Justina(int SDcardMode = SD_allowed, int SDcardChipSelectPin = 10);
    Justina(Stream** const pAltInputStreams, Print** const pAltOutputStreams, int altIOstreamCount, int SDcardMode = SD_allowed, int SDcardChipSelectPin = 10);
#else
    Justina(int SDcardMode = SD_allowed, int SDcardChipSelectPin = SD_CHIP_SELECT_PIN);
    Justina(Stream** const pAltInputStreams, Print** const pAltOutputStreams, int altIOstreamCount, int SDcardMode = SD_allowed, int SDcardChipSelectPin = SD_CHIP_SELECT_PIN);
#endif
    ~Justina();

    void constructorCommonPart();

    // set pointer to system (main) call back function
    // -----------------------------------------------

    void setSystemCallbackFunction(void (*func)(long& appFlags));


    // sets pointers to the locations where the Arduino program stored information about user-defined (external) cpp functions (user callback functions)
    // -------------------------------------------------------------------------------------------------------------------------------------------------

    void registerBoolUserCppFunctions(const CppBoolFunction* const  pCppBoolFunctions, const int cppBoolFunctionCount);
    void registerCharUserCppFunctions(const CppCharFunction* const  pCppCharFunctions, const int cppCharFunctionCount);
    void registerIntUserCppFunctions(const CppIntFunction* const  pCppIntFunctions, const int cppIntFunctionCount);
    void registerLongUserCppFunctions(const CppLongFunction* const pCppLongFunctions, const int cppLongFunctionCount);
    void registerFloatUserCppFunctions(const CppFloatFunction* const pCppFloatFunctions, const int cppfloatFunctionCount);
    void register_pCharUserCppFunctions(const Cpp_pCharFunction* const pCpp_pCharFunctions, const int cpp_pCharFunctionCount);
    void registerVoidUserCppFunctions(const CppVoidFunction* const pCppVoidCommands, const int cppVoidCommandCount);


    // pass control to Justina interpreter
    // -----------------------------------

    void begin();                                                   // call from Arduino main program


    // Justina print functions
    // -----------------------

    int readFrom(int streamNumber);
    int readFrom(int streamNumber, char* buffer, int length);

    size_t writeTo(int streamNumber, char c);
    size_t writeTo(int streamNumber, char* s, int size);

    size_t printTo(int streamNumber, char c);
    size_t printTo(int streamNumber, unsigned char c);
    size_t printTo(int streamNumber, int i);
    size_t printTo(int streamNumber, unsigned int i);
    size_t printTo(int streamNumber, long l);
    size_t printTo(int streamNumber, unsigned long l);
    size_t printTo(int streamNumber, double d);
    size_t printTo(int streamNumber, char* s);
    size_t printTo(int streamNumber, const char* s);

    size_t printlnTo(int streamNumber, char c);
    size_t printlnTo(int streamNumber, unsigned char c);
    size_t printlnTo(int streamNumber, int i);
    size_t printlnTo(int streamNumber, unsigned int i);
    size_t printlnTo(int streamNumber, long l);
    size_t printlnTo(int streamNumber, unsigned long l);
    size_t printlnTo(int streamNumber, double d);
    size_t printlnTo(int streamNumber, char* s);
    size_t printlnTo(int streamNumber, const char* s);
    size_t printlnTo(int streamNumber);

    int read();
    int read(char* buffer, int length);

    size_t write(char c);
    size_t write(char* s, int size);

    size_t print(char c);
    size_t print(unsigned char c);
    size_t print(int i);
    size_t print(unsigned int i);
    size_t print(long l);
    size_t print(unsigned long l);
    size_t print(double d);
    size_t print(char* s);
    size_t print(const char* s);

    size_t println(char c);
    size_t println(unsigned char c);
    size_t println(int i);
    size_t println(unsigned int i);
    size_t println(long l);
    size_t println(unsigned long l);
    size_t println(double d);
    size_t println(char* s);
    size_t println(const char* s);
    size_t println();
private:

    // reset Interpreter to clean state
    // --------------------------------

    void resetMachine(bool withUserVariables);

    void initInterpreterVariables(bool withUserVariables);
    void deleteIdentifierNameObjects(char** pIdentArray, int identifiersInUse, bool isUserVar = false);
    void deleteStringArrayVarsStringObjects(Val* varValues, char* varType, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar = false, bool isLocalVar = false);
    void deleteVariableValueObjects(Val* varValues, char* varType, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar = false, bool isLocalVar = false);
    void deleteConstStringObjects(char* pToken);
    void deleteOneArrayVarStringObjects(Val* varValues, int index, bool isUserVar, bool isLocalVar);
    void deleteLastValueFiFoStringObjects();

    void danglingPointerCheckAndCount(bool withUserVariables);


    // parsing
    // -------

    // read one character from a stream (stream must be set prior to call)
    char getCharacter(bool& charFetched, bool& killNow, bool& forcedStop, bool& forcedAbort, bool& setStdConsole, bool enableTimeOut = false, bool useLongTimeout = false);

    bool flushInputCharacters(bool& stop, bool& forcedAbort);

    // add character (as read from stream) to the source statement input buffer; strip comment characters, redundant white space; handle escape sequences within source; ... 
    bool addCharacterToInput(bool& lastCharWasSemiColon, bool& withinString, bool& withinStringEscSequence, bool& within1LineComment, bool& withinMultiLineComment,
        bool& redundantSemiColon, bool isEndOfFile, bool& bufferOverrun, bool  _flushAllUntilEOF, int& _lineCount, int& _statementCharCount, char c);

    // parse one statement from source statement input buffer
    parsingResult_type parseStatement(char*& pInputLine, char*& pNextParseStatement, int& clearIndicator, bool isNewSourceLine = false, long sourceLine = 0);
    bool parseAsResWord(char*& pNext, parsingResult_type& result);
    bool parseAsNumber(char*& pNext, parsingResult_type& result);
    bool parseAsStringConstant(char*& pNext, parsingResult_type& result);
    bool parseTerminalToken(char*& pNext, parsingResult_type& result);
    bool parseAsInternCPPfunction(char*& pNext, parsingResult_type& result);
    bool parseAsExternCPPfunction(char*& pNext, parsingResult_type& result);
    bool parseAsJustinaFunction(char*& pNext, parsingResult_type& result);
    bool parseAsVariable(char*& pNext, parsingResult_type& result);
    bool parseAsIdentifierName(char*& pNext, parsingResult_type& result);

    // checking command statement syntax
    bool checkCommandKeyword(parsingResult_type& result, int& resWordIndex);
    bool checkCommandArgToken(parsingResult_type& result, int& clearIndicatore, int resWordIndex);

    // various checks while parsing
    bool checkArrayDimCountAndSize(parsingResult_type& result, int* arrayDef_dims, int& dimCnt);
    bool checkJustinaFunctionArguments(parsingResult_type& result, int& minArgCnt, int& maxArgCnt);
    bool checkInternCppFuncArgArrayPattern(parsingResult_type& result);
    bool checkExternCppFuncArgArrayPattern(parsingResult_type& result);
    bool checkJustinaFuncArgArrayPattern(parsingResult_type& result, bool isFunctionClosingParenthesis);
    bool checkAllJustinaFunctionsDefined(int& index) const;

    // basic parsing routines for constants, without other syntax checks etc. 
    bool parseIntFloat(char*& pNext, char*& pch, Val& value, char& valueType, int& predefinedConstIndex, parsingResult_type& result);
    bool parseString(char*& pNext, char*& pch, char*& string, char& valueType, int& predefinedConstIndex, parsingResult_type& result, bool isIntermediateString);

    // find an identifier (Justina variable or Justina function), init a Justina variable
    int getIdentifier(char** pIdentArray, int& identifiersInUse, int maxIdentifiers, char* pIdentNameToCheck, int identLength, bool& createNew, bool isUserVar = false);
    bool initVariable(uint16_t varTokenStep, uint16_t constTokenStep);

    // process parsed input and start execution
    bool finaliseParsing(parsingResult_type& result, bool& kill, int lineCount, char* pErrorPos, bool allCharsReceived);
    bool prepareForIdleMode(parsingResult_type result, execResult_type execResult, bool& kill, int& clearIndicator, Stream*& pStatementInputStream, int& statementInputStreamNumber);


    // execution
    // ---------

    execResult_type  exec(char* startHere);
    execResult_type  execParenthesesPair(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount, bool& forcedStopRequest, bool& forcedAbortRequest);
    execResult_type  execProcessedCommand(bool& isFunctionReturn, bool& forcedStopRequest, bool& forcedAbortRequest);
    execResult_type  execAllProcessedOperators();
    execResult_type  execUnaryOperation(bool isPrefix);
    execResult_type  execInfixOperation();
    void makeIntermediateConstant(LE_evalStack* pEvalStackLvl);
    execResult_type  execInternalCppFunction(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount, bool& forcedStopRequest, bool& forcedAbortRequest);
    execResult_type  execExternalCppFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount);
    execResult_type  launchJustinaFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount);
    execResult_type  launchEval(LE_evalStack*& pFunctionStackLvl, char* parsingInput);
    void  terminateJustinaFunction(bool addZeroReturnValue = false);
    void  terminateEval();

    // Justina functions: initialize parameter variables with provided arguments (pass by reference)
    void initFunctionParamVarWithSuppliedArg(int suppliedArgCount, LE_evalStack*& pFirstArgStackLvl);
    // Justina functions: initialize parameter variables with default values
    void initFunctionDefaultParamVariables(char*& calledFunctionTokenStep, int suppliedArgCount, int paramCount);
    // Justina functions: initialize other local variables 
    void initFunctionLocalNonParamVariables(char* calledFunctionTokenStep, int paramCount, int localVarCount);

    // when the 'end' keyword of a for..end loop is encountered, test the control variable value against the final loop value 
    execResult_type testForLoopCondition(bool& fail);

    // request the user to answer a Justina question
    bool getConsoleCharacters(bool& forcedStop, bool& forcedAbort, bool& doCancel, bool& doDefault, char* input, int& length, char terminator = 0xff);

    // save result of last expression evaluated into last value buffer
    void saveLastValue(bool& overWritePrevious);

    // push a token to the evaluation stack
    void pushTerminalToken(int tokenType);
    void pushInternCppFunctionName(int tokenType);
    void pushExternCppFunctionName(int tokenType);
    void pushJustinaFunctionName(int tokenType);
    void pushGenericName(int tokenType);
    void pushConstant(int tokenType);
    void pushVariable(int tokenType);

    // copy function arguments with attributes from the evaluation stack to value and attribute arrays, for use by internal and external (user callback) functions
    execResult_type copyValueArgsFromStack(LE_evalStack*& pStackLvl, int argCount, bool* argIsVar, bool* argIsArray, char* valueType, Val* args, bool passVarRefOrConst = false, Val* dummyArgs = nullptr);

    // fetch variable base address (where a value is stored, or a pointer to a char*, a source variable (function parameters) or (array variable) the start of array storage (on the heap)
    void* fetchVarBaseAddress(TokenIsVariable* pVarToken, char*& pVarType, char& valueType, char& sourceVarScopeAndFlags);

    // replace array variable base address and subscripts with the array element address on the evaluation stack
    Justina::execResult_type arrayAndSubscriptsToarrayElement(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);
    void* arrayElemAddress(void* varBaseAddress, int* dims);        // fetch the address of an array element

    // clear execution stacks
    void clearEvalStack();
    void clearEvalStackLevels(int n);
    void clearFlowCtrlStack(int& deleteImmModeCmdStackLevels, bool errorWhileCurrentlyStoppedPrograms = false, bool isAbortCommand = false);
    void clearParsedCommandLineStack(int n);

    execResult_type deleteVarStringObject(LE_evalStack* pStackLvl);
    execResult_type deleteIntermStringObject(LE_evalStack* pStackLvl);


    // streams, SD card and files
    // --------------------------

    execResult_type startSD();
    void SD_closeAllFiles();

#if defined ARDUINO_ARCH_ESP32
    char* SD_ESP32_convert_accessMode(int mode);
#endif
    execResult_type SD_open(int& fileNumber, char* filePath, int mod = READ_FILE);
    execResult_type SD_openNext(int dirFileNumber, int& fileNumber, File* pDirectory, int mod = READ_FILE);

    void SD_closeFile(int fileNumber);
    execResult_type SD_listFiles();
    execResult_type SD_fileChecks(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, File*& pFile, int allowFileTypes = 1);
    execResult_type SD_fileChecks(bool argIsLong, bool argIsFloat, Val arg, File*& pFile, int allowFileTypes = 1);
    execResult_type SD_fileChecks(File*& pFile, int fileNumber, int allowFileTypes = 1);
    execResult_type setStream(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, int& streamNumber, bool forOutput = false);
    execResult_type setStream(int streamNumber, bool forOutput = false);
    execResult_type setStream(int streamNumber, Stream*& pStream, bool forOutput = false);
    execResult_type determineStream(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, Stream*& pStream, int& streamNumber, bool forOutput = false, int allowFileTypes = 1);
    execResult_type determineStream(int streamNumber, Stream*& pStream, bool forOutput = false, int allowFileTypes = 1);

    bool pathValid(char* path);
    bool fileIsOpen(char* path);


    // Justina error handling, debugging, tracing
    // ------------------------------------------

    bool trapError(bool& isEndOfStatementSeparator, execResult_type& execResult);
    void checkTriggerResult(execResult_type& execResult);
    void checkForStop(bool& isActiveBreakpoint, bool& doStopForDebugNow, bool& appFlagsRequestStop, bool& isFunctionReturn, char* programCnt_previousStatementStart);
    void parseAndExecTraceOrBPviewString(int BPindex = -1);
    void traceAndPrintDebugInfo(execResult_type execResult);
    parsingResult_type parseTriggerString(int BPindex);

    // printing
    // --------    

    // output formatting, preparing for printing
    execResult_type checkFmtSpecifiers(bool isDispFmt, int argCount, char* valueType, Val* operands, char& specifier,
        int& precision, int& flags);
    void makeFormatString(int flags, bool longPrefix, char* specifier, char* fmtString);
    void printToString(int width, int precision, bool inputIsString, bool isIntFmt, char* valueType, Val* operands, char* fmtString,
        Val& fcnResult, int& charsPrinted, bool expandStrings = false);

    // unparse statement and pretty print, print parsing result (OK or error number), print variables, print call stack, SD card directory
    void prettyPrintStatements(int outputStream, int instructionCount, char* startToken = nullptr, char* errorProgCounter = nullptr, int* sourceErrorPos = nullptr);
    void printParsingResult(parsingResult_type result, int funcNotDefIndex, char* const pInputLine, int lineCount, char* pErrorPos);
    void printExecError(execResult_type execResult, bool showStopmessage);
    void printVariables(bool userVars);
    void printCallStack();
    void printDirectory(File dir, int numTabs);


    // system (main) callbacks
    // -----------------------

    void execPeriodicHousekeeping(bool* pKillNow, bool* pForcedStop = nullptr, bool* pForcedAbort = nullptr, bool* pSetStdConsole = nullptr);


    // utilities
    // ---------

    // add surrounding quotes AND expand backslash and double quote characters in string
    void quoteAndExpandEscSeq(char*& input);

    // find / jump to tokens in program memory
    int findTokenStep(char*& pStep, bool excludeCurrent, char tokenTypeToFind, char criterium1, char criterium2 = -1, char criterium3 = -1, int* matchedCritNum = nullptr, int* tokenIndex = nullptr);
    int jumpTokens(int n, char*& pStep, int& tokenCode);
    int jumpTokens(int n, char*& pStep);
    int jumpTokens(int n);

    // delete one user variable
    parsingResult_type deleteUserVariable(char* userVarName = nullptr);

    // replace a string stored in a system variable by another value
    void replaceSystemStringValue(char*& systemString, const char* pNewString);

};

// ******************************************************************
// ***                     class Breakpoints                      ***
// ******************************************************************

class Breakpoints {
    friend class Justina;

    Justina* _pJustina;

    struct BreakpointData {
        char BPenabled : 1;                             // breakpoint is enabled (program will halt)
        char BPwithViewExpr : 1;
        char BPwithHitCount : 1;
        char BPwithTriggerExpr : 1;

        long sourceLine{ 0 };                           // if breakpoint encountered, inform user on what source line
        char* pProgramStep{ nullptr };                  // compare with current program counter to find breakpoint entry 
        char* pView{ nullptr };                         // pointer to view expression (string)
        char* pTrigger{ nullptr };                      // pointer to trigger expression (string)
        long hitCount{ 0 };                             // pointer to number of hits triggering breakpoint
        long hitCounter{ 0 };                           // hit counter
    };

    bool _breakPontsAreOn{ true };
    int _breakpointsUsed{ 0 };
    long _BPLineRangeMemorySize{};
    long _maxBreakpointCount{};
    char* _BPlineRangeStorage{ nullptr };               // pointer to start of array keeping track of source line ranges for debugging with breakpoints
    long _BPlineRangeStorageUsed{ 0 };
    BreakpointData* _pBreakpointData{ nullptr };

    // methods
    Breakpoints(Justina* pJustina, long lineRanges_memorySize, long maxBreakpointCount);
    ~Breakpoints();

    void resetBreakpointsState();

    // after program parsing
    Justina::parsingResult_type collectSourceLineRangePairs(const char semiColonBPallowed_token, bool& parsedStatementStartsOnNewLine, bool& parsedStatementStartLinesAdjacent,
        long statementStartsAtLine, long& parsedStatementStartsAtLine, long& BPstartLine, long& BPendLine, long& BPpreviousEndLine);
    Justina::parsingResult_type addOneSourceLineRangePair(long gapLineRange, long adjacentLineRange);

    // maintaining breakpoints
    Justina::execResult_type maintainBP(long breakpointLine, char actionCmdCode, int extraAttribCount = 0, const char* viewString = nullptr, long hitCount = 0, const char* triggerString = nullptr);
    Justina::execResult_type findParsedStatementForSourceLine(long sourceLine, char*& pProgramStep);

    long BPsourceLineFromToBPlineSequence(long BPsourceLineOrIndex, bool toIndex = true);
    Justina::execResult_type progMem_getSetClearBP(long lineSequenceNum, char*& pProgramStep, bool& BPwasSet, bool doSet = false, bool doClear = false);
    Justina::execResult_type maintainBreakpointTable(long sourceLine, char* pProgramStep, bool BPwasSet, bool doSet, bool doClear, bool doEnable, bool doDisable,
        int extraAttribCount, const char* viewString, long hitCount, const char* triggerString);
    BreakpointData* findBPtableRow(char* pParsedStatement, int& row);
    long findLineNumberForBPstatement(char* pProgramStepToFind);
    void  printBreakpoints();
    void printLineRangesToDebugOut(Stream* output);
};

#endif
