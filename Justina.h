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


#ifndef _JUSTINA_h
#define _JUSTINA_h

#include "arduino.h"
#include <SPI.h>
#include <SD.h>

#include <stdlib.h>
#include <memory>

#define ProductName "Justina: JUST an INterpreter for Arduino"
#define LegalCopyright "Copyright (C) Herwig Taveirne, 2022"
#define ProductVersion "1.0.1"
#define BuildDate "December 8, 2022"


// ******************************************************************
// ***                      class LinkedList                      ***
// ******************************************************************

// store and retrieve data in linked lists from linked list

class LinkedList {

    // --------------------
    // *   enumerations   *
    // --------------------

    static const int listNameSize = 9;             // including terminating '\0'


    enum listType_type {                                       // identifier type
        list_isToken,
        list_isVariable,
        list_isExtFunction,
        list_isStack
    };


    // ------------------
    // *   structures   *
    // ------------------


    struct ListElemHead {                                        // list element structure (fixed length for all data types)
        ListElemHead* pNext;                                     // pointer to next list element
        ListElemHead* pPrev;                                     // pointer to previous list element (currently not used; needed if deleting other list elements than last) 
    };


    // -----------------
    // *   variables   *
    // -----------------



    static int _listIDcounter;                               // number of lists created

    ListElemHead* _pFirstElement = nullptr;                      // pointers to first and last list element
    ListElemHead* _pLastElement = nullptr;
    int _listElementCount{ 0 };                                // list element count (currently not used)
    listType_type _listType{};

    char _listName[listNameSize] = "";                                         // includes terminating '\0'
    int _listID{ 0 };                                       // list ID (in order of creation) 


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

public:
    LinkedList();                   // constructor
    ~LinkedList();                   // constructor

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
};


class MyParser;

// *****************************************************************
// ***                class Justina_interpreter                  ***
// *****************************************************************




class Justina_interpreter {

    ////static constexpr int _progMemorySize{ 2000 };             // size, in bytes, of program memory (stores parsed program)
    static constexpr int IMM_MEM_SIZE{ 300 };               // size, in bytes, of user command memory (stores parsed user statements)

    static constexpr int MAX_USERVARNAMES{ 255 };           // max. user variables allowed. Absolute parser limit: 255
    static constexpr int MAX_PROGVARNAMES{ 255 };           // max. program variable NAMES allowed (same name may be reused for global, static, local & parameter variables). Absolute limit: 255
    static constexpr int MAX_STAT_VARS{ 255 };              // max. static variables allowed across all parsed functions (only). Absolute limit: 255
    static constexpr int MAX_LOCAL_VARS{ 255 };             // max. local variables allowed across all parsed functons, including function parameters. Absolute limit: 255
    static constexpr int MAX_LOC_VARS_IN_FUNC{ 32 };        // max. local and parameter variables allowed (only) in an INDIVIDUAL parsed function. Absolute limit: 255 
    static constexpr int MAX_EXT_FUNCS{ 32 };               // max. user functions allowed. Absolute limit: 255
    static constexpr int MAX_ARRAY_DIMS{ 3 };               // max. array dimensions allowed. Absolute limit: 3 
    static constexpr int MAX_ARRAY_ELEM{ 200 };             // max. elements allowed in an array. Absolute limit: 2^15-1 = 32767. Individual dimensions are limited to a size of 255
    static constexpr int MAX_LAST_RESULT_DEPTH{ 10 };       // max. depth of 'last results' FiFo

    static constexpr int MAX_IDENT_NAME_LEN{ 20 };          // max length of identifier names, excluding terminating '\0'
    static constexpr int MAX_ALPHA_CONST_LEN{ 255 };        // max length of character strings, excluding terminating '\0' (also if stored in variables). Absolute limit: 255
    static constexpr int MAX_USER_INPUT_LEN{ 100 };         // max. length of text a user can enter with an input statement. Absolute limit: 255

    static constexpr int MAX_STATEMENT_LEN{ 300 };          // max. length of a single user statement 

    static constexpr int DEFAULT_PRINT_WIDTH{ 30 };          // default width of the print field.
    static constexpr int DEFAULT_NUM_PRECISION{ 3 };         // default numeric precision.
    static constexpr int DEFAULT_STRCHAR_TO_PRINT{ 30 };     // default # alphanumeric characters to print

    static constexpr long GETCHAR_TIMEOUT{ 200 };              // milli seconds

    static constexpr int MAX_OPEN_SD_FILES{ 5 };            // SD card: max. concurrent open files

    const int MAX_PRINT_WIDTH = 255;                        // max. width of the print field. Absolute limit: 255. With as defined as in c++ printf 'format.width' sub-specifier
    const int MAX_NUM_PRECISION = 8;                        // max. numeric precision. Precision as defined as in c++ printf 'format.precision' sub-specifier
    const int MAX_STRCHAR_TO_PRINT = 255;                   // max. # of alphanumeric characters to print. Absolute limit: 255. Defined as in c++ printf 'format.precision' sub-specifier


    // these values are grouped in a CmdBlockDef structure and are shared between multiple commands
    enum blockType_type {
        // value 1: block type
        block_none,                                             // command is not a block command
        block_extFunction,
        block_for,
        block_while,
        block_if,
        block_alterFlow,                                        // alter flow in specific open block types
        block_genericEnd,                                       // ends anytype of open block

        block_eval,                                             // execution only, signals execution of eval() string 

        // value 2, 3, 4: position in open block, min & max position of previous block command within same block level
        block_na,                                               // not applicable
        block_startPos,                                         // command starts an open block
        block_midPos1,                                          // command only allowed in open block  
        block_midPos2,                                          // command only allowed in open block
        block_endPos,                                           // command ends an open block
        block_inOpenFunctionBlock,                              // command can only occur if currently a function block is open
        block_inOpenLoopBlock,                                   // command can only occur if at least one loop block is open

        // alternative for value 2: type of command (only if block type = block_none)
        cmd_program,
        cmd_globalVar,
        cmd_localVar,
        cmd_staticVar,
        cmd_deleteVar
    };


    // unique identification code of a command
    enum cmd_code {
        cmdcod_none,        // no command being executed

        cmdcod_program,
        cmdcod_deleteVar,
        cmdcod_clear,
        cmdcod_printVars,
        cmdcod_clearAll,
        cmdcod_clearProg,
        cmdcod_printCallSt,
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
        cmdcod_skip,
        cmdcod_trace,
        cmdcod_debug,
        cmdcod_nop,
        cmdcod_quit,
        cmdcod_info,
        cmdcod_input,
        cmdcod_print,
        cmdcod_dispfmt,
        cmdcod_dispmod,
        cmdcod_declCB,
        cmdcod_clearCB,
        cmdcod_callback,
        cmdcod_receiveProg,
        cmdcod_listFiles,
        cmdcod_initSD,
        cmdcod_ejectSD,
        cmdcod_closeFile,
        cmdcod_test //// test
    };


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
        fnccod_asc,
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
        fnccod_repchar,
        fnccod_strstr,
        fnccod_strcmp,

        fnccod_cint,
        fnccod_cfloat,
        fnccod_cstr,

        fnccod_millis,
        fnccod_micros,
        fnccod_delay,
        fnccod_delayMicroseconds,
        fnccod_digitalRead,                 // Arduino functions
        fnccod_digitalWrite,
        fnccod_pinMode,
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
        fnccod_bitsMaskedClear,
        fnccod_bitsMaskedSet,
        fnccod_bitsMaskedRead,
        fnccod_bitsMaskedWrite,
        fnccod_byteRead,
        fnccod_byteWrite,
        fnccod_reg32Read,
        fnccod_reg8Read,
        fnccod_reg32Write,
        fnccod_reg8Write,

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
        fnccod_isSpace,
        fnccod_isUpperCase,
        fnccod_isWhitespace,

        fnccod_openFile,
    };

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
        termcod_leftPar,
        termcod_rightPar,
    };

    enum tokenType_type {                                       // token type
        tok_no_token,                                           // no token to process
        tok_isReservedWord,
        tok_isInternFunction,
        tok_isExternFunction,
        tok_isConstant,
        tok_isVariable,
        tok_isGenericName,

        // all terminal tokens: at the end of the list ! (occupy only one character in program, combining token type and index)
        tok_isTerminalGroup1,       // if index < 15 -    because too many operators to fit in 4 bits
        tok_isTerminalGroup2,       // if index between 16 and 31
        tok_isTerminalGroup3,       // if index between 32 and 47

        tok_isEvalEnd               // execution only, signals end of parsed eval() statements
    };

    enum parseTokenResult_type {                                // token parsing result
        result_tokenFound = 0,

        // incomplete expression errors
        result_statementTooLong = 1000,
        result_tokenNotFound,
        result_expressionNotComplete,
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

        // token expected errors
        result_constantValueExpected = 1200,
        result_variableNameExpected,
        result_functionDefExpected,
        result_assignmentOrTerminatorExpected,

        // used memory errors
        result_maxVariableNamesReached = 1300,
        result_maxLocalVariablesReached,
        result_maxStaticVariablesReached,
        result_maxExtFunctionsReached,

        // token errors
        result_identifierTooLong = 1400,
        result_spaceMissing,
        result_token_not_recognised,
        result_alphaConstTooLong,
        result_alphaConstInvalidEscSeq,
        result_alphaNoCtrlCharAllowed,
        result_alphaClosingQuoteMissing,
        result_numberInvalidFormat,
        result_parse_overflow,                // underflow not detected during parsing

        // function definition or call errors
        result_nameInUseForVariable = 1500,
        result_wrong_arg_count,
        result_functionAlreadyDefinedBefore,
        result_mandatoryArgFoundAfterOptionalArgs,
        result_functionDefMaxArgsExceeded,
        result_prevCallsWrongArgCount,
        result_functionDefsCannotBeNested,
        result_fcnScalarAndArrayArgOrderNotConsistent,
        result_scalarArgExpected,
        result_arrayArgExpected,
        result_redefiningIntFunctionNotAllowed,
        result_undefinedFunctionOrArray,
        result_arrayParamMustHaveEmptyDims,
        result_constantArrayNotAllowed,
        result_functionNeedsParentheses,

        // variable errors
        result_varNameInUseForFunction = 1600,
        result_varNotDeclared,
        result_varRedeclared,
        result_varDefinedAsArray,
        result_varDefinedAsScalar,
        result_varControlVarInUse,
        result_controlVarIsConstant,
        result_illegalInDeclaration,
        result_illegalInProgram,
        result_noOpenFunction,
        result_varUsedInProgram,

        // array errors
        result_arrayDefNoDims = 1700,
        result_arrayDefNegativeDim,
        result_arrayDefDimTooLarge,
        result_arrayDefMaxDimsExceeded,
        result_arrayDefMaxElementsExceeded,
        result_arrayUseNoDims,
        result_arrayUseWrongDimCount,
        result_arrayParamExpected,
        result_arrayInit_emptyStringExpected,
        result_arrayDimNotValid,
        result_noValidInitializer,

        // command errors 
        result_resWordExpectedAsCmdPar = 1800,
        result_expressionExpectedAsCmdPar,
        result_varWithoutAssignmentExpectedAsCmdPar,
        result_varWithOptionalAssignmentExpectedAsCmdPar,
        result_variableExpectedAsCmdPar,
        result_identExpectedAsCmdPar,
        result_cmdParameterMissing,
        result_cmdHasTooManyParameters,

        // generic identifier errors
        result_allUserCBAliasesSet = 1900,
        result_userCBAliasRedeclared,

        // block command errors
        result_programCmdMissing = 2000,
        result_onlyImmediateMode,
        result_onlyProgramStart,
        result_onlyInsideProgram,
        result_onlyInsideFunction,
        result_onlyOutsideFunction,
        result_onlyImmediateOrInFunction,
        result_onlyInProgOutsideFunction,
        result_onlyImmediateEndOfLine,////


        result_event_endParsing,


        result_noOpenBlock,
        result_noBlockEnd,
        result_noOpenLoop,
        result_notAllowedInThisOpenBlock,
        result_wrongBlockSequence,

        // tracing and eval() parsing errors
        result_trace_eval_resWordNotAllowed = 2100,
        result_trace_eval_genericNameNotAllowed,
        result_trace_userFunctonNotAllowed,             // tracing restriction only
        result_trace_evalFunctonNotAllowed,             // tracing restriction only

        // other program errors
        result_progMemoryFull = 2200,
        result_parse_kill
    };


    enum execResult_type {
        result_execOK = 0,

        // arrays
        result_array_subscriptOutsideBounds = 3000,
        result_array_subscriptNonInteger,
        result_array_subscriptNonNumeric,
        result_array_dimCountInvalid,
        result_array_valueTypeIsFixed,

        // internal functions
        result_arg_outsideRange = 3100,
        result_arg_integerTypeExpected,
        result_arg_numberExpected,
        result_arg_invalid,
        result_arg_integerDimExpected,
        result_arg_dimNumberInvalid,
        result_arg_stringExpected,
        result_arg_numValueExpected,
        result_arg_tooManyArgs,
        result_arg_nonEmptyStringExpected,
        result_arg_testexpr_numberExpected,

        result_array_dimNumberNonInteger = 3200,
        result_array_dimNumberInvalid,
        result_arg_varExpected,
        result_numericVariableExpected,
        result_aliasNotDeclared,

        // numbers and strings
        result_outsideRange = 3300,
        result_numberOutsideRange,
        result_numberNonInteger,
        result_numberExpected,
        result_integerExpected,
        result_stringExpected,
        result_operandsNumOrStringExpected,
        result_undefined,
        result_overflow,
        result_underflow,
        result_divByZero,
        result_testexpr_numberExpected,
        result_stringTooLong,

        // abort, kill, quit, debug
        result_noProgramStopped = 3400,        // 'go' command not allowed because not in debug mode
        result_notWithinBlock,
        result_skipNotAllowedHere,

        // evaluation function errors
        result_eval_nothingToEvaluate = 3500,
        result_eval_parsingError,

        // SD card
        result_SD_noCardOrCardError = 3600,
        result_SD_couldNotOpenFile,
        result_SD_fileIsNotOpen,
        result_SD_fileAlreadyOpen,
        result_SD_invalidFileNumber,
        result_SD_maxOpenFilesReached,


        // **************************************************
        // *** MANDATORY =>LAST<= range of errors: events ***
        // **************************************************
        result_startOfEvents = 9000,

        // abort, kill, quit, stop, skip debug: EVENTS (first handled as errors - which they are not - initially following the same flow)
        result_stopForDebug = result_startOfEvents,    // 'Stop' command executed (from inside a program only): this enters debug mode
        result_abort,                                  // abort running program (return to Justina prompt)
        result_kill,                                   // caller requested to exit Justina interpreter
        result_quit,                                   // 'Quit' command executed (exit Justina interpreter)

        result_initiateProgramLoad                      // command processed to start loading a program
    };

    enum dbType_type {
        db_continue = 0,
        db_singleStep,
        db_stepOut,
        db_stepOver,
        db_stepOutOfBlock,
        db_stepToBlockEnd,
        db_skip
    };


    static constexpr int _defaultPrintFlags = 0x00;

    static constexpr char c_extFunctionFirstOccurFlag = 0x10;     // flag: min > max means not initialized
    static constexpr char c_extFunctionMaxArgs = 0xF;             // must fit in 4 bits

    // these constants are used to check to which token group (or group of token groups) a parsed token belongs
    static constexpr uint8_t lastTokenGroup_0 = 1 << 0;          // operator
    static constexpr uint8_t lastTokenGroup_1 = 1 << 1;          // comma
    static constexpr uint8_t lastTokenGroup_2 = 1 << 2;          // (line start), semicolon, keyword, generic identifier
    static constexpr uint8_t lastTokenGroup_3 = 1 << 3;          // number, alphanumeric constant, right bracket
    static constexpr uint8_t lastTokenGroup_4 = 1 << 4;          // internal or external function name
    static constexpr uint8_t lastTokenGroup_5 = 1 << 5;          // left parenthesis
    static constexpr uint8_t lastTokenGroup_6 = 1 << 6;          // variable

    // groups of token groups: combined token groups (for testing valid token sequences when next token will be parsed)
    static constexpr uint8_t lastTokenGroups_5_2_1_0 = lastTokenGroup_5 | lastTokenGroup_2 | lastTokenGroup_1 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_3 = lastTokenGroup_6 | lastTokenGroup_3;
    static constexpr uint8_t lastTokenGroups_6_3_0 = lastTokenGroup_6 | lastTokenGroup_3 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_3_2_0 = lastTokenGroup_6 | lastTokenGroup_3 | lastTokenGroup_2 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_5_3_0 = lastTokenGroup_6 | lastTokenGroup_5 | lastTokenGroup_3 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_5_4_2_1_0 = lastTokenGroup_6 | lastTokenGroup_5 | lastTokenGroup_4 | lastTokenGroup_2 | lastTokenGroup_1 | lastTokenGroup_0;
    static constexpr uint8_t lastTokenGroups_6_5_3_2_1_0 = lastTokenGroup_6 | lastTokenGroup_5 | lastTokenGroup_3 | lastTokenGroup_2 | lastTokenGroup_1 | lastTokenGroup_0;


    // infix operator accociativity: bit b7 indiciates right-to-left associativity
    // note: prefix operators always have right-to-left associativity; postfix operators: always left_to_right 
    static constexpr uint8_t op_RtoL = 0x80;                 // infix operator: right-to-left associativity 
    static constexpr uint8_t op_long = 0x40;                 // operators: operand(s) must be long (no casting), result is long 
    static constexpr uint8_t res_long = 0x20;                // result is long (operators can be long or float)


    // terminals - should NOT start and end with an alphanumeric character or with an underscore
    // note: if a termnal is designated as 'single character', then other terminals should not contain this character
    static constexpr char* term_semicolon = ";";        // must be single character
    static constexpr char* term_comma = ",";            // must be single character
    static constexpr char* term_leftPar = "(";          // must be single character
    static constexpr char* term_rightPar = ")";         // must be single character


    // operators
    static constexpr char* term_assign = "=";
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

    static constexpr char* term_incr = "++";
    static constexpr char* term_decr = "--";

    static constexpr char* term_lt = "<";
    static constexpr char* term_gt = ">";
    static constexpr char* term_ltoe = "<=";
    static constexpr char* term_gtoe = ">=";
    static constexpr char* term_neq = "!=";
    static constexpr char* term_eq = "==";

    static constexpr char* term_plus = "+";
    static constexpr char* term_minus = "-";
    static constexpr char* term_mult = "*";
    static constexpr char* term_div = "/";
    static constexpr char* term_mod = "%";
    static constexpr char* term_pow = "**";

    static constexpr char* term_and = "&&";
    static constexpr char* term_or = "||";
    static constexpr char* term_not = "!";

    static constexpr char* term_bitShLeft = "<<";
    static constexpr char* term_bitShRight = ">>";
    static constexpr char* term_bitAnd = "&";
    static constexpr char* term_bitOr = "|";
    static constexpr char* term_bitXor = "^";
    static constexpr char* term_bitCompl = "~";





    // type & info about of parenthesis level
    static constexpr uint8_t extFunctionBit{ B00000001 };
    static constexpr uint8_t extFunctionPrevDefinedBit{ B00000010 };
    static constexpr uint8_t intFunctionBit{ B00000100 };
    static constexpr uint8_t openParenthesisBit{ B00001000 };                                  // not a function
    static constexpr uint8_t arrayBit{ B00010000 };
    static constexpr uint8_t varAssignmentAllowedBit{ B00100000 };
    static constexpr uint8_t varHasPrefixIncrDecrBit{ B01000000 };
    static constexpr uint8_t varIsConstantBit{ B10000000 };


    // commands (FUNCTION, FOR, ...): allowed command parameters (naming: cmdPar_<n[nnn]> with A'=variable with (optional) assignment, 'E'=expression, 'E'=expression, 'R'=keyword
    static const char cmdPar_100[4];
    static const char cmdPar_101[4];
    static const char cmdPar_102[4];
    static const char cmdPar_103[4];
    static const char cmdPar_104[4];
    static const char cmdPar_105[4];
    static const char cmdPar_106[4];
    static const char cmdPar_107[4];
    static const char cmdPar_108[4];
    static const char cmdPar_109[4];
    static const char cmdPar_110[4];
    static const char cmdPar_111[4];
    static const char cmdPar_112[4];
    static const char cmdPar_113[4];
    static const char cmdPar_114[4];
    static const char cmdPar_999[4];////test

    // commands parameters: types allowed
    static constexpr uint8_t cmdPar_none = 0;
    static constexpr uint8_t cmdPar_resWord = 1;            // !!! note: keywords as parameters: not implemented
    static constexpr uint8_t cmdPar_varNoAssignment = 2;    // and no operators
    static constexpr uint8_t cmdPar_varOptAssignment = 3;
    static constexpr uint8_t cmdPar_expression = 4;
    static constexpr uint8_t cmdPar_extFunction = 5;
    static constexpr uint8_t cmdPar_numConstOnly = 6;
    static constexpr uint8_t cmdPar_ident = 7;

    // flags may be combined with value of one of the allowed types above
    static constexpr uint8_t cmdPar_flagMask = 0x18;             // allowed 0 to n times. Only for last command parameter
    static constexpr uint8_t cmdPar_multipleFlag = 0x08;             // allowed 0 to n times. Only for last command parameter
    static constexpr uint8_t cmdPar_optionalFlag = 0x10;             // allowed 0 to 1 times. If parameter is present, next parameters do not have to be optional 

    // bits b3210: indicate command (not parameter) usage restrictions 
    static constexpr char cmd_usageRestrictionMask = 0x0F;               // mask

    static constexpr char cmd_noRestrictions = 0x00;                  // command has no usage restrictions 
    static constexpr char cmd_onlyInProgram = 0x01;                   // command is only allowed insde a program
    static constexpr char cmd_onlyInProgOutsideFunc = 0x02;    // command is only allowed insde a program
    static constexpr char cmd_onlyInFunctionBlock = 0x03;               // command is only allowed inside a function block
    static constexpr char cmd_onlyImmediate = 0x04;                   // command is only allowed in immediate mode
    static constexpr char cmd_onlyOutsideFunctionBlock = 0x05;             // command is only allowed outside a function block (so also in immediate mode)
    static constexpr char cmd_onlyImmOrInsideFuncBlock = 0x06;   // command is only allowed inside a function block are in immediare mode
    static constexpr char cmd_onlyProgramTop = 0x07;                        // only as first program statement
    static constexpr char cmd_onlyImmediateOutsideBlock = 0x08;                        // command is only allowed in immediate mode, and only outside blocks

    // bit b7: skip command during execution
    static constexpr char cmd_skipDuringExec = 0x80;




    // variable scope and value type bits: 

    // bit b7: program variable name has a global program variable associated with it. Only used during parsing, not stored in token
    //         user variables: user variable is used by program. Not stored in token 
    static constexpr uint8_t var_nameHasGlobalValue = 0x80;          // flag: global program variable attached to this variable NAME (note that meaning is different from 'var_isGlobal' constant)
    static constexpr uint8_t var_userVarUsedByProgram = 0x80;        // flag: user variable is used by program

    // bits b654: variable scope. Use: (1) during parsing: temporarily store the variable type associated with a particular reference of a variable name 
    // (2) stored in 'variable' token to indicate the variable type associated with a particular reference of a variable name 
    static constexpr uint8_t var_scopeMask = 0x70;               // mask
    static constexpr uint8_t var_isUser = 5 << 4;                    // variable is a user variable, in or outside function
    static constexpr uint8_t var_isGlobal = 4 << 4;                  // variable is global, in or outside function
    static constexpr uint8_t var_isStaticInFunc = 3 << 4;            // variable is static in function
    static constexpr uint8_t var_isLocalInFunc = 2 << 4;             // variable is local (non-parameter) in function
    static constexpr uint8_t var_isParamInFunc = 1 << 4;             // variable is function parameter
    static constexpr uint8_t var_scopeToSpecify = 0 << 4;             // scope is not yet defined (temporary use during parsing; never stored in token)

    // bit b3: variable is an array (and not a scalar)
    static constexpr uint8_t var_isArray = 0x08;                     // stored with variable attributes and in 'variable' token. Value can not be changed during execution

    // bitb2: variable is a constant
    static constexpr uint8_t var_isConstantVar = 0x04;                     // stored with variable attributes and in 'variable' token. Can not be changed at runtime

    // bit 0 (maintain in token only): 'forced function variable in debug mode' (for pretty printing only)
    static constexpr uint8_t var_isForcedFunctionVar = 1;

    // bits b210: value type 
    // - PARSED constants: value type bits are maintained in the 'constant' token (but not in same bit positions)
    // - INTERMEDIATE constants (execution only) and variables: value type is maintained together with variable / intermediate constant data (per variable, array or constant) 
    // Note: because the value type is not fixed for scalar variables (type can dynamically change at runtime), this info is not maintained in the parsed 'variable' token 

    static constexpr uint8_t value_typeMask = 0x03;                    // mask for value type 
    static constexpr uint8_t value_isVarRef = 0x00;
    static constexpr uint8_t value_isLong = 0x01;
    static constexpr uint8_t value_isFloat = 0x02;
    static constexpr uint8_t value_isStringPointer = 0x03;

    // application flag bits:flags signaling specific Justina status conditions
    static constexpr long appFlag_errorConditionBit = 0x01L;       // bit 0: a Justina parsing or execution error has occured
    static constexpr long appFlag_statusAbit = 0x10L;              // status bits A and B: bits 5 and 4. Justina status (see below)
    static constexpr long appFlag_statusBbit = 0x20L;
    static constexpr long appFlag_waitingForUser = 0x40L;

    // application flag bits b54: application status
    static constexpr long appFlag_statusMask = 0x30L;
    static constexpr long appFlag_idle = 0x00L;
    static constexpr long appFlag_parsing = 0x10L;
    static constexpr long appFlag_executing = 0x20L;
    static constexpr long appFlag_stoppedInDebug = 0x30L;



    // constants used during execution, only stored within the stack for value tokens

    // bit b0: intermediate constant (not a parsed constant, not a constant stored in a variable) 
    static constexpr uint8_t constIsIntermediate = 0x01;
    // bit b1: the address is the address of an array element. If this bit is zero, the address is the scalar or array variable base address 
    static constexpr uint8_t var_isArray_pendingSubscripts = 0x02;

    // block statements
    static constexpr uint8_t withinIteration = 0x01;        // flag is set at the start of each iteration and cleared at the end
    static constexpr uint8_t forLoopInit = 0x02;            // flag signals start of first iteration of a FOR loop
    static constexpr uint8_t breakFromLoop = 0x04;          // flag: break statement encountered
    static constexpr uint8_t testFail = 0x08;               // flag: loop test failed


    static constexpr int _userCBarrayDepth = 10;
    static constexpr char passCopyToCallback = 0x40;       // flag: string is an empty string 


    static constexpr unsigned long callbackPeriod = 10;      // in ms; should be considerably less than any heartbeat period defined in main program



    // --------------------------
    // *   unions, structures   *
    // --------------------------

        // storage for tokens
        // note: to avoid boundary alignment of structure members, character placeholders of correct size are used for all structure members

    union CstValue {
        char longConst[4];
        char floatConst[4];
        char pStringConst[4];                                 // pointer to string object
    };

    struct TokenIsResWord {                                     // keyword token (command): length 2 or 4 (if not a block command, token step is not stored and length will be 2)
        char tokenType;                                         // will be set to specific token type
        char tokenIndex;                                        // index into list of tokens of a specific type
        char toTokenStep[2];                                    // tokens for block commands (IF, FOR, BREAK, END, ...): step n� of block start token or next block token (uint16_t)
    };

    struct TokenIsConstant {                                    // token storage for a numeric constant token: length 5
        char tokenType;                                         // will be set to specific token type
        CstValue cstValue;
    };

    struct TokenIsIntFunction {                                 // token storage for internal function: length 2
        char tokenType;                                         // will be set to specific token type
        char tokenIndex;                                        // index into list of tokens
    };

    struct TokenIsExtFunction {                                 // token storage for external function: length 2
        char tokenType;                                         // will be set to specific token type
        char identNameIndex;                                    // index into external function name and additional data storage 
    };

    struct TokenIsVariable {                                    // token storage for variable: length 4
        char tokenType;                                         // will be set to specific token type
        char identInfo;                                         // global, parameter, local, static variable; array or scalar
        char identNameIndex;                                    // index into variable name storage
        char identValueIndex;                                   // for global variables: equal to name index, for static and local variables: pointing to different storage areas 
    };

    struct TokenIsTerminal {                                    // operators, separators, parenthesis: length 1 (token type and index combined)
        char tokenTypeAndIndex;                                 // will be set to specific token type (operator, left parenthesis, ...), AND bits 7 to 4 are set to token index
    };


    union TokenPointer {
        char* pTokenChars;
        TokenIsResWord* pResW;
        TokenIsConstant* pCstToken;
        TokenIsIntFunction* pIntFnc;
        TokenIsExtFunction* pExtFnc;
        TokenIsVariable* pVar;
        TokenIsTerminal* pTermTok;                             // terminal token
    };


    union Val {
        void* pBaseValue;                                        // address of a variable value (which can be a long, float, a string pointer or a variable address itself)
        // global, static, local variables; parameters with default initialisation (if no argument provided)
        long longConst;                                        // long
        float floatConst;                                        // float
        char* pStringConst;                                     // pointer to a character string
        void* pArray;                                          // pointer to memory block reserved for array

        long* pLongConst;
        float* pFloatConst;
        char** ppStringConst;
        void** ppArray;

        char bytes[4];
    };


    struct ExtFunctionData {
        char* pExtFunctionStartToken;                           // ext. function: pointer to start of function (token)

        char paramOnlyCountInFunction;
        char localVarCountInFunction;                           // needed to reserve run time storage for local variables 
        char staticVarCountInFunction;                          // needed when in debugging mode only
        char spare;                                             // boundary alignment

        char localVarNameRefs_startIndex;                       // not in function, but overall, needed when in debugging mode only
        char staticVarStartIndex;                               // needed when in debugging mode only
        char paramIsArrayPattern[2];                            // parameter pattern: b15 flag set when parsing function definition or first function call; b14-b0 flags set when corresponding parameter or argument is array      
    };


    // execution

    struct GenericTokenLvl {                                    // only to determine token type and for finding source error position during unparsing (for printing)
        tokenType_type tokenType;
        char spare[3];                                          // boundary alignment
        char* tokenAddress;                                     // must be second 4-byte word
    };

    struct GenNameLvl {
        char tokenType;
        char spare[3];
        char* pStringConst;
        char* tokenAddress;                                     // must be second 4-byte word, only for finding source error position during unparsing (for printing)
    };

    struct VarOrConstLvl {
        char tokenType;
        char valueType;
        char sourceVarScopeAndFlags;                                    // is array; is array element; SOURCE variable scope
        char valueAttributes;
        char* tokenAddress;                                     // must be second 4-byte word, only for finding source error position during unparsing (for printing)
        Val value;                                              // float or pointer (4 byte)
        char* varTypeAddress;                                        // variables only: pointer to variable value type
    };

    struct FunctionLvl {
        char tokenType;
        char index;
        char spare[2];
        char* tokenAddress;                                     // must be second 4-byte word, only for finding source error position during unparsing (for printing)
    };

    struct TerminalTokenLvl {
        char tokenType;
        char index;
        char spare[2];                                          // boundary alignment
        char* tokenAddress;                                     // must be second 4-byte word, only for finding source error position during unparsing (for printing)
    };

    union LE_evalStack {
        GenericTokenLvl genericToken;
        GenNameLvl genericName;
        VarOrConstLvl varOrConst;
        FunctionLvl function;
        TerminalTokenLvl terminal;
    };



    // flow control data
    // -----------------
    // 
    // each function called, EXCEPT the currently ACTIVE function (deepest call stack level), and all other block commands (e.g. while...end, etc.), use a flow control stack level
    // flow control data for the currently active function - or the main program level if no function is currently active - is stored in structure '_activeFunctionData' (NOT on the flow control stack)
    // -> if executing a command in immediate mode, and not within a called function or open block, the control flow stack has no elements
    // -> if executing a 'start block' command (like 'while', ...), a structure of type 'OpenBlockTestData' containing flow control data for that open block is pushed to the flow control stack,
    //    and structure '_activeFunctionData' still contains flow control data for the currently active function.
    //    if a block is ended, the corresponding flow control data will be popped from the stack
    // -> if calling a function, flow control data for what is now becoming the caller (stored in structure _activeFunctionData) is pushed to the flow control stack,
    //    and flow control data for the CALLED function will now be stored in structure '_activeFunctionData'  
    //    if a function is ended, the corresponding flow control data will be COPIED to structure '_activeFunctionData' again before it is popped from the stack


    struct OpenBlockTestData {
        char blockType;                 // command block: will identify stack level as an if...end, for...end, ... block
        char loopControl;               // flags: within iteration, request break from loop, test failed
        char testValueType;             // 'for' loop tests: value type used for loop tests
        char spare;                     // boundary alignment

        // FOR...END loop only
        char* pControlValueType;
        Val pControlVar;
        Val step;
        Val finalValue;
        char* nextTokenAddress;         // address of token directly following 'FOR...; statement
    };

    struct OpenFunctionData {           // data about all open functions (active + call stack)
        char blockType;                 // command block: will identify stack level as a function block
        char functionIndex;             // user function index 
        char callerEvalStackLevels;     // evaluation stack levels in use by caller(s) and main (call stack)
        // within a function, as in immediate mode, only one (block) command can be active at a time (ended by semicolon), in contrast to command blocks, which can be nested, so command data can be stored here:
        // data is stored when a keyword is processed and it is cleared when the ending semicolon (ending the command) is processed
        char activeCmd_ResWordCode;     // keyword code (set to 'cmdcod_none' again when semicolon is processed)

        char* activeCmd_tokenAddress;   // address in program memory of parsed keyword token                                

        // value area pointers (note: a value is a long, a float or a pointer to a string or array, or (if reference): pointer to 'source' (referenced) variable))
        Val* pLocalVarValues;           // points to local variable value storage area
        char** ppSourceVarTypes;        // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        char* pVariableAttributes;      // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) and 'is array' and 'is constant var' flags

        char* pNextStep;                // next step to execute (look ahead)
        char* errorStatementStartStep;  // first token in statement where execution error occurs (error reporting)
        char* errorProgramCounter;      // token to point to in statement (^) if execution error occurs (error reporting)
    };


    // Note: structures starting with 'LE_' are used to cast list elements for easy handling

    struct CmdBlockDef {                                        // block commands
        char blockType;                                         // block type ('for' block, 'if' block,...)
        char blockPosOrAction;                                     // position of command (keyword) in block (0=start, 1, 2 = mid, 3=end)
        char blockMinPredecessor;                               // minimum position of previous command (keyword) for open block
        char blockMaxPredecessor;                               // maximum position
    };

    struct ResWordDef {                                         // keywords with pattern for parameters (if keyword is used as command, starting an instruction)
        const char* _resWordName;
        const char resWordCode;
        const char restrictions;                                // specifies where he use of a keyword is allowed (in a program, in a function, ...)
        const char spare1, spare2;                                    // boundary alignment
        const char* pCmdAllowedParTypes;
        const CmdBlockDef cmdBlockDef;                          // block commands: position in command block and min, max required position of previous block command 
    };

    struct FuncDef {                                            // function names with min & max number of arguments allowed 
        const char* funcName;
        char functionCode;
        char minArgs;                                           // internal (intrinsic) functions: min & max n� of allowed arguments
        char maxArgs;
        char arrayPattern;                                      // order of arraysand scalars; bit b0 to bit b7 refer to parameter 1 to 8, if a bit is set, an array is expected as argument
    };

    struct TerminalDef {                                        // function names with min & max number of arguments allowed 
        const char* terminalName;
        char terminalCode;
        char prefix_priority;                                   // 0: not a prefix operator
        char infix_priority;                                    // 0: not an infix operator
        char postfix_priority;                                  // 0: not a postfix operator
        char associativityAnduse;
    };



    // stack for open blocks and open parenthesis (shared)

    struct OpenParenthesesLvl {                                         // must fit in 8 bytes (2 words). If stack level is open parenthesis:
        // functions only : if definition already parsed, min & max number of arguments required
        // if not, then current state of min & max argument count found in COMPLETELY PARSED calls to function
        char minArgs;                                           // note: 1 for parenthesis without function
        char maxArgs;                                           // note: 1 for parenthesis without function

        // arrays only
        char arrayDimCount;                                     // previously defined array: dimension count. Zero if new array or if scalar variable.

        // functions and arrays
        char identifierIndex;                                   // functions and variables: index to name pointer
        char variableScope;                                     // variables: scope (user, global, static, local, parameter)
        char actualArgsOrDims;                                  // actual number of arguments found (function) or dimension count (prev. defined array) 
        char flags;                                             // if stack level is open parenthesis (not open block): other flags 
    };

    struct OpenCmdBlockLvl {
        CmdBlockDef cmdBlockDef;                                // storage for info about block commands
        char tokenStep[2];                                     // block commands: step n� of next block command, or to block start command, in open block
        char fcnBlock_functionIndex;                            // function definition block only: function index
    };

    union LE_parsingStack {
        OpenParenthesesLvl openPar;
        OpenCmdBlockLvl openBlock;
    };

    struct OpenFile {
        File file;
        bool fileNumberInUse;                                   // file number = position in structure (base 0) + 1
    };

    // block commands only (FOR, END, etc.): type of block, position in block, sequence check in block: allowed previous block commands 
    static constexpr CmdBlockDef cmdBlockExtFunction{ block_extFunction,block_startPos,block_na,block_na };                // 'IF' block mid position 2, min & max previous position is block start & block position 1, resp.
    static constexpr CmdBlockDef cmdBlockWhile{ block_while,block_startPos,block_na,block_na };                            // 'WHILE' block start
    static constexpr CmdBlockDef cmdBlockFor{ block_for, block_startPos,block_na,block_na };                               // 'FOR' block start
    static constexpr CmdBlockDef cmdBlockIf{ block_if,block_startPos,block_na,block_na };                                  // 'IF' block start
    static constexpr CmdBlockDef cmdBlockIf_elseIf{ block_if,block_midPos1,block_startPos,block_midPos1 };                 // 'IF' block mid position 1, min & max previous position is block start & block position 1, resp.
    static constexpr CmdBlockDef cmdBlockIf_else{ block_if,block_midPos2,block_startPos,block_midPos1 };                   // 'IF' block mid position 2, min & max previous position is block start & block position 1, resp.

    // 'alter flow' block commands require an open block of a specific type, NOT necessary in the current inner open block 
    // the second value ('position in block') is specified to indicate which type of open block is required (e.g. a RETURN command can only occur within a FUNCTION...END block)
    static constexpr CmdBlockDef cmdBlockOpenBlock_loop{ block_alterFlow,block_inOpenLoopBlock,block_na,block_na };        // only if an open FOR or WHILE block 
    static constexpr CmdBlockDef cmdBlockOpenBlock_function{ block_alterFlow,block_inOpenFunctionBlock,block_na,block_na };// only if an open FUNCTION definition block 

    // used to close any type of currently open inner block
    static constexpr CmdBlockDef cmdBlockGenEnd{ block_genericEnd,block_endPos,block_na,block_endPos };            // all block types: block end 

    // other commands: first value indicates it's not a block command, other positions not used
    static constexpr CmdBlockDef cmdBlockNone{ block_none, block_na,block_na,block_na };                                   // not a 'block' command

    // sizes MUST be specified AND must be exact
    static const ResWordDef _resWords[47];                          // keyword names
    static const FuncDef _functions[101];                            // function names with min & max arguments allowed
    static const TerminalDef _terminals[38];                        // terminals (ncluding operators)


    // ---------
    // variables
    // ---------

    OpenFile openFiles[MAX_OPEN_SD_FILES];                      // open files: file paths and attributed file numbers
    
    int _openFileCount = 0;
    int _activeFileNum = 0;                                   // console is active for I/O
    bool _SDinitOK = false;

    int _resWordCount;                                          // index into list of keywords
    int _functionCount;                                         // index into list of internal (intrinsic) functions
    int _terminalCount;

    bool _isProgramCmd = false;
    bool _isExtFunctionCmd = false;                             // FUNCTION command is being parsed (not the complete function)
    bool _isGlobalOrUserVarCmd = false;                                // VAR command is being parsed
    bool _isLocalVarCmd = false;                                // LOCAL command is being parsed
    bool _isStaticVarCmd = false;                               // STATIC command is being parsed
    bool _isAnyVarCmd = false;                                     // VAR, LOCAL or STATIC command is being parsed
    bool _isConstVarCmd = false;
    bool _isDeleteVarCmd = false;
    bool _isClearProgCmd = false;
    bool _isClearAllCmd = false;
    bool _isForCommand = false;

    bool _initiateProgramLoad = false;
    bool _userVarUnderConstruction = false;                       // user variable is created, but process is not terminated

    bool _isDeclCBcmd = false;
    bool _isClearCBcmd = false;
    bool _isCallbackCmd = false;

    bool _leadingSpaceCheck{ false };

    // parsing stack: value supplied when pushing data to stack OR value returned when stack drops 
    char _minFunctionArgs{ 0 };                                // if external function defined prior to call: min & max allowed arguments. Otherwise, counters to keep track of min & max actual arguments in previous calls 
    char _maxFunctionArgs{ 0 };
    int _functionIndex{ 0 };
    int _variableNameIndex{ 0 };
    int _variableScope{ 0 };
    bool _varIsConstant{ 0 };

    int _tokenIndex{ 0 };


    uint16_t _lastTokenStep, _lastVariableTokenStep;
    uint16_t _blockCmdTokenStep, _blockStartCmdTokenStep;   // pointers to keywords used as block commands                           
    LE_parsingStack* _pParsingStack;
    LE_parsingStack* _pFunctionDefStack;

    Justina_interpreter::tokenType_type _lastTokenType = Justina_interpreter::tok_no_token;               // type of last token parsed
    Justina_interpreter::tokenType_type _lastTokenType_hold = Justina_interpreter::tok_no_token;
    Justina_interpreter::tokenType_type _previousTokenType = Justina_interpreter::tok_no_token;

    termin_code _lastTermCode;               // type of last token parsed
    termin_code _lastTermCode_hold;
    termin_code _previousTermCode;


    bool _lastTokenIsString;
    bool _lastTokenIsTerminal;
    bool _lastTokenIsTerminal_hold;
    bool _previousTokenIsTerminal;

    bool _lastTokenIsPrefixOp, _lastTokenIsPostfixOp;
    bool _lastTokenIsPrefixIncrDecr;

    // used for expression syntax checking (parsing)
    bool _thisLvl_lastIsVariable;                               // variable name, array name with 
    bool _thislvl_lastIsConstVar;
    bool _thisLvl_assignmentStillPossible;
    bool _thisLvl_lastOpIsIncrDecr;

    // used to check command argument constraints
    bool _lvl0_withinExpression;                    // currently parsing an expression
    bool _lvl0_isPurePrefixIncrDecr;                // the prefix increment/decrement operator just parsed is the first token of a (sub-) expression
    bool _lvl0_isPureVariable;                      // the variable token just parsed is the first token of a (sub-) expression (or the second token but only if preceded by a prefix incr/decr token)
    bool _lvl0_isVarWithAssignment;                 // operator just parsed is a (compound or pure) assignment operator, preceded by a 'pure' variable (see preceding line)

    int _initVarOrParWithUnaryOp;                    // initialiser unary operators only: -1 = minus, 1 = plus, 0 = no unary op 

    Justina_interpreter* _pInterpreter;

    const char* _pCmdAllowedParTypes;
    int _cmdParSpecColumn{ 0 };
    int _cmdArgNo{ 0 };
    bool _isCommand = false;                                    // a command is being parsed (instruction starting with a keyword)
    int _parenthesisLevel = 0;                               // current number of open parentheses
    uint8_t _lastTokenGroup_sequenceCheck_bit = 0;                   // bits indicate which token group the last token parsed belongs to          
    bool _extFunctionBlockOpen = false;                         // commands within FUNCTION...END block are being parsed (excluding END command)
    int _blockLevel = 0;                                     // current number of open blocks

    LinkedList parsingStack;                                      // during parsing: linked list keeping track of open parentheses and open blocks

    bool _coldStart{};
    char* _pTraceString{ nullptr };
    char* _pEvalString{ nullptr };
    bool _parsingExecutingTraceString{ false };
    bool _parsingEvalString{ false };
    long _evalParseErrorCode{ 0L };

    // counting of heap objects (note: linked list element count is maintained within the linked list objects)

    // name strings for variables and functions
    int _identifierNameStringObjectCount = 0, _identifierNameStringObjectErrors = 0;
    int _userVarNameStringObjectCount = 0, _userVarNameStringObjectErrors = 0;

    // constant strings
    int _parsedStringConstObjectCount = 0, _parsedStringConstObjectErrors = 0;
    int _intermediateStringObjectCount = 0, _intermediateStringObjectErrors = 0;
    int _lastValuesStringObjectCount = 0, _lastValuesStringObjectErrors = 0;

    // strings as value of variables
    int _globalStaticVarStringObjectCount = 0, _globalStaticVarStringObjectErrors = 0;
    int _userVarStringObjectCount = 0, _userVarStringObjectErrors = 0;
    int _localVarStringObjectCount = 0, _localVarStringObjectErrors = 0;
    int _systemVarStringObjectCount = 0, _systemVarStringObjectErrors = 0;

    // array storage 
    int _globalStaticArrayObjectCount = 0, _globalStaticArrayObjectErrors = 0;
    int _userArrayObjectCount = 0, _userArrayObjectErrors = 0;
    int _localArrayObjectCount = 0, _localArrayObjectErrors = 0;

    // local variable storage area
    int _localVarValueAreaCount = 0, _localVarValueAreaErrors = 0;


    bool _atLineStart = true;
    bool _lastValueIsStored = false;

    // calculation result print
    int _dispWidth = DEFAULT_PRINT_WIDTH, _dispNumPrecision = DEFAULT_NUM_PRECISION, _dispCharsToPrint = DEFAULT_STRCHAR_TO_PRINT, _dispFmtFlags = _defaultPrintFlags;
    char _dispNumSpecifier[2] = "G";      // room for 1 character and an extra terminating \0 
    bool _dispIsIntFmt{ false };              // initialized during reset          
    char  _dispNumberFmtString[20] = "", _dispStringFmtString[20] = "%*.*s%n";        // long enough to contain all format specifier parts; initialized during reset

    // for print command
    int _printWidth = DEFAULT_PRINT_WIDTH, _printNumPrecision = DEFAULT_NUM_PRECISION, _printCharsToPrint = DEFAULT_STRCHAR_TO_PRINT, _printFmtFlags = _defaultPrintFlags;
    char _printNumSpecifier[2] = "G";      // room for 1 character and an extra terminating \0 (initialized during reset)

    // display output settings
    int _promptAndEcho{ 2 };                // print prompt and print input echo
    int _printLastResult{ 1 };              // print last result: 0 = do not print, 1 = print, 2 = print and expand backslash sequences in string constants  

    char _statement[MAX_STATEMENT_LEN + 1] = "";
    bool _programMode{ false };
    bool _quitJustina{ false };
    bool _keepInMemory{ true };
    bool _isPrompt{ false };

    int _userVarCount{ 0 };                                        // counts number of user variables (names and values) 
    int _programVarNameCount{ 0 };                                 // counts number of variable names (global variables: also stores values) 
    int _localVarCountInFunction{ 0 };                             // counts number of local variables in a specific function (names only, values not used)
    int _paramOnlyCountInFunction{ 0 };
    int _localVarCount{ 0 };                                      // local variable count (across all functions)
    int _staticVarCountInFunction{ 0 };
    int _staticVarCount{ 0 };                                      // static variable count (across all functions)
    int _extFunctionCount{ 0 };                                    // external function count
    int _lastValuesCount{ 0 };
    int _userCBprocStartSet_count = 0;
    int _userCBprocAliasSet_count = 0;

    long _appFlags = 0x00L;                                         // bidirectional flags to transfer info / requests between caller and Justina library

    // number of currently [called external functions + open eval() levels + stopped programs]: equals flow ctrl stack levels minus open loop (if, for, ...) blocks (= blocks being executed)
    int _callStackDepth{ 0 };
    // number of stopped programs: equals imm mode cmd stack depth minus open eval() strings (= eval() strings being executed)
    int _openDebugLevels{ 0 };


    int _stepCallStackLevel{ 0 };                                   // call stack levels at the moment of a step... command
    int _stepFlowCtrlStackLevels{ 0 };                              // ALL flow control stack levels at the moment of a step... command

    int _stepCmdExecuted{ db_continue };
    bool _debugCmdExecuted{ false };

    char _arrayDimCount{ 0 };
    char* _programCounter{ nullptr };                                // pointer to token memory address (not token step n�)

    char* _lastProgramStep, * _lastUserCmdStep;                     // location in Justine program memory where final 'tok_no_token' token is placed

    uint16_t _paramIsArrayPattern{ 0 };

    char _programName[MAX_IDENT_NAME_LEN + 1];

    Stream* _pConsole{ nullptr };
    long _progMemorySize{};////
    Stream** _pTerminal{ nullptr };
    int _definedTerminals{ 0 };

    // program storage
    ////char _programStorage[_progMemorySize + IMM_MEM_SIZE];
    char* _programStorage;

    Sd2Card _SDcard;


    // Justina variable storage
    // ------------------------

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
    // storage space for local function variable types (including function parameters) is only reserved during execution of a procedure (based on info collected during parsing) 
    // the variable type is maintained in one byte for each variable (see relevant constant definitions): 
    // - bit 7: program variables: global program variable attached to this program variable NAME; user variables: flag - variable in use by program 
    // - bit b654: variable type ('scope'): user, program global, local, static, function parameter 
    // - bit b3: variable is an array
    // - bits b210: value type (long, float, ... see constant definitions) 

    // the 'identInfo' field in variable tokens only stores variable scope (bits 654) and bit b3 (variable is array) 


    // local variable value storage: pointers to data areas etc. for a local function's instance
    OpenFunctionData _activeFunctionData;

    // user variable storage
    char* userVarNames[MAX_USERVARNAMES];                               // store distinct user variable names: ONLY for user variables (same name as program variable is OK)
    Val userVarValues[MAX_USERVARNAMES];
    char userVarType[MAX_USERVARNAMES];

    // variable name storage                                         
    char* programVarNames[MAX_PROGVARNAMES];                            // store distinct variable names: COMMON NAME for all program variables (global, static, local)
    char programVarValueIndex[MAX_PROGVARNAMES]{ 0 };                   // temporarily maintains index to variable storage during function parsing
    Val globalVarValues[MAX_PROGVARNAMES];                              // if variable name is in use for global variable: store global value (float, pointer to string, pointer to array of floats)
    char globalVarType[MAX_PROGVARNAMES]{ 0 };                          // stores value type (float, pointer to string) and 'is array' flag

    // static variable value storage
    Val staticVarValues[MAX_STAT_VARS];                                 // store static variable values (float, pointer to string, pointer to array of floats) 
    char staticVarType[MAX_STAT_VARS]{ 0 };                             // stores value type (float, pointer to string) and 'is array' flag
    char staticVarNameRef[MAX_STAT_VARS]{ 0 };                          // used while in DEBUGGING mode only: index of static variable NAME

    // local variable value storage
    char localVarNameRef[MAX_LOCAL_VARS]{ 0 };                           // used while in DEBUGGING mode only: index of local variable NAME


    // temporary local variable stoarage during function parsing (without values)
    char localVarType[MAX_LOC_VARS_IN_FUNC]{ 0 };                 // parameter, local variables: temporarily maintains array flag during function parsing (storage reused by functions during parsing)
    char localVarDims[MAX_LOC_VARS_IN_FUNC][4]{ 0 };              // LOCAL variables: temporarily maintains dimensions during function parsing (storage reused by functions during parsing)



    // function key data storage
    char* extFunctionNames[MAX_EXT_FUNCS];
    ExtFunctionData extFunctionData[MAX_EXT_FUNCS];

    LE_evalStack* _pEvalStackTop{ nullptr }, * _pEvalStackMinus1{ nullptr }, * _pEvalStackMinus2{ nullptr };
    void* _pFlowCtrlStackTop{ nullptr }, * _pFlowCtrlStackMinus1{ nullptr }, * _pFlowCtrlStackMinus2{ nullptr };
    char* _pImmediateCmdStackTop{ nullptr };

    Val lastResultValueFiFo[MAX_LAST_RESULT_DEPTH];                // keep last evaluation results
    char lastResultTypeFiFo[MAX_LAST_RESULT_DEPTH]{  };


    // evaluation stack
    // ----------------

    // maintains intermediate results of a calculation (execution phase). Implemented as a linked list

    LinkedList evalStack;


    // flow control stack
    // ------------------

    // if statements are currently being executed, structure _activeFunctionData maintains data about either the main program level (command line statements) ...
    // ... or the active function (depends on where code is actually executed)
    // the flow control stack (linked list flowCtrlStack) on the other hand maintains data about open block commands (loops, ...) and callers (functions or program main level) 
    // => entries from stack top (newest entries) to bottom: 
    // - entries for any open block commands in the active function (or main program level, if code is currently executed from there)
    // - for each caller in the call stack:
    //    - an entry for the caller (could be another function or the main program level) 
    //    - entries for any open block commands in the caller

    // - if a program is currently stopped (debug mode), data about the stopped function is also pushed to the flowcontrol stack (it has 'called' debug level, so to speak) ...
    //   ... and _activeFunctionData now contains data about a 'new' main program level (any command line statements executed for debugging purposes) 

    // if execution of a NEW program is started while in debug mode, the whole process as described above is repeated. So, you can have more than one program being suspended

    LinkedList flowCtrlStack;

    // immediate mode command stack
    // ----------------------------

    // while at least one program is stopped (debug mode), the parsed code of the original command line from where execution started is pushed to a separate stack, and popped again ...
    // ...when the program resumes. If multiple programs are currently stopped (see: flow control stack), this stack will contain multiple entries
    LinkedList immModeCommandStack;


    // callback functions and storage

    unsigned long _lastCallBackTime{ 0 }, _currenttime{ 0 }, _previousTime{ 0 };

    void (*_housekeepingCallback)(bool& requestQuit, long& appFlags);                                         // pointer to callback function for heartbeat

    void (*_callbackUserProcStart[_userCBarrayDepth])(const void** pdata, const char* valueType, const int argCount);             // user functions: pointers to c++ procedures                                   

    char _callbackUserProcAlias[_userCBarrayDepth][MAX_IDENT_NAME_LEN + 1];       // user functions aliases                                   
    void* _callbackUserData[_userCBarrayDepth][3]{ nullptr };                          // user functions: pointers to data                                   


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

public:
    Justina_interpreter(Stream* const pConsole, long progMemSize);               // constructor
    ~Justina_interpreter();               // deconstructor
    bool setMainLoopCallback(void (*func)(bool& requistQuit, long& appFlags));                   // set callback functions
    bool setUserFcnCallback(void (*func) (const void** pdata, const char* valueType, const int argCount));
    bool run(Stream* const pConsole, Stream** const pTerminal, int definedTerms);

private:
    bool parseAsResWord(char*& pNext, parseTokenResult_type& result);
    bool parseAsNumber(char*& pNext, parseTokenResult_type& result);
    bool parseAsStringConstant(char*& pNext, parseTokenResult_type& result);
    bool parseTerminalToken(char*& pNext, parseTokenResult_type& result);
    bool parseAsInternFunction(char*& pNext, parseTokenResult_type& result);
    bool parseAsExternFunction(char*& pNext, parseTokenResult_type& result);
    bool parseAsVariable(char*& pNext, parseTokenResult_type& result);
    bool parseAsIdentifierName(char*& pNext, parseTokenResult_type& result);

    bool checkCommandKeyword(parseTokenResult_type& result);
    bool checkCommandArgToken(parseTokenResult_type& result, int& clearIndicatore);
    bool checkExtFunctionArguments(parseTokenResult_type& result, int& minArgCnt, int& maxArgCnt);
    bool checkArrayDimCountAndSize(parseTokenResult_type& result, int* arrayDef_dims, int& dimCnt);
    int getIdentifier(char** pIdentArray, int& identifiersInUse, int maxIdentifiers, char* pIdentNameToCheck, int identLength, bool& createNew, bool isUserVar = false);
    bool checkInternFuncArgArrayPattern(parseTokenResult_type& result);
    bool checkExternFuncArgArrayPattern(parseTokenResult_type& result, bool isFunctionClosingParenthesis);
    bool initVariable(uint16_t varTokenStep, uint16_t constTokenStep);


    void resetMachine(bool withUserVariables);
    void initInterpreterVariables(bool withUserVariables);
    void danglingPointerCheckAndCount(bool withUserVariables);
    void deleteIdentifierNameObjects(char** pIdentArray, int identifiersInUse, bool isUserVar = false);
    void deleteStringArrayVarsStringObjects(Val* varValues, char* varType, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar = false, bool isLocalVar = false);
    void deleteVariableValueObjects(Val* varValues, char* varType, int varNameCount, int paramOnlyCount, bool checkIfGlobalValue, bool isUserVar = false, bool isLocalVar = false);
    void deleteLastValueFiFoStringObjects();
    void deleteConstStringObjects(char* pToken);
    void parseAndExecTraceString();
    parseTokenResult_type parseStatement(char*& pInputLine, char*& pNextParseStatement, int& clearIndicator);
    bool allExternalFunctionsDefined(int& index);
    void prettyPrintStatements(int instructionCount, char* startToken = nullptr, char* errorProgCounter = nullptr, int* sourceErrorPos = nullptr);
    void printParsingResult(parseTokenResult_type result, int funcNotDefIndex, char* const pInputLine, int lineCount, char* pErrorPos);

    void expandStringBackslashSequences(char*& input);

    void* fetchVarBaseAddress(TokenIsVariable* pVarToken, char*& pVarType, char& valueType, char& sourceVarScopeAndFlags);
    void* arrayElemAddress(void* varBaseAddress, int* dims);

    execResult_type  exec(char* startHere);
    execResult_type  execParenthesesPair(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);
    execResult_type  execAllProcessedOperators();

    execResult_type  execUnaryOperation(bool isPrefix);
    execResult_type  execInfixOperation();
    execResult_type  execInternalFunction(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);
    execResult_type  launchExternalFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount);
    execResult_type  launchEval(LE_evalStack*& pFunctionStackLvl, char* parsingInput);
    execResult_type  terminateExternalFunction(bool addZeroReturnValue = false);
    execResult_type  terminateEval();
    execResult_type execProcessedCommand(bool& isFunctionReturn, bool& cmdLineRequestsProgramStop, bool& userRequestsAbort);
    execResult_type testForLoopCondition(bool& fail);

    execResult_type checkFmtSpecifiers(bool isDispFmt, bool valueIsString, int suppliedArgCount, char* valueType, Val* operands, char& numSpecifier,
        int& width, int& precision, int& flags);
    void makeFormatString(int flags, bool isIntFmt, char* numFmt, char* fmtString);
    void printToString(int width, int precision, bool isFmtString, bool isIntFmt, char* valueType, Val* operands, char* fmtString,
        Val& fcnResult, int& charsPrinted);

    void initFunctionParamVarWithSuppliedArg(int suppliedArgCount, LE_evalStack*& pFirstArgStackLvl);
    void initFunctionDefaultParamVariables(char*& calledFunctionTokenStep, int suppliedArgCount, int paramCount);
    void initFunctionLocalNonParamVariables(char* calledFunctionTokenStep, int paramCount, int localVarCount);

    void makeIntermediateConstant(LE_evalStack* pEvalStackLvl);

    Justina_interpreter::execResult_type arrayAndSubscriptsToarrayElement(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);

    void saveLastValue(bool& overWritePrevious);
    void clearEvalStack();
    void clearEvalStackLevels(int n);
    void clearFlowCtrlStack(int& deleteImmModeCmdStackLevels, execResult_type execResult = result_execOK, bool debugModeError = false);
    void clearImmediateCmdStack(int n);

    void deleteOneArrayVarStringObjects(Justina_interpreter::Val* varValues, int index, bool isUserVar, bool isLocalVar);
    execResult_type deleteVarStringObject(LE_evalStack* pStackLvl);
    execResult_type deleteIntermStringObject(LE_evalStack* pStackLvl);

    execResult_type copyValueArgsFromStack(LE_evalStack*& pStackLvl, int argCount, bool* argIsVar, bool* argIsArray, char* valueType, Val* args, bool passVarRefOrConst = false, Val* dummyArgs = nullptr);

    int findTokenStep(char*& pStep, int tokenTypeToFind, char tokenCodeToFind, char tokenCode2ToFind = -1);
    int jumpTokens(int n, char*& pStep, int& tokenCode);
    int jumpTokens(int n, char*& pStep);
    int jumpTokens(int n);

    void pushTerminalToken(int& tokenType);
    void pushFunctionName(int& tokenType);
    void pushGenericName(int& tokenType);
    void pushConstant(int& tokenType);
    void pushVariable(int& tokenType);


    bool getKey(char& c, bool enableTimeOut = false);
    bool readText(bool& doAbort, bool& doStop, bool& doCancel, bool& doDefault, char* input, int& length);

    bool addCharacterToInput(bool& lastCharWasSemiColon, bool& withinString, bool& withinStringEscSequence, bool& within1LineComment, bool& withinMultiLineComment,
        bool& redundantSemiColon, bool isEndOfFile, bool& bufferOverrun, bool  _flushAllUntilEOF, int& _lineCount, int& _statementCharCount, char c);
    bool processAndExec(parseTokenResult_type result, bool& kill, int lineCount, char* pErrorPos, int clearIndicator);
    void traceAndPrintDebugInfo();
    void printVariables(bool userVars);
    parseTokenResult_type deleteUserVariable(char* userVarName = nullptr);

    execResult_type open(int& fileNumber, char* filePath, int mod = FILE_WRITE);
    execResult_type close(int fileNumber);
    execResult_type initSD();
    execResult_type ejectSD();
    execResult_type listFiles();
};

#endif
