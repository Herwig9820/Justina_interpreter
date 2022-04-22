#include "Justina.h"

#define printCreateDeleteHeapObjects 0


/***********************************************************
*                       class MyParser                     *
*             parse character string into tokens           *
***********************************************************/

// -------------------------------------------------
// *   // initialisation of static class members   *
// -------------------------------------------------

// commands (FUNCTION, FOR, ...): allowed command parameters

const char MyParser::cmdPar_N [4] { cmdPar_none,                    cmdPar_none,                                    cmdPar_none,                                cmdPar_none };
const char MyParser::cmdPar_P [4] { cmdPar_programName,             cmdPar_none,                                    cmdPar_none,                                cmdPar_none };
const char MyParser::cmdPar_E [4] { cmdPar_expression,              cmdPar_none,                                    cmdPar_none,                                cmdPar_none };
const char MyParser::cmdPar_F [4] { cmdPar_extFunction,             cmdPar_none,                                    cmdPar_none,                                cmdPar_none };
const char MyParser::cmdPar_AEE [4] { cmdPar_varOptAssignment,      cmdPar_expression,                              cmdPar_expression | cmdPar_optionalFlag ,   cmdPar_none };
const char MyParser::cmdPar_P_mult [4] { cmdPar_programName,        cmdPar_programName | cmdPar_multipleFlag,       cmdPar_none,                                cmdPar_none };
const char MyParser::cmdPar_AA_mult [4] { cmdPar_varOptAssignment,  cmdPar_varOptAssignment | cmdPar_multipleFlag,  cmdPar_none,                                cmdPar_none };

const char MyParser::cmdPar_test [4] { cmdPar_programName
                                        | cmdPar_optionalFlag,      cmdPar_programName,                             cmdPar_programName | cmdPar_multipleFlag,   cmdPar_none };  // test: either 0 or 2 to n parameters ok

// commands: reserved words

const MyParser::ResWordDef MyParser::_resWords [] {
    //  name        id code         where allowed           padding (boundary alignment)    param spec      control info
    //  ----        -------         -------------           ----------------------------    ----------      ------------   
    {"TEST",    cmdcod_test,    cmd_noRestrictions,                                 0,0,    cmdPar_test,    cmdDeleteVar},

    {"PROGRAM", cmdcod_program, cmd_onlyProgramTop | cmd_skipDuringExec,            0,0,    cmdPar_P,       cmdProgram},
    {"DELETE",  cmdcod_delete,  cmd_onlyImmediate,                                  0,0,    cmdPar_P_mult,  cmdDeleteVar},
    {"CLEAR",   cmdcod_clear,   cmd_onlyImmediate,0,0, cmdPar_N, cmdBlockOther},
    {"VARS",    cmdcod_vars,    cmd_onlyImmediate,                                  0,0,    cmdPar_N,       cmdBlockOther},
    {"FUNCTION",cmdcod_function,cmd_onlyInProgram | cmd_skipDuringExec,             0,0,    cmdPar_F,       cmdBlockExtFunction},

    {"STATIC",  cmdcod_static,  cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_AA_mult, cmdStaticVar},
    {"LOCAL",   cmdcod_local,   cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_AA_mult, cmdLocalVar},
    {"VAR",     cmdcod_var,     cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,  0,0,    cmdPar_AA_mult, cmdGlobalVar},

    {"FOR",     cmdcod_for,     cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_AEE,     cmdBlockFor},
    {"WHILE",   cmdcod_while,   cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_E,       cmdBlockWhile},
    {"IF",      cmdcod_if,      cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_E,       cmdBlockIf},
    {"ELSEIF",  cmdcod_elseif,  cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_E,       cmdBlockIf_elseIf},
    {"ELSE",    cmdcod_else,    cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_N,       cmdBlockIf_else},

    {"BREAK",   cmdcod_break,   cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_N,       cmdBlockOpenBlock_loop},        // allowed if at least one open loop block (any level) 
    {"CONTINUE",cmdcod_continue,cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_N,       cmdBlockOpenBlock_loop },       // allowed if at least one open loop block (any level) 
    {"RETURN",  cmdcod_return,  cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_E,       cmdBlockOpenBlock_function},    // allowed if currently an open function definition block 

    {"END",     cmdcod_end,     cmd_noRestrictions,                                 0,0,    cmdPar_N,       cmdBlockGenEnd},                // closes inner open command block
};

// internal (intrinsic) functions
// the 8 array pattern bits indicate the order of arrays and scalars; bit b0 to bit b7 refer to parameter 1 to 8, if a bit is set, an array is expected as argument
// only the first 8 parameters can be defined as an array parameter

const MyParser::FuncDef MyParser::_functions [] {
    //  name        id code             #par    array pattern
    //  ----        -------             ----    -------------   
    {"varAddress",  fnccod_varAddress,  1,1,    0b0},
    {"varIndirect", fnccod_varIndirect, 1,1,    0b0},
    {"varName",     fnccod_varName,     1,1,    0b0},
    {"ifte",        fnccod_ifte,        3,3,    0b0},
    {"and",         fnccod_and,         1,8,    0b0},
    {"or",          fnccod_or,          1,8,    0b0},
    {"not",         fnccod_not,         1,1,    0b0},
    {"sin",         fnccod_sin,         1,1,    0b0},
    {"cos",         fnccod_cos,         1,1,    0b0},
    {"tan",         fnccod_tan,         1,1,    0b0},
    {"time",        fnccod_time,        0,0,    0b0},
    {"sqrt",        fnccod_sqrt,        1,1,    0b0},
    {"ubound",      fnccod_ubound,      2,2,    0b00000001},
    {"L",           fnccod_l,           0,1,    0b0}
};


// terminal tokens
// priority: bits b7654: priority if prefix operator, b3210: if infix operator or other terminal (0 = lowest, 15 = highest) 
// use and associativity: defines whether terminal can be used as prefix and /or as infix operator; associativity for prefix / infix operators (only)   
 // table sorted by infix operator priority (priority bits 3210)

const MyParser::TerminalDef MyParser::_terminals [] {
    //  name        id code             prio        associativity & use         
    //  ----        -------             ----        -------------------   
    {term_comma,    termcod_comma,      0x00,        0x00},
    {term_semicolon,termcod_semicolon,  0x00,        0x00},
    {term_rightPar, termcod_rightPar,   0x00,        0x00},

    // operators
    {term_assign,   termcod_assign,     0x01,        trm_assocRtoL},
    {term_lt,       termcod_lt,         0x02,        0x00},
    {term_gt,       termcod_gt,         0x02,        0x00},
    {term_eq,       termcod_eq,         0x02,        0x00},
    {term_concat,   termcod_concat,     0x03,        0x00},
    {term_plus,     termcod_plus,       0x64,        trm_assocRtoLasPrefix},
    {term_minus,    termcod_minus,      0x64,        trm_assocRtoLasPrefix},
    {term_mult,     termcod_mult,       0x05,        0x00},
    {term_div,      termcod_div,        0x05,        0x00},
    {term_pow,      termcod_pow,        0x07,        trm_assocRtoL},
    {term_ltoe,     termcod_ltoe,       0x02,        0x00},
    {term_gtoe,     termcod_gtoe,       0x02,        0x00},
    {term_neq,      termcod_ne,         0x02,        0x00},

    {term_leftPar,  termcod_leftPar,    0x08,        0x00},
};


// -------------------
// *   constructor   *
// -------------------

MyParser::MyParser( Interpreter* const pcalculator ) : _pcalculator( pcalculator ) {
    _resWordCount = (sizeof( _resWords )) / sizeof( _resWords [0] );
    _functionCount = (sizeof( _functions )) / sizeof( _functions [0] );
    _terminalCount = (sizeof( _terminals )) / sizeof( _terminals [0] );

    _blockLevel = 0;
    _extFunctionBlockOpen = false;
}


// ---------------------
// *   deconstructor   *
// ---------------------

MyParser::~MyParser() {
    resetMachine( true );             // delete all objects created on the heap
}


// -----------------------------------------------------------------------------------------
// *   delete all identifier names (char strings)                                          *
// *   note: this excludes UNQUALIFIED identifier names stored as alphanumeric constants   *
// -----------------------------------------------------------------------------------------

void MyParser::deleteIdentifierNameObjects( char** pIdentNameArray, int identifiersInUse ) {
    int index = 0;          // points to last variable in use
    while ( index < identifiersInUse ) {                       // points to variable in use
#if printCreateDeleteHeapObjects
        Serial.print( "(HEAP) Delete heap object # " ); Serial.print( _heapObjectCount ); Serial.print( ": identifier name, addr " );
        Serial.println( (uint32_t) * (pIdentNameArray + index) - RAMSTART );
#endif
        delete [] * (pIdentNameArray + index);
        _heapObjectCount--;
        index++;
    }
}


// ----------------------------------------------------------------------------------------------
// *   delete variable heap objects: scalar variable strings and array variable array storage   *
// ----------------------------------------------------------------------------------------------

void MyParser::deleteArrayElementStringObjects( Interpreter::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue ) {
    int index = 0;
    while ( index < varNameCount ) {
        if ( !checkIfGlobalValue || (varType [index] & (_pcalculator->var_hasGlobalValue)) ) { // global value ?
            if ( (varType [index] & (_pcalculator->var_isArray | _pcalculator->var_isStringPointer)) ==
                (_pcalculator->var_isArray | _pcalculator->var_isStringPointer) ) {              // array of strings

                void* pArrayStorage = varValues [index].pArray;        // void pointer to an array of string pointers; element 0 contains dimensions and dimension count
                int dimensions = (((char*) pArrayStorage) [3]);  // can range from 1 to MAX_ARRAY_DIMS
                int arrayElements = 1;                                  // determine array size
                for ( int dimCnt = 0; dimCnt < dimensions; dimCnt++ ) { arrayElements *= (int) ((((char*) pArrayStorage) [dimCnt])); }

                // delete non-empty strings
                for ( int arrayElem = 1; arrayElem <= arrayElements; arrayElem++ ) {  // array element 0 contains dimensions and count
                    char* pString = ((char**) pArrayStorage) [arrayElem];
                    uint32_t stringPointerAddress = (uint32_t) & (((char**) pArrayStorage) [arrayElem]);
                    if ( pString != nullptr ) {
#if printCreateDeleteHeapObjects
                        Serial.print( "(HEAP) Delete heap object # " ); Serial.print( _heapObjectCount ); Serial.print( ": array element string value, addr " );
                        Serial.println( (uint32_t) pString - RAMSTART );     // applicable to string and array (same pointer)
#endif
                        delete []  pString;                                  // applicable to string and array (same pointer)
                        _heapObjectCount--;
                    }
                }
            }
        }
        index++;
    }
}


// ----------------------------------------------------------------------------------------------
// *   delete variable heap objects: scalar variable strings and array variable array storage   *
// ----------------------------------------------------------------------------------------------

// note: make sure array variable element string objects have been deleted prior to calling this routine

void MyParser::deleteVariableValueObjects( Interpreter::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue ) {
    int index = 0;
    while ( index < varNameCount ) {
        if ( !checkIfGlobalValue || (varType [index] & (_pcalculator->var_hasGlobalValue)) ) { // global value ?
            if ( varType [index] & (_pcalculator->var_isArray | _pcalculator->var_isStringPointer) ) {              // scalar string or array (of strings or floats)
#if printCreateDeleteHeapObjects
                Serial.print( "(HEAP) Delete heap object # " ); Serial.print( _heapObjectCount ); Serial.print( ": variable string value or array storage, addr " );
                Serial.println( (uint32_t) varValues [index].pStringConst - RAMSTART );     // applicable to string and array (same pointer)
#endif
                delete []  varValues [index].pStringConst;                                  // applicable to string and array (same pointer)
                _heapObjectCount--;
            }
        }
        index++;
    }
}


// -----------------------------------------------------------------------------------------
// *   delete all alphanumeric constant value heap objects                                 *
// *   note: this includes UNQUALIFIED identifier names stored as alphanumeric constants   *
// -----------------------------------------------------------------------------------------

// must be called before deleting tokens (list elements) 

void MyParser::deleteConstStringObjects( char* programStart ) {
    char* pAnum;
    Interpreter::TokenPointer prgmCnt;
    prgmCnt.pTokenChars = programStart;
    uint8_t tokenType = *prgmCnt.pTokenChars & 0x0F;
    while ( tokenType != '\0' ) {                                                                    // for all tokens in token list
        if ( (tokenType == Interpreter::tok_isStringConst) || (tokenType == Interpreter::tok_isGenericName) ) {
#if printCreateDeleteHeapObjects
            memcpy( &pAnum, prgmCnt.pAnumP->pStringConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            Serial.print( "(HEAP) Delete heap object # " ); Serial.print( _heapObjectCount ); Serial.print( ": string const value, addr " );
            Serial.println( (uint32_t) pAnum - RAMSTART );
#endif
            delete [] pAnum;
            _heapObjectCount--;
        }
        uint8_t tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*prgmCnt.pTokenChars >> 4) & 0x0F;
        prgmCnt.pTokenChars += tokenLength;
        tokenType = *prgmCnt.pTokenChars & 0x0F;
    }
}


// --------------------
// *   reset parser   *
// --------------------

void MyParser::resetMachine( bool withUserVariables ) {
    // delete identifier name objects on the heap (variable names, external function names) 
    deleteIdentifierNameObjects( _pcalculator->programVarNames, _pcalculator->_programVarNameCount );
    deleteIdentifierNameObjects( _pcalculator->extFunctionNames, _pcalculator->_extFunctionCount );
    if ( withUserVariables ) { deleteIdentifierNameObjects( _pcalculator->userVarNames, _pcalculator->_userVarCount ); }

    // delete variable heap objects: array variable element string objects
    deleteArrayElementStringObjects( _pcalculator->globalVarValues, _pcalculator->globalVarType, _pcalculator->_programVarNameCount, true );
    deleteArrayElementStringObjects( _pcalculator->staticVarValues, _pcalculator->staticVarType, _pcalculator->_staticVarCount, false );
    if ( withUserVariables ) { deleteArrayElementStringObjects( _pcalculator->userVarValues, _pcalculator->userVarType, _pcalculator->_userVarCount, false ); }

    // delete variable heap objects: scalar variable strings and array variable array storage 
    deleteVariableValueObjects( _pcalculator->globalVarValues, _pcalculator->globalVarType, _pcalculator->_programVarNameCount, true );
    deleteVariableValueObjects( _pcalculator->staticVarValues, _pcalculator->staticVarType, _pcalculator->_staticVarCount, false );
    if ( withUserVariables ) { deleteVariableValueObjects( _pcalculator->userVarValues, _pcalculator->userVarType, _pcalculator->_userVarCount, false ); }

    // delete alphanumeric constants: before clearing program memory
    deleteConstStringObjects( _pcalculator->_programStorage );
    deleteConstStringObjects( _pcalculator->_programStorage + _pcalculator->PROG_MEM_SIZE );

    parsingStack.deleteList();                                                               // delete list to keep track of open parentheses and open command blocks
    _blockLevel = 0;
    _extFunctionBlockOpen = false;

    // init calculator variables: AFTER deleting heap objects
    _pcalculator->_programVarNameCount = 0;
    _pcalculator->_staticVarCount = 0;
    _pcalculator->_localVarCountInFunction = 0;
    _pcalculator->_paramOnlyCountInFunction = 0;
    _pcalculator->_extFunctionCount = 0;
    if ( withUserVariables ) { _pcalculator->_userVarCount = 0; }
    else {
        int index = 0;          // clear user variable flag 'variable is used by program'
        while ( index++ < _pcalculator->_userVarCount ) { _pcalculator->userVarType [index] = _pcalculator->userVarType [index] & ~_pcalculator->var_userVarUsedByProgram; }
    }

    _pcalculator->_programStart = _pcalculator->_programStorage + (_pcalculator->_programMode ? 0 : _pcalculator->PROG_MEM_SIZE);
    _pcalculator->_programSize = _pcalculator->_programSize + (_pcalculator->_programMode ? _pcalculator->PROG_MEM_SIZE : _pcalculator->IMM_MEM_SIZE);
    _pcalculator->_programCounter = _pcalculator->_programStart;                          // start of 'immediate mode' program area

    *_pcalculator->_programStorage = '\0';                                    //  current end of program 
    *_pcalculator->_programStart = '\0';                                      //  current end of program (immediate mode)

    //// test
    if ( withUserVariables && (_heapObjectCount != 0) ) { Serial.print( "\r\n\r\n****************************\r\nmissing object deletes: " ); Serial.println( _heapObjectCount ); } ////
}


// -------------------------------------------------------------------------
// *   check if identifier storage exists already, optionally create new   *
// -------------------------------------------------------------------------

int MyParser::getIdentifier( char** pIdentNameArray, int& identifiersInUse, int maxIdentifiers, char* pIdentNameToCheck, int identLength, bool& createNewName ) {

    char* pIdentifierName;
    int index = 0;          // points to last variable in use
    while ( index < identifiersInUse ) {                       // points to variable in use
        pIdentifierName = pIdentNameArray [index];
        if ( strlen( pIdentifierName ) == identLength ) {                                    // identifier with name with same length found
            if ( strncmp( pIdentifierName, pIdentNameToCheck, identLength ) == 0 ) { break; } // storage for identifier name was created already 
        }
        index++;
    }
    if ( index == identifiersInUse ) { index = -1; }                                  // not found
    if ( !createNewName ) { return index; }                                                 // if check only: identNameIndex to identifier name or -1, createNewName = false

    createNewName = (index == -1);                                       // create new ?

    // create new identifier if it does not exist yet ?
    // upon return, createNew indicates whether new identifier storage NEEDED to be created ...
    // and if it was possible, identifiersInUse will be set to the new identifier count

    if ( createNewName ) {
        if ( identifiersInUse == maxIdentifiers ) { return index; }                // create identifier name failed: return -1 with createNewName = true

        pIdentifierName = new char [_maxIdentifierNameLen + 1 + 1];                      // create standard length char array on the heap, including '\0' and an extra character 
        _heapObjectCount++;
#if printCreateDeleteHeapObjects
        Serial.print( "(HEAP) Create object # " ); Serial.print( _heapObjectCount ); Serial.print( ": identifier name, addr " );
        Serial.println( (uint32_t) pIdentifierName - RAMSTART );
#endif
        strncpy( pIdentifierName, pIdentNameToCheck, identLength );                            // store identifier name in newly created character array
        pIdentifierName [identLength] = '\0';                                                 // string terminating '\0'
        pIdentNameArray [identifiersInUse] = pIdentifierName;
        identifiersInUse++;
        return identifiersInUse - 1;                                                   // identNameIndex to newly created identifier name
    }
}

// --------------------------------------------------------------
// *   initialize a variable or an array with (a) constant(s)   *
// --------------------------------------------------------------

bool MyParser::initVariable( uint16_t varTokenStep, uint16_t constTokenStep ) {
    float f { 0. };        // last token is a number constant: dimension spec
    char* pString { nullptr };

    // parsing: initialize variables and arrays with a constant number or (arrays: empty) string

    // fetch variable location and attributes
    bool isArrayVar = ((Interpreter::TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identInfo & _pcalculator->var_isArray;
    bool isGlobalVar = (((Interpreter::TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isGlobal;
    bool isUserVar = (((Interpreter::TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isUser;
    int varValueIndex = ((Interpreter::TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identValueIndex;
    void* pVarStorage = isGlobalVar ? _pcalculator->globalVarValues : isUserVar ? _pcalculator->userVarValues : _pcalculator->staticVarValues;
    char* pVarTypeStorage = isGlobalVar ? _pcalculator->globalVarType : isUserVar ? _pcalculator->userVarType : _pcalculator->staticVarType;
    void* pArrayStorage;        // array storage (if array) 

    // fetch constant (numeric or alphanumeric) 
    bool isNumberCst = (Interpreter::tokenType_type) ((((Interpreter::TokenIsRealCst*) (_pcalculator->_programStorage + constTokenStep))->tokenType) & 0x0F) == Interpreter::tok_isRealConst;
    if ( isNumberCst ) { memcpy( &f, ((Interpreter::TokenIsRealCst*) (_pcalculator->_programStorage + constTokenStep))->realConst, sizeof( f ) ); }

    else { memcpy( &pString, ((Interpreter::TokenIsStringCst*) (_pcalculator->_programStorage + constTokenStep))->pStringConst, sizeof( pString ) ); }
    int length = isNumberCst ? 0 : strlen( pString );

    if ( isArrayVar ) {
        pArrayStorage = ((void**) pVarStorage) [varValueIndex];        // void pointer to an array 
        int dimensions = (((char*) pArrayStorage) [3]);  // can range from 1 to MAX_ARRAY_DIMS
        int arrayElements = 1;                                  // determine array size
        for ( int dimCnt = 0; dimCnt < dimensions; dimCnt++ ) { arrayElements *= (int) ((((char*) pArrayStorage) [dimCnt])); }
        // fill up with numeric constants or (empty strings:) null pointers
        if ( isNumberCst ) { for ( int arrayElem = 1; arrayElem <= arrayElements; arrayElem++ ) { ((float*) pArrayStorage) [arrayElem] = f; } }
        else {                                                      // alphanumeric constant
            if ( length != 0 ) { return false; };       // to limit memory usage, no mass initialisation with non-empty strings
            for ( int arrayElem = 1; arrayElem <= arrayElements; arrayElem++ ) { ((char**) pArrayStorage) [arrayElem] = nullptr; }
        }
    }

    else {                                  // scalar
        if ( isNumberCst ) {
            ((float*) pVarStorage) [varValueIndex] = f;
        }      // store numeric constant
        else {                                                  // alphanumeric constant
            if ( length == 0 ) {
                ((char**) pVarStorage) [varValueIndex] = nullptr;       // an empty string does not create a heap object
            }
            else { // create string object and store string
                char* pVarAlphanumValue = new char [length + 1];                  // create char array on the heap to store alphanumeric constant, including terminating '\0'
                _heapObjectCount++;
#if printCreateDeleteHeapObjects
                Serial.print( "(HEAP) Create object # " ); Serial.print( _heapObjectCount ); Serial.print( ": variable string value, addr " );
                Serial.println( (uint32_t) pVarAlphanumValue - RAMSTART );
#endif
                // store alphanumeric constant in newly created character array
                strcpy( pVarAlphanumValue, pString );              // including terminating \0
                ((char**) pVarStorage) [varValueIndex] = pVarAlphanumValue;       // store pointer to string
            }
        }
    }
    pVarTypeStorage [varValueIndex] = (pVarTypeStorage [varValueIndex] & ~_pcalculator->var_typeMask) | (isNumberCst ? _pcalculator->var_isFloat : _pcalculator->var_isStringPointer);
    return true;
};


// --------------------------------------------------------------
// *   check if all external functions referenced are defined   *
// --------------------------------------------------------------

bool MyParser::allExternalFunctionsDefined( int& index ) {
    index = 0;
    while ( index < _pcalculator->_extFunctionCount ) {                       // points to variable in use
        if ( _pcalculator->extFunctionData [index].pExtFunctionStartToken == nullptr ) { return false; }
        index++;
    }
    return true;
}


// ----------------------------------------------------------------------------------------------------------------------
// *   parse ONE instruction in a character string, ended by an optional ';' character and a '\0' mandatary character   *
// ----------------------------------------------------------------------------------------------------------------------

MyParser::parseTokenResult_type MyParser::parseInstruction( char*& pInputStart ) {
    _lastTokenType_hold = Interpreter::tok_no_token;
    _lastTokenType = Interpreter::tok_no_token;                                                      // no token yet
    _lastTokenIsTerminal = false;

    _parenthesisLevel = 0;
    _isProgramCmd = false;
    _isExtFunctionCmd = false;
    _isGlobalOrUserVarCmd = false;
    _isLocalVarCmd = false;
    _isStaticVarCmd = false;
    _isAnyVarCmd = false;
    _isDeleteVarCmd = false;
    _isCommand = false;

    parseTokenResult_type result = result_tokenFound;                                   // possible error will be determined during parsing 
    Interpreter::tokenType_type& t = _lastTokenType;
    char* pNext = pInputStart;                                                          // set to first character in instruction
    char* pNext_hold = pNext;

    do {                                                                                // parse ONE token in an instruction
        bool isLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;
        bool isRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;
        bool isComma = _lastTokenIsTerminal ? (_lastTermCode == termcod_comma) : false;
        bool isSemicolon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
        bool isOperator = _lastTokenIsTerminal ? (_lastTermCode <= termcod_opRangeEnd) : false;

        // determine token group of last token parsed (bits b4 to b0): this defines which tokens are allowed as next token
        _lastTokenGroup_sequenceCheck_bit = (isOperator || isComma) ? lastTokenGroup_0 :
            ((t == Interpreter::tok_no_token) || isSemicolon || (t == Interpreter::tok_isReservedWord)) ? lastTokenGroup_1 :
            ((t == Interpreter::tok_isRealConst) || (t == Interpreter::tok_isStringConst) || isRightPar) ? lastTokenGroup_2 :
            ((t == Interpreter::tok_isInternFunction) || (t == Interpreter::tok_isExternFunction)) ? lastTokenGroup_3 :
            isLeftPar ? lastTokenGroup_4 : lastTokenGroup_5;     // token group 5: scalar or array variable name


        // a space may be required between last token and next token (not yet known), if one of them is a reserved word
        // and the other token is either a reserved word, an alphanumeric constant or a parenthesis
        // space check result is OK if a check is not required or if a space is present anyway
        _leadingSpaceCheck = ((t == Interpreter::tok_isReservedWord) || (t == Interpreter::tok_isStringConst) || isRightPar) && (pNext [0] != ' ');

        // move to the first character of next token (within one instruction)
        while ( pNext [0] == ' ' ) { pNext++; }                                         // skip leading spaces
        if ( pNext [0] == '\0' ) { break; }                                              // safety: instruction was not ended by a semicolon (should never happen) 

        // parsing routines below try to parse characters as a token of a specific type
        // if a function returns true, then either proceed OR skip reminder of loop ('continue') if 'result' indicates a token has been found
        // if a function returns false, then break with 'result' containing the error

        _previousTokenType = _lastTokenType_hold;                                   // remember the second last parsed token during parsing of a next token
        _previousTermCode = _lastTermCode_hold;                                     // only relevant for certain tokens
        _previousTokenIsTerminal = _lastTokenIsTerminal_hold;

        _lastTokenType_hold = _lastTokenType;                                       // remember the last parsed token during parsing of a next token
        _lastTermCode_hold = _lastTermCode;                                         // only relevant for certain tokens
        _lastTokenIsTerminal_hold = _lastTokenIsTerminal;

        pNext_hold = pNext;

        do {                                                                                                                // one loop only
            if ( (_pcalculator->_programCounter + sizeof( Interpreter::TokenIsStringCst ) + 1) > (_pcalculator->_programStart + _pcalculator->_programSize) ) { result = result_progMemoryFull; break; };
            if ( !parseAsResWord( pNext, result ) ) { break; } if ( result == result_tokenFound ) { continue; }             // check before checking for identifier  
            if ( !parseTerminalToken( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }       // check before checking for number
            if ( !parseAsNumber( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }
            if ( !parseAsStringConstant( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }
            if ( !parseAsInternFunction( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }     // check before checking for identifier (ext. function / variable) 
            if ( !parseAsExternFunction( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }     // check before checking for variable
            if ( !parseAsVariable( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }
            if ( !parseAsIdentifierName( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }     // at the end
            result = result_token_not_recognised;
        } while ( false );

        // one token parsed (or error)
        if ( result != result_tokenFound ) { break; }                                   // exit loop if token error (syntax, ...). Checked before checking command syntax
        if ( !checkCommandSyntax( result ) ) { pNext = pNext_hold; break; }             // exit loop if command syntax error (pNext altered: set correctly again)

    } while ( true );

    // one instruction parsed (or error: no token found OR command syntax error OR semicolon encountered): quit
    pInputStart = pNext;                                                                // set to next character (if error: indicates error position)
    return result;
}


// --------------------------------------------------------------------------------------------
// *   if instruction is a command (starting with a reserved word): apply additional checks   *
// *   this check is applied AFTER parsing each token and checking its syntax                 *
// --------------------------------------------------------------------------------------------

bool MyParser::checkCommandSyntax( parseTokenResult_type& result ) {                    // command syntax checks
    static Interpreter::tokenType_type cmdSecondLastTokenType = Interpreter::tok_no_token;                           // type of last token parsed
    static int cmdSecondLastTokenIndex = 0;
    static bool cmdSecondLastIsLvl0CommaSep = false;
    static bool isSecondExpressionToken = false;
    static bool expressionStartsWithVariable = false;
    static bool expressionStartsWithArrayVar = false;
    static bool expressionStartsWithGenericName = false;
    static bool isExpression = false;
    static uint8_t allowedParType = cmdPar_none;                                         // init

    // is the start of a new command ? Check token preceding the last parsed token 
    bool isInstructionStart = (_lastTokenType_hold == Interpreter::tok_no_token) || (_lastTokenIsTerminal_hold ? (_lastTermCode_hold == termcod_semicolon) : false);

    if ( isInstructionStart ) {
        _isCommand = (_lastTokenType == Interpreter::tok_isReservedWord);                            // reserved word at start of instruction ? is a command
        _varDefAssignmentFound = false;

        // start of a command ?
        // --------------------

        if ( _isCommand ) {
            _pCmdAllowedParTypes = _resWords [_tokenIndex].pCmdAllowedParTypes;         // remember allowed parameter types
            _commandParNo = 0;                                                          // reset actual command parameter counter

            isExpression = false;
            expressionStartsWithVariable = false;                                               // scalar or array
            expressionStartsWithArrayVar = false;
            expressionStartsWithGenericName = false;

            cmdSecondLastTokenType = Interpreter::tok_isReservedWord;                              // ini: token sequence within current command (command parameters)
            cmdSecondLastTokenIndex = 0;
            cmdSecondLastIsLvl0CommaSep = false;
            isSecondExpressionToken = false;

            // determine command and where allowed
            CmdBlockDef cmdBlockDef = _resWords [_tokenIndex].cmdBlockDef;

            _isExtFunctionCmd = _resWords [_tokenIndex].resWordCode == cmdcod_function;
            _isProgramCmd = _resWords [_tokenIndex].resWordCode == cmdcod_program;
            _isGlobalOrUserVarCmd = _resWords [_tokenIndex].resWordCode == cmdcod_var;
            _isLocalVarCmd = _resWords [_tokenIndex].resWordCode == cmdcod_local;
            _isStaticVarCmd = _resWords [_tokenIndex].resWordCode == cmdcod_static;
            _isDeleteVarCmd = _resWords [_tokenIndex].resWordCode == cmdcod_delete;

            _isAnyVarCmd = _isGlobalOrUserVarCmd || _isLocalVarCmd || _isStaticVarCmd;      //  VAR, LOCAL, STATIC

            // is command allowed here ? Check restrictions
            char cmdRestriction = _resWords [_tokenIndex].restrictions & cmd_usageRestrictionMask;
            if ( cmdRestriction == cmd_onlyProgramTop ) {
                if ( _lastTokenStep != 0 ) { result = result_onlyProgramStart; return false; }
            }
            else {
                if ( _lastTokenStep == 0 ) { result = result_programCmdMissing; return false; }
            }
            if ( _pcalculator->_programMode && (cmdRestriction == cmd_onlyImmediate) ) { result = result_onlyImmediateMode; return false; }
            if ( !_pcalculator->_programMode && (cmdRestriction == cmd_onlyInProgram) ) { result = result_onlyInsideProgram; return false; }
            if ( !_extFunctionBlockOpen && (cmdRestriction == cmd_onlyInFunctionBlock) ) { result = result_onlyInsideFunction; return false; }
            if ( _extFunctionBlockOpen && (cmdRestriction == cmd_onlyOutsideFunctionBlock) ) { result = result_onlyOutsideFunction; return false; }
            if ( ((!_pcalculator->_programMode) || _extFunctionBlockOpen) && (cmdRestriction == cmd_onlyInProgramOutsideFunctionBlock) ) { result = result_onlyInProgOutsideFunction; return false; };
            if ( (_pcalculator->_programMode && !_extFunctionBlockOpen) && (cmdRestriction == cmd_onlyImmOrInsideFuncBlock) ) { result = result_onlyImmediateOrInFunction; return false; };

            if ( _extFunctionBlockOpen && _isExtFunctionCmd ) { result = result_functionDefsCannotBeNested; return false; } // separate message to indicate 'no nesting'

            // not a block command: nothing more to do here 
            if ( cmdBlockDef.blockType == block_none ) { return true; }

            if ( cmdBlockDef.blockPosOrAction == block_startPos ) {                        // is a block start command ?                          
                _blockLevel++;                                                          // increment stack counter and create corresponding list element
                _pParsingStack = (LE_parsingStack*) parsingStack.appendListElement( sizeof( LE_parsingStack ) );
                _pParsingStack->openBlock.cmdBlockDef = cmdBlockDef;                // store in stack: block type, block position (start), n/a, n/a
                memcpy( _pParsingStack->openBlock.tokenStep, &_lastTokenStep, sizeof( char [2] ) );                      // store in stack: pointer to block start command token of open block
                _blockStartCmdTokenStep = _lastTokenStep;                                     // remember pointer to block start command token of open block
                _blockCmdTokenStep = _lastTokenStep;                                          // remember pointer to last block command token of open block
                _extFunctionBlockOpen = _extFunctionBlockOpen || _isExtFunctionCmd;    // open until block closing END command     
                return true;                                                         // nothing more to do
            }

            if ( _blockLevel == 0 ) { result = result_noOpenBlock; return false; }      // not a block start and no open block: error

            if ( (cmdBlockDef.blockType == block_alterFlow) && (_blockLevel > 0) ) {
                // check for a compatible open block (e.g. a BREAK command can only occur if at least one open loop block exists)
                // parenthesis level is zero, because this is a block start command -> all stack levels are block levels
                LE_parsingStack* pStackLvl = _pParsingStack;                                   // start with current open block level
                while ( pStackLvl != nullptr ) {
                    if ( (pStackLvl->openBlock.cmdBlockDef.blockType == block_extFunction) &&   // an open external function block has been found (call or definition)
                        (cmdBlockDef.blockPosOrAction == block_inOpenFunctionBlock) ) {                // and current flow altering command is allowed in open function block
                        // store pointer from 'alter flow' token (command) to block start command token of compatible open block (from RETURN to FUNCTION token)
                        memcpy( ((Interpreter::TokenIsResWord*) (_pcalculator->_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.tokenStep, sizeof( char [2] ) );
                        break;                                                                      // -> applicable open block level found
                    }
                    if ( ((pStackLvl->openBlock.cmdBlockDef.blockType == block_for) ||
                        (pStackLvl->openBlock.cmdBlockDef.blockType == block_while)) &&         // an open loop block has been found (e.g. FOR ... END block)
                        (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock) ) {                    // and current flow altering command is allowed in open loop block
                        // store pointer from 'alter flow' token (command) to block start command token of compatible open block (e.g. from BREAK to FOR token)
                        memcpy( ((Interpreter::TokenIsResWord*) (_pcalculator->_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.tokenStep, sizeof( char [2] ) );
                        break;                                                                      // -> applicable open block level found
                    }
                    pStackLvl = (LE_parsingStack*) parsingStack.getPrevListElement( pStackLvl );
                }
                if ( pStackLvl == nullptr ) { result = (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock) ? result_noOpenLoop : result_noOpenFunction; }
                return (pStackLvl != nullptr);
            }

            if ( (cmdBlockDef.blockType != _pParsingStack->openBlock.cmdBlockDef.blockType) &&    // same block type as open block (or block type is generic block end) ?
                (cmdBlockDef.blockType != block_genericEnd) ) {
                result = result_notAllowedInThisOpenBlock; return false;                // wrong block type: error
            }

            bool withinRange = (_pParsingStack->openBlock.cmdBlockDef.blockPosOrAction >= cmdBlockDef.blockMinPredecessor) &&     // sequence of block commands OK ?
                (_pParsingStack->openBlock.cmdBlockDef.blockPosOrAction <= cmdBlockDef.blockMaxPredecessor);
            if ( !withinRange ) { result = result_wrongBlockSequence; return false; }   // sequence of block commands (for current stack level) is not OK: error

            // pointer from previous open block token to this open block token (e.g. pointer from IF token to ELSEIF or ELSE token)
            memcpy( ((Interpreter::TokenIsResWord*) (_pcalculator->_programStorage + _blockCmdTokenStep))->toTokenStep, &_lastTokenStep, sizeof( char [2] ) );
            _blockCmdTokenStep = _lastTokenStep;                                              // remember pointer to last block command token of open block

            if ( cmdBlockDef.blockPosOrAction == block_endPos ) {                          // is this a block END command token ? 
                if ( _pParsingStack->openBlock.cmdBlockDef.blockType == block_extFunction ) { _extFunctionBlockOpen = false; }       // FUNCTON definition blocks cannot be nested
                memcpy( ((Interpreter::TokenIsResWord*) (_pcalculator->_programStorage + _lastTokenStep))->toTokenStep, &_blockStartCmdTokenStep, sizeof( char [2] ) );
                parsingStack.deleteListElement( nullptr );                                   // decrement stack counter and delete corresponding list element

                _blockLevel--;                                                          // also set pointer to currently last element in stack (if it exists)
                if ( _blockLevel + _parenthesisLevel > 0 ) { _pParsingStack = (LE_parsingStack*) parsingStack.getLastListElement(); }
                if ( _blockLevel > 0 ) {
                    // retrieve pointer to block start command token and last block command token of open block
                    memcpy( &_blockStartCmdTokenStep, _pParsingStack->openBlock.tokenStep, sizeof( char [2] ) );         // pointer to block start command token of open block       
                    uint16_t tokenStep = _blockStartCmdTokenStep;                            // init pointer to last block command token of open block
                    uint16_t tokenStepPointedTo;
                    memcpy( &tokenStepPointedTo, ((Interpreter::TokenIsResWord*) (_pcalculator->_programStorage + tokenStep))->toTokenStep, sizeof( char [2] ) );
                    while ( tokenStepPointedTo != 0xFFFF )
                    {
                        tokenStep = tokenStepPointedTo;
                        memcpy( &tokenStepPointedTo, ((Interpreter::TokenIsResWord*) (_pcalculator->_programStorage + tokenStep))->toTokenStep, sizeof( char [2] ) );
                    }

                    _blockCmdTokenStep = tokenStep;                                        // pointer to last block command token of open block                       
                }
            }
            else { _pParsingStack->openBlock.cmdBlockDef = cmdBlockDef; }           // overwrite (block type (same or generic end), position, min & max predecessor)

            return true;
        };
    }


    // parsing a command parameter right now ? 
    // ---------------------------------------

    if ( !_isCommand ) { return true; }                                                 // not within a command                                                

    // parsing a command parameter: apply additional command syntax rules

    bool isResWord = (_lastTokenType == Interpreter::tok_isReservedWord);

    bool isSemiColonSep = _lastTokenIsTerminal ? (_terminals [_tokenIndex].terminalCode == termcod_semicolon) : false;
    bool isLeftParenthesis = _lastTokenIsTerminal ? (_terminals [_tokenIndex].terminalCode == termcod_leftPar) : false;
    bool isLvl0CommaSep = _lastTokenIsTerminal ? ((_terminals [_tokenIndex].terminalCode == termcod_comma) && (_parenthesisLevel == 0)) : false;
    bool isAssignmentOp = _lastTokenIsTerminal ? (_terminals [_tokenIndex].terminalCode == termcod_assign) : false;
    bool isNonAssignmentOp = _lastTokenIsTerminal ? (((_terminals [_tokenIndex].terminalCode <= termcod_opRangeEnd)) && (_terminals [_tokenIndex].terminalCode != termcod_assign)) : false;

    bool isExpressionFirstToken = (!isResWord) && ((cmdSecondLastTokenType == Interpreter::tok_isReservedWord) || (cmdSecondLastIsLvl0CommaSep));

    if ( isResWord || (isLvl0CommaSep) ) {
        isExpression = false; expressionStartsWithVariable = false; expressionStartsWithArrayVar = false;
        expressionStartsWithGenericName = false;
    }
    if ( isExpressionFirstToken ) {
        isExpression = true;
        if ( _lastTokenType == Interpreter::tok_isVariable ) {
            expressionStartsWithVariable = true;
            expressionStartsWithArrayVar = true;
        }
        else if ( _lastTokenType == Interpreter::tok_isGenericName ) { expressionStartsWithGenericName = true; }
    }

    if ( expressionStartsWithVariable && isLeftParenthesis && isSecondExpressionToken ) { expressionStartsWithArrayVar = true; }

    // check whether an assignment operator is ok as a next token (still to parse)
    _varDefAssignmentFound = false;

    // if first token of a command parameter or a semicolon: check command parameter count  
    bool multipleParameter = false, optionalParameter = false;
    if ( isResWord || isExpressionFirstToken || isSemiColonSep ) {
        allowedParType = (_commandParNo == sizeof( _pCmdAllowedParTypes )) ? cmdPar_none : (uint8_t) (_pCmdAllowedParTypes [_commandParNo]);
        multipleParameter = (allowedParType & cmdPar_multipleFlag);
        optionalParameter = (allowedParType & cmdPar_optionalFlag);
        if ( !multipleParameter ) { _commandParNo++; }                                   // increase parameter count, unless multiple parameters of this type are accepted  
        allowedParType = allowedParType & ~cmdPar_flagMask;
    }

    if ( isSemiColonSep ) {                                                             // semicolon: end of command                                                    
        if ( (allowedParType != cmdPar_none) && !multipleParameter && !optionalParameter ) {    // missing parameters ?
            result = result_cmdParameterMissing; return false;
        }

        _isProgramCmd = false;
        _isExtFunctionCmd = false;
        _isAnyVarCmd = false;
        _isGlobalOrUserVarCmd = false;
        _isLocalVarCmd = false;
        _isStaticVarCmd = false;
        _isDeleteVarCmd = false;
        return true;                                                                    // nothing more to do for this command
    }

    // if command parameter first token: check parameter validity (skip block if not first token) 
    if ( isResWord || isExpressionFirstToken ) {
        if ( allowedParType == cmdPar_none ) {
            result = result_cmdHasTooManyParameters; return false;
        }

        else if ( allowedParType == cmdPar_resWord ) {
            if ( !isResWord ) {
                result = result_resWordExpectedAsCmdPar; return false;
            }
        }
        else if ( (allowedParType == cmdPar_varNameOnly) || (allowedParType == cmdPar_varOptAssignment) ) {
            if ( !expressionStartsWithVariable ) {                                                  // variable can be array as well
                result = result_variableExpectedAsCmdPar; return false;
            }
        }

        else if ( allowedParType == cmdPar_expression ) {
            if ( isResWord || expressionStartsWithGenericName ) { result = result_expressionExpectedAsCmdPar; return false; }
        }

        else if ( allowedParType == cmdPar_programName ) {
            if ( !expressionStartsWithGenericName ) { result = result_nameExpectedAsCmdPar; return false; }
        }
    }

    // Is the current command parameter an expression and is this the second main level element of this expression ? 
    // Then check parameter validity (skip block if this is not second parameter element)  
    if ( (isSecondExpressionToken && isExpression) ) {
        // If assignment operator, check that first token is variable and assignment is allowed  
        if ( isAssignmentOp ) {
            if ( (!expressionStartsWithVariable) || (allowedParType == cmdPar_varNameOnly) ) { result = (parseTokenResult_type) result_varWithoutAssignmentExpectedAsCmdPar; return false; }
            if ( _isAnyVarCmd ) { _varDefAssignmentFound = true; }
        }

        // If other operator, check that expression is allowed
        else if ( isNonAssignmentOp ) {
            if ( allowedParType != cmdPar_expression ) { result = (parseTokenResult_type) result_variableExpectedAsCmdPar; return false; };
        }
    }

    bool previousWasTerminal = ((cmdSecondLastTokenType == Interpreter::tok_isTerminalGroup1) || (cmdSecondLastTokenType == Interpreter::tok_isTerminalGroup2) || (cmdSecondLastTokenType == Interpreter::tok_isTerminalGroup3));
    bool previousParamMainLvlElementIsArray = previousWasTerminal ? ((_terminals [cmdSecondLastTokenIndex].terminalCode == termcod_rightPar) && (_parenthesisLevel == 0)) : false;
    if ( previousParamMainLvlElementIsArray ) {          // previous expression main level element is an array element ?  
        bool isSecondMainLvlElement = _arrayElemAssignmentAllowed;     // because only then an assignment is possible

        // If assignment operator, check that first token is variable and assignment is allowed  
        if ( isAssignmentOp ) {
            if ( (allowedParType == cmdPar_varNameOnly) || !isSecondMainLvlElement ) { result = (parseTokenResult_type) result_varWithoutAssignmentExpectedAsCmdPar; return false; }
            if ( _isAnyVarCmd ) { _varDefAssignmentFound = true; }
        }

        // check that expression is allowed (because of array element) - no further check needed
        else if ( isNonAssignmentOp ) {
            if ( (allowedParType != cmdPar_expression) && (isSecondMainLvlElement) ) { result = (parseTokenResult_type) result_variableExpectedAsCmdPar; return false; };
        }
    }

    // remember past values
    cmdSecondLastTokenType = _lastTokenType;                                               // within current command
    cmdSecondLastTokenIndex = _tokenIndex;
    cmdSecondLastIsLvl0CommaSep = isLvl0CommaSep;
    isSecondExpressionToken = isExpressionFirstToken;
    return true;
}


// -------------------------------------------------------
// *   try to parse next characters as a reserved word   *
// -------------------------------------------------------

bool MyParser::parseAsResWord( char*& pNext, parseTokenResult_type& result ) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int resWordIndex;

    if ( !isalpha( pNext [0] ) ) { return true; }                                       // first character is not a letter ? Then it's not a reserved word (it can still be something else)
    while ( isalnum( pNext [0] ) || (pNext [0] == '_') ) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    for ( resWordIndex = _resWordCount - 1; resWordIndex >= 0; resWordIndex-- ) {          // for all defined reserved words: check against alphanumeric token (NOT ending by '\0')
        if ( strlen( _resWords [resWordIndex]._resWordName ) != pNext - pch ) { continue; }          // token has correct length ? If not, skip remainder of loop ('continue')                            
        if ( strncmp( _resWords [resWordIndex]._resWordName, pch, pNext - pch ) != 0 ) { continue; } // token corresponds to reserved word ? If not, skip remainder of loop ('continue') 

        // token is reserved word, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( _parenthesisLevel > 0 ) { pNext = pch; result = result_resWordNotAllowedHere; return false; }
        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1) ) { pNext = pch; result = result_resWordNotAllowedHere; return false; }
        if ( !_isCommand ) {                                                             // within commands: do not test here


            bool lastIsSemiColon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
            if ( !lastIsSemiColon && (_lastTokenType != Interpreter::tok_no_token) ) {
                pNext = pch; result = result_resWordNotAllowedHere; return false;
            }
        }
        if ( _leadingSpaceCheck ) { pNext = pch; result = result_spaceMissing; return false; }

        _tokenIndex = resWordIndex;                                                     // needed in case it's the start of a command (to determine parameters)

        // token is a reserved word, and it's allowed here

        // if NOT a block command, bytes for token step are not needed 
        bool hasTokenStep = (_resWords [resWordIndex].cmdBlockDef.blockType != block_none);

        Interpreter::TokenIsResWord* pToken = (Interpreter::TokenIsResWord*) _pcalculator->_programCounter;
        pToken->tokenType = Interpreter::tok_isReservedWord | ((sizeof( Interpreter::TokenIsResWord ) - (hasTokenStep ? 0 : 2)) << 4);
        pToken->tokenIndex = resWordIndex;
        if ( hasTokenStep ) { pToken->toTokenStep [0] = 0xFF; pToken->toTokenStep [1] = 0xFF; }                  // -1: no token ref. Because uint16_t not necessarily aligned with word size: store as two sep. bytes                            

        _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
        _lastTokenType = Interpreter::tok_isReservedWord;
        _lastTokenIsTerminal = false;

        _pcalculator->_programCounter += sizeof( Interpreter::TokenIsResWord ) - (hasTokenStep ? 0 : 2);
        *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
        result = result_tokenFound;                                                     // flag 'valid token found'
        return true;
    }

    pNext = pch;                                                                        // reset pointer to first character to parse (because no token was found)
    return true;                                                                        // token is not a reserved word (but can still be something else)
}


// ------------------------------------------------
// *   try to parse next characters as a number   *
// ------------------------------------------------

bool MyParser::parseAsNumber( char*& pNext, parseTokenResult_type& result ) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    // all numbers will be positive, because leading '-' or '+' characters are parsed separately as prefix operators
    // this is important if next infix operator (power) has higher priority then this prefix operator: -2^4 <==> -(2^4) <==> -16, AND NOT (-2)^4 <==> 16 
    float f = strtof( pch, &pNext );                                                    // token can be parsed as float ?
    if ( pch == pNext ) { return true; }                                                // token is not a number if pointer pNext was not moved

    if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command
    // token is a number constant, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_numConstNotAllowedHere; return false; }

    // overflow ? (underflow is not detected with strtof() ) 
    if ( !isfinite( f ) ) { pNext = pch; result = result_overflow; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
    if ( !tokenAllowed ) { pNext = pch; result = result_numConstNotAllowedHere; return false; ; }

    // Note: in a declaration statement, operators other than assignment are not allowed, which is detected in terminal token parsing
    // -> if previous token was operator: it's an assignment
    bool isParamDecl = (_isExtFunctionCmd);                                          // parameter declarations :  constant can ONLY FOLLOW an assignment operator
    bool isAssignmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;

    if ( isParamDecl && !isAssignmentOp ) { pNext = pch; result = result_numConstNotAllowedHere; return false; }

    // token is a number, and it's allowed here
    Interpreter::TokenIsRealCst* pToken = (Interpreter::TokenIsRealCst*) _pcalculator->_programCounter;
    pToken->tokenType = Interpreter::tok_isRealConst | (sizeof( Interpreter::TokenIsRealCst ) << 4);
    memcpy( pToken->realConst, &f, sizeof( f ) );                                           // float not necessarily aligned with word size: copy memory instead

    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && isAssignmentOp);

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = Interpreter::tok_isRealConst;
    _lastTokenIsTerminal = false;

    if ( doNonLocalVarInit ) { initVariable( _lastVariableTokenStep, _lastTokenStep ); }     // initialisation of global / static variable ? (operator: is always assignment)

    _pcalculator->_programCounter += sizeof( Interpreter::TokenIsRealCst );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ----------------------------------------------------------------
// *   try to parse next characters as an alphanumeric constant   *
// ----------------------------------------------------------------

bool MyParser::parseAsStringConstant( char*& pNext, parseTokenResult_type& result ) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int escChars = 0;

    if ( (pNext [0] != '\"') ) { return true; }                                         // no opening quote ? Is not an alphanumeric cst (it can still be something else)
    pNext++;                                                                            // skip opening quote

    if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is an alphanumeric constant, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
    if ( !tokenAllowed ) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; ; }

    // Note: in a declaration statement, operators other than assignment are not allowed, which is detected in terminal token parsing
    // -> if previous token was operator: it's an assignment
    bool isParamDecl = (_isExtFunctionCmd);                                             // parameter declarations :  constant can ONLY FOLLOW an assignment operator
    bool isAssignmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;
    if ( isParamDecl && !isAssignmentOp ) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }

    bool isArrayDimSpec = (_isAnyVarCmd) && (_parenthesisLevel > 0);                    // array declaration: dimensions must be number constants (global, static, local arrays)
    if ( isArrayDimSpec ) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }

    if ( _leadingSpaceCheck ) { pNext = pch; result = result_spaceMissing; return false; }

    while ( pNext [0] != '\"' ) {                                                       // do until closing quote, if any
        // if no closing quote found, an invalid escape sequence or a control character detected, reset pointer to first character to parse, indicate error and return
        if ( pNext [0] == '\0' ) { pNext = pch; result = result_alphaClosingQuoteMissing; return false; }
        if ( pNext [0] < ' ' ) { pNext = pch; result = result_alphaNoCtrlCharAllowed; return false; }
        if ( pNext [0] == '\\' ) {
            if ( (pNext [1] == '\\') || (pNext [1] == '\"') ) { pNext++; escChars++; }  // valid escape sequences: ' \\ ' (add backslash) and ' \" ' (add double quote)
            else { pNext = pch; result = result_alphaConstInvalidEscSeq; return false; }
        }
        pNext++;
    };

    // if alphanumeric constant is too long, reset pointer to first character to parse, indicate error and return
    if ( pNext - (pch + 1) - escChars > _maxAlphaCstLen ) { pNext = pch; result = result_alphaConstTooLong; return false; }

    // token is an alphanumeric constant, and it's allowed here
    char* pStringCst = new char [pNext - (pch + 1) - escChars + 1];                                // create char array on the heap to store alphanumeric constant, including terminating '\0'
    _heapObjectCount++;
#if printCreateDeleteHeapObjects
    Serial.print( "(HEAP) Create object # " ); Serial.print( _heapObjectCount ); Serial.print( ": string const value, addr " );
    Serial.println( (uint32_t) pStringCst - RAMSTART );
#endif
    // store alphanumeric constant in newly created character array
    pStringCst [pNext - (pch + 1) - escChars] = '\0';                                 // store string terminating '\0' (pch + 1 points to character after opening quote, pNext points to closing quote)
    char* pSource = pch + 1, * pDestin = pStringCst;                                  // pSource points to character after opening quote
    while ( pSource + escChars < pNext ) {                                              // store alphanumeric constant in newly created character array (terminating '\0' already added)
        if ( pSource [0] == '\\' ) { pSource++; escChars--; }                           // if escape sequences found: skip first escape sequence character (backslash)
        pDestin++ [0] = pSource++ [0];
    }
    pNext++;                                                                            // skip closing quote

    Interpreter::TokenIsStringCst* pToken = (Interpreter::TokenIsStringCst*) _pcalculator->_programCounter;
    pToken->tokenType = Interpreter::tok_isStringConst | (sizeof( Interpreter::TokenIsStringCst ) << 4);
    memcpy( pToken->pStringConst, &pStringCst, sizeof( pStringCst ) );            // pointer not necessarily aligned with word size: copy memory instead

    bool isLocalVarInitCheck = (_isLocalVarCmd && isAssignmentOp);
    bool isArrayVar = ((Interpreter::TokenIsVariable*) (_pcalculator->_programStorage + _lastVariableTokenStep))->identInfo & _pcalculator->var_isArray;
    if ( isLocalVarInitCheck && isArrayVar && (strlen( pStringCst ) > 0) ) {
        pNext = pch; result = result_arrayInit_emptyStringExpected; return false;        // only check (init when function is called)
    }

    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && isAssignmentOp);          // (operator: is always assignment)

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = Interpreter::tok_isStringConst;
    _lastTokenIsTerminal = false;

    if ( doNonLocalVarInit ) {                                     // initialisation of global / static variable ? 
        if ( !initVariable( _lastVariableTokenStep, _lastTokenStep ) ) { pNext = pch; result = result_arrayInit_emptyStringExpected; return false; };
    }

    _pcalculator->_programCounter += sizeof( Interpreter::TokenIsStringCst );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ---------------------------------------------------------------------------------
// *   try to parse next characters as a terminal token (one- or two-characters)   *
// ---------------------------------------------------------------------------------

// Array parsing: check that max dimension count and maximum array size is not exceeded
// ------------------------------------------------------------------------------------

bool MyParser::checkArrayDimCountAndSize( parseTokenResult_type& result, int* arrayDef_dims, int& dimCnt ) {

    bool lastIsLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;
    if ( lastIsLeftPar ) { result = result_arrayDefNoDims; return false; }

    dimCnt++;

    if ( dimCnt > _pcalculator->MAX_ARRAY_DIMS ) { result = result_arrayDefMaxDimsExceeded; return false; }
    float f { 0 };        // last token is a number constant: dimension spec
    memcpy( &f, ((Interpreter::TokenIsRealCst*) (_pcalculator->_programStorage + _lastTokenStep))->realConst, sizeof( f ) );
    if ( f < 1 ) { result = result_arrayDefNegativeDim; return false; }
    arrayDef_dims [dimCnt - 1] = (int) f;
    int arrayElements = 1;
    for ( int cnt = 0; cnt < dimCnt; cnt++ ) { arrayElements *= arrayDef_dims [cnt]; }
    if ( arrayElements > _pcalculator->MAX_ARRAY_ELEM ) { result = result_arrayDefMaxElementsExceeded; return false; }
    return true;
}


// External function definition statement parsing: check order of mandatory and optional arguments, check if max. n° not exceeded
// -------------------------------------------------------------------------------------------------------------------------------

bool MyParser::checkExtFunctionArguments( parseTokenResult_type& result, int& minArgCnt, int& maxArgCnt ) {
    bool lastIsRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;

    bool argWasMandatory = (_lastTokenType == Interpreter::tok_isVariable) || lastIsRightPar;         // variable without assignment to a constant or param array def. parenthesis
    bool alreadyOptArgs = (minArgCnt != maxArgCnt);
    if ( argWasMandatory && alreadyOptArgs ) { result = result_mandatoryArgFoundAfterOptionalArgs; return false; }
    if ( argWasMandatory ) { minArgCnt++; }
    maxArgCnt++;
    // check that max argument count is not exceeded (number must fit in 4 bits)
    if ( maxArgCnt > c_extFunctionMaxArgs ) { result = result_functionDefMaxArgsExceeded; return false; }
    return true;
}


// Internal function: check that order of arrays and scalar variables is consistent with function definition 
// ---------------------------------------------------------------------------------------------------------

bool MyParser::checkInternFuncArgArrayPattern( parseTokenResult_type& result ) {
    int funcIndex = _pParsingStack->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
    char paramIsArrayPattern = _functions [funcIndex].arrayPattern;
    int argNumber = _pParsingStack->openPar.actualArgsOrDims;

    if ( argNumber > 0 ) {
        bool isArray = false;
        if ( _lastTokenType == Interpreter::tok_isVariable ) {                                      // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((Interpreter::TokenIsVariable*) (_pcalculator->_programStorage + _lastTokenStep))->identInfo) & _pcalculator->var_isArray;
        }

        if ( ((paramIsArrayPattern >> (argNumber - 1)) & 0b1) != isArray ) { result = isArray ? result_scalarArgExpected : result_arrayArgExpected; return false; }
    }
}


// External function: check that order of arrays and scalar variables is consistent with previous calls and function definition 
// ----------------------------------------------------------------------------------------------------------------------------

bool MyParser::checkExternFuncArgArrayPattern( parseTokenResult_type& result, bool isFunctionClosingParenthesis ) {

    int funcIndex = _pParsingStack->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
    int argNumber = _pParsingStack->openPar.actualArgsOrDims;
    uint16_t paramIsArrayPattern { 0 };
    memcpy( &paramIsArrayPattern, _pcalculator->extFunctionData [funcIndex].paramIsArrayPattern, sizeof( char [2] ) );
    if ( argNumber > 0 ) {

        bool isArray = false;
        bool lastIsRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;

        if ( _isExtFunctionCmd ) { isArray = lastIsRightPar; }  // function definition: if variable name followed by empty parameter list ' () ': array parameter
        else if ( _lastTokenType == Interpreter::tok_isVariable ) {                                      // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((Interpreter::TokenIsVariable*) (_pcalculator->_programStorage + _lastTokenStep))->identInfo) & _pcalculator->var_isArray;
        }

        uint16_t paramArrayMask = 1 << (argNumber - 1);
        if ( paramIsArrayPattern & 0x8000 ) {                   // function not used yet (before it was defined now: no need to check, just set array bit)
            paramIsArrayPattern = paramIsArrayPattern | (isArray ? paramArrayMask : 0);
        }
        else {  // error message can not be more specific (scalar expected, array expected) because maybe function has not been defined yet
            if ( (paramIsArrayPattern & paramArrayMask) != (isArray ? paramArrayMask : 0) ) { result = result_fcnScalarAndArrayArgOrderNotConsistent; return false; }
        }
    }

    if ( isFunctionClosingParenthesis ) { paramIsArrayPattern = paramIsArrayPattern & ~0x8000; }    // function name used now: order of scalar and array parameters is now fixed
    memcpy( _pcalculator->extFunctionData [funcIndex].paramIsArrayPattern, &paramIsArrayPattern, sizeof( char [2] ) );
    return true;
}


// --------------------------
// * Parse a terminal token * 
// --------------------------

bool MyParser::parseTerminalToken( char*& pNext, parseTokenResult_type& result ) {

    // external function definition statement parsing: count number of mandatory and optional arguments in function definition for storage
    static int extFunctionDef_minArgCounter { 0 };
    static int extFunctionDef_maxArgCounter { 0 };

    // array definition statement parsing: record dimensions (if 1 dimension only: set dim 2 to zero) 
    static int array_dimCounter { 0 };
    static int arrayDef_dims [_pcalculator->MAX_ARRAY_DIMS] { 0 };

    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int termIndex;

    for ( termIndex = _terminalCount - 1; termIndex >= 0; termIndex-- ) {                  // for all defined function names: check against alphanumeric token (NOT ending by '\0')
        int len = strlen( _terminals [termIndex].terminalName );    // token has correct length ? If not, skip remainder of loop ('continue')                            
        // do not look for trailing space, to use strncmp() wih number of non-space characters found, because a space is not required after an operator
        if ( strncmp( _terminals [termIndex].terminalName, pch, len ) == 0 ) { break; }      // token corresponds to terminal name ? Then exit loop    
    }
    if ( termIndex < 0 ) { return true; }                                                // token is not a one-character token (and it's not a two-char token, because these start with same character)
    pNext += strlen( _terminals [termIndex].terminalName );                                                                            // move to next character

    int nextTermIndex;  // peek: is next token a terminal ? nextTermIndex will be -1 if not
    char* peek = pNext;     // first character of next token (or '\0')
    while ( peek [0] == ' ' ) { peek++; }
    for ( nextTermIndex = _terminalCount - 1; nextTermIndex >= 0; nextTermIndex-- ) {                  // for all defined function names: check against alphanumeric token (NOT ending by '\0')
        int len = strlen( _terminals [nextTermIndex].terminalName );    // token has correct length ? If not, skip remainder of loop ('continue')                            
        // do not look for trailing space, to use strncmp() wih number of non-space characters found, because a space is not required after an operator
        if ( strncmp( _terminals [nextTermIndex].terminalName, peek, len ) == 0 ) { break; }      // token corresponds to terminal name ? Then exit loop   
    }


    Interpreter::tokenType_type tokenType;
    uint8_t flags { B0 };

    switch ( _terminals [termIndex].terminalCode ) {

    case termcod_leftPar: {
        // -------------------------------------
        // Case 1: is token a left parenthesis ?
        // -------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is left parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_4_3_1_0) ) { pNext = pch;  result = result_parenthesisNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
        if ( !tokenAllowed ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }

        if ( _isAnyVarCmd && (_parenthesisLevel > 0) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }     // no parenthesis nesting in array declarations
        // parenthesis nesting in function definitions, only to declare an array parameter AND only if followed by a closing parenthesis 
        if ( (_isExtFunctionCmd) && (_parenthesisLevel > 0) && (_lastTokenType != Interpreter::tok_isVariable) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }
        if ( _isProgramCmd || _isDeleteVarCmd ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

        if ( _leadingSpaceCheck ) { pNext = pch; result = result_spaceMissing; return false; }

        // token is a left parenthesis, and it's allowed here

        // store specific flags in stack, because if nesting functions or parentheses, values will be overwritten
        flags = (_lastTokenType == Interpreter::tok_isExternFunction) ? _pcalculator->extFunctionBit :
            (_lastTokenType == Interpreter::tok_isInternFunction) ? _pcalculator->intFunctionBit :
            (_lastTokenType == Interpreter::tok_isVariable) ? _pcalculator->arrayBit : _pcalculator->openParenthesisBit;     // is it following a(n internal or external) function name ?
        // external function (call or definition) opening parenthesis
        if ( _lastTokenType == Interpreter::tok_isExternFunction ) {
            if ( _pcalculator->extFunctionData [_functionIndex].pExtFunctionStartToken != nullptr ) { flags = flags | _pcalculator->extFunctionPrevDefinedBit; }
        }

        bool isSecondSubExpressionToken = (_previousTokenIsTerminal ?
            ((_previousTermCode == termcod_semicolon) || (_previousTermCode == termcod_leftPar) || (_previousTermCode == termcod_comma)) : false);
        isSecondSubExpressionToken = isSecondSubExpressionToken || (_previousTokenType == Interpreter::tok_no_token) || (_previousTokenType == Interpreter::tok_isReservedWord);

        // last token before left parenthesis is variable name AND the start of a (sub-) expression, but NOT part of a variable definition command
        bool assignmentOK = (_lastTokenType == Interpreter::tok_isVariable) && isSecondSubExpressionToken;
        if ( assignmentOK ) { flags = flags | _pcalculator->arrayElemAssignmentAllowedBit; }      // after the corresponding closing parenthesis, assignment will be allowed

        // if function DEFINITION: initialize variables for counting of allowed mandatory and optional arguments (not an array parameter, would be parenthesis level 1)
        if ( _isExtFunctionCmd && (_parenthesisLevel == 0) ) {      // not an array parameter (would be parenthesis level 1)
            extFunctionDef_minArgCounter = 0;
            extFunctionDef_maxArgCounter = 0;            // init count; range from 0 to a hardcoded maximum 
        }

        // if LOCAL, STATIC or GLOBAL array DEFINITION or USE (NOT: parameter array): initialize variables for reading dimensions 
        if ( flags & _pcalculator->arrayBit ) {                    // always count, also if not first definition (could happen for global variables)
            array_dimCounter = 0;
            for ( int i = 0; i < _pcalculator->MAX_ARRAY_DIMS; i++ ) { arrayDef_dims [i] = 0; }        // init dimensions (dimension count will result from dimensions being non-zero
        }

        // left parenthesis only ? (not a function or array opening parenthesis): min & max allowed argument count not yet initialised
        if ( _lastTokenGroup_sequenceCheck_bit & lastTokenGroup_4 ) {
            _minFunctionArgs = 1;                                    // initialize min & max allowed argument count to 1
            _maxFunctionArgs = 1;
        }

        // min & max argument count: either allowed range (if function previously defined), current range of actual args counts (if previous calls only), or not initialized
        _parenthesisLevel++;                                                            // increment stack counter and create corresponding list element
        _pParsingStack = (LE_parsingStack*) parsingStack.appendListElement( sizeof( LE_parsingStack ) );
        _pParsingStack->openPar.minArgs = _minFunctionArgs;
        _pParsingStack->openPar.maxArgs = _maxFunctionArgs;
        _pParsingStack->openPar.actualArgsOrDims = 0;
        _pParsingStack->openPar.arrayDimCount = _pcalculator->_arrayDimCount;         // dimensions of previously defined array. If zero, then this array did not yet exist, or it's a sclarar variable
        _pParsingStack->openPar.flags = flags;
        _pParsingStack->openPar.identifierIndex = (_lastTokenType == Interpreter::tok_isInternFunction) ? _functionIndex :
            (_lastTokenType == Interpreter::tok_isExternFunction) ? _functionIndex :
            (_lastTokenType == Interpreter::tok_isVariable) ? _variableNameIndex : 0;
        _pParsingStack->openPar.variableQualifier = _variableQualifier;

        break; }


    case termcod_rightPar: {
        // --------------------------------------
        // Case 2: is token a right parenthesis ?
        // --------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is right parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_4_2) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
        if ( !tokenAllowed ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }
        if ( _parenthesisLevel == 0 ) { pNext = pch; result = result_missingLeftParenthesis; return false; }

        flags = _pParsingStack->openPar.flags;


        // 2.1 External function definition (not a call), OR array parameter definition, closing parenthesis ?
        // -------------------------------------------------------------------

        if ( _isExtFunctionCmd ) {
            if ( _parenthesisLevel == 1 ) {          // function definition closing parenthesis
                // stack level will not change until closing parenthesis (because within definition, no nesting of parenthesis is possible)
                // stack min & max values: current range of args counts that occured in previous calls (not initialized if no earlier calls occured)

                // if empty function parameter list, then do not increment parameter count (function taking no parameters)

                bool emptyParamList = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;            // ok because no nesting allowed
                _pParsingStack->openPar.actualArgsOrDims += (emptyParamList ? 0 : 1);

                // check order of mandatory and optional arguments, check if max. n° not exceeded
                if ( !emptyParamList ) { if ( !checkExtFunctionArguments( result, extFunctionDef_minArgCounter, extFunctionDef_maxArgCounter ) ) { pNext = pch; return false; }; }

                int funcIndex = _pParsingStack->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
                // if previous calls, check if range of actual argument counts that occured in previous calls corresponds to mandatory and optional arguments defined now
                bool previousCalls = (_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) != c_extFunctionFirstOccurFlag;
                if ( previousCalls ) {                                                      // stack contains current range of actual args occured in previous calls
                    if ( ((int) _pParsingStack->openPar.minArgs < extFunctionDef_minArgCounter) ||
                        (int) _pParsingStack->openPar.maxArgs > extFunctionDef_maxArgCounter ) {
                        pNext = pch; result = result_prevCallsWrongArgCount; return false;  // argument count in previous calls to this function does not correspond 
                    }
                }

                // store min required & max allowed n° of arguments in identifier storage
                // this replaces the range of actual argument counts that occured in previous calls (if any)
                _pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1] = (extFunctionDef_minArgCounter << 4) | (extFunctionDef_maxArgCounter);

                // check that order of arrays and scalar variables is consistent with previous callsand function definition
                if ( !checkExternFuncArgArrayPattern( result, true ) ) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
            }
        }


        // 2.2 Array definition dimension spec closing parenthesis ?
        // ---------------------------------------------------------

        else if ( _isAnyVarCmd ) {                        // note: parenthesis level is 1 (because no inner parenthesis allowed)
            if ( !checkArrayDimCountAndSize( result, arrayDef_dims, array_dimCounter ) ) { pNext = pch; return false; }

            int varNameIndex = _pParsingStack->openPar.identifierIndex;
            uint8_t varQualifier = _pParsingStack->openPar.variableQualifier;

            bool isUserVar = (varQualifier == _pcalculator->var_isUser);
            bool isGlobalVar = (varQualifier == _pcalculator->var_isGlobal);
            bool isStaticVar = (varQualifier == _pcalculator->var_isStaticInFunc);
            bool isLocalVar = (varQualifier == _pcalculator->var_isLocalInFunc);            // but not function parameter definitions

            float* pArray;
            int arrayElements = 1;              // init
            int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : _pcalculator->programVarValueIndex [varNameIndex];

            // user, global and static arrays: create array on the heap. Array dimensions will be stored in array element 0
            if ( isUserVar || isGlobalVar || isStaticVar ) {
                for ( int dimCnt = 0; dimCnt < array_dimCounter; dimCnt++ ) { arrayElements *= arrayDef_dims [dimCnt]; }
                pArray = new float [arrayElements + 1];
                _heapObjectCount++;

#if printCreateDeleteHeapObjects
                Serial.print( "(HEAP) Create object # " ); Serial.print( _heapObjectCount ); Serial.print( ": array storage, addr " );
                Serial.println( (uint32_t) pArray - RAMSTART );
#endif
                // only now, the array flag can be set, because only now the object exists
                if ( isUserVar ) {
                    _pcalculator->userVarValues [valueIndex].pArray = pArray;
                    _pcalculator->userVarType [varNameIndex] |= _pcalculator->var_isArray;             // set array bit
                    _pcalculator->_userVarCount++;                                                     // user array variable is now considered 'created'//// ook doen voor global en static ? (consistentie)
                }
                else if ( isGlobalVar ) {
                    _pcalculator->globalVarValues [valueIndex].pArray = pArray;
                    _pcalculator->globalVarType [varNameIndex] |= _pcalculator->var_isArray;             // set array bit
                }
                else if ( isStaticVar ) {
                    _pcalculator->staticVarValues [valueIndex].pArray = pArray;
                    _pcalculator->staticVarType [_pcalculator->_staticVarCount - 1] |= _pcalculator->var_isArray;             // set array bit
                }

                // global and static variables are initialized at parsing time. If no explicit initializer, initialize array elements to zero now
                bool arrayHasInitializer = false;
                arrayHasInitializer = (nextTermIndex < 0) ? false : _terminals [nextTermIndex].terminalCode == termcod_assign;
                if ( !arrayHasInitializer ) {                    // no explicit initializer: initialize now (as real) 
                    for ( int arrayElem = 1; arrayElem <= arrayElements; arrayElem++ ) { ((float*) pArray) [arrayElem] = 0.; }
                }
            }

            // local arrays (note: NOT for function parameter arrays): set pointer to dimension storage 
            // the array flag has been set when local variable was created (including function parameters, which are also local variables)
            // dimensions are not stored in array value array (because created at runtime) but are temporarily stored here during function parsing  
            else if ( isLocalVar ) {
                pArray = (float*) _pcalculator->localVarDims [_pcalculator->_localVarCountInFunction - 1];
            }

            // global, static and local arrays: store array dimensions (local arrays: temporary storage during parsing only)
            // store dimensions in element 0: char 0 to 2 is dimensions; char 3 = dimension count 
            for ( int i = 0; i < _pcalculator->MAX_ARRAY_DIMS; i++ ) {
                ((char*) pArray) [i] = arrayDef_dims [i];
            }
            ((char*) pArray) [3] = array_dimCounter;        // (note: for param arrays, set to max dimension count during parsing)
        }


        // 2.3 Internal or external function call, or parenthesis pair, closing parenthesis ?
        // ----------------------------------------------------------------------------------

        else if ( flags & (_pcalculator->intFunctionBit | _pcalculator->extFunctionBit | _pcalculator->openParenthesisBit) ) {
            // if empty function call argument list, then do not increment argument count (function call without arguments)
            bool emptyArgList = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;            // ok because no nesting allowed
            _pParsingStack->openPar.actualArgsOrDims += (emptyArgList ? 0 : 1);
            int actualArgs = (int) _pParsingStack->openPar.actualArgsOrDims;

            // call to not yet defined external function ? (there might be previous calls)
            bool callToNotYetDefinedFunc = ((flags & (_pcalculator->extFunctionBit | _pcalculator->extFunctionPrevDefinedBit)) == _pcalculator->extFunctionBit);
            if ( callToNotYetDefinedFunc ) {
                // check that max argument count is not exceeded (number must fit in 4 bits)
                if ( actualArgs > c_extFunctionMaxArgs ) { pNext = pch; result = result_functionDefMaxArgsExceeded; return false; }

                // if at least one previous call (maybe a nested call) is completely parsed, retrieve current range of actual args that occured in these previous calls
                // and update this range with the argument count of the current external function call that is at its closing parenthesis
                int funcIndex = _pParsingStack->openPar.identifierIndex;            // of current function call: stored in stack for current PARENTHESIS level
                bool prevExtFuncCompletelyParsed = (_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) != c_extFunctionFirstOccurFlag;
                if ( prevExtFuncCompletelyParsed ) {
                    _pParsingStack->openPar.minArgs = ((_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) >> 4) & 0x0F;
                    _pParsingStack->openPar.maxArgs = (_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) & 0x0F;
                    if ( (int) _pParsingStack->openPar.minArgs > actualArgs ) { _pParsingStack->openPar.minArgs = actualArgs; }
                    if ( (int) _pParsingStack->openPar.maxArgs < actualArgs ) { _pParsingStack->openPar.maxArgs = actualArgs; }
                }
                // no previous call: simply set this range to the argument count of the current external function call that is at its closing parenthesis
                else { _pParsingStack->openPar.minArgs = actualArgs; _pParsingStack->openPar.maxArgs = actualArgs; }

                // store the up to date range of actual argument counts in identifier storage
                _pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1] = (_pParsingStack->openPar.minArgs << 4) | (_pParsingStack->openPar.maxArgs);
            }

            // if call to previously defined external function, to an internal function, or if open parenthesis, then check argument count 
            else {
                bool isOpenParenthesis = (flags & _pcalculator->openParenthesisBit);
                if ( isOpenParenthesis ) { _pParsingStack->openPar.minArgs = 1; _pParsingStack->openPar.maxArgs = 1; }
                bool argCountWrong = ((actualArgs < (int) _pParsingStack->openPar.minArgs) ||
                    (actualArgs > ( int ) _pParsingStack->openPar.maxArgs));
                if ( argCountWrong ) { pNext = pch; result = result_wrong_arg_count; return false; }
            }

            // check that order of arrays and scalar variables is consistent with function definition and (external functions only: with previous calls) 
            if ( flags & _pcalculator->intFunctionBit ) { if ( !checkInternFuncArgArrayPattern( result ) ) { pNext = pch; return false; }; }
            else if ( flags & _pcalculator->extFunctionBit ) { if ( !checkExternFuncArgArrayPattern( result, true ) ) { pNext = pch; return false; }; }
        }


        // 2.4 Array element spec closing parenthesis ?
        // --------------------------------------------

        else if ( flags & _pcalculator->arrayBit ) {
            // check if array dimension count corresponds (individual dimension adherence can only be checked at runtime)
            // for function parameters, array dimension count can only be checked at runtime as well
            // if previous token is left parenthesis (' () '), then do not increment argument count
            bool lastWasLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;            // ok because no nesting allowed
            if ( !lastWasLeftPar ) { _pParsingStack->openPar.actualArgsOrDims++; }

            int varNameIndex = _pParsingStack->openPar.identifierIndex;
            uint8_t varQualifier = _pParsingStack->openPar.variableQualifier;
            bool isParam = (varQualifier == _pcalculator->var_isParamInFunc);
            int actualDimCount = _pParsingStack->openPar.actualArgsOrDims;
            if ( actualDimCount == 0 ) { pNext = pch; result = result_arrayUseNoDims; return false; } // dim count too high: already handled when preceding comma was parsed
            if ( !isParam ) {
                if ( actualDimCount != (int) _pParsingStack->openPar.arrayDimCount ) { pNext = pch; result = result_arrayUseWrongDimCount; return false; }
            }
        }

        else {}     // for documentation only: all cases handled


        // token is a right parenthesis, and it's allowed here

        _arrayElemAssignmentAllowed = (flags & _pcalculator->arrayElemAssignmentAllowedBit);          // assignment possible next ? (to array element)
        parsingStack.deleteListElement( nullptr );                                           // decrement open parenthesis stack counter and delete corresponding list element
        _parenthesisLevel--;

        // set pointer to currently last element in stack
        if ( _blockLevel + _parenthesisLevel > 0 ) { _pParsingStack = (LE_parsingStack*) parsingStack.getLastListElement(); }

        break;
    }


    case termcod_comma: {
        // ------------------------------------
        // Case 3: is token a comma separator ?
        // ------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is comma separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2) ) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
        if ( !tokenAllowed ) { pNext = pch; result = result_separatorNotAllowedHere; return false; ; }

        // if no open parenthesis, a comma can only occur to separate command parameters
        if ( (_parenthesisLevel == 0) && !_isCommand ) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

        flags = (_parenthesisLevel > 0) ? _pParsingStack->openPar.flags : 0;


        // 3.1 External function definition (not a call) parameter separator ? 
        // -------------------------------------------------------------------

        if ( _isExtFunctionCmd ) {
            if ( _parenthesisLevel == 1 ) {          // not an array parameter (would be parenthesis level 2)
                _pParsingStack->openPar.actualArgsOrDims++;
                // check order of mandatory and optional arguments, check if max. n° not exceeded
                if ( !checkExtFunctionArguments( result, extFunctionDef_minArgCounter, extFunctionDef_maxArgCounter ) ) { pNext = pch; return false; };

                // Check order of mandatory and optional arguments (function: parenthesis levels > 0)
                if ( !checkExternFuncArgArrayPattern( result, false ) ) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
            }
        }


        // 3.2 Array definition dimension spec separator ? 
        // -----------------------------------------------

        else if ( _isAnyVarCmd ) {
            if ( _parenthesisLevel == 1 ) {             // // parenthesis level 1: separator between array dimension specs (level 0: sep. between variables)             
                // Check dimension count and array size 
                if ( !checkArrayDimCountAndSize( result, arrayDef_dims, array_dimCounter ) ) { pNext = pch; return false; }
            }
        }


        // 3.3 Internal or external function call argument separator ?
        // -----------------------------------------------------------

        else if ( flags & (_pcalculator->intFunctionBit | _pcalculator->extFunctionBit | _pcalculator->openParenthesisBit) ) {
            // note that actual argument count is at least one more than actual argument count, because at least one more to go (after the comma)
            _pParsingStack->openPar.actualArgsOrDims++;           // include argument before the comma in argument count     
            int actualArgs = (int) _pParsingStack->openPar.actualArgsOrDims;

            // call to not yet defined external function ? (because there might be previous calls as well)
            bool callToNotYetDefinedFunc = ((_pParsingStack->openPar.flags & (_pcalculator->extFunctionBit | _pcalculator->extFunctionPrevDefinedBit)) == _pcalculator->extFunctionBit);
            if ( callToNotYetDefinedFunc ) {
                // check that max argument count is not exceeded (number must fit in 4 bits)
                if ( actualArgs > c_extFunctionMaxArgs ) { pNext = pch; result = result_functionDefMaxArgsExceeded; return false; }
            }

            // if call to previously defined external function, to an internal function, or if open parenthesis, then check argument count 
            else {
                bool isOpenParenthesis = (flags & _pcalculator->openParenthesisBit);
                if ( isOpenParenthesis ) { _pParsingStack->openPar.minArgs = 1; _pParsingStack->openPar.maxArgs = 1; }
                bool argCountWrong = (actualArgs >= (int) _pParsingStack->openPar.maxArgs);       // check against allowed maximum number of arguments for this function
                if ( argCountWrong ) { pNext = pch; result = isOpenParenthesis ? result_missingRightParenthesis : result_wrong_arg_count; return false; }
            }

            // check that order of arrays and scalar variables is consistent with function definition and (external functions only: with previous calls) 
            if ( flags & _pcalculator->intFunctionBit ) { if ( !checkInternFuncArgArrayPattern( result ) ) { pNext = pch; return false; }; }
            else if ( flags & _pcalculator->extFunctionBit ) { if ( !checkExternFuncArgArrayPattern( result, false ) ) { pNext = pch; return false; }; }
        }


        // 3.4 Array element spec separator ?
        // ----------------------------------

        else if ( flags & _pcalculator->arrayBit ) {
            // check if array dimension count corresponds (individual boundary adherence can only be checked at runtime)
            _pParsingStack->openPar.actualArgsOrDims++;
            if ( (int) _pParsingStack->openPar.actualArgsOrDims == (int) _pParsingStack->openPar.arrayDimCount ) { pNext = pch; result = result_arrayUseWrongDimCount; return false; }
        }

        else {}     // for documentation only: all cases handled

        // token is a comma separator, and it's allowed here
        break; }


    case termcod_semicolon: {
        // ----------------------------------------
        // Case 4: is token a semicolon separator ?
        // ----------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is semicolon separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( _parenthesisLevel > 0 ) { pNext = pch; result = result_missingRightParenthesis; return false; }
        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1) ) { pNext = pch; result = result_expressionNotComplete; return false; }

        // token is a semicolon separator, and it's allowed here
        break; }


    default:    // terminals
    {
        // ----------------------------
        // Case 5: token is an operator
        // ----------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is an operator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return

        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_4_2_1_0) ) { pNext = pch; result = result_operatorNotAllowedHere; return false; }

        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2) && (_terminals [termIndex].terminalCode != termcod_plus) && (_terminals [termIndex].terminalCode != termcod_minus) ) {
            pNext = pch; result = result_invalidPrefixOperator; return false;
        }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
        if ( !tokenAllowed ) { pNext = pch; result = result_operatorNotAllowedHere; return false; ; }

        if ( _isProgramCmd || _isDeleteVarCmd ) { pNext = pch; result = result_operatorNotAllowedHere; return false; }

        // if assignment, check whether it's allowed here 
        if ( _terminals [termIndex].terminalCode == termcod_assign ) {

            bool isSecondSubExpressionToken = (_previousTokenIsTerminal ?
                ((_previousTermCode == termcod_semicolon) || (_previousTermCode == termcod_leftPar) || (_previousTermCode == termcod_comma)) : false);
            isSecondSubExpressionToken = isSecondSubExpressionToken || (_previousTokenType == Interpreter::tok_no_token) || (_previousTokenType == Interpreter::tok_isReservedWord);

            bool lastWasRightPar = (_lastTermCode == termcod_rightPar);            // ok because no nesting allowed

            bool assignmentToScalarVarOK = ((_lastTokenType == Interpreter::tok_isVariable) && isSecondSubExpressionToken);
            bool assignmentToArrayElemOK = (lastWasRightPar && _arrayElemAssignmentAllowed && (!_isExtFunctionCmd));
            if ( !(assignmentToScalarVarOK || assignmentToArrayElemOK) ) { pNext = pch; result = result_assignmNotAllowedHere; return false; }
        }

        else  if ( _isExtFunctionCmd || _isAnyVarCmd ) {
            if ( (_terminals [termIndex].terminalCode == termcod_plus) || (_terminals [termIndex].terminalCode == termcod_minus) ) {
                // normally, a prefix operator needs its own token (example: expression -2^2 evaluates as -(2^2) yielding -4, whereas a number -2 (stored as one token) ^2 would yield 4, which is incorrect
                // but initializers are pure constants: no prefix operators are allowed here, because this would create a constant expression
                // however negative numbers are legal as initialiser: discard the prefix operator, to make it part of the number token
                if ( nextTermIndex >= 0 ) { pNext = pch; result = result_operatorNotAllowedHere; return false; } // next token is terminal as well. It risks to be another prefix operator
                else { pNext = pch; return true; }         // do not move input pointer
            }
            else { pNext = pch; result = result_operatorNotAllowedHere; return false; }
        }

        // token is an operator, and it's allowed here
    }
    }


    // create token
    // ------------

    // too many terminals for 1 terminal group: provide multiple groups
    tokenType = (termIndex <= 0x0F) ? Interpreter::tok_isTerminalGroup1 : (termIndex <= 0x1F) ? Interpreter::tok_isTerminalGroup2 : Interpreter::tok_isTerminalGroup3;                                              // remember: token is a left parenthesis
    _tokenIndex = termIndex;

    Interpreter::TokenIsTerminal* pToken = (Interpreter::TokenIsTerminal*) _pcalculator->_programCounter;
    pToken->tokenTypeAndIndex = tokenType | ((termIndex & 0x0F) << 4);     // terminal tokens only: token type character includes token index too 

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = tokenType;
    _lastTokenIsTerminal = true;
    _lastTermCode = (termin_code) _terminals [termIndex].terminalCode;

    _pcalculator->_programCounter += sizeof( Interpreter::TokenIsTerminal );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ----------------------------------------------------------------------------
// *   try to parse next characters as an internal (built in) function name   *
// ----------------------------------------------------------------------------

bool MyParser::parseAsInternFunction( char*& pNext, parseTokenResult_type& result ) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int funcIndex;

    if ( !isalpha( pNext [0] ) ) { return true; }                                       // first character is not a letter ? Then it's not a function name (it can still be something else)
    while ( isalnum( pNext [0] ) || (pNext [0] == '_') ) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    for ( funcIndex = _functionCount - 1; funcIndex >= 0; funcIndex-- ) {                  // for all defined function names: check against alphanumeric token (NOT ending by '\0')
        if ( strlen( _functions [funcIndex].funcName ) != pNext - pch ) { continue; }   // token has correct length ? If not, skip remainder of loop ('continue')                            
        if ( strncmp( _functions [funcIndex].funcName, pch, pNext - pch ) != 0 ) { continue; }      // token corresponds to function name ? If not, skip remainder of loop ('continue')    

        // token is a function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_functionNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
        if ( !tokenAllowed ) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

        if ( _isExtFunctionCmd ) { pNext = pch; result = result_redefiningIntFunctionNotAllowed; return false; }
        if ( _isAnyVarCmd ) { pNext = pch; result = result_variableNameExpected; return false; }        // is a variable declaration: internal function name not allowed

        // token is function, and it's allowed here
        _minFunctionArgs = _functions [funcIndex].minArgs;                       // set min & max for allowed argument count (note: minimum is 0)
        _maxFunctionArgs = _functions [funcIndex].maxArgs;
        _functionIndex = funcIndex;


        Interpreter::TokenIsIntFunction* pToken = (Interpreter::TokenIsIntFunction*) _pcalculator->_programCounter;
        pToken->tokenType = Interpreter::tok_isInternFunction | (sizeof( Interpreter::TokenIsIntFunction ) << 4);
        pToken->tokenIndex = funcIndex;

        _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
        _lastTokenType = Interpreter::tok_isInternFunction;
        _lastTokenIsTerminal = false;

        _pcalculator->_programCounter += sizeof( Interpreter::TokenIsIntFunction );
        *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
        result = result_tokenFound;                                                     // flag 'valid token found'
        return true;
    }

    pNext = pch;                                                                        // reset pointer to first character to parse (because no token was found)
    return true;                                                                        // token is not a function name (but can still be something else)
}


// ------------------------------------------------------------------------
// *   try to parse next characters as an external (user) function name   *
// ------------------------------------------------------------------------

bool MyParser::parseAsExternFunction( char*& pNext, parseTokenResult_type& result ) {

    if ( _isProgramCmd || _isDeleteVarCmd ) { return true; }                             // looking for an UNQUALIFIED identifier name; prevent it's mistaken for a variable name (same format)

    // 1. Is this token a function name ? 
    // ----------------------------------

    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    if ( !isalpha( pNext [0] ) ) { return true; }                                       // first character is not a letter ? Then it's not an identifier name (it can still be something else)
    while ( isalnum( pNext [0] ) || (pNext [0] == '_') ) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    // peek next character: is it a left parenthesis ?
    char* peek1 = pNext; while ( peek1 [0] == ' ' ) { peek1++; }

    if ( peek1 [0] != term_leftPar [0] ) { pNext = pch; return true; }   // not an external function 
    if ( (_isExtFunctionCmd) && (_parenthesisLevel > 0) ) { pNext = pch; return true; }        // only array parameter allowed now
    if ( _isAnyVarCmd ) { pNext = pch; return true; }                                   // is a variable declaration: not an external function

    // name already in use as global or user variable name ? Then it's not an external function
    bool createNewName = false;
    int index = getIdentifier( _pcalculator->programVarNames, _pcalculator->_programVarNameCount, _pcalculator->MAX_PROGVARNAMES, pch, pNext - pch, createNewName );
    if ( index != -1 ) { pNext = pch; return true; }                // is a variable
    index = getIdentifier( _pcalculator->userVarNames, _pcalculator->_userVarCount, _pcalculator->MAX_USERVARNAMES, pch, pNext - pch, createNewName );
    if ( index != -1 ) { pNext = pch; return true; }                // is a user variable


    // 2. Is a function name allowed here ? 
    // ------------------------------------

    if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is an external function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_functionNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
    if ( !tokenAllowed ) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

    // if function name is too long, reset pointer to first character to parse, indicate error and return
    if ( pNext - pch > _maxIdentifierNameLen ) { pNext = pch; result = result_identifierTooLong;  return false; }

    // if in immediate mode: the function must be defined earlier (in a program)
    if ( !_pcalculator->_programMode ) {
        createNewName = false;                                                              // only check if function is defined, do NOT YET create storage for it
        index = getIdentifier( _pcalculator->extFunctionNames, _pcalculator->_extFunctionCount, _pcalculator->MAX_EXT_FUNCS, pch, pNext - pch, createNewName );
        if ( index == -1 ) { pNext = pch; result = result_undefinedFunctionOrArray; return false; }
    }

    // token is an external function (definition or call), and it's allowed here


    // 3. Has function attribute storage already been created for this function ? (because of a previous function definition or a previous function call)
    // --------------------------------------------------------------------------------------------------------------------------------------------------

    createNewName = true;                                                              // if new external function, create storage for it
    index = getIdentifier( _pcalculator->extFunctionNames, _pcalculator->_extFunctionCount, _pcalculator->MAX_EXT_FUNCS, pch, pNext - pch, createNewName );
    if ( index == -1 ) { pNext = pch; result = result_maxExtFunctionsReached; return false; }
    char* funcName = _pcalculator->extFunctionNames [index];                                    // either new or existing function name
    if ( createNewName ) {                                                                      // new function name
        // init max (bits 7654) & min (bits 3210) allowed n° OR actual n° of arguments; store in last position (behind string terminating character)
        funcName [_maxIdentifierNameLen + 1] = c_extFunctionFirstOccurFlag;                          // max (bits 7654) < (bits 3210): indicates value is not yet updated by parsing previous calls closing parenthesis
        _pcalculator->extFunctionData [index].pExtFunctionStartToken = nullptr;                      // initialize. Pointer will be set when function definition is parsed (checked further down)
        _pcalculator->extFunctionData [index].paramIsArrayPattern [1] = 0x80;                        // set flag to indicate a new function name is parsed (definition or call)
        _pcalculator->extFunctionData [index].paramIsArrayPattern [0] = 0x00;                        // boundary alignment 
    }

    // if function storage was created already: check for double function definition
    else if ( _isExtFunctionCmd ) {                                                     // this is a function definition (not a call)
        // pointer to function starting token already defined: this is a double definition
        if ( _pcalculator->extFunctionData [index].pExtFunctionStartToken != nullptr ) { pNext = pch; result = result_functionAlreadyDefinedBefore; return false; }
    }

    // Is this an external function definition( not a function call ) ?
    if ( _isExtFunctionCmd ) {
        _pcalculator->extFunctionData [index].pExtFunctionStartToken = _pcalculator->_programCounter;            // store pointer to function start token 
        // variable name usage array: reset in-procedure reference flags to be able to keep track of in-procedure variable value types used
        // KEEP all other settings
        for ( int i = 0; i < _pcalculator->_programVarNameCount; i++ ) { _pcalculator->globalVarType [i] = (_pcalculator->globalVarType [i] & ~_pcalculator->var_qualifierMask) | _pcalculator->var_qualToSpecify; }
        _pcalculator->_localVarCountInFunction = 0;             // reset local and parameter variable count in function
        _pcalculator->_paramOnlyCountInFunction = 0;             // reset local and parameter variable count in function
        _pcalculator->extFunctionData [index].localVarCountInFunction = 0;
        _pcalculator->extFunctionData [index].paramOnlyCountInFunction = 0;

        _pFunctionDefStack = _pParsingStack;               // stack level for FUNCTION definition block
        _pFunctionDefStack->openBlock.fcnBlock_functionIndex = index;  // store in BLOCK stack level: only if function def

    }

    // if function was defined prior to this occurence (which is then a call), retrieve min & max allowed arguments for checking actual argument count
    // if function not yet defined: retrieve current state of min & max of actual argument count found in COMPLETELY PARSED previous calls to same function 
    // if no previous occurences at all: data is not yet initialized (which is ok)
    _minFunctionArgs = ((funcName [_maxIdentifierNameLen + 1]) >> 4) & 0x0F;         // use only for passing to parsing stack
    _maxFunctionArgs = (funcName [_maxIdentifierNameLen + 1]) & 0x0F;
    _functionIndex = index;


    // 4. Store token in program memory
    // --------------------------------

    Interpreter::TokenIsExtFunction* pToken = (Interpreter::TokenIsExtFunction*) _pcalculator->_programCounter;
    pToken->tokenType = Interpreter::tok_isExternFunction | (sizeof( Interpreter::TokenIsExtFunction ) << 4);
    pToken->identNameIndex = index;

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = Interpreter::tok_isExternFunction;
    _lastTokenIsTerminal = false;

    _pcalculator->_programCounter += sizeof( Interpreter::TokenIsExtFunction );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// --------------------------------------------------
// *   try to parse next characters as a variable   *
// --------------------------------------------------

bool MyParser::parseAsVariable( char*& pNext, parseTokenResult_type& result ) {

    if ( _isProgramCmd || _isDeleteVarCmd ) { return true; }                             // looking for an UNQUALIFIED identifier name; prevent it's mistaken for a variable name (same format)

    // 1. Is this token a variable name ? 
    // ----------------------------------

    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    if ( !isalpha( pNext [0] ) ) { return true; }                                       // first character is not a letter ? Then it's not a variable name (it can still be something else)
    while ( isalnum( pNext [0] ) || (pNext [0] == '_') ) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')


    // 2. Is a variable name allowed here ? 
    // ------------------------------------

    if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is a variable, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if ( !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_variableNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen);
    if ( !tokenAllowed ) { pNext = pch; result = result_variableNotAllowedHere; return false; ; }

    // scalar or array variable ? (could still be function 'array' argument; this will be detected further below)
    char* peek1 = pNext; while ( peek1 [0] == ' ' ) { peek1++; }                                                // peek next character: is it a left parenthesis ?
    char* peek2; if ( peek1 [0] == term_leftPar [0] ) { peek2 = peek1 + 1; while ( peek2 [0] == ' ' ) { peek2++; } }         // also find the subsequent character
    bool isArray = (peek1 [0] == term_leftPar [0]);
    if ( _isExtFunctionCmd ) {                                     // only (array) parameter allowed now
        if ( _parenthesisLevel == 0 ) { pNext = pch; result = result_functionDefExpected; return false; }           // is not an array parameter declaration
        if ( isArray && (_parenthesisLevel == 1) && (peek2 [0] != term_rightPar [0]) ) { pNext = pch; result = result_arrayParamExpected; return false; }           // is not an array parameter declaration
    }

    if ( _isAnyVarCmd ) {
        if ( _varDefAssignmentFound ) { pNext = pch; result = result_constantValueExpected; return false; }
    }

    // Note: in a declaration statement, operators other than assignment are not allowed, which is detected in terminal token parsing
    // -> if previous token was operator: it's an assignment
    bool isParamDecl = (_isExtFunctionCmd);                                          // parameter declarations: initialising ONLY with a constant, not with a variable
    bool isAssgnmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;
    if ( isParamDecl && isAssgnmentOp )                                                    // if operator: it is an assignment
    {
        pNext = pch; result = result_variableNotAllowedHere; return false;
    }

    bool isArrayDimSpec = (_isAnyVarCmd) && (_parenthesisLevel > 0);                    // array declaration: dimensions must be number constants (global, static, local arrays)
    if ( isArrayDimSpec ) { pNext = pch; result = result_variableNotAllowedHere; return false; }

    // if variable name is too long, reset pointer to first character to parse, indicate error and return
    if ( pNext - pch > _maxIdentifierNameLen ) { pNext = pch; result = result_identifierTooLong;  return false; }

    // name already in use as external function name ?
    bool createNewName { false };
    int varNameIndex = getIdentifier( _pcalculator->extFunctionNames, _pcalculator->_extFunctionCount, _pcalculator->MAX_EXT_FUNCS, pch, pNext - pch, createNewName );
    if ( varNameIndex != -1 ) { pNext = pch; result = result_varNameInUseForFunction; return false; }

    // token is a variable NAME, and a variable is allowed here


    // 3. Check whether this name exists already for variables, and create if needed
    // -----------------------------------------------------------------------------------------------------------------------------------------------

    // note that multiple distinct program variables (global, static, local) and function parameters can all share the same name, which is only stored once 
    // user variable names are stored separately

    // set pointers to variable name storage areas for program variable names and user variable names, respectively
    char** pvarNames [2]; pvarNames [0] = _pcalculator->programVarNames; pvarNames [1] = _pcalculator->userVarNames;
    int* varNameCount [2]; varNameCount [0] = &_pcalculator->_programVarNameCount; varNameCount [1] = &_pcalculator->_userVarCount;
    int maxVarNames [2]; maxVarNames [0] = _pcalculator->MAX_PROGVARNAMES; maxVarNames [1] = _pcalculator->MAX_USERVARNAMES;
    char* varType [2]; varType [0] = _pcalculator->globalVarType; varType [1] = _pcalculator->userVarType;
    Interpreter::Val* varValues [2]; varValues [0] = _pcalculator->globalVarValues; varValues [1] = _pcalculator->userVarValues;

    // 0: program variable, 1: user variable
    int primaryNameRange = _pcalculator->_programMode ? 0 : 1;
    int secondaryNameRange = _pcalculator->_programMode ? 1 : 0;

    // init: program parsing: assume program variable name for now; immediate mode parsing: assume user variable name
    bool isProgramVar = _pcalculator->_programMode;
    int activeNameRange = primaryNameRange;

    // check if variable exists already (program mode: as program variable; immediate mode: as user variable)
    // if a variable DEFINITION, then create variable name if it does not exist yet
    // note: this only concerns the NAME, not yet the actual variable (program variables: local, static, param and global variables can all share the same name)
    createNewName = _isExtFunctionCmd || _isAnyVarCmd;
    varNameIndex = getIdentifier( pvarNames [primaryNameRange], *varNameCount [primaryNameRange], maxVarNames [primaryNameRange], pch, pNext - pch, createNewName );

    if ( _isExtFunctionCmd || _isAnyVarCmd ) {               // variable or parameter DEFINITION: if name didn't exist, it should have been created now
        if ( varNameIndex == -1 ) { pNext = pch; result = result_maxVariableNamesReached; return false; }      // name still does not exist: error
        // name exists (newly created or pre-existing)
        // variable name is new: clear all variable value type flags and indicate 'qualifier not determined yet'
        // variable value type (array, float or string) will be set later
        if ( createNewName ) {
            varType [primaryNameRange][varNameIndex] = _pcalculator->var_qualToSpecify;      // new name was created now
            // NEW user variables only: if array definition, then decrease variable count by 1 for now, and increase by 1 again when array dim spec is validated
            // this ensures that a scalar is not created when an error is encountered later within dim spec parsing
            if ( !isProgramVar && isArray ) { (*varNameCount [primaryNameRange])--; }    // the variable is not considered 'created' yet
        }
    }
    else { // not a variable definition, just a variable reference
        if ( varNameIndex == -1 ) {
            // variable name does not exist in primary range (and no error produced, so it was not a variable definition):
            // check if the name is defined in the secondary name range
            varNameIndex = getIdentifier( pvarNames [secondaryNameRange], *varNameCount [secondaryNameRange], maxVarNames [secondaryNameRange], pch, pNext - pch, createNewName );
            if ( varNameIndex == -1 ) { pNext = pch; result = result_varNotDeclared; return false; }  // if the name doesn't exist, the variable doesn't
            isProgramVar = !_pcalculator->_programMode;                  // program parsing: is program variable; immediate mode: is user variable
            activeNameRange = secondaryNameRange;
        }

        // user variable referenced in program: set flag
        if ( _pcalculator->_programMode && !isProgramVar ) { varType [activeNameRange][varNameIndex] = varType [activeNameRange][varNameIndex] | _pcalculator->var_userVarUsedByProgram; }
    }


    // 4. The variable NAME exists now, but we still need to check whether storage space for the variable itself has been created / allocated
    //    Note: local variable storage is created at runtime
    // --------------------------------------------------------------------------------------------------------------------------------------

    bool variableNotYetKnown = false;                                                                             // init

    // 4.1 Currently parsing a FUNCTION...END block ? 
    // ----------------------------------------------

    // note: only while parsing program instructions
    if ( _extFunctionBlockOpen ) {
        // first use of a particular variable NAME in a function ?  (in a variable declaration, or just using the name in an expression)
        bool isFirstVarNameRefInFnc = (((uint8_t) varType [activeNameRange][varNameIndex] & _pcalculator->var_qualifierMask) == _pcalculator->var_qualToSpecify);
        if ( isFirstVarNameRefInFnc ) {                                                                         // variable not yet referenced within currently parsed procedure

            // determine variable qualifier
            // if a variable definition statement: set qualifier to parameter, local or static (global and usar variable definition: not possible in a function) 
            // if a variable reference: we will determine the qualifier in a moment 
            uint8_t varQual = _isExtFunctionCmd ? _pcalculator->var_isParamInFunc : _isLocalVarCmd ? _pcalculator->var_isLocalInFunc : _isStaticVarCmd ? _pcalculator->var_isStaticInFunc : _pcalculator->var_qualToSpecify;
            varType [activeNameRange][varNameIndex] = (varType [activeNameRange][varNameIndex] & ~_pcalculator->var_qualifierMask) | varQual;     //set qualifier bits (will be stored in token AND needed during parsing current procedure)

            if ( _isStaticVarCmd ) {                                             // definition of NEW static variable for function
                variableNotYetKnown = true;
                if ( _pcalculator->_staticVarCount == _pcalculator->MAX_STAT_VARS ) { pNext = pch; result = result_maxStaticVariablesReached; return false; }
                _pcalculator->programVarValueIndex [varNameIndex] = _pcalculator->_staticVarCount;
                if ( !isArray ) { _pcalculator->staticVarValues [_pcalculator->_staticVarCount].realConst = 0.; }           // initialize variable (if initializer and/or array: will be overwritten)
                _pcalculator->staticVarType [_pcalculator->_staticVarCount] = _pcalculator->var_isFloat;                                         // init (for array or scalar)
                _pcalculator->staticVarType [_pcalculator->_staticVarCount] = (_pcalculator->staticVarType [_pcalculator->_staticVarCount] & ~_pcalculator->var_isArray); // init (array flag will be added when storage is created)    
                _pcalculator->_staticVarCount++;
            }

            else if ( _isExtFunctionCmd || _isLocalVarCmd ) {               // definition of NEW parameter (in function definition) or NEW local variable for function
                variableNotYetKnown = true;
                if ( _pcalculator->_localVarCountInFunction == _pcalculator->MAX_LOC_VARS_IN_FUNC ) { pNext = pch; result = result_maxLocalVariablesReached; return false; }
                _pcalculator->programVarValueIndex [varNameIndex] = _pcalculator->_localVarCountInFunction;
                // param and local variables: array flag temporarily stored during function parsing       
                // storage space creation and initialisation will occur when function is called durig execution 
                _pcalculator->localVarType [_pcalculator->_localVarCountInFunction] = (_pcalculator->localVarType [_pcalculator->_localVarCountInFunction] & ~_pcalculator->var_isArray) |
                    (isArray ? _pcalculator->var_isArray : 0); // init (no storage needs to be created: set array flag here) 
                _pcalculator->_localVarCountInFunction++;
                if ( _isExtFunctionCmd ) { _pcalculator->_paramOnlyCountInFunction++; }

                // ext. function index: in stack level for FUNCTION definition command
                int fcnIndex = _pFunctionDefStack->openBlock.fcnBlock_functionIndex;
                _pcalculator->extFunctionData [fcnIndex].localVarCountInFunction = _pcalculator->_localVarCountInFunction;        // after incrementing count
                if ( _isExtFunctionCmd ) { _pcalculator->extFunctionData [fcnIndex].paramOnlyCountInFunction = _pcalculator->_paramOnlyCountInFunction; }
            }

            else {
                // not a variable definition:  CAN BE an EXISTING global or user variable, within a function
                // it CANNOT be a local or static variable, because this is the first reference of this variable name in the function and it's not a variable definition
                // if the variable name refers to a user variable, the variable exists, so it's known then
                variableNotYetKnown = isProgramVar ? (!(varType [activeNameRange][varNameIndex] & _pcalculator->var_hasGlobalValue)) : false;
                // variable is NEW ? Variable has not been declared
                if ( variableNotYetKnown ) {              // undeclared global program variable                                                             
                    pNext = pch; result = result_varNotDeclared; return false;
                }
                // existing global or user variable
                varType [activeNameRange][varNameIndex] = (varType [activeNameRange][varNameIndex] & ~_pcalculator->var_qualifierMask) | (isProgramVar ? _pcalculator->var_isGlobal : _pcalculator->var_isUser);
            }                                                                                               // IS the use of an EXISTING global or user variable, within a function

        }

        else {  // if variable name already referenced before in function (global / user variable use OR param, local, static declaration), then it has been defined already
            bool isLocalDeclaration = (_isExtFunctionCmd || _isLocalVarCmd || _isStaticVarCmd); // local variable declaration ? (parameter, local, static)
            if ( isLocalDeclaration ) { pNext = pch; result = result_varRedeclared; return false; }
        }
    }


    // 4.2 NOT parsing FUNCTION...END block 
    // ------------------------------------

    // note: while parsing program instructions AND while parsing instructions entered in immediate mode
    else {
        variableNotYetKnown = !(varType [activeNameRange][varNameIndex] & (isProgramVar ? _pcalculator->var_hasGlobalValue : _pcalculator->var_isUser));
        // qualifier 'var_isGlobal' (program variables): set, because could be cleared by previously parsed function (will be stored in token)
        varType [activeNameRange][varNameIndex] = (varType [activeNameRange][varNameIndex] & ~_pcalculator->var_qualifierMask) | (isProgramVar ? _pcalculator->var_isGlobal : _pcalculator->var_isUser);

        if ( variableNotYetKnown ) {
            if ( !_isGlobalOrUserVarCmd ) {                           // all variable must be defined before parsing a reference to it 
                pNext = pch; result = result_varNotDeclared; return false;
            }

            // is a declaration of a new program global variable (in program mode), or a new user user variable (in immediate mode) 
            // variable qualifier : don't care for now (global varables: reset at start of next external function parsing)
            if ( !isArray ) { varValues [activeNameRange][varNameIndex].realConst = 0.; }                  // initialize variable (if initializer and/or array: will be overwritten)
            varType [activeNameRange][varNameIndex] = varType [activeNameRange][varNameIndex] | _pcalculator->var_isFloat;         // init (for scalar and array)
            varType [activeNameRange][varNameIndex] = varType [activeNameRange][varNameIndex] | (isProgramVar ? _pcalculator->var_hasGlobalValue : _pcalculator->var_isUser);   // set 'has global value' bit
            varType [activeNameRange][varNameIndex] = (varType [activeNameRange][varNameIndex] & ~_pcalculator->var_isArray); // init (array flag may only be added when storage is created) 
        }

        else {  // the global or user variable exists already: check for double definition
            if ( _isGlobalOrUserVarCmd ) {
                if ( !(_pcalculator->_programMode ^ isProgramVar) ) { pNext = pch; result = result_varRedeclared; return false; }
            }
        }
    }


    // 5. If NOT a new variable, check if it corresponds to the variable definition (scalar or array) and retrieve array dimension count (if array)
    //    If it is a FOR loop control variable, check that it is not in use by a FOR outer loop (in same function)
    // --------------------------------------------------------------------------------------------------------------------------------------------

    uint8_t varQualifier = varType [activeNameRange][varNameIndex] & _pcalculator->var_qualifierMask;  // use to determine parameter, local, static, global
    bool isGlobalOrUserVar = isProgramVar ?
        ((_extFunctionBlockOpen && (varQualifier == _pcalculator->var_isGlobal)) ||                             // NOTE: outside a function, test against 'var_hasGlobalValue'
            (!_extFunctionBlockOpen && (varType [activeNameRange][varNameIndex] & _pcalculator->var_hasGlobalValue))) :
        varType [activeNameRange][varNameIndex] & _pcalculator->var_isUser;
    bool isStaticVar = (_extFunctionBlockOpen && (varQualifier == _pcalculator->var_isStaticInFunc));
    bool isLocalVar = (_extFunctionBlockOpen && (varQualifier == _pcalculator->var_isLocalInFunc));
    bool isParam = (_extFunctionBlockOpen && (varQualifier == _pcalculator->var_isParamInFunc));
    int valueIndex = isGlobalOrUserVar ? varNameIndex : _pcalculator->programVarValueIndex [varNameIndex];


    if ( !variableNotYetKnown ) {  // not a variable definition but a variable use
        bool existingArray = false;
        _pcalculator->_arrayDimCount = 0;                  // init: if new variable (or no array), then set dimension count to zero

        existingArray = isGlobalOrUserVar ? (varType [activeNameRange][valueIndex] & _pcalculator->var_isArray) :
            isStaticVar ? (_pcalculator->staticVarType [valueIndex] & _pcalculator->var_isArray) :
            (_pcalculator->localVarType [valueIndex] & _pcalculator->var_isArray);           // param or local

        // if not a function definition: array name does not have to be followed by a left parenthesis (passing the array and not an array element)
        if ( !_isExtFunctionCmd ) {
            // Is this variable part of a function call argument, without further nesting of parenthesis, and has it been defined as an array ? 
            bool isPartOfFuncCallArgument = (_parenthesisLevel > 0) ? (_pParsingStack->openPar.flags & (_pcalculator->intFunctionBit | _pcalculator->extFunctionBit)) : false;
            if ( isPartOfFuncCallArgument && existingArray ) {
                // if NOT followed by an array element enclosed in parenthesis, it references the complete array
                // this is only allowed if not part of an expression: check

                bool isFuncCallArgument = _lastTokenIsTerminal ? ((_lastTermCode == termcod_leftPar) || (_lastTermCode == termcod_comma)) : false;
                isFuncCallArgument = isFuncCallArgument && ((peek1 [0] == term_comma [0]) || (peek1 [0] == term_rightPar [0]));
                if ( isFuncCallArgument ) { isArray = true; }
            }
            if ( existingArray ^ isArray ) { pNext = pch; result = isArray ? result_varDefinedAsScalar : result_varDefinedAsArray; return false; }
        }


        // if existing array: retrieve dimension count against existing definition, for testing against definition afterwards
        if ( existingArray ) {
            float* pArray = nullptr;
            if ( isStaticVar ) { pArray = _pcalculator->staticVarValues [valueIndex].pArray; }
            else if ( isGlobalOrUserVar ) { pArray = varValues [activeNameRange][valueIndex].pArray; }
            else if ( isLocalVar ) { pArray = (float*) _pcalculator->localVarDims [valueIndex]; }   // dimensions and count are stored in a float
            // retrieve dimension count from array element 0, character 3 (char 0 to 2 contain the dimensions) 
            _pcalculator->_arrayDimCount = isParam ? _pcalculator->MAX_ARRAY_DIMS : ((char*) pArray) [3];
        }


        // if FOR loop control variable, check it is not in use by a FOR outer loop of same function  
        if ( (_lastTokenType == Interpreter::tok_isReservedWord) && (_blockLevel > 1) ) {     // minimum 1 other (outer) open block
            Interpreter::TokenPointer prgmCnt;
            prgmCnt.pTokenChars = _pcalculator->_programStorage + _lastTokenStep;  // address of reserved word
            int tokenIndex = prgmCnt.pResW->tokenIndex;
            CmdBlockDef cmdBlockDef = _resWords [tokenIndex].cmdBlockDef;

            // variable is a control variable of a FOR loop ?
            if ( cmdBlockDef.blockType == block_for ) {

                // check if control variable is in use by a FOR outer loop
                LE_parsingStack* pStackLvl = (LE_parsingStack*) parsingStack.getLastListElement();        // current open block level
                do {
                    pStackLvl = (LE_parsingStack*) parsingStack.getPrevListElement( pStackLvl );    // an outer block stack level
                    if ( pStackLvl == nullptr ) { break; }
                    if ( pStackLvl->openBlock.cmdBlockDef.blockType == block_for ) {    // outer block is FOR loop as well
                        // find token for control variable for this outer loop
                        uint16_t tokenStep { 0 };
                        memcpy( &tokenStep, pStackLvl->openBlock.tokenStep, sizeof( char [2] ) );
                        tokenStep = tokenStep + sizeof( Interpreter::TokenIsResWord );  // now pointing to control variable of outer loop

                        // compare variable qualifier, name index and value index of outer and inner loop control variable
                        prgmCnt.pTokenChars = _pcalculator->_programStorage + tokenStep;  // address of outer loop control variable
                        bool isSameControlVariable = ((varQualifier == uint8_t( prgmCnt.pVar->identInfo & _pcalculator->var_qualifierMask ))
                            && ((int) prgmCnt.pVar->identNameIndex == varNameIndex)
                            && ((int) prgmCnt.pVar->identValueIndex == valueIndex));
                        if ( isSameControlVariable ) { pNext = pch; result = result_varControlVarInUse; return false; }
                    }
                } while ( true );
            }
        }
    }

    _variableNameIndex = varNameIndex;          // will be pushed to parsing stack
    _variableQualifier = varQualifier;


    // 6. Store token in program memory
    // --------------------------------

    Interpreter::TokenIsVariable* pToken = (Interpreter::TokenIsVariable*) _pcalculator->_programCounter;
    pToken->tokenType = Interpreter::tok_isVariable | (sizeof( Interpreter::TokenIsVariable ) << 4);
    pToken->identInfo = varQualifier | (isArray ? _pcalculator->var_isArray : 0);              // qualifier, array flag ? (fixed -> store in token)  
    pToken->identNameIndex = varNameIndex;
    pToken->identValueIndex = valueIndex;                      // points to storage area element for the variable  

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastVariableTokenStep = _lastTokenStep;
    _lastTokenType = Interpreter::tok_isVariable;
    _lastTokenIsTerminal = false;

    _pcalculator->_programCounter += sizeof( Interpreter::TokenIsVariable );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ----------------------------------------------------------------------
// *   try to parse next characters as an UNQUALIFIED identifier name   *
// ----------------------------------------------------------------------

bool MyParser::parseAsIdentifierName( char*& pNext, parseTokenResult_type& result ) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    if ( (!_isProgramCmd) && (!_isDeleteVarCmd) ) { return true; }

    if ( !isalpha( pNext [0] ) ) { return true; }                                       // first character is not a letter ? Then it's not an identifier name (it can still be something else)
    while ( isalnum( pNext [0] ) || (pNext [0] == '_') ) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    // if variable name is too long, reset pointer to first character to parse, indicate error and return
    if ( pNext - pch > _maxIdentifierNameLen ) { pNext = pch; result = result_identifierTooLong;  return false; }

    // token is an identifier name, and it's allowed here
    char* pProgramName = new char [pNext - pch + 1];                    // create char array on the heap to store identifier name, including terminating '\0'
    _heapObjectCount++;
#if printCreateDeleteHeapObjects
    Serial.print( "(HEAP) Create object # " ); Serial.print( _heapObjectCount ); Serial.print( ": generic name, addr " );
    Serial.println( (uint32_t) pProgramName - RAMSTART );
#endif
    strncpy( pProgramName, pch, pNext - pch );                            // store identifier name in newly created character array
    pProgramName [pNext - pch] = '\0';                                                 // string terminating '\0'

    Interpreter::TokenIsStringCst* pToken = (Interpreter::TokenIsStringCst*) _pcalculator->_programCounter;
    pToken->tokenType = Interpreter::tok_isGenericName | (sizeof( Interpreter::TokenIsStringCst ) << 4);
    memcpy( pToken->pStringConst, &pProgramName, sizeof( pProgramName ) );            // pointer not necessarily aligned with word size: copy memory instead

    bool doNonLocalVarInit = (_lastTokenIsTerminal && (_isGlobalOrUserVarCmd || _isStaticVarCmd));

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = Interpreter::tok_isGenericName;
    _lastTokenIsTerminal = false;

    _pcalculator->_programCounter += sizeof( Interpreter::TokenIsStringCst );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// -----------------------------------------
// *   pretty print a parsed instruction   *
// -----------------------------------------
void MyParser::prettyPrintInstructions( bool printOneInstruction, char* startToken, char* errorProgCounter, int* sourceErrorPos ) {

    // input: stored tokens
    Interpreter::TokenPointer progCnt;
    progCnt.pTokenChars = (startToken == nullptr) ? _pcalculator->_programStart : startToken;
    int tokenType = *progCnt.pTokenChars & 0x0F;

    // output: printable token (text)
    const int maxCharsPretty { 100 };           // must be long enough to hold one token in text (e.g. a variable name)
    int outputLength = 0;                       // init: first position

    while ( tokenType != Interpreter::tok_no_token ) {                                                                    // for all tokens in token list
        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*progCnt.pTokenChars >> 4) & 0x0F;
        Interpreter::TokenPointer nextProgCnt;
        nextProgCnt.pTokenChars = progCnt.pTokenChars + tokenLength;
        int nextTokenType = *nextProgCnt.pTokenChars & 0x0F;                                                                // next token type (look ahead)
        bool errorTokenHasLeadingSpace = false;
        bool isSemicolon = false;

        char prettyToken [maxCharsPretty] = "";

        switch ( tokenType ) {
        case Interpreter::tok_isReservedWord:
        {
            Interpreter::TokenIsResWord* pToken = (Interpreter::TokenIsResWord*) progCnt.pTokenChars;
            bool nextIsTerminal = ((nextTokenType == Interpreter::tok_isTerminalGroup1) || (nextTokenType == Interpreter::tok_isTerminalGroup2) || (nextTokenType == Interpreter::tok_isTerminalGroup3));
            bool nextIsSemicolon = false;
            if ( nextIsTerminal ) {
                int nextTokenIndex = ((nextProgCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F);
                nextTokenIndex += ((nextTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (nextTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
                nextIsSemicolon = (_terminals [nextTokenIndex].terminalCode == termcod_semicolon);
            }

            sprintf( prettyToken, nextIsSemicolon ? "%s" : "%s ", _resWords [progCnt.pResW->tokenIndex]._resWordName );
            break;
        }

        case Interpreter::tok_isInternFunction:
            strcpy( prettyToken, _functions [progCnt.pIntFnc->tokenIndex].funcName );
            break;

        case Interpreter::tok_isExternFunction:
        {
            int identNameIndex = (int) progCnt.pExtFnc->identNameIndex;   // external function list element
            char* identifierName = _pcalculator->extFunctionNames [identNameIndex];
            strcpy( prettyToken, identifierName );
            break;
        }

        case Interpreter::tok_isVariable:
        {
            int identNameIndex = (int) (progCnt.pVar->identNameIndex);
            uint8_t varQualifier = progCnt.pVar->identInfo;
            bool isUserVar = (progCnt.pVar->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isUser;
            char* identifierName = isUserVar ? _pcalculator->userVarNames [identNameIndex] : _pcalculator->programVarNames [identNameIndex];
            strcpy( prettyToken, identifierName );
            break;
        }

        case Interpreter::tok_isRealConst:
        {
            float f;
            memcpy( &f, progCnt.pFloat->realConst, sizeof( f ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( prettyToken, "%.3G", f );
            break;
        }

        case Interpreter::tok_isStringConst:
        case Interpreter::tok_isGenericName:
        {
            char* pAnum { nullptr };
            memcpy( &pAnum, progCnt.pAnumP->pStringConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( prettyToken, "\"%s\"", pAnum );
            break;
        }

        default:  // terminal
        {
            int index = (progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F;
            index += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);

            if ( _terminals [index].terminalCode == termcod_concat ) {
                strcat( prettyToken, " " );          // readability
                errorTokenHasLeadingSpace = true;
            }
            strcat( prettyToken, _terminals [index].terminalName );         // concatenate with empty string or single-space string
            if ( (_terminals [index].terminalCode == termcod_semicolon) || (_terminals [index].terminalCode == termcod_concat) ) {
                strcat( prettyToken, " " );          // readability
            }

            isSemicolon = (_terminals [index].terminalCode == termcod_semicolon);
            break; }
        }


        // print pretty token
        // ------------------

        int tokenSourceLength = strlen( prettyToken );
        if ( isSemicolon ) {
            if ( (nextTokenType != Interpreter::tok_no_token) && !printOneInstruction ) { _pcalculator->_pConsole->print( prettyToken ); }
        }
        else { _pcalculator->_pConsole->print( prettyToken ); }

        // if printing one instruction, return output error position based on token where execution error was produced
        if ( printOneInstruction ) {
            if ( errorProgCounter == progCnt.pTokenChars ) {
                *sourceErrorPos = outputLength + (errorTokenHasLeadingSpace ? 1 : 0);
            }
            else if ( isSemicolon ) { break; }
            outputLength += tokenSourceLength;
        }


        // advance to next token
        // ---------------------

        progCnt.pTokenChars = nextProgCnt.pTokenChars;
        tokenType = nextTokenType;                                                     // next token type
    }

    // exit
    _pcalculator->_pConsole->println(); _pcalculator->_isPrompt = false;
}


/*
// -----------------------------------------
// *   pretty print a parsed instruction   *
// -----------------------------------------

void MyParser::old_prettyPrintProgram() {
    // define these variables outside switch statement, to prevent undefined behaviour
    const int maxCharsPretty { 100 };       //// check lengte
    char prettyToken [maxCharsPretty] = "";
    int identNameIndex, valueIndex;
    char s [100] = "";      //// check op overrun
    char qual [20] = "";
    char pch [3] = "";
    int len;
    int index;
    char* identifierName, * varStrValue;
    bool isStringValue;
    float f;
    char* pAnum;
    uint32_t funcStart = 0;
    char tokenInfo = 0;
    uint8_t varQualifier = 0;
    bool isArray;
    bool hasTokenStep;

    TokenPointer progCnt;
    progCnt.pTokenChars = _pcalculator->_programStart;
    int tokenType = *progCnt.pTokenChars & 0x0F;
    char pTokenStepPointedTo [2];
    uint16_t toTokenStep;
    TokenIsResWord* pTokenChars;

    while ( tokenType != '\0' ) {                                                                    // for all tokens in token list
        uint16_t tokenStep = (uint16_t) (progCnt.pTokenChars - _pcalculator->_programStorage);
        strcpy( prettyToken, "" );

        switch ( tokenType ) {
        case Interpreter::tok_isReservedWord:
            pTokenChars = (TokenIsResWord*) progCnt.pTokenChars;
            hasTokenStep = (_resWords [progCnt.pResW->tokenIndex].cmdBlockDef.blockType != block_none);
            if ( hasTokenStep ) {
                memcpy( &toTokenStep, pTokenChars->toTokenStep, sizeof( char [2] ) );
                sprintf( s, "(step %d) resW: %s, points to step %d", tokenStep, _resWords [progCnt.pResW->tokenIndex]._resWordName, toTokenStep );
            }
            else { sprintf( s, "(step %d) resW: %s", tokenStep, _resWords [progCnt.pResW->tokenIndex]._resWordName ); }
            break;

        case Interpreter::tok_isInternFunction:
            sprintf( s, "(step %d) int func: %s", tokenStep, _functions [progCnt.pIntFnc->tokenIndex].funcName );
            break;

        case Interpreter::tok_isExternFunction:
            identNameIndex = (int) progCnt.pExtFnc->identNameIndex;   // external function list element
            identifierName = _pcalculator->extFunctionNames [identNameIndex];
            funcStart = (uint32_t) _pcalculator->extFunctionData [identNameIndex].pExtFunctionStartToken;
            if ( funcStart != 0 ) { funcStart -= (uint32_t) _pcalculator->_programStorage; }
            sprintf( s, "(step %d) ext func nr %d: %s, start: %lu", tokenStep, identNameIndex, identifierName, funcStart );
            break;

        case Interpreter::tok_isVariable:
            identNameIndex = (int) (progCnt.pVar->identNameIndex);
            valueIndex = (int) (progCnt.pVar->identValueIndex);

            identifierName = _pcalculator->programVarNames [identNameIndex];
            tokenInfo = progCnt.pVar->identInfo;
            isArray = (tokenInfo & _pcalculator->var_isArray);
            varQualifier = (tokenInfo & _pcalculator->var_qualifierMask);

            //// aanpassen:
            ////isUserVar = (progCnt.pVar->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isUser;
            ////identifierName = isUserVar ? _pcalculator->userVarNames [identNameIndex] : _pcalculator->programVarNames [identNameIndex];

            isStringValue = (varQualifier == _pcalculator->var_isGlobal) ? (_pcalculator->globalVarType [valueIndex] & _pcalculator->var_typeMask) == _pcalculator->var_isStringPointer :
                (varQualifier == _pcalculator->var_isStaticInFunc) ? (_pcalculator->staticVarType [valueIndex] & _pcalculator->var_typeMask) == _pcalculator->var_isStringPointer :
                (_pcalculator->localVarType [valueIndex] & _pcalculator->var_typeMask) == _pcalculator->var_isStringPointer;


            strcpy( qual, varQualifier == _pcalculator->var_isGlobal ? "global" : varQualifier == _pcalculator->var_isParamInFunc ? "Param" :
                varQualifier == _pcalculator->var_isLocalInFunc ? "local" : varQualifier == _pcalculator->var_isStaticInFunc ? "static" : "???" );

            if ( isArray ) {
                sprintf( s, "(step %d) %s array: %s", tokenStep, qual, identifierName );
            }
            else {
                if ( isStringValue ) {
                    if ( varQualifier == _pcalculator->var_isGlobal ) { varStrValue = _pcalculator->globalVarValues [identNameIndex].pStringConst; } // also ok for array pointer
                    else if ( varQualifier == _pcalculator->var_isStaticInFunc ) { varStrValue = _pcalculator->staticVarValues [valueIndex].pStringConst; }
                    else { varStrValue = nullptr; }
                    sprintf( s, "(step %d) %s string: %s, AN cst: <%s>", tokenStep, qual, identifierName, (varStrValue == nullptr) ? "" : varStrValue );
                }

                else {
                    if ( varQualifier == _pcalculator->var_isGlobal ) { f = _pcalculator->globalVarValues [identNameIndex].realConst; }
                    else if ( varQualifier == _pcalculator->var_isStaticInFunc ) { f = _pcalculator->staticVarValues [valueIndex].realConst; }
                    else { f = 0. + valueIndex; }      // no local variable storage yet (test value only)
                    sprintf( s, "(step %d) %s float: %s, Num: %.3G", tokenStep, qual, identifierName, f );
                }
            }
            break;

        case Interpreter::tok_isRealConst:
            memcpy( &f, progCnt.pFloat->realConst, sizeof( f ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "(step %d) Num: %.3G", tokenStep, f );
            break;

        case Interpreter::tok_isStringConst:
            memcpy( &pAnum, progCnt.pAnumP->pStringConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "(step %d) AN cst: <%s>", tokenStep, pAnum );
            break;

        case Interpreter::tok_isGenericName:
            memcpy( &pAnum, progCnt.pAnumP->pStringConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "(step %d) Identifier name: %s", tokenStep, pAnum );
            break;

        case Interpreter::tok_isTerminalGroup1:
            len = strlen( singleCharTokens );
            index = (progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F;
            if ( index < len ) { pch [0] = singleCharTokens [index]; pch [1] = '\0'; }
            else {
                strcat( pch, ((index == len) ? "<=" : (index == len + 1) ? ">=" : "<>") );
            }
            sprintf( s, "(step %d) Op: %s", tokenStep, pch );
            break;

        case Interpreter::tok_isCommaSeparator:
            sprintf( s, "(step %d) Sep: %c", tokenStep, singleCharTokens [(progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        case Interpreter::tok_isSemiColonSeparator:
            sprintf( s, "(step %d) Sep: %c", tokenStep, singleCharTokens [(progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        case Interpreter::tok_isLeftParenthesis:
            sprintf( s, "(step %d) Par: %c", tokenStep, singleCharTokens [(progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        case Interpreter::tok_isRightParenthesis:
            sprintf( s, "(step %d) Par.: %c", tokenStep, singleCharTokens [(progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        }

        // append pretty printed token to character string (if still place left)
        if ( strlen( s ) <= maxCharsPretty ) { strcat( prettyToken, s ); }
        ////if ( strlen( prettyToken ) > 0 ) { Serial.println( prettyToken ); }
        int tokenLength = (tokenType >=Interpreter::tok_isTerminalGroup1) ? 1 : (*progCnt.pTokenChars >> 4) & 0x0F;
        progCnt.pTokenChars += tokenLength;
        tokenType = *progCnt.pTokenChars & 0x0F;
    }
}
*/

// ----------------------------
// *   print parsing result   *
// ----------------------------

void MyParser::printParsingResult( parseTokenResult_type result, int funcNotDefIndex, char* const pInstruction, int lineCount, char* const pErrorPos ) {
    char parsingInfo [_pcalculator->_maxInstructionChars];
    if ( result == result_tokenFound ) {                                                // prepare message with parsing result
        strcpy( parsingInfo, _pcalculator->_programMode ? "Program parsed without errors" : "" );
    }

    else  if ( (result == result_undefinedFunctionOrArray) && _pcalculator->_programMode ) {     // in program mode only 
        // during external function call parsing, it is not always known whether the function exists (because function can be defined after a call) 
        // -> a linenumber can not be given, but the undefined function can
        sprintf( parsingInfo, "\r\n  Parsing error %d: function: %s", result, _pcalculator->extFunctionNames [funcNotDefIndex] );
    }

    else {                                                                              // parsing error
        // instruction not parsed (because of error): print source instruction where error is located (can not 'unparse' yet for printing instruction)
        char point [pErrorPos - pInstruction + 3];                               // 2 extra positions for 2 leading spaces, 2 for '^' and '\0' characters
        memset( point, ' ', pErrorPos - pInstruction + 2 );
        point [pErrorPos - pInstruction + 2] = '^';
        point [pErrorPos - pInstruction + 3] = '\0';

        _pcalculator->_pConsole->print( "\r\n  " ); _pcalculator->_pConsole->println( pInstruction );
        _pcalculator->_pConsole->println( point );
        if ( _pcalculator->_programMode ) { sprintf( parsingInfo, "  Parsing error %d: statement ending at line %d", result, lineCount ); }
        else { sprintf( parsingInfo, "  Parsing error %d", result ); }
    }

    if ( strlen( parsingInfo ) > 0 ) { _pcalculator->_pConsole->println( parsingInfo ); _pcalculator->_isPrompt = false; }
};




