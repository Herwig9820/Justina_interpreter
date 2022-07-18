// MyParser.h

#ifndef _JUSTINA_h
#define _JUSTINA_h

#include "arduino.h"
#include <stdlib.h>
#include <memory>

/***********************************************************
*                    class LinkedList                   *
*    append and remove list elements from linked list      *
***********************************************************/

class LinkedList {

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

    static int _listIDcounter;                               // number of lists created

    ListElemHead* _pFirstElement = nullptr;                      // pointers to first and last list element
    ListElemHead* _pLastElement = nullptr;
    int _listElementCount{ 0 };                                // list element count (currently not used)
    listType_type _listType{};

public:
    int _listID{ 0 };                                       // list ID (in order of creation) 


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

public:

    LinkedList();                   // constructor
    ~LinkedList();                   // constructor
    char* appendListElement(int size);
    char* deleteListElement(void* pPayload);                  // pointer to payload of list element to be removed
    void deleteList();
    char* getFirstListElement();
    char* getLastListElement();
    char* getPrevListElement(void* pPayload);
    char* getNextListElement(void* pPayload);
    int getElementCount();
    int getListID();
};


class MyParser;

/***********************************************************
*                      class Interpreter                    *
*        parse and execute user input and programs         *
***********************************************************/

class Interpreter {

public:

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
        tok_isTerminalGroup3        // if index between 32 and 47
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
        result_arg_outsideRange,
        result_arg_integerExpected,
        result_arg_invalid,
        result_arg_dimNumberIntegerExpected,
        result_arg_dimNumberInvalid,
        result_arg_stringExpected,
        result_arg_numValueExpected,
        result_array_dimNumberNonInteger,
        result_array_dimNumberInvalid,
        result_arg_varExpected,
        result_numericVariableExpected,
        result_aliasNotDeclared,

        // numbers and strings
        result_outsideRange,
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
        result_stringTooLong
    };


    // printing (to string, to stream)
    const int _defaultPrintWidth = 30, _defaultNumPrecision = 3, _defaultCharsToPrint = 30, _defaultPrintFlags = 0x00;       // at start up
    const int _maxPrintFieldWidth = 200, _maxNumPrecision = 7, _maxCharsToPrint = 200, _printFlagMask = 0x1F;

    static constexpr uint8_t extFunctionBit{ B00000001 };
    static constexpr uint8_t extFunctionPrevDefinedBit{ B00000010 };
    static constexpr uint8_t intFunctionBit{ B00000100 };
    static constexpr uint8_t openParenthesisBit{ B00001000 };                                  // not a function
    static constexpr uint8_t arrayBit{ B00010000 };
    static constexpr uint8_t arrayElemAssignmentAllowedBit{ B00100000 };
    static constexpr uint8_t arrayElemPostfixIncrDecrAllowedBit{ B01000000 };

    static constexpr int PROG_MEM_SIZE{ 2000 };
    static constexpr int IMM_MEM_SIZE{ 300 };
    static constexpr int MAX_USERVARNAMES{ 32 };                       // max. vars (all types: global, static, local, parameter). Absolute limit: 255
    static constexpr int MAX_PROGVARNAMES{ 64 };                       // max. vars (all types: global, static, local, parameter). Absolute limit: 255
    static constexpr int MAX_STAT_VARS{ 32 };                      // max. static vars (only). Absolute limit: 255
    static constexpr int MAX_LOC_VARS_IN_FUNC{ 32 };               // max. local and parameter vars (only) in an INDIVIDUAL function. Absolute limit: 255 
    static constexpr int MAX_EXT_FUNCS{ 16 };                      // max. external functions. Absolute limit: 255
    static constexpr int MAX_ARRAY_DIMS{ 3 };                        // 1, 2 or 3 is allwed: must fit in 3 bytes
    static constexpr int MAX_ARRAY_ELEM{ 200 };                      // max. n� of floats in a single array
    static constexpr int MAX_LAST_RESULT_DEPTH{ 10 };

    static const uint8_t _maxIdentifierNameLen{ 15 };           // max length of identifier names, excluding terminating '\0'

    // storage for tokens
    // note: to avoid boundary alignment of structure members, character placeholders of correct size are used for all structure members

    union CstValue {
        char longConst[4];
        char floatConst[4];
        char pStringConst[4];                                 // pointer to string object
    };

    struct TokenIsResWord {                                     // reserved word token (command): length 4 (if not a block command, token step is not stored and length will be 2)
        char tokenType;                                         // will be set to specific token type
        char tokenIndex;                                        // index into list of tokens of a specific type
        char toTokenStep[2];                                     // tokens for block commands (IF, FOR, BREAK, END, ...): step n� of block start token or next block token (uint16_t)
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
        void* pBaseValue;                                        // address of a variable value (which can be a float, a string pointer or a variable address itself)
        // global, static, local variables; parameters with default initialisation (if no argument provided)
        long longConst;                                        // long
        float floatConst;                                        // float
        char* pStringConst;                                     // pointer to a character string
        float* pArray;                                          // pointer to memory block reserved for array

        long* pLongConst;
        float* pFloatConst;
        char** ppStringConst;
        float** ppArray;
    };


    struct ExtFunctionData {
        char* pExtFunctionStartToken;                           // ext. function: pointer to start of function (token)
        char paramOnlyCountInFunction;
        char localVarCountInFunction;                             // needed to reserve run time storage for local variables 
        char paramIsArrayPattern[2];                         // parameter pattern: b15 flag set when parsing function definition or first function call; b14-b0 flags set when corresponding parameter or argument is array      
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
        char variableAttributes;                                    // is array; is array element; SOURCE variable scope
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



    struct blockTestData {
        char blockType;                 // command block: will identify stack level as an if...end, for...end, ... block
        char withinIteration;           // flag is set at the start of each iteration and cleared at the end 
        char fail;                      // 0x0 (pass) or 0x1 (fail)
        char breakFromLoop;

        // FOR...END loop only
        float* pControlVar;
        char* pControlValueType;
        float step;
        float finalValue;
        char* nextTokenAddress;         // address of token directly following 'FOR...; statement
    };

    struct FunctionData {
        char blockType;                 // command block: will identify stack level as a function block
        char functionIndex;             // for error messages only
        char callerEvalStackLevels;     // evaluation stack levels in use by caller(s) and main (call stack)
        // within a function, as in immediate mode, only one (block) command can be active at a time (ended by semicolon), in contrast to command blocks, which can be nested, so command data can be stored here:
        // data is stored when a reserved word is processed and it is cleared when the ending semicolon (ending the command) is processed
        char activeCmd_ResWordCode;     // reserved word code (set to 'cmdcod_none' again when semicolon is processed)

        char* activeCmd_tokenAddress;   // address of parsed reserved word token                                

        Val* pLocalVarValues;           // local variable value: real, pointer to string or array, or (if reference): pointer to 'source' (referenced) variable
        char** ppSourceVarTypes;        // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        char* pVariableAttributes;      // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) 

        char* pNextStep;             // next step to execute (look ahead)
        char* errorStatementStartStep;  // first token in statement where execution error occurs (error reporting)
        char* errorProgramCounter;      // token to point to in statement (^) if execution error occurs (error reporting)
    };


    // variable scope and value type bits: 

    // bit b7: program variable name has a global program variable associated with it. Only used during parsing, not stored in token
    //         user variables: user variable is used by program. Not stored in token 
    static constexpr uint8_t var_nameHasGlobalValue = 0x80;          // flag: global program variable attached to this variable name (note that meaning is different from 'var_isGlobal' constant)
    static constexpr uint8_t var_userVarUsedByProgram = 0x80;        // flag: user variable is used by program

    // bits b654: variable scope. Use: (1) during parsing: temporarily store the variable type associated with a particular reference of a variable name 
    // (2) stored in 'variable' token to indicate the variable type associated with a particular reference of a variable name 
    static constexpr uint8_t var_scopeMask = 0x70;               // mask
    static constexpr uint8_t var_isUser = 5 << 4;                    // variable is a user variable, in or outside function
    static constexpr uint8_t var_isGlobal = 4 << 4;                  // variable is global, in or outside function
    static constexpr uint8_t var_isStaticInFunc = 3 << 4;            // variable is static in function
    static constexpr uint8_t var_isLocalInFunc = 2 << 4;             // variable is local in function (non-parameter)
    static constexpr uint8_t var_isParamInFunc = 1 << 4;             // variable is function parameter
    static constexpr uint8_t var_scopeToSpecify = 0 << 4;             // scope is not yet defined (temporary use during parsing; never stored in token)

    // bit b3: variable is an array (and not a scalar)
    static constexpr uint8_t var_isArray = 0x08;                     // stored with variable attributes and in 'variable' token. Can not be changed at runtime

    // bits b210: value type 
    // - PARSED constants: these constants have a different 'constant' token type, so value type bits are NOT maintained
    // - INTERMEDIATE constants (execution only) and variables: value type is maintained together with variable / intermediate constant data (per variable, array or constant) 
    // Note: because the value type is not fixed for scalar variables (type can dynamically change at runtime), this info is not maintained in the parsed 'variable' token 

    static constexpr uint8_t value_typeMask = 0x07;                    // mask: float, char* 
    static constexpr uint8_t value_noValue = 0 << 0;
    static constexpr uint8_t value_isLong = 1 << 0;
    static constexpr uint8_t value_isFloat = 2 << 0;
    static constexpr uint8_t value_isStringPointer = 3 << 0;
private:
    static constexpr uint8_t value_isVarRef = 4 << 0;
public:

    // constants used during execution, only stored within the stack for value tokens

    // bit b0: intermediate constant (not a parsed constant, not a constant stored in a variable) 
    static constexpr uint8_t constIsIntermediate = 0x01;
    // bit b1: the address is the address of an array element. If this bit is zero, the address is the scalar or array variable base address 
    static constexpr uint8_t var_isArray_pendingSubscripts = 0x02;



    static const int _userCBarrayDepth = 10;

    static constexpr  int _maxInstructionChars{ 300 };
    static constexpr char promptText[10] = "Justina> ";
    static constexpr int _promptLength = sizeof(promptText) - 1;////


    // counting of heap objects (note: linked list element count is maintained within the linked list objects)

    // name strings for variables and functions
    int identifierNameStringObjectCount = 0;
    int userVarNameStringObjectCount = 0;

    // constant strings
    int parsedStringConstObjectCount = 0;
    int intermediateStringObjectCount = 0;
    int lastValuesStringObjectCount = 0;

    // strings as value of variables
    int globalStaticVarStringObjectCount = 0;
    int userVarStringObjectCount = 0;
    int localVarStringObjectCount = 0;

    // array storage 
    int globalStaticArrayObjectCount = 0;
    int userArrayObjectCount = 0;
    int localArrayObjectCount = 0;

    bool _atLineStart = true;
    bool _lastValueIsStored = false;

    // calculation result print
    int _dispWidth = _defaultPrintWidth, _dispNumPrecision = _defaultNumPrecision, _dispCharsToPrint = _defaultCharsToPrint, _dispFmtFlags = _defaultPrintFlags;
    char _dispNumSpecifier[2] = "G";      // room for 1 character and an extra terminating \0 (space voor length sub-specifier) (initialized during reset)
    bool _dispIsHexFmt{ false };              // initialized during reset          
    char  _dispNumberFmtString[20] = "", _dispStringFmtString[20] = "%*.*s%n";        // long enough to contain all format specifier parts; initialized during reset

     // for print command
    int _printWidth = _defaultPrintWidth, _printNumPrecision = _defaultNumPrecision, _printCharsToPrint = _defaultCharsToPrint, _printFmtFlags = _defaultPrintFlags;
    char _printNumSpecifier[2] = "G";      // room for 2 characters and an extra terminating \0 (space voor length sub-specifier) (initialized during reset)

    // display output settings
    int _promptAndEcho{ 2 };              // output prompt and echo of input
    bool _printLastResult{ true };

    char _instruction[_maxInstructionChars + 1] = "";
    int _instructionCharCount{ 0 };
    bool _programMode{ false };
    bool _flushAllUntilEOF{ false };
    bool _quitCalcAtEOF{ false };
    bool _keepInMemory{ true };                        //// maak afhankelijk van command parameter
    bool _isPrompt{ false };

    int _lineCount{ 0 };                             // taking into account new line after 'load program' command ////
    int _StarCmdCharCount{ 0 };

    int _userVarCount{ 0 };                                        // counts number of user variables (names and values) 
    int _programVarNameCount{ 0 };                                        // counts number of variable names (global variables: also stores values) 
    int _localVarCountInFunction{ 0 };                             // counts number of local variables in a specific function (names only, values not used)
    int _paramOnlyCountInFunction{ 0 };
    int _staticVarCount{ 0 };                                      // static variable count (across all functions)
    int _extFunctionCount{ 0 };                                    // external function count
    int _lastResultCount{ 0 };
    int _userCBprocStartSet_count = 0;
    int _userCBprocAliasSet_count = 0;

    char _arrayDimCount{ 0 };
    char* _programCounter{ nullptr };                                // pointer to token memory address (not token step n�)

    uint16_t _paramIsArrayPattern{ 0 };

    Stream* _pConsole{ nullptr };
    Stream** _pTerminal{ nullptr };
    int _definedTerminals{ 0 };

    // program storage
    char _programStorage[PROG_MEM_SIZE + IMM_MEM_SIZE];
    char* _programStart;
    int  _programSize;

    MyParser* _pmyParser;


    // variable storage
    // ----------------

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

    // local variable value storage
    FunctionData _activeFunctionData;

    // temporary local variable stoarage during function parsing (without values)
    char localVarType[MAX_LOC_VARS_IN_FUNC]{ 0 };                 // parameter, local variables: temporarily maintains array flag during function parsing (storage reused by functions during parsing)
    char localVarDims[MAX_LOC_VARS_IN_FUNC][4]{ 0 };              // LOCAL variables: temporarily maintains dimensions during function parsing (storage reused by functions during parsing)



    // function key data storage
    char* extFunctionNames[MAX_EXT_FUNCS];
    ExtFunctionData extFunctionData[MAX_EXT_FUNCS];

    LE_evalStack* _pEvalStackTop{ nullptr }, * _pEvalStackMinus1{ nullptr }, * _pEvalStackMinus2{ nullptr };
    void* _pFlowCtrlStackTop{ nullptr }, * _pFlowCtrlStackMinus1{ nullptr }, * _pFlowCtrlStackMinus2{ nullptr };

    Val lastResultValueFiFo[MAX_LAST_RESULT_DEPTH];                // keep last evaluation results
    char lastResultTypeFiFo[MAX_LAST_RESULT_DEPTH]{ value_noValue };

    LinkedList evalStack;
    LinkedList flowCtrlStack;


    // callback functions and storage

    void (*_callbackFcn)(bool& requestQuit);                                         // pointer to callback function for heartbeat

    void (*_callbackUserProcStart[_userCBarrayDepth])(const void** pdata, const char* valueType);             // user functions: pointers to c++ procedures                                   

    char _callbackUserProcAlias[_userCBarrayDepth][_maxIdentifierNameLen + 1];       // user functions aliases                                   
    void* _callbackUserData[_userCBarrayDepth][3]{ nullptr };                          // user functions: pointers to data                                   


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

    Interpreter(Stream* const pConsole);               // constructor
    ~Interpreter();               // deconstructor
    bool run(Stream* const pConsole, Stream** const pTerminal, int definedTerms);
    bool processCharacter(char c);

    bool setMainLoopCallback(void (*func)(bool& requistQuit));                   // set callback functions
    bool setUserFcnCallback(void (*func) (const void** pdata, const char* valueType));

    void* fetchVarBaseAddress(TokenIsVariable* pVarToken, char*& pVarType, char& valueType, char& variableAttributes, char& sourceVarAttributes);
    void* arrayElemAddress(void* varBaseAddress, int* dims);

    execResult_type  exec();
    execResult_type  execParenthesesPair(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);
    execResult_type  execAllProcessedOperators();

    execResult_type  execUnaryOperation(bool isPrefix);
    execResult_type  execInfixOperation();
    execResult_type  execInternalFunction(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);
    execResult_type  launchExternalFunction(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);
    execResult_type  terminateExternalFunction(bool addZeroReturnValue = false);
    execResult_type execProcessedCommand(bool& isFunctionReturn);
    execResult_type testForLoopCondition(bool& fail);

    execResult_type checkFmtSpecifiers(bool isDispFmt, bool isFmtString, int suppliedArgCount, char* valueType, Val* operands, char& numSpecifier,
        bool& isHexFmt, int& width, int& precision, int& flags);
    execResult_type makeFormatString(int flags, bool isHexFmt, char* numFmt, char* fmtString);
    execResult_type printToString(int width, int precision, bool isFmtString, bool isHexFmt, Val* operands, char* fmtString,
        Val& fcnResult, int& charsPrinted);

    void initFunctionDefaultParamVariables(char*& calledFunctionTokenStep, int suppliedArgCount, int paramCount);
    void initFunctionLocalNonParamVariables(char* calledFunctionTokenStep, int paramCount, int localVarCount);

    void makeIntermediateConstant(LE_evalStack* pEvalStackLvl);

    Interpreter::execResult_type arrayAndSubscriptsToarrayElement(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pLeftParStackLvl, int argCount);

    void saveLastValue(bool& overWritePrevious);
    void clearEvalStack();
    void clearEvalStackLevels(int n);
    void clearFlowCtrlStack();

    execResult_type makeFormatString();
    execResult_type deleteVarStringObject(LE_evalStack* pStackLvl);
    execResult_type deleteIntermStringObject(LE_evalStack* pStackLvl);

    execResult_type copyValueArgsFromStack(LE_evalStack*& pStackLvl, int argCount, bool* argIsVar, char* valueType, Val* args, bool passVarRefOrConst = false);

    int findTokenStep(int tokenTypeToFind, char tokenCodeToFind, char*& pStep);
    int jumpTokens(int n, char*& pStep, int& tokenCode);
    int jumpTokens(int n, char*& pStep);
    int jumpTokens(int n);

    void PushTerminalToken(int& tokenType);
    void pushResWord(int& tokenType);
    void pushFunctionName(int& tokenType);
    void pushGenericName(int& tokenType);
    void pushConstant(int& tokenType);
    void pushVariable(int& tokenType);
    void pushIdentifierName(int& tokenType);
};


/***********************************************************
*                       class MyParser                     *
*             parse character string into tokens           *
***********************************************************/

class MyParser {//// naming

    // --------------------
    // *   enumerations   *
    // --------------------

public:

    // unique identification code of a command
    enum cmd_code {
        cmdcod_none,        // no command being executed

        cmdcod_program,
        cmdcod_delete,
        cmdcod_clear,
        cmdcod_vars,
        cmdcod_function,
        cmdcod_static,
        cmdcod_local,
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
        cmdcod_print,
        cmdcod_dispfmt,
        cmdcod_dispmod,
        cmdcod_decCBproc,
        cmdcod_callback,
        cmdcod_test
    };

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

    enum func_code {
        fnccod_varAddress,
        fnccod_varIndirect,
        fnccod_varName,
        fnccod_ifte,
        fnccod_and,
        fnccod_or,
        fnccod_not,
        fnccod_sin,
        fnccod_cos,
        fnccod_tan,
        fnccod_millis,
        fnccod_sqrt,
        fnccod_ubound,
        fnccod_dims,
        fnccod_valueType,
        fnccod_last,
        fnccod_asc,
        fnccod_char,
        fnccod_nl,
        fnccod_fmtNum,
        fnccod_fmtStr,
        fnccod_sysVar
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

    enum parseTokenResult_type {                                // token parsing result
        result_tokenFound = 0,

        // incomplete expression errors
        result_tokenNotFound = 1000,
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
        result_identifierNotAllowedHere,

        // token expected errors
        result_constantValueExpected = 1200,
        result_variableNameExpected,
        result_functionDefExpected,

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
        result_overflow,                // underflow not detected during parsing

        // function errors
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

        // variable errors
        result_varNameInUseForFunction = 1600,
        result_varNotDeclared,
        result_varRedeclared,
        result_varDefinedAsArray,
        result_varDefinedAsScalar,
        result_varControlVarInUse,

        // array errors
        result_arrayDefNoDims = 1700,
        result_arrayDefNegativeDim,
        result_arrayDefMaxDimsExceeded,
        result_arrayDefMaxElementsExceeded,
        result_arrayUseNoDims,
        result_arrayUseWrongDimCount,
        result_arrayParamExpected,
        result_arrayInit_emptyStringExpected,
        result_arrayDimNotValid,

        // command errors 
        result_resWordExpectedAsCmdPar = 1800,
        result_expressionExpectedAsCmdPar,
        result_varWithoutAssignmentExpectedAsCmdPar,
        result_variableExpectedAsCmdPar,
        result_varRefExpectedAsCmdPar,
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

        result_noOpenBlock,
        result_noBlockEnd,
        result_noOpenLoop,
        result_noOpenFunction,
        result_notAllowedInThisOpenBlock,
        result_wrongBlockSequence,

        // other program errors
        result_progMemoryFull = 1000
    };


    // --------------------------
    // *   unions, structures   *
    // --------------------------

    // Note: structures starting with 'LE_' are used to cast list elements for easy handling

    struct CmdBlockDef {                                        // block commands
        char blockType;                                         // block type ('for' block, 'if' block,...)
        char blockPosOrAction;                                     // position of command (reserved word) in block (0=start, 1, 2 = mid, 3=end)
        char blockMinPredecessor;                               // minimum position of previous command (reserved word) for open block
        char blockMaxPredecessor;                               // maximum position
    };

    struct ResWordDef {                                         // reserved words with pattern for parameters (if reserved word is used as command, starting an instruction)
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


private:
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

    // -----------------
    // *   constants   *
    // -----------------

public:

    static constexpr char c_extFunctionFirstOccurFlag = 0x10;     // flag: min > max means not initialized
    static constexpr char c_extFunctionMaxArgs = 0xF;             // must fit in 4 bits

    // these constants are used to check to which token group (or group of token groups) a parsed token belongs
    static constexpr uint8_t lastTokenGroup_0 = 1 << 0;          // operator
    static constexpr uint8_t lastTokenGroup_1 = 1 << 1;          // comma
    static constexpr uint8_t lastTokenGroup_2 = 1 << 2;          // (line start), semicolon, reserved word, generic identifier
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

    // commands parameters: types allowed
    static constexpr uint8_t cmdPar_none = 0;
    static constexpr uint8_t cmdPar_resWord = 1;            // !!! note: reserved words as parameters: not implemented
    static constexpr uint8_t cmdPar_varNameOnly = 2;
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
    static constexpr char cmd_onlyInProgramOutsideFunctionBlock = 0x02;    // command is only allowed insde a program
    static constexpr char cmd_onlyInFunctionBlock = 0x03;               // command is only allowed inside a function block
    static constexpr char cmd_onlyImmediate = 0x04;                   // command is only allowed in immediate mode
    static constexpr char cmd_onlyOutsideFunctionBlock = 0x05;             // command is only allowed outside a function block (so also in immediate mode)
    static constexpr char cmd_onlyImmOrInsideFuncBlock = 0x06;   // command is only allowed inside a function block
    static constexpr char cmd_onlyProgramTop = 0x07;                        // only as first program statement

    // bit b7: skip command during execution
    static constexpr char cmd_skipDuringExec = 0x80;


    // commands (FUNCTION, FOR, ...): allowed command parameters (naming: cmdPar_<n[nnn]> with A'=variable with (optional) assignment, 'E'=expression, 'E'=expression, 'R'=reserved word
    static const char cmdPar_N[4];                             // command takes no parameters
    static const char cmdPar_P[4];
    static const char cmdPar_100[4];
    static const char cmdPar_101[4];
    static const char cmdPar_102[4];
    static const char cmdPar_E[4];
    static const char cmdPar_E_2[4];
    static const char cmdPar_E_3[4];
    static const char cmdPar_E_opt[4];
    static const char cmdPar_E_optMult[4];
    static const char cmdPar_V[4];
    static const char cmdPar_F[4];
    static const char cmdPar_AEE[4];
    static const char cmdPar_I_mult[4];
    static const char cmdPar_AA_mult[4];


private:
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

    // other commands: first value indicates it's not a block command, second value specifies command (last positions not used)
    static constexpr CmdBlockDef cmdProgram{ block_none, cmd_program , block_na , block_na };
    static constexpr CmdBlockDef cmdGlobalVar{ block_none, cmd_globalVar , block_na , block_na };
    static constexpr CmdBlockDef cmdLocalVar{ block_none, cmd_localVar , block_na , block_na };
    static constexpr CmdBlockDef cmdStaticVar{ block_none, cmd_staticVar , block_na , block_na };
    static constexpr CmdBlockDef cmdDeleteVar{ block_none, cmd_deleteVar , block_na , block_na };
    static constexpr CmdBlockDef cmdBlockOther{ block_none, block_na,block_na,block_na };                                   // not a 'block' command

    // used to close any type of currently open inner block
    static constexpr CmdBlockDef cmdBlockGenEnd{ block_genericEnd,block_endPos,block_na,block_endPos };            // all block types: block end 


    // terminals - should NOT start and end with an alphanumeric character or with an underscore
    // note: if a termnal is designated as 'single character', then other terminals should not contain this character
    static constexpr char* term_semicolon = ";";        // must be single character
    static constexpr char* term_comma = ",";            // must be single character
    static constexpr char* term_leftPar = "(";          // must be single character
    static constexpr char* term_rightPar = ")";         // must be single character

public:

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



    static const ResWordDef _resWords[];                       // reserved word names
    static const FuncDef _functions[];                         // function names with min & max arguments allowed
    static const TerminalDef _terminals[];
    static const uint8_t _maxAlphaCstLen{ 60 };                 // max length of character strings, excluding terminating '\0' (also if stored in variables)


    // -----------------
    // *   variables   *
    // -----------------

private:
    bool _isProgramCmd = false;
    bool _isExtFunctionCmd = false;                             // FUNCTION command is being parsed (not the complete function)
    bool _isGlobalOrUserVarCmd = false;                                // VAR command is being parsed
    bool _isLocalVarCmd = false;                                // LOCAL command is being parsed
    bool _isStaticVarCmd = false;                               // STATIC command is being parsed
    bool _isAnyVarCmd = false;                                     // VAR, LOCAL or STATIC command is being parsed
    bool _isDeleteVarCmd = false;

    bool _isDecCBprocCmd = false;
    bool _isCallbackCmd = false;

    bool _varDefAssignmentFound = false;
    bool _leadingSpaceCheck{ false };

    // parsing stack: value supplied when pushing data to stack OR value returned when stack drops 
    char _minFunctionArgs{ 0 };                                // if external function defined prior to call: min & max allowed arguments. Otherwise, counters to keep track of min & max actual arguments in previous calls 
    char _maxFunctionArgs{ 0 };
    int _functionIndex{ 0 };
    int _variableNameIndex{ 0 };
    int _variableScope{ 0 };
    bool _arrayElemAssignmentAllowed{ false };                    // value returned: assignment to array element is allowed next
    bool _arrayElemPostfixIncrDecrAllowed{ false };

    int _tokenIndex{ 0 };
    int _resWordCount;                                          // index into list of reserved words
    int _functionCount;                                         // index into list of internal (intrinsic) functions
    int _terminalCount;


    uint16_t _lastTokenStep, _lastVariableTokenStep;
    uint16_t _blockCmdTokenStep, _blockStartCmdTokenStep;   // pointers to reserved words used as block commands                           
    LE_parsingStack* _pParsingStack;
    LE_parsingStack* _pFunctionDefStack;

    Interpreter::tokenType_type _lastTokenType = Interpreter::tok_no_token;               // type of last token parsed
    Interpreter::tokenType_type _lastTokenType_hold = Interpreter::tok_no_token;
    Interpreter::tokenType_type _previousTokenType = Interpreter::tok_no_token;

    termin_code _lastTermCode;               // type of last token parsed
    termin_code _lastTermCode_hold;
    termin_code _previousTermCode;

    int _lastTokenIsTerminal;
    int _lastTokenIsTerminal_hold;
    int _previousTokenIsTerminal;

    bool _lastTokenIsPrefixOp, _lastTokenIsPostfixOp;
    bool _prefixIncrDecrIsFirstToken;

    Interpreter* _pInterpreter;

public:
    const char* _pCmdAllowedParTypes;
    int _cmdParSpecColumn{ 0 };
    int _cmdArgNo{ 0 };
    int _cmdExprArgTokenNo{ 0 };
    bool _isCommand = false;                                    // a command is being parsed (instruction starting with a reserved word)
    int _parenthesisLevel = 0;                               // current number of open parentheses
    uint8_t _lastTokenGroup_sequenceCheck_bit = 0;                   // bits indicate which token group the last token parsed belongs to          
    bool _extFunctionBlockOpen = false;                         // commands within FUNCTION...END block are being parsed (excluding END command)
    int _blockLevel = 0;                                     // current number of open blocks
    LinkedList parsingStack;                                      // during parsing: linked list keeping track of open parentheses and open blocks


    // ------------------------------------
    // *   methods (doc: see .cpp file)   *
    // ------------------------------------

public:
    bool parseAsResWord(char*& pNext, parseTokenResult_type& result);
    bool parseAsNumber(char*& pNext, parseTokenResult_type& result);
    bool parseAsStringConstant(char*& pNext, parseTokenResult_type& result);
    bool parseTerminalToken(char*& pNext, parseTokenResult_type& result);
    bool parseAsInternFunction(char*& pNext, parseTokenResult_type& result);
    bool parseAsExternFunction(char*& pNext, parseTokenResult_type& result);
    bool parseAsVariable(char*& pNext, parseTokenResult_type& result);
    bool parseAsIdentifierName(char*& pNext, parseTokenResult_type& result);

    bool checkCommandSyntax(parseTokenResult_type& result);
    bool checkExtFunctionArguments(parseTokenResult_type& result, int& minArgCnt, int& maxArgCnt);
    bool checkArrayDimCountAndSize(parseTokenResult_type& result, int* arrayDef_dims, int& dimCnt);
    int getIdentifier(char** pIdentArray, int& identifiersInUse, int maxIdentifiers, char* pIdentNameToCheck, int identLength, bool& createNew, bool isUserVar = false);
    bool checkInternFuncArgArrayPattern(parseTokenResult_type& result);
    bool checkExternFuncArgArrayPattern(parseTokenResult_type& result, bool isFunctionClosingParenthesis);
    bool initVariable(uint16_t varTokenStep, uint16_t constTokenStep);


    MyParser(Interpreter* const pInterpreter);                                                 // constructor
    ~MyParser();                                                 // constructor
    void resetMachine(bool withUserVariables);
    void deleteIdentifierNameObjects(char** pIdentArray, int identifiersInUse, bool isUserVar = false);
    void deleteArrayElementStringObjects(Interpreter::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue, bool isUserVar = false, bool isLocalVar = false);
    void deleteVariableValueObjects(Interpreter::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue, bool isUserVar = false, bool isLocalVar = false);
    void deleteLastValueFiFoStringObjects();
    void deleteConstStringObjects(char* pToken);
    parseTokenResult_type parseSource(char* const inputLine, char*& pErrorPos);
    parseTokenResult_type  parseInstruction(char*& pInputLine);
    void deleteParsedData();
    bool allExternalFunctionsDefined(int& index);
    void prettyPrintInstructions(bool oneInstruction, char* startToken = nullptr, char* errorProgCounter = nullptr, int* sourceErrorPos = nullptr);
    void printParsingResult(parseTokenResult_type result, int funcNotDefIndex, char* const pInputLine, int lineCount, char* const pErrorPos);


};


#endif
