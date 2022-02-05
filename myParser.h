// MyParser.h

#ifndef _MYPARSER_h
#define _MYPARSER_h

#include "arduino.h"
#include <stdlib.h>
#include <memory>

class Calculator;

/***********************************************************
*                    class MyLinkedLists                   *
*    append and remove list elements from linked list      *
***********************************************************/

class MyLinkedLists {

    // --------------------
    // *   enumerations   *
    // --------------------

public:

    enum listType_type {                                       // identifier type
        list_isToken,
        list_isVariable,
        list_isExtFunction,
        list_isStack
    };


    // ------------------
    // *   structures   *
    // ------------------

public:

    struct ListElemHead {                                        // list element structure (fixed length for all data types)
        ListElemHead* pNext;                                     // pointer to next list element
        ListElemHead* pPrev;                                     // pointer to previous list element (currently not used; needed if deleting other list elements than last) 
    };


    // -----------------
    // *   variables   *
    // -----------------

private:

    static int8_t _listIDcounter;                               // number of lists created

    ListElemHead* _pFirstElement = nullptr;                      // pointers to first and last list element
    ListElemHead* _pLastElement = nullptr;
    int _listElementCount { 0 };                                // list element count (currently not used)
    listType_type _listType {};

public:
    int8_t _listID { 0 };                                       // list ID (in order of creation) 


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

public:

    MyLinkedLists();                   // constructor
    char* appendListElement( uint8_t size );
    char* deleteListElement( void* pPayload );                  // pointer to payload of list element to be removed
    void deleteList();
    char* getFirstListElement();
    char* getLastListElement();
    char* getPrevListElement( void* pPayload );
    char* getNextListElement( void* pPayload );
};


/***********************************************************
*                       class MyParser                     *
*             parse character string into tokens           *
***********************************************************/

class MyParser {

    // --------------------
    // *   enumerations   *
    // --------------------

private:

    enum tokenType_type {                                       // token type
        tok_no_token,                                           // no token to process
        tok_isReservedWord,
        tok_isAlphaConst,
        tok_isInternFunction,
        tok_isExternFunction,
        tok_isNumConst,
        tok_isVariable,
        // all terminal tokens: at the end of the list ! (occupy only one character in program, combining token type and index)
        tok_isOperator,
        tok_isLeftParenthesis,
        tok_isRightParenthesis,
        tok_isCommaSeparator,
        tok_isSemiColonSeparator
    };

    enum blockType_type {
        // value 1: block type
        block_none,                                             // command is not a block command
        block_extFunction,
        block_for,
        block_while,
        block_if,
        block_alterFlow,                                        // alter flow in specific open block types
        block_genericEnd,                                       // ends anytype of open block

        // value 2, 3, 4: position in open block, min & max position of previous block command within same block level
        block_na,                                               // not applicable
        block_startPos,                                         // command starts an open block
        block_midPos1,                                          // command only allowed in open block  
        block_midPos2,                                          // command only allowed in open block
        block_endPos,                                           // command ends an open block
        block_inOpenFunctionBlock,                              // command can only occur if currently a function block is open
        block_inOpenLoopBlock,                                   // command can only occur if at least one loop block is open

        // alternative for value 2: action (only if block type = block_none)
        cmd_globalVar,
        cmd_localVar,
        cmd_staticVar
    };

    enum parseTokenResult_type {                                // token parsing result
        result_tokenFound,                                      // token found

        // incomplete expression errors
        result_tokenNotFound = 1000,                                   // no token found
        result_expressionNotComplete,
        result_missingLeftParenthesis,

        // token not allowed errors
        result_separatorNotAllowedHere = 1100,
        result_operatorNotAllowedHere,
        result_parenthesisNotAllowedHere,
        result_resWordNotAllowedHere,
        result_functionNotAllowedHere,
        result_variableNotAllowedHere,
        result_alphaConstNotAllowedHere,
        result_numConstNotAllowedHere,
        result_assignmNotAllowedHere,

        // token expected errors
        result_constantValueExpected = 1200,
        result_variableNameExpected,
        result_functionDefExpected,

        // used memory errors
        result_maxVariableNamesReached = 1300,                               // more errors 
        result_maxLocalVariablesReached,
        result_maxStaticVariablesReached,
        result_maxExtFunctionsReached,

        // token errors
        result_identifierTooLong = 1400,
        result_alphaConstTooLong,
        result_alphaConstInvalidEscSeq,
        result_closingQuoteMissing,
        result_spaceMissing,
        result_token_not_recognised,

        // function errors
        result_nameInUseForVariable = 1500,
        result_wrong_arg_count,
        result_undefinedFunction,                              // external (user) function errors
        result_functionAlreadyDefinedBefore,
        result_mandatoryArgFoundAfterOptionalArgs,
        result_functionDefMaxArgsExceeded,
        result_prevCallsWrongArgCount,
        result_functionDefsCannotBeNested,
        result_fcnScalarAndArrayArgOrderNotConsistent,
        result_redefiningIntFunctionNotAllowed,

        // variable errors
        result_varNameInUseForFunction = 1600,
        result_varNotDeclared,
        result_varRedeclared,
        result_varDefinedAsArray,
        result_varDefinedAsScalar,

        // array errors
        result_arrayDefNoDims = 1700,
        result_arrayDefNegativeDim,
        result_arrayDefMaxDimsExceeded,
        result_arrayDefMaxElementsExceeded,
        result_arrayUseNoDims,
        result_arrayUseWrongDimCount,
        result_arrayParamExpected,
        result_noMassInitWithNonEmptyStrings,

        // command errors 
        result_resWordExpectedAsCmdPar = 1800,
        result_expressionExpectedAsCmdPar,
        result_varWithoutAssignmentExpectedAsCmdPar,
        result_variableExpectedAsCmdPar,
        result_cmdParameterMissing,
        result_cmdHasTooManyParameters,

        // block command errors
        result_noOpenBlock = 1900,
        result_notAllowedInThisOpenBlock,
        result_notAllowedInsideFunction,
        result_wrongBlockSequence,
        result_noBlockEnd,
        result_noOpenLoop,
        result_noOpenFunction,
        result_notInsideProgram,
        result_onlyInsideProgram,

        // other program errors
        result_progMemoryFull = 2000
    };


    // --------------------------
    // *   unions, structures   *
    // --------------------------

private:

    // Note: structures starting with 'LE_' are used to cast list elements for easy handling

    struct CmdBlockDef {                                        // block commands
        char blockType;                                         // block type ('for' block, 'if' block,...)
        char blockPosOrAction;                                     // position of command (reserved word) in block (0=start, 1, 2 = mid, 3=end)
        char blockMinPredecessor;                               // minimum position of previous command (reserved word) for open block
        char blockMaxPredecessor;                               // maximum position
    };

    struct ResWordDef {                                         // reserved words with pattern for parameters (if reserved word is used as command, starting an instruction)
        const char* _resWordName;
        const char* pCmdAllowedParTypes;
        const CmdBlockDef cmdBlockDef;                          // block commands: position in command block and min, max required position of previous block command 
    };

    struct FuncDef {                                            // function names with min & max number of arguments allowed 
        const char* funcName;
        char minArgs;                                           // internal (intrinsic) functions: min & max n° of allowed arguments
        char maxArgs;
    };


    // storage for tokens
    // note: to avoid boundary alignment of structure members, character placeholders of correct size are used for all structure members

    struct TokenIsResWord {                                     // generic token: length 4
        char tokenType;                                         // will be set to specific token type
        char tokenIndex;                                        // index into list of tokens of a specific type
        char toTokenStep [2];                                     // tokens for block commands (IF, FOR, BREAK, END, ...): step n° of block start token or next block token (int16_t)
    };
    struct TokenIsFloatCst {                                    // token storage for a numeric constant token: length 5
        char tokenType;                                         // will be set to specific token type
        char numConst [4];                                       // placeholder for float - avoiding boundary alignment
    };

    struct TokenIsAlphanumCst {                                 // token storage for an alphanumeric constant token: length 5
        char tokenType;                                         // will be set to specific token type
        char pAlphanumConst [4];                                 // pointer to string object
    };

    struct TokenIsIntFunction {                                 // operators, separators, parenthesis: length 2
        char tokenType;                                         // will be set to specific token type
        char tokenIndex;                                        // index into list of tokens
    };

    struct TokenIsExtFunction {                                 // token storage for variable: length 2
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


    union TokPnt {
        char* pToken;
        TokenIsResWord* pResW;
        TokenIsFloatCst* pFloat;
        TokenIsAlphanumCst* pAnumP;
        TokenIsIntFunction* pIntFnc;
        TokenIsExtFunction* pExtFnc;
        TokenIsVariable* pVar;
        TokenIsTerminal* pTerminal;                             // terminal token
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
        char identifierIndex;                                   // functions and variables: index
        char actualArgsOrDims;                                  // actual number of arguments found (function) or dimension count (prev. defined array) 
        char flags;                                             // if stack level is open parenthesis (not open block): other flags 
    };

    struct OpenCmdBlockLvl {
        CmdBlockDef cmdBlockDef;                                // storage for info about block commands
        char tokenStep [2];                                     // block commands: step n° of next block command, or to block start command, in open block
        char fcnBlock_functionIndex;                            // function definition block only: function index
    };

    union LE_stack {
        OpenParenthesesLvl openPar;
        OpenCmdBlockLvl openBlock;
    };

    // -----------------
    // *   constants   *
    // -----------------

private:

    static constexpr char extFunctionFirstOccurFlag = 0x10;     // flag: min > max means not initialized
    static constexpr char extFunctionMaxArgs = 0xF;             // must fit in 4 bits

    // these constants are used to check to which token group (or group of token groups) a parsed token belongs
    static constexpr int8_t lastTokenGroup_0 = 1 << 0;          // operator, comma
    static constexpr int8_t lastTokenGroup_1 = 1 << 1;          // (line start), semicolon, reserved word
    static constexpr int8_t lastTokenGroup_2 = 1 << 2;          // number, alphanumeric constant, right bracket
    static constexpr int8_t lastTokenGroup_3 = 1 << 3;          // internal or external function name
    static constexpr int8_t lastTokenGroup_4 = 1 << 4;          // left parenthesis
    static constexpr int8_t lastTokenGroup_5 = 1 << 5;          // variable

    // groups of token groups: combined token groups (for testing valid token sequences when next token will be parsed)
    static constexpr int8_t lastTokenGroups_4_1_0 = lastTokenGroup_4 | lastTokenGroup_1 | lastTokenGroup_0;
    static constexpr int8_t lastTokenGroups_5_2_1 = lastTokenGroup_5 | lastTokenGroup_2 | lastTokenGroup_1;
    static constexpr int8_t lastTokenGroups_5_4_2 = lastTokenGroup_5 | lastTokenGroup_4 | lastTokenGroup_2;
    static constexpr int8_t lastTokenGroups_5_4_3_1_0 = lastTokenGroup_5 | lastTokenGroup_4 | lastTokenGroup_3 | lastTokenGroup_1 | lastTokenGroup_0;
    static constexpr int8_t lastTokenGroups_5_2 = lastTokenGroup_5 | lastTokenGroup_2;




    // commands parameters: types allowed
    static constexpr int8_t cmdPar_none = 0;
    static constexpr int8_t cmdPar_resWord = 1;            // !!! note: reserved words as parameters: not implemented
    static constexpr int8_t cmdPar_varOnly = 2;
    static constexpr int8_t cmdPar_varOptAssignment = 3;
    static constexpr int8_t cmdPar_expression = 4;
    static constexpr int8_t cmdPar_extFunction = 5;
    static constexpr int8_t cmdPar_numConstOnly = 6;

    static constexpr int8_t cmdPar_multipleFlag = 0x08;             // may be combined with value of one of the allowed types: will be allowed 0 to n times

    // first parameter only: indicate command (not parameter) usage restrictions in bits 7654
    static constexpr int8_t cmd_restrictMask = 0xF0;            // mask for checking where command is allowed
    static constexpr int8_t cmd_noRestrictions = 0x00;          // command has no usage restrictions 
    static constexpr int8_t cmd_onlyInProgram = 0x10;           // command is only allowed insde a program
    static constexpr int8_t cmd_onlyImmediate = 0x20;           // command is only allowed in immediate mode
    static constexpr int8_t cmd_onlyInFunction = 0x30;          // command is only allowed inside a function block
    static constexpr int8_t cmd_onlyOutsideFunction = 0x40;     // command is only allowed inside a function block


    // commands (FUNCTION, FOR, ...): allowed command parameters (naming: cmdPar_<n[nnn]> with A'=variable with (optional) assignment, 'E'=expression, 'E'=expression, 'R'=reserved word
    static const char cmdPar_fnc_AA_mult [4];                   // only within functin block: allow: 'A'=variable with (optional) assignment : 1 + (0 to n) times                       
    static const char cmdPar_fnc_VV_mult [4];                   // only within functin block: allow: 'A'=variable (only) : 1 + (0 to n) times                       
    static const char cmdPar_noFnc_AA_mult [4];                 // only outside function block: allow: 'A'=variable (only) : 1 + (0 to n) times 

    static const char cmdPar_EVEV [4];                          //// test
    static const char cmdPar_N [4];                             // command takes no parameters
    static const char cmdPar_E [4];                             // allow: 'E'=expression  
    static const char cmdPar_V [4];                             // allow: 'V'=variable (only)
    static const char cmdPar_A [4];                             // allow: 'A'=variable with (optional) assignment
    static const char cmdPar_R [4];                             // allow: 'R'=reserved word
    static const char cmdPar_F [4];                             // allow: 'F'=function definition 
    static const char cmdPar_AEE [4];                           // allow: 'A'=variable with (optional) assignment, 'E'=expression, 'E'=expression


    // block commands only (FOR, END, etc.): type of block, position in block OR (block_none:) action, sequence check in block: allowed previous block commands 
    static constexpr CmdBlockDef cmdBlockNone { block_none, block_na,block_na,block_na };                                   // not a 'block' command
    static constexpr CmdBlockDef cmdBlockExtFunction { block_extFunction,block_startPos,block_na,block_na };                // 'IF' block mid position 2, min & max previous position is block start & block position 1, resp.
    static constexpr CmdBlockDef cmdBlockWhile { block_while,block_startPos,block_na,block_na };                            // 'WHILE' block start
    static constexpr CmdBlockDef cmdBlockFor { block_for, block_startPos,block_na,block_na };                               // 'FOR' block start
    static constexpr CmdBlockDef cmdBlockIf { block_if,block_startPos,block_na,block_na };                                  // 'IF' block start
    static constexpr CmdBlockDef cmdBlockIf_elseIf { block_if,block_midPos1,block_startPos,block_midPos1 };                 // 'IF' block mid position 1, min & max previous position is block start & block position 1, resp.
    static constexpr CmdBlockDef cmdBlockIf_else { block_if,block_midPos2,block_startPos,block_midPos1 };                   // 'IF' block mid position 2, min & max previous position is block start & block position 1, resp.

    // second value: action
    static constexpr CmdBlockDef cmdGlobalVar { block_none, cmd_globalVar , block_na , block_na };
    static constexpr CmdBlockDef cmdLocalVar { block_none, cmd_localVar , block_na , block_na };
    static constexpr CmdBlockDef cmdStaticVar { block_none, cmd_staticVar , block_na , block_na };

    // 'alter flow' block commands require an open block of a specific type, NOT necessary in the current inner open block 
    // the second value ('position in block') is specified to indicate which type of open block is required (e.g. a RETURN command can only occur within a FUNCTION...END block)
    static constexpr CmdBlockDef cmdBlockOpenBlock_loop { block_alterFlow,block_inOpenLoopBlock,block_na,block_na };        // only if an open FOR or WHILE block 
    static constexpr CmdBlockDef cmdBlockOpenBlock_function { block_alterFlow,block_inOpenFunctionBlock,block_na,block_na };// only if an open FUNCTION definition block 

    // used to close any type of currently open inner block
    static constexpr CmdBlockDef cmdBlockGenEnd { block_genericEnd,block_endPos,block_na,block_endPos };            // all block types: block end 

    static const ResWordDef _resWords [];                       // reserved word names
    static const FuncDef _functions [];                         // function names with min & max arguments allowed 
    static const char* const singleCharTokens;                  // all one-character tokens (and possibly first character of two-character tokens)
    static const int8_t _maxIdentifierNameLen { 14 };           // max length of identifier names, excluding terminating '\0'
    static const int8_t _maxAlphaCstLen { 15 };                 // max length of alphanumeric constants, excluding terminating '\0' (also if stored in variables)


    // -----------------
    // *   variables   *
    // -----------------

private:
    bool _isCommand = false;                                    // a command is being parsed (instruction starting with a reserved word)
    bool _isExtFunctionCmd = false;                             // FUNCTION command is being parsed (not the complete function)
    bool _isGlobalVarCmd = false;                                // VAR command is being parsed
    bool _isLocalVarCmd = false;                                // LOCAL command is being parsed
    bool _isStaticVarCmd = false;                               // STATIC command is being parsed
    bool _isAnyVarCmd = false;                                     // VAR, LOCAL or STATIC command is being parsed

    bool _varDefAssignmentFound = false;
    bool _extFunctionBlockOpen = false;                         // commands within FUNCTION...END block are being parsed (excluding END command)
    bool _leadingSpaceCheck { false };

    // parsing stack: value supplied when pushing data to stack OR value returned when stack drops 
    char _minFunctionArgs { 0 };                                // if external function defined prior to call: min & max allowed arguments. Otherwise, counters to keep track of min & max actual arguments in previous calls 
    char _maxFunctionArgs { 0 };
    int _extFunctionIndex { 0 };
    int _variableNameIndex { 0 };
    bool _arrayElemAssignmentAllowed { false };                    // value returned: assignment to array element is allowed next

    int8_t _commandParNo { 0 };
    int8_t _tokenIndex { 0 };
    int8_t _resWordNo;                                          // index into list of reserved words
    int8_t _functionNo;                                         // index into list of internal (intrinsic) functions
    int8_t _lastTokenGroup_sequenceCheck = 0;                   // bits indicate which token group the last token parsed belongs to          
    int8_t _parenthesisLevel = 0;                               // current number of open parentheses
    int8_t _blockLevel = 0;                                     // current number of open blocks


    const char* _pCmdAllowedParTypes;
    int16_t _lastTokenStep, _lastVariableTokenStep;
    int16_t _blockCmdTokenStep, _blockStartCmdTokenStep;   // pointers to reserved words used as block commands                           
    LE_stack* _pCurrStackLvl;
    LE_stack* _pFunctionDefStackLvl;

    tokenType_type _lastTokenType = tok_no_token;               // type of last token parsed
    tokenType_type _lastTokenType_hold = tok_no_token;
    tokenType_type _previousTokenType = tok_no_token;

    MyLinkedLists myStack;                                      // during parsing: linked list keeping track of open parentheses and open blocks


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

private:

    parseTokenResult_type  parseInstruction( char*& pInputLine, char* pInfo, int8_t& cnt );
    bool parseAsResWord( char*& pNext, int8_t& cnt, parseTokenResult_type& result );
    bool parseAsNumber( char*& pNext, int8_t& cnt, parseTokenResult_type& result );
    bool parseTerminalToken( char*& pNext, int8_t& cnt, parseTokenResult_type& result );
    bool parseAsInternFunction( char*& pNext, int8_t& cnt, parseTokenResult_type& result );
    bool parseAsExternFunction( char*& pNext, int8_t& cnt, parseTokenResult_type& result );
    bool parseAsVariable( char*& pNext, int8_t& cnt, parseTokenResult_type& result );
    bool parseAsAlphanumConstant( char*& pNext, int8_t& cnt, parseTokenResult_type& result );
    bool checkCommandSyntax( parseTokenResult_type& result );
    void prettyPrintParsedInstruction( char* pPretty, int charsPretty );
    void deleteAllIdentifierNames( char** pIdentArray, int identifiersInUse );
    bool checkExtFunctionArguments( parseTokenResult_type& result, int8_t& minArgCnt, int8_t& maxArgCnt );
    bool checkArrayDimCountAndSize( parseTokenResult_type& result, int8_t* arrayDef_dims, int8_t& dimCnt );
    int getIdentifier( char** pIdentArray, int& identifiersInUse, int maxIdentifiers, char* pIdentNameToCheck, int8_t identLength, bool& createNew );
    bool allExternalFunctionsDefined( int& index );
    bool checkFuncArgArrayPattern( parseTokenResult_type& result, bool isFunctionClosingParenthesis );
    bool initVariable( int16_t varTokenStep, int16_t constTokenStep );

public:

    MyParser();                                                 // constructor
    void resetMachine();
    void deleteAllAlphanumStrValues( char* pToken );
    int8_t parseSource( char* const inputLine, char* info, char* pretty, int charsPrettyLine );
    void deleteParsedData();
};


/***********************************************************
*                      class Calculator                    *
*        parse and execute user input and programs         *
***********************************************************/

class Calculator {

public:

    static constexpr int8_t extFunctionBit { B00000001 };
    static constexpr int8_t extFunctionPrevDefinedBit { B00000010 };
    static constexpr int8_t intFunctionBit { B00000100 };
    static constexpr int8_t openParenthesisBit { B00001000 };                                  // not a function
    static constexpr int8_t arrayBit { B00010000 };
    static constexpr int8_t arrayElemAssignmentAllowedBit { B00100000 };

    static constexpr int PROG_MEM_SIZE { 2000 };
    static constexpr int IMM_MEM_SIZE { 200 };
    static constexpr int MAX_VARNAMES { 64 };                       // max. vars (all types: global, static, local, parameter). Absolute limit: 255
    static constexpr int MAX_STAT_VARS { 32 };                      // max. static vars (only). Absolute limit: 255
    static constexpr int MAX_LOC_VARS_IN_FUNC { 16 };               // max. local and parameter vars (only) in an individual function. Absolute limit: 127 (index: bit 7 used in extFunctionData[]...)
    static constexpr int MAX_EXT_FUNCS { 16 };                      // max. external functions. Absolute limit: 255
    static constexpr int MAX_ARRAY_DIMS { 3 };                        // 1, 2 or 3 is allwed: must fit in 3 bytes
    static constexpr int MAX_ARRAY_ELEM { 200 };                      // max. n° of floats in a single array

    union Val {
        float numConst;                                         // variable contains number: float
        char* pAlphanumConst;                                   // variable contains string: pointer to a character string
        float* pNumArray;                                       // variable is an array: pointer to array
    };

    struct ExtFunctionData {
        char* pExtFunctionStartToken;                           // ext. function: pointer to start of function (token)
        char locVarCnt_statVarInit;                             // needed to reserve run time storage for local variables
        char paramIsArrayPattern [2];                         // parameter pattern: b15 flag set when parsing function definition or first function call; b14-b0 flags set when corresponding parameter or argument is array      
    };


    // variable type: 

    // bit b7: variable name has a global variable associated with it. Only used during parsing, not stored in token
    static constexpr int8_t var_hasGlobalValue = 0x80;              // flag: global variable attached to this name

    // bits b654: variable qualifier. Use: (1) during parsing: temporarily store the variable type associated with a particular reference of a variable name 
    // (2) stored in 'variable' token to indicate the variable type associated with a particular reference of a variable name 
    static constexpr int8_t var_qualifierMask = 0x70;               // mask
    static constexpr int8_t var_isGlobal = 4 << 4;                  // variable is global, in or outside function
    static constexpr int8_t var_isStaticInFunc = 3 << 4;            // variable is static in function
    static constexpr int8_t var_isLocalInFunc = 2 << 4;             // variable is local in function
    static constexpr int8_t var_isParamInFunc = 1 << 4;             // variable is function parameter
    static constexpr int8_t var_qualToSpecify = 0 << 4;             // qualifier is not yet defined (temporary use during parsing; never stored in token)

    // bit b3: global variable definition encountered in program during parsing ('VAR' cmd) 
    static constexpr int8_t var_globalDefInProg = 0x08;             // temporary use during parsing; never stored in token

    // bit b2: variable is an array (and not a scalar)
    static constexpr int8_t var_isArray = 0x04;                     // stored with variable attributes and in 'variable' token. Can not be changed at runtime

    // bits b10: variable type (b1: spare) 
    // stored with variable attributes, but NOT in 'variable' token, because only fixed for arrays (scalars: type can dynamically change at runtime)
    static constexpr int8_t var_typeMask = 0x01;                    // mask: float, char* 
    static constexpr int8_t var_isFloat = 0 << 0;
    static constexpr int8_t var_isStringPointer = 1 << 0;




    static constexpr  int _maxInstructionChars { 300 };
    static constexpr  int _maxCharsPretty { 2000 };//// verkleinen, print instructie per instructie

    char _instruction [_maxInstructionChars + 1] = "";
    int _instructionCharCount { 0 };
    char _pretty [_maxCharsPretty];
    char _parsingInfo [200];
    bool _programMode { false };


    int _varNameCount { 0 };                                        // counts number of variable names (global variables: also stores values) 
    int _localVarCountInFunction { 0 };                             // counts number of local variables in a specific function (names only, values not used)
    int _staticVarCount { 0 };                                      // static variable count (across all functions)
    int _extFunctionCount { 0 };                                    // external function count
    char _arrayDimCount { 0 };
    char* _programCounter { nullptr };                                // pointer to token memory address (not token step n°)
    int16_t _paramIsArrayPattern { 0 };

    // program storage
    char _programStorage [PROG_MEM_SIZE + IMM_MEM_SIZE];
    char* _programStart;
    int  _programSize;                            

    // variable name storage                                         
    char* varNames [MAX_VARNAMES];                                  // store distinct variable names
    char varValueIndex [MAX_VARNAMES] { 0 };                        // temporarily maintains index to variable storage during function parsing

    // global variable value storage
    Val globalVarValues [MAX_VARNAMES];                              // if variable name in use for global variable: store global value (float, pointer to string, pointer to array of floats)
    char globalVarType [MAX_VARNAMES] { 0 };                           // stores global variable usage flags and global variable type (float, pointer to string, pointer to array of floats); ...
                                                                    // ... during parsing, temporary storage for variable qualifier flags  
    // static variable value storage
    Val staticVarValues [MAX_STAT_VARS];                            // store static variable values (float, pointer to string, pointer to array of floats) 
    char staticVarType [MAX_STAT_VARS] { 0 };                       // static variables: stores variable type (float, pointer to string, pointer to array of floats)

    // temporary local variable stoarage during functin parsing (without values)
    char localVarType [MAX_LOC_VARS_IN_FUNC] { 0 };                 // parameter, local variables: temporarily maintains array flag during function parsing (storage reused by functions during parsing)
    char localVarDims [MAX_LOC_VARS_IN_FUNC][4] { 0 };              // LOCAL variables: temporarily maintains dimensions during function parsing (storage reused by functions during parsing)

    // function key data storage
    char* extFunctionNames [MAX_EXT_FUNCS];
    ExtFunctionData extFunctionData [MAX_EXT_FUNCS];

    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

    Calculator();               // constructor
    bool processCharacter( char c );
};

extern Calculator calculator;
extern MyParser myParser;

#endif

