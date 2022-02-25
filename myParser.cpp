#include "myParser.h"

#define printCreateDeleteHeapObjects 0


/***********************************************************
*                    class MyLinkedLists                   *
*    append and remove list elements from linked list      *
***********************************************************/

// ---------------------------------------------
// *   initialisation of static class member   *
// ---------------------------------------------

int MyLinkedLists::_listIDcounter = 0;


// -------------------
// *   constructor   *
// -------------------

MyLinkedLists::MyLinkedLists() {
    _listID = _listIDcounter;
    _listIDcounter++;
}


// --------------------------------------------------
// *   append a list element to the end of a list   *
// --------------------------------------------------

char* MyLinkedLists::appendListElement( int size ) {
    ListElemHead* p = (ListElemHead*) (new char [sizeof( ListElemHead ) + size]);       // create list object with payload of specified size in bytes

    if ( _pFirstElement == nullptr ) {                                                  // not yet any elements
        _pFirstElement = p;
        p->pPrev = nullptr;                                                             // is first element in list: no previous element
    }
    else {
        _pLastElement->pNext = p;
        p->pPrev = _pLastElement;
    }
    _pLastElement = p;
    p->pNext = nullptr;
    _listElementCount++;
#if printCreateDeleteHeapObjects
    Serial.print( "(LIST) Create elem # " ); Serial.print( _listElementCount );
    Serial.print( " list ID " ); Serial.print( _listID );
    Serial.print( " addr " ); Serial.println( (uint32_t) p - RAMSTART );
#endif

    return (char*) (p + 1);                                          // pointer to payload of newly created element
}


// -----------------------------------------------------
// *   delete a heap object and remove it from list    *
// -----------------------------------------------------

char* MyLinkedLists::deleteListElement( void* pPayload ) {                              // input: pointer to payload of a list element

    ListElemHead* pElem = (ListElemHead*) pPayload;                                     // still points to payload: check if nullptr
    if ( pElem == nullptr ) { pElem = _pLastElement; }                                  // nullptr: delete last element in list (if it exists)
    else { pElem = pElem - 1; }                                                         // pointer to list element header

    if ( pElem == nullptr ) { return nullptr; }                                         // still nullptr: return

    ListElemHead* p = pElem->pNext;                                                     // remember return value

    // before deleting object, remove from list:
    // change pointers from previous element (or _pFirstPointer, if no previous element) and next element (or _pLastPointer, if no next element)
    ((pElem->pPrev == nullptr) ? _pFirstElement : pElem->pPrev->pNext) = pElem->pNext;
    ((pElem->pNext == nullptr) ? _pLastElement : pElem->pNext->pPrev) = pElem->pPrev;

#if printCreateDeleteHeapObjects
    Serial.print( "(LIST) Delete elem # " ); Serial.print( _listElementCount );
    Serial.print( " list ID " ); Serial.print( _listID );
    Serial.print( " addr " ); Serial.println( (uint32_t) pElem - RAMSTART );
#endif
    _listElementCount--;
    delete []pElem;
    return (char*) (p + 1);                                           // pointer to payload of next element in list, or nullptr if last element deleted
}


// ------------------------------------------
// *   delete all list elements in a list   *
// ------------------------------------------

void MyLinkedLists::deleteList() {
    if ( _pFirstElement == nullptr ) return;

    ListElemHead* pHead = _pFirstElement;
    while ( pHead != nullptr ) {
        char* pNextPayload = deleteListElement( (char*) (pHead + 1) );
        pHead = ((ListElemHead*) pNextPayload) - 1;                                     // points to list element header 
    }
}


// ----------------------------------------------------
// *   get a pointer to the first element in a list   *
// ----------------------------------------------------

char* MyLinkedLists::getFirstListElement() {
    return (char*) (_pFirstElement + 1);
}


//----------------------------------------------------
// *   get a pointer to the last element in a list   *
//----------------------------------------------------

char* MyLinkedLists::getLastListElement() {
    return (char*) (_pLastElement + 1);
}


// -------------------------------------------------------
// *   get a pointer to the previous element in a list   *
// -------------------------------------------------------

char* MyLinkedLists::getPrevListElement( void* pPayload ) {                                 // input: pointer to payload of a list element  
    if ( pPayload == nullptr ) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*) pPayload) - 1;                                     // points to list element header
    if ( pElem->pPrev == nullptr ) { return nullptr; }
    return (char*) (pElem->pPrev + 1);                                                      // points to payload of previous element
}


//----------------------------------------------------
// *   get a pointer to the next element in a list   *
//----------------------------------------------------

char* MyLinkedLists::getNextListElement( void* pPayload ) {
    if ( pPayload == nullptr ) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*) pPayload) - 1;                                     // points to list element header
    if ( pElem->pNext == nullptr ) { return nullptr; }
    return (char*) (pElem->pNext + 1);                                                      // points to payload of previous element
}


/***********************************************************
*                       class MyParser                     *
*             parse character string into tokens           *
***********************************************************/

// -------------------------------------------------
// *   // initialisation of static class members   *
// -------------------------------------------------

// commands (FUNCTION, FOR, ...): allowed command parameters (naming: cmdPar_<n[nnn]> with A'=variable with (optional) assignment, 'E'=expression, 'E'=expression, 'R'=reserved word
const char MyParser::cmdPar_N [4] { cmdPar_none, cmdPar_none, cmdPar_none, cmdPar_none };
const char MyParser::cmdPar_P [4] { cmdPar_programName, cmdPar_none, cmdPar_none, cmdPar_none };
const char MyParser::cmdPar_E [4] { cmdPar_expression, cmdPar_none, cmdPar_none, cmdPar_none };
const char MyParser::cmdPar_F [4] { cmdPar_extFunction, cmdPar_none, cmdPar_none, cmdPar_none };
const char MyParser::cmdPar_AEE [4] { cmdPar_varOptAssignment, cmdPar_expression, cmdPar_expression | cmdPar_optionalFlag , cmdPar_none };
const char MyParser::cmdPar_P_mult [4] { cmdPar_programName, cmdPar_programName | cmdPar_multipleFlag, cmdPar_none, cmdPar_none };
const char MyParser::cmdPar_AA_mult [4] { cmdPar_varOptAssignment,cmdPar_varOptAssignment | cmdPar_multipleFlag, cmdPar_none, cmdPar_none };

const char MyParser::cmdPar_test [4] { cmdPar_programName | cmdPar_optionalFlag, cmdPar_programName, cmdPar_programName | cmdPar_multipleFlag, cmdPar_none };  // test: either 0 or 2 to n parameters ok

// reserved words: names, allowed parameters (if used as command, starting an instruction) and type of block command
//// PROGRAM...END, DEBUG (in programma), STEP (manueel), PRINTPROG, CLEARPROG, CLEARALL, CLEARUSERVARS, VARS, PEEK, POKE, CTRL-C (abort),... 
const MyParser::ResWordDef MyParser::_resWords [] {
{"TEST", cmdPar_test, cmdDeleteVar, cmd_noRestrictions},

{"PROGRAM", cmdPar_P, cmdProgram, cmd_onlyProgramTop},
{"DELETE",cmdPar_P_mult, cmdDeleteVar, cmd_onlyImmediate},                                                      // variable list
{"CLEAR", cmdPar_N, cmdBlockOther, cmd_onlyImmediate},
{"VARS",cmdPar_N, cmdBlockOther, cmd_onlyImmediate},
{"FUNCTION",cmdPar_F, cmdBlockExtFunction,cmd_onlyInProgram},

{"STATIC", cmdPar_AA_mult, cmdStaticVar,cmd_onlyInFunctionBlock},                                         // minimum 1 variable (with optional cst assignment)
{"LOCAL", cmdPar_AA_mult, cmdLocalVar,cmd_onlyInFunctionBlock},                                             // minimum 1 variable (with optional cst assignment)
{"VAR", cmdPar_AA_mult, cmdGlobalVar,cmd_onlyOutsideFunctionBlock},                           // minimum 1 variable (with optional cst assignment)

{"FOR",cmdPar_AEE,cmdBlockFor, cmd_onlyImmediateOrInsideFunctionBlock},
{"WHILE",cmdPar_E,cmdBlockWhile, cmd_onlyImmediateOrInsideFunctionBlock},
{"IF", cmdPar_E, cmdBlockIf, cmd_onlyImmediateOrInsideFunctionBlock},
{"ELSEIF", cmdPar_E, cmdBlockIf_elseIf, cmd_onlyImmediateOrInsideFunctionBlock},
{"ELSE", cmdPar_N, cmdBlockIf_else, cmd_onlyImmediateOrInsideFunctionBlock},

{"BREAK", cmdPar_N,cmdBlockOpenBlock_loop,cmd_onlyImmediateOrInsideFunctionBlock},                                         // allowed if at least one open loop block (any level) 
{"CONTINUE", cmdPar_N,cmdBlockOpenBlock_loop,cmd_onlyImmediateOrInsideFunctionBlock },                                      // allowed if at least one open loop block (any level) 
{"RETURN", cmdPar_E, cmdBlockOpenBlock_function, cmd_onlyImmediateOrInsideFunctionBlock},                                   // allowed if currently an open function definition block 

{"END",cmdPar_N,cmdBlockGenEnd, cmd_noRestrictions},                                                    // closes one open command block
};

// internal (intrinsic) functions: name and min & max number of arguments taken
const MyParser::FuncDef MyParser::_functions [] { {"varAddress",1,1}, {"varIndirect",1 ,1},{"varName",1 ,1},
{"if", 3,3}, {"and",1,9}, {"or",1,9}, {"not",1,1}, {"sin",1,1}, {"cos",1,1}, {"tan",1,1} , {"time", 0,0} };
const char* const MyParser::singleCharTokens = "(),;:+-*/^<>=";                         // all one-character tokens


// -------------------
// *   constructor   *
// -------------------

MyParser::MyParser( Calculator* const pcalculator ) : _pcalculator( pcalculator ) {
    _resWordNo = (sizeof( _resWords )) / sizeof( _resWords [0] );
    _functionNo = (sizeof( _functions )) / sizeof( _functions [0] );

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

void MyParser::deleteArrayElementStringObjects( Calculator::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue ) {
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

void MyParser::deleteVariableValueObjects( Calculator::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue ) {
    int index = 0;
    while ( index < varNameCount ) {
        if ( !checkIfGlobalValue || (varType [index] & (_pcalculator->var_hasGlobalValue)) ) { // global value ?
            if ( varType [index] & (_pcalculator->var_isArray | _pcalculator->var_isStringPointer) ) {              // scalar string or array (of strings or floats)
#if printCreateDeleteHeapObjects
                Serial.print( "(HEAP) Delete heap object # " ); Serial.print( _heapObjectCount ); Serial.print( ": variable string value or array storage, addr " );
                Serial.println( (uint32_t) varValues [index].pAlphanumConst - RAMSTART );     // applicable to string and array (same pointer)
#endif
                delete []  varValues [index].pAlphanumConst;                                  // applicable to string and array (same pointer)
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
    TokPnt prgmCnt;
    prgmCnt.pToken = programStart;
    uint8_t tokenType = *prgmCnt.pToken & 0x0F;
    while ( tokenType != '\0' ) {                                                                    // for all tokens in token list
        if ( (tokenType == tok_isAlphaConst) || (tokenType == tok_isGenericName) ) {
#if printCreateDeleteHeapObjects
            memcpy( &pAnum, prgmCnt.pAnumP->pAlphanumConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            Serial.print( "(HEAP) Delete heap object # " ); Serial.print( _heapObjectCount ); Serial.print( ": string const value, addr " );
            Serial.println( (uint32_t) pAnum - RAMSTART );
#endif
            delete [] pAnum;
            _heapObjectCount--;
        }
        uint8_t tokenLength = (tokenType >= tok_isOperator) ? 1 : (*prgmCnt.pToken >> 4) & 0x0F;
        prgmCnt.pToken += tokenLength;
        tokenType = *prgmCnt.pToken & 0x0F;
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

    myStack.deleteList();                                                               // delete list to keep track of open parentheses and open command blocks
    _blockLevel = 0;
    _extFunctionBlockOpen = false;

    // init calculator variables: AFTER deleting heap objects
    _pcalculator->_programVarNameCount = 0;
    _pcalculator->_staticVarCount = 0;
    _pcalculator->_localVarCountInFunction = 0;
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
    float f;        // last token is a number constant: dimension spec
    char* pString;

    // parsing: initialize variables and arrays with a constant number or (arrays: empty) string

    // fetch variable location and attributes
    bool isArrayVar = ((TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identInfo & _pcalculator->var_isArray;
    bool isGlobalVar = (((TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isGlobal;
    bool isUserVar = (((TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isUser;
    int varValueIndex = ((TokenIsVariable*) (_pcalculator->_programStorage + varTokenStep))->identValueIndex;
    void* pVarStorage = isGlobalVar ? _pcalculator->globalVarValues : isUserVar ? _pcalculator->userVarValues : _pcalculator->staticVarValues;
    char* pVarTypeStorage = isGlobalVar ? _pcalculator->globalVarType : isUserVar ? _pcalculator->userVarType : _pcalculator->staticVarType;
    void* pArrayStorage;        // array storage (if array) 

    // fetch constant (numeric or alphanumeric) 
    bool isNumberCst = (tokenType_type) ((((TokenIsFloatCst*) (_pcalculator->_programStorage + constTokenStep))->tokenType) & 0x0F) == tok_isNumConst;
    if ( isNumberCst ) { memcpy( &f, ((TokenIsFloatCst*) (_pcalculator->_programStorage + constTokenStep))->numConst, sizeof( f ) ); }

    else { memcpy( &pString, ((TokenIsAlphanumCst*) (_pcalculator->_programStorage + constTokenStep))->pAlphanumConst, sizeof( pString ) ); }
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
    _lastTokenType_hold = tok_no_token;
    _lastTokenType = tok_no_token;                                                      // no token yet
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
    tokenType_type& t = _lastTokenType;
    char* pNext = pInputStart;                                                          // set to first character in instruction
    char* pNext_hold = pNext;

    do {                                                                                // parse ONE token in an instruction

        // determine token group of last token parsed (bits b4 to b0): this defines which tokens are allowed as next token
        _lastTokenGroup_sequenceCheck = ((t == tok_isOperator) || (t == tok_isCommaSeparator)) ? lastTokenGroup_0 :
            ((t == tok_no_token) || (t == tok_isSemiColonSeparator) || (t == tok_isReservedWord)) ? lastTokenGroup_1 :
            ((t == tok_isNumConst) || (t == tok_isAlphaConst) || (t == tok_isRightParenthesis)) ? lastTokenGroup_2 :
            ((t == tok_isInternFunction) || (t == tok_isExternFunction)) ? lastTokenGroup_3 :
            (t == tok_isLeftParenthesis) ? lastTokenGroup_4 : lastTokenGroup_5;     // token group 5: scalar or array variable

        // a space may be required between last token and next token (not yet known), if one of them is a reserved word
        // and the other token is either a reserved word, an alphanumeric constant or a parenthesis
        // space check result is OK if a check is not required or if a space is present anyway
        _leadingSpaceCheck = ((t == tok_isReservedWord) || (t == tok_isAlphaConst) || (t == tok_isRightParenthesis)) && (pNext [0] != ' ');

        // move to the first character of next token (within one instruction)
        while ( pNext [0] == ' ' ) { pNext++; }                                         // skip leading spaces
        if ( pNext [0] == '\0' ) { break; }                                              // safety: instruction was not ended by a semicolon (should never happen) 

        // parsing routines below try to parse characters as a token of a specific type
        // if a function returns true, then either proceed OR skip reminder of loop ('continue') if 'result' indicates a token has been found
        // if a function returns false, then break with 'result' containing the error

        _previousTokenType = _lastTokenType_hold;                                   // remember the second last parsed token during parsing of a next token
        _lastTokenType_hold = _lastTokenType;                                       // remember the last parsed token during parsing of a next token
        pNext_hold = pNext;

        do {                                                                                                                // one loop only
            if ( (_pcalculator->_programCounter + sizeof( TokenIsAlphanumCst ) + 1) > (_pcalculator->_programStart + _pcalculator->_programSize) ) { result = result_progMemoryFull; break; };
            if ( !parseAsResWord( pNext, result ) ) { break; } if ( result == result_tokenFound ) { continue; }             // check before checking for identifier  
            if ( !parseAsNumber( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }             // check before checking for single char token
            if ( !parseAsAlphanumConstant( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }
            if ( !parseTerminalToken( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }
            if ( !parseAsInternFunction( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }     // check before checking for identifier (ext. function / variable) 
            if ( !parseAsExternFunction( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }     // check before checking for variable
            if ( !parseAsVariable( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }
            if ( !parseAsIdentifierName( pNext, result ) ) { break; }  if ( result == result_tokenFound ) { continue; }     // at the end
            result = result_token_not_recognised;
        } while ( false );

        // one token parsed (or error)
        if ( result != result_tokenFound ) { break; }                                   // exit loop if token error (syntax, ...). Checked before checking command syntax
        if ( !checkCommandSyntax( result ) ) { pNext = pNext_hold; break; }             // exit loop if command syntax error (pNext altered: set correctly again)

    } while ( !(t == tok_isSemiColonSeparator) );                                       // exit loop if semicolon is found

    // one instruction parsed (or error: no token found OR command syntax error OR semicolon encountered): quit
    pInputStart = pNext;                                                                // set to next character (if error: indicates error position)
    return result;
}


// --------------------------------------------------------------------------------------------
// *   if instruction is a command (starting with a reserved word): apply additional checks   *
// *   this check is applied AFTER parsing each token and checking its syntax                 *
// --------------------------------------------------------------------------------------------

bool MyParser::checkCommandSyntax( parseTokenResult_type& result ) {                    // command syntax checks
    static tokenType_type secondLastTokenType = tok_no_token;                           // type of last token parsed
    static bool secondLastIsLvl0CommaSep = false;
    static bool isSecondExpressionToken = false;
    static bool expressionStartsWithVariable = false;
    static bool expressionStartsWithArrayVar = false;
    static bool expressionStartsWithGenericName = false;
    static bool isExpression = false;
    static uint8_t allowedParType = cmdPar_none;                                         // init

    // is the start of a new command ? Check previous token 
    bool isInstructionStart = (_lastTokenType_hold == tok_no_token) || (_lastTokenType_hold == tok_isSemiColonSeparator);

    if ( isInstructionStart ) {
        _isCommand = (_lastTokenType == tok_isReservedWord);                            // reserved word at start of instruction ? is a command
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

            secondLastTokenType = tok_isReservedWord;                                   // token sequence within current command (command parameters)
            secondLastIsLvl0CommaSep = false;
            isSecondExpressionToken = false;

            // determine command and where allowed
            CmdBlockDef cmdBlockDef = _resWords [_tokenIndex].cmdBlockDef;
            _isExtFunctionCmd = (cmdBlockDef.blockType == block_extFunction);
            _isProgramCmd = ((cmdBlockDef.blockType == block_none) && (cmdBlockDef.blockPosOrAction == cmd_program));
            _isGlobalOrUserVarCmd = ((cmdBlockDef.blockType == block_none) && (cmdBlockDef.blockPosOrAction == cmd_globalVar));
            _isLocalVarCmd = ((cmdBlockDef.blockType == block_none) && (cmdBlockDef.blockPosOrAction == cmd_localVar));
            _isStaticVarCmd = ((cmdBlockDef.blockType == block_none) && (cmdBlockDef.blockPosOrAction == cmd_staticVar));
            _isDeleteVarCmd = ((cmdBlockDef.blockType == block_none) && (cmdBlockDef.blockPosOrAction == cmd_deleteVar));

            _isAnyVarCmd = _isGlobalOrUserVarCmd || _isLocalVarCmd || _isStaticVarCmd;      //  VAR, LOCAL, STATIC

            // is command allowed here ? Check restrictions
            char cmdRestriction = _resWords [_tokenIndex].restrictions;
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
            if ( (_pcalculator->_programMode && !_extFunctionBlockOpen) && (cmdRestriction == cmd_onlyImmediateOrInsideFunctionBlock) ) { result = result_onlyImmediateOrInFunction; return false; };

            if ( _extFunctionBlockOpen && _isExtFunctionCmd ) { result = result_functionDefsCannotBeNested; return false; } // separate message to indicate 'no nesting'

            // not a block command: nothing more to do here 
            if ( cmdBlockDef.blockType == block_none ) { return true; }

            if ( cmdBlockDef.blockPosOrAction == block_startPos ) {                        // is a block start command ?                          
                _blockLevel++;                                                          // increment stack counter and create corresponding list element
                _pCurrStackLvl = (LE_stack*) myStack.appendListElement( sizeof( LE_stack ) );
                _pCurrStackLvl->openBlock.cmdBlockDef = cmdBlockDef;                // store in stack: block type, block position (start), n/a, n/a
                memcpy( _pCurrStackLvl->openBlock.tokenStep, &_lastTokenStep, sizeof( char [2] ) );                      // store in stack: pointer to block start command token of open block
                _blockStartCmdTokenStep = _lastTokenStep;                                     // remember pointer to block start command token of open block
                _blockCmdTokenStep = _lastTokenStep;                                          // remember pointer to last block command token of open block
                _extFunctionBlockOpen = _extFunctionBlockOpen || _isExtFunctionCmd;    // open until block closing END command     
                return true;                                                         // nothing more to do
            }

            if ( _blockLevel == 0 ) { result = result_noOpenBlock; return false; }      // not a block start and no open block: error

            if ( (cmdBlockDef.blockType == block_alterFlow) && (_blockLevel > 0) ) {
                // check for a compatible open block (e.g. a BREAK command can only occur if at least one open loop block exists)
                // parenthesis level is zero, because this is a block start command -> all stack levels are block levels
                LE_stack* pStackLvl = _pCurrStackLvl;                                   // start with current open block level
                while ( pStackLvl != nullptr ) {
                    if ( (pStackLvl->openBlock.cmdBlockDef.blockType == block_extFunction) &&   // an open external function block has been found (call or definition)
                        (cmdBlockDef.blockPosOrAction == block_inOpenFunctionBlock) ) {                // and current flow altering command is allowed in open function block
                        // store pointer from 'alter flow' token (command) to block start command token of compatible open block (from RETURN to FUNCTION token)
                        memcpy( ((TokenIsResWord*) (_pcalculator->_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.tokenStep, sizeof( char [2] ) );
                        break;                                                                      // -> applicable open block level found
                    }
                    if ( ((pStackLvl->openBlock.cmdBlockDef.blockType == block_for) ||
                        (pStackLvl->openBlock.cmdBlockDef.blockType == block_while)) &&         // an open loop block has been found (e.g. FOR ... END block)
                        (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock) ) {                    // and current flow altering command is allowed in open loop block
                        // store pointer from 'alter flow' token (command) to block start command token of compatible open block (e.g. from BREAK to FOR token)
                        memcpy( ((TokenIsResWord*) (_pcalculator->_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.tokenStep, sizeof( char [2] ) );
                        break;                                                                      // -> applicable open block level found
                    }
                    pStackLvl = (LE_stack*) myStack.getPrevListElement( pStackLvl );
                }
                if ( pStackLvl == nullptr ) { result = (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock) ? result_noOpenLoop : result_noOpenFunction; }
                return (pStackLvl != nullptr);
            }

            if ( (cmdBlockDef.blockType != _pCurrStackLvl->openBlock.cmdBlockDef.blockType) &&    // same block type as open block (or block type is generic block end) ?
                (cmdBlockDef.blockType != block_genericEnd) ) {
                result = result_notAllowedInThisOpenBlock; return false;                // wrong block type: error
            }

            bool withinRange = (_pCurrStackLvl->openBlock.cmdBlockDef.blockPosOrAction >= cmdBlockDef.blockMinPredecessor) &&     // sequence of block commands OK ?
                (_pCurrStackLvl->openBlock.cmdBlockDef.blockPosOrAction <= cmdBlockDef.blockMaxPredecessor);
            if ( !withinRange ) { result = result_wrongBlockSequence; return false; }   // sequence of block commands (for current stack level) is not OK: error

            // pointer from previous open block token to this open block token (e.g. pointer from IF token to ELSEIF or ELSE token)
            memcpy( ((TokenIsResWord*) (_pcalculator->_programStorage + _blockCmdTokenStep))->toTokenStep, &_lastTokenStep, sizeof( char [2] ) );
            _blockCmdTokenStep = _lastTokenStep;                                              // remember pointer to last block command token of open block

            if ( cmdBlockDef.blockPosOrAction == block_endPos ) {                          // is this a block END command token ? 
                if ( _pCurrStackLvl->openBlock.cmdBlockDef.blockType == block_extFunction ) { _extFunctionBlockOpen = false; }       // FUNCTON definition blocks cannot be nested
                memcpy( ((TokenIsResWord*) (_pcalculator->_programStorage + _lastTokenStep))->toTokenStep, &_blockStartCmdTokenStep, sizeof( char [2] ) );
                myStack.deleteListElement( nullptr );                                   // decrement stack counter and delete corresponding list element

                _blockLevel--;                                                          // also set pointer to currently last element in stack (if it exists)
                if ( _blockLevel + _parenthesisLevel > 0 ) { _pCurrStackLvl = (LE_stack*) myStack.getLastListElement(); }
                if ( _blockLevel > 0 ) {
                    // retrieve pointer to block start command token and last block command token of open block
                    memcpy( &_blockStartCmdTokenStep, _pCurrStackLvl->openBlock.tokenStep, sizeof( char [2] ) );         // pointer to block start command token of open block       
                    uint16_t tokenStep = _blockStartCmdTokenStep;                            // init pointer to last block command token of open block
                    uint16_t tokenStepPointedTo;
                    memcpy( &tokenStepPointedTo, ((TokenIsResWord*) (_pcalculator->_programStorage + tokenStep))->toTokenStep, sizeof( char [2] ) );
                    while ( tokenStepPointedTo != 0xFFFF )
                    {
                        tokenStep = tokenStepPointedTo;
                        memcpy( &tokenStepPointedTo, ((TokenIsResWord*) (_pcalculator->_programStorage + tokenStep))->toTokenStep, sizeof( char [2] ) );
                    }

                    _blockCmdTokenStep = tokenStep;                                        // pointer to last block command token of open block                       
                }
            }
            else { _pCurrStackLvl->openBlock.cmdBlockDef = cmdBlockDef; }           // overwrite (block type (same or generic end), position, min & max predecessor)

            return true;
        };
    }


    // parsing a command parameter right now ? 
    // ---------------------------------------

    if ( !_isCommand ) { return true; }                                                 // not within a command                                                

    // parsing a command parameter: apply additional command syntax rules
    bool isSemiColonSep = (_lastTokenType == tok_isSemiColonSeparator);
    bool isLeftParenthesis = (_lastTokenType == tok_isLeftParenthesis);
    bool isLvl0CommaSep = (_lastTokenType == tok_isCommaSeparator) && (_parenthesisLevel == 0);
    bool isNonAssignmentOp = (_lastTokenType == tok_isOperator) ? (singleCharTokens [_tokenIndex] != ':') : false;
    bool isAssignmentOp = (_lastTokenType == tok_isOperator) ? (singleCharTokens [_tokenIndex] == ':') : false;
    bool isResWord = (_lastTokenType == tok_isReservedWord);
    bool isExpressionFirstToken = (!isResWord) && ((secondLastTokenType == tok_isReservedWord) || (secondLastIsLvl0CommaSep));

    if ( isResWord || (isLvl0CommaSep) ) {
        isExpression = false; expressionStartsWithVariable = false; expressionStartsWithArrayVar = false;
        expressionStartsWithGenericName = false;
    }
    if ( isExpressionFirstToken ) {
        isExpression = true;
        if ( _lastTokenType == tok_isVariable ) {
            expressionStartsWithVariable = true;
            expressionStartsWithArrayVar = true;
        }
        else if ( _lastTokenType == tok_isGenericName ) { expressionStartsWithGenericName = true; }
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

    bool previousParamMainLvlElementIsArray = (secondLastTokenType == tok_isRightParenthesis) && (_parenthesisLevel == 0);
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
    secondLastTokenType = _lastTokenType;                                               // within current command
    secondLastIsLvl0CommaSep = isLvl0CommaSep;
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

    for ( resWordIndex = _resWordNo - 1; resWordIndex >= 0; resWordIndex-- ) {          // for all defined reserved words: check against alphanumeric token (NOT ending by '\0')
        if ( strlen( _resWords [resWordIndex]._resWordName ) != pNext - pch ) { continue; }          // token has correct length ? If not, skip remainder of loop ('continue')                            
        if ( strncmp( _resWords [resWordIndex]._resWordName, pch, pNext - pch ) != 0 ) { continue; } // token corresponds to reserved word ? If not, skip remainder of loop ('continue') 

        // token is reserved word, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( _parenthesisLevel > 0 ) { pNext = pch; result = result_resWordNotAllowedHere; return false; }
        if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_5_2_1) ) { pNext = pch; result = result_resWordNotAllowedHere; return false; }
        if ( !_isCommand ) {                                                             // within commands: do not test here
            if ( (_lastTokenType != tok_isSemiColonSeparator) && (_lastTokenType != tok_no_token) ) {
                pNext = pch; result = result_resWordNotAllowedHere; return false;
            }
        }
        if ( _leadingSpaceCheck ) { pNext = pch; result = result_spaceMissing; return false; }

        _tokenIndex = resWordIndex;                                                     // needed in case it's the start of a command (to determine parameters)

        // token is a reserved word, and it's allowed here

        // if NOT a block command, bytes for token step are not needed 
        bool hasTokenStep = (_resWords [resWordIndex].cmdBlockDef.blockType != block_none);

        TokenIsResWord* pToken = (TokenIsResWord*) _pcalculator->_programCounter;
        pToken->tokenType = tok_isReservedWord | ((sizeof( TokenIsResWord ) - (hasTokenStep ? 0 : 2)) << 4);
        pToken->tokenIndex = resWordIndex;
        if ( hasTokenStep ) { pToken->toTokenStep [0] = 0xFF; pToken->toTokenStep [1] = 0xFF; }                  // -1: no token ref. Because uint16_t not necessarily aligned with word size: store as two sep. bytes                            

        _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
        _lastTokenType = tok_isReservedWord;

        _pcalculator->_programCounter += sizeof( TokenIsResWord ) - (hasTokenStep ? 0 : 2);
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

    float f = strtof( pch, &pNext );                                                    // token can be parsed as float ?
    if ( pch == pNext ) { return true; }                                                // token is not a number if pointer pNext was not moved

    // number found: if it is allowed here in the token sequence, proceed 
    // if NOT allowed here, then check if it's a '+' or '-' operator (FOLLOWED by a number)
    bool sequenceOK = true;
    if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_4_1_0) ) {                   // number found but not allowed here
        pNext = pch;                                                                    // reset pointer to first character to parse
        sequenceOK = (pch [0] == '+') || (pch [0] == '-');                              // token can still be a '+' or '-' operator, FOLLOWED by a number: check first character
        if ( sequenceOK ) { return true; }
    }

    if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is a number, but is it allowed here ? 
    if ( !sequenceOK ) { result = result_numConstNotAllowedHere; }                   // is not a '+' or '-' operator either: indicate error 

    // within commands: skip this test (if '_isCommand' is true, then test expression is false)
    // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
    if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_numConstNotAllowedHere; return false; ; }

    // Note: in a declaration statement, operators other than assignment are not allowed, which is detected in terminal token parsing
    // -> if previous token was operator: it's an assignment
    bool isParamDecl = (_isExtFunctionCmd);                                          // parameter declarations :  constant can ONLY FOLLOW an assignment operator
    if ( isParamDecl && (_lastTokenType != tok_isOperator) ) { pNext = pch; result = result_numConstNotAllowedHere; return false; }

    // token is a number, and it's allowed here
    TokenIsFloatCst* pToken = (TokenIsFloatCst*) _pcalculator->_programCounter;
    pToken->tokenType = tok_isNumConst | (sizeof( TokenIsFloatCst ) << 4);
    memcpy( pToken->numConst, &f, sizeof( f ) );                                           // float not necessarily aligned with word size: copy memory instead

    ////  geen beperking nodig: initialisatie door runtime scannen van 'LOCAL' commands bij start procedure tot local var count bereikt is 
    /* dit mag dus weg:
    bool checkLocalVarInit = (_isLocalVarCmd && (_lastTokenType == tok_isOperator));
    if ( checkLocalVarInit && (f != 0) ) { pNext = pch; result = result_varLocalInit_zeroValueExpected; return false; }
    */
    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && (_lastTokenType == tok_isOperator));

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = tok_isNumConst;
    if ( doNonLocalVarInit ) { initVariable( _lastVariableTokenStep, _lastTokenStep ); }     // initialisation of global / static variable ? (operator: is always assignment)

    _pcalculator->_programCounter += sizeof( TokenIsFloatCst );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ----------------------------------------------------------------
// *   try to parse next characters as an alphanumeric constant   *
// ----------------------------------------------------------------

bool MyParser::parseAsAlphanumConstant( char*& pNext, parseTokenResult_type& result ) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int escChars = 0;

    if ( (pNext [0] != '\"') ) { return true; }                                         // no opening quote ? Is not an alphanumeric cst (it can still be something else)
    pNext++;                                                                            // skip opening quote

    if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is an alphanumeric constant, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }

    // within commands: skip this test (if '_isCommand' is true, then test expression is false)
    // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
    if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; ; }

    // Note: in a declaration statement, operators other than assignment are not allowed, which is detected in terminal token parsing
    // -> if previous token was operator: it's an assignment
    bool isParamDecl = (_isExtFunctionCmd);                                             // parameter declarations :  constant can ONLY FOLLOW an assignment operator
    if ( isParamDecl && (_lastTokenType != tok_isOperator) ) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }

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
    char* pAlphanumCst = new char [pNext - (pch + 1) - escChars + 1];                                // create char array on the heap to store alphanumeric constant, including terminating '\0'
    _heapObjectCount++;
#if printCreateDeleteHeapObjects
    Serial.print( "(HEAP) Create object # " ); Serial.print( _heapObjectCount ); Serial.print( ": string const value, addr " );
    Serial.println( (uint32_t) pAlphanumCst - RAMSTART );
#endif
    // store alphanumeric constant in newly created character array
    pAlphanumCst [pNext - (pch + 1) - escChars] = '\0';                                 // store string terminating '\0' (pch + 1 points to character after opening quote, pNext points to closing quote)
    char* pSource = pch + 1, * pDestin = pAlphanumCst;                                  // pSource points to character after opening quote
    while ( pSource + escChars < pNext ) {                                              // store alphanumeric constant in newly created character array (terminating '\0' already added)
        if ( pSource [0] == '\\' ) { pSource++; escChars--; }                           // if escape sequences found: skip first escape sequence character (backslash)
        pDestin++ [0] = pSource++ [0];
    }
    pNext++;                                                                            // skip closing quote

    TokenIsAlphanumCst* pToken = (TokenIsAlphanumCst*) _pcalculator->_programCounter;
    pToken->tokenType = tok_isAlphaConst | (sizeof( TokenIsAlphanumCst ) << 4);
    memcpy( pToken->pAlphanumConst, &pAlphanumCst, sizeof( pAlphanumCst ) );            // pointer not necessarily aligned with word size: copy memory instead

    ////  geen beperking nodig: initialisatie door runtime scannen van 'LOCAL' commands bij start procedure tot local var count bereikt is 
    /* dit mag dus weg:
    bool checkLocalVarInit = (_isLocalVarCmd && (_lastTokenType == tok_isOperator));
    if ( checkLocalVarInit && (strlen( pAlphanumCst ) > 0) ) { pNext = pch; result = result_varLocalInit_emptyStringExpected; return false; }
    */
    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && (_lastTokenType == tok_isOperator));          // (operator: is always assignment)

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = tok_isAlphaConst;
    if ( doNonLocalVarInit ) {                                     // initialisation of global / static variable ? 
        if ( !initVariable( _lastVariableTokenStep, _lastTokenStep ) ) { pNext = pch; result = result_arrayInit_emptyStringExpected; return false; };
    }

    _pcalculator->_programCounter += sizeof( TokenIsAlphanumCst );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ---------------------------------------------------------------------------------
// *   try to parse next characters as a terminal token (one- or two-characters)   *
// ---------------------------------------------------------------------------------

// External function definition statement parsing: check order of mandatory and optional arguments, check if max. n° not exceeded
// -------------------------------------------------------------------------------------------------------------------------------

bool MyParser::checkExtFunctionArguments( parseTokenResult_type& result, int& minArgCnt, int& maxArgCnt ) {
    bool argWasMandatory = (_lastTokenType == tok_isVariable) || (_lastTokenType == tok_isRightParenthesis);         // variable without assignment to a constant or param array def. parenthesis
    bool alreadyOptArgs = (minArgCnt != maxArgCnt);
    if ( argWasMandatory && alreadyOptArgs ) { result = result_mandatoryArgFoundAfterOptionalArgs; return false; }
    if ( argWasMandatory ) { minArgCnt++; }
    maxArgCnt++;
    // check that max argument count is not exceeded (number must fit in 4 bits)
    if ( maxArgCnt > extFunctionMaxArgs ) { result = result_functionDefMaxArgsExceeded; return false; }
}


// Array parsing: check that max dimension count and maximum array size is not exceeded
// ------------------------------------------------------------------------------------

bool MyParser::checkArrayDimCountAndSize( parseTokenResult_type& result, int* arrayDef_dims, int& dimCnt ) {
    if ( _lastTokenType == tok_isLeftParenthesis ) { result = result_arrayDefNoDims; return false; }

    dimCnt++;

    if ( dimCnt > _pcalculator->MAX_ARRAY_DIMS ) { result = result_arrayDefMaxDimsExceeded; return false; }
    float f;        // last token is a number constant: dimension spec
    memcpy( &f, ((TokenIsFloatCst*) (_pcalculator->_programStorage + _lastTokenStep))->numConst, sizeof( f ) );
    if ( f < 1 ) { result = result_arrayDefNegativeDim; return false; }
    arrayDef_dims [dimCnt - 1] = (int) f;
    int arrayElements = 1;
    for ( int cnt = 0; cnt < dimCnt; cnt++ ) { arrayElements *= arrayDef_dims [cnt]; }
    if ( arrayElements > _pcalculator->MAX_ARRAY_ELEM ) { result = result_arrayDefMaxElementsExceeded; return false; }
}

// Array parsing: check that order of arrays and scalar variables is consistent with previous calls and function definition 
// ------------------------------------------------------------------------------------------------------------------------

bool MyParser::checkFuncArgArrayPattern( parseTokenResult_type& result, bool isFunctionClosingParenthesis ) {

    int funcIndex = _pCurrStackLvl->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
    int argNumber = _pCurrStackLvl->openPar.actualArgsOrDims;
    uint16_t paramIsArrayPattern;
    memcpy( &paramIsArrayPattern, _pcalculator->extFunctionData [funcIndex].paramIsArrayPattern, sizeof( char [2] ) );
    if ( argNumber > 0 ) {

        bool isArray = false;
        if ( _isExtFunctionCmd ) { isArray = (_lastTokenType == tok_isRightParenthesis); }  // function definition: if variable name followed by empty parameter list ' () ': array parameter
        else if ( _lastTokenType == tok_isVariable ) {                                      // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((TokenIsVariable*) (_pcalculator->_programStorage + _lastTokenStep))->identInfo) & _pcalculator->var_isArray;
        }

        uint16_t paramArrayMask = 1 << (argNumber - 1);
        if ( paramIsArrayPattern & 0x8000 ) {                   // function not used yet (before it was defined now: no need to check, just set array bit)
            paramIsArrayPattern = paramIsArrayPattern | (isArray ? paramArrayMask : 0);
        }
        else {
            if ( (paramIsArrayPattern & paramArrayMask) != (isArray ? paramArrayMask : 0) ) { result = result_fcnScalarAndArrayArgOrderNotConsistent; return false; }
        }
    }

    if ( isFunctionClosingParenthesis ) { paramIsArrayPattern = paramIsArrayPattern & ~0x8000; }    // function name used now: order of scalar and array parameters is now fixed
    memcpy( _pcalculator->extFunctionData [funcIndex].paramIsArrayPattern, &paramIsArrayPattern, sizeof( char [2] ) );
}


// -------------------------------------------
// * Parse a single / double character token * 
// -------------------------------------------

bool MyParser::parseTerminalToken( char*& pNext, parseTokenResult_type& result ) {

    // external function definition statement parsing: count number of mandatory and optional arguments in function definition for storage
    static int extFunctionDef_minArgCounter { 0 };
    static int extFunctionDef_maxArgCounter { 0 };

    // array definition statement parsing: record dimensions (if 1 dimension only: set dim 2 to zero) 
    static int array_dimCounter { 0 };
    static int arrayDef_dims [_pcalculator->MAX_ARRAY_DIMS] { 0 };

    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    char* pSingleChar = strchr( singleCharTokens, pNext [0] );                          // locate this character in the list of available one-character tokens and return pointer to it 
    if ( pSingleChar == nullptr ) { return true; }                                      // token is not a one-character token (and it's not a two-char token, because these start with same character)

    pNext++;                                                                            // move to next character
    int singleCharIndex = pSingleChar - singleCharTokens;                            // index defines single (or double) character token
    tokenType_type tokenType;
    uint8_t flags { B0 };
    char* peek;

    switch ( pch [0] ) {

    case '(': {
        // -------------------------------------
        // Case 1: is token a left parenthesis ?
        // -------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is left parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_5_4_3_1_0) ) { pNext = pch;  result = result_parenthesisNotAllowedHere; return false; }

        // within commands: skip this test (if '_isCommand' is true, then test expression is false)
        // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
        if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }

        if ( _isAnyVarCmd && (_parenthesisLevel > 0) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }     // no parenthesis nesting in array declarations
        // parenthesis nesting in function definitions, only to declare an array parameter AND only if followed by a closing parenthesis 
        if ( (_isExtFunctionCmd) && (_parenthesisLevel > 0) && (_lastTokenType != tok_isVariable) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }
        if ( _isProgramCmd || _isDeleteVarCmd ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

        if ( _leadingSpaceCheck ) { pNext = pch; result = result_spaceMissing; return false; }

        // token is a left parenthesis, and it's allowed here

        // store specific flags in stack, because if nesting functions or parentheses, values will be overwritten
        flags = (_lastTokenType == tok_isExternFunction) ? _pcalculator->extFunctionBit :
            (_lastTokenType == tok_isInternFunction) ? _pcalculator->intFunctionBit :
            (_lastTokenType == tok_isVariable) ? _pcalculator->arrayBit : _pcalculator->openParenthesisBit;     // is it following a(n internal or external) function name ?
        // external function (call or definition) opening parenthesis
        if ( _lastTokenType == tok_isExternFunction ) {
            if ( _pcalculator->extFunctionData [_extFunctionIndex].pExtFunctionStartToken != nullptr ) { flags = flags | _pcalculator->extFunctionPrevDefinedBit; }
        }
        bool isSecondSubExpressionToken = ((_previousTokenType == tok_no_token) || (_previousTokenType == tok_isSemiColonSeparator) ||
            (_previousTokenType == tok_isLeftParenthesis) || (_previousTokenType == tok_isCommaSeparator) || (_previousTokenType == tok_isReservedWord));
        // last token before left parenthesis is variable name AND the start of a (sub-) expression, but NOT part of a variable definition command
        bool assignmentOK = (_lastTokenType == tok_isVariable) && isSecondSubExpressionToken;
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
        if ( _lastTokenGroup_sequenceCheck & lastTokenGroup_4 ) {
            _minFunctionArgs = 1;                                    // initialize min & max allowed argument count to 1
            _maxFunctionArgs = 1;
        }

        // min & max argument count: either allowed range (if function previously defined), current range of actual args counts (if previous calls only), or not initialized
        _parenthesisLevel++;                                                            // increment stack counter and create corresponding list element
        _pCurrStackLvl = (LE_stack*) myStack.appendListElement( sizeof( LE_stack ) );
        _pCurrStackLvl->openPar.minArgs = _minFunctionArgs;
        _pCurrStackLvl->openPar.maxArgs = _maxFunctionArgs;
        _pCurrStackLvl->openPar.actualArgsOrDims = 0;
        _pCurrStackLvl->openPar.arrayDimCount = _pcalculator->_arrayDimCount;         // dimensions of previously defined array. If zero, then this array did not yet exist, or it's a sclarar variable
        _pCurrStackLvl->openPar.flags = flags;
        _pCurrStackLvl->openPar.identifierIndex = (_lastTokenType == tok_isExternFunction) ? _extFunctionIndex :
            (_lastTokenType == tok_isVariable) ? _variableNameIndex : 0;
        _pCurrStackLvl->openPar.variableQualifier = _variableQualifier;

        tokenType = tok_isLeftParenthesis;                                              // remember: token is a left parenthesis
        break; }


    case ')': {
        // --------------------------------------
        // Case 2: is token a right parenthesis ?
        // --------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is right parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_5_4_2) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

        // within commands: skip this test (if '_isCommand' is true, then test expression is false)
        // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
        if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }
        if ( _parenthesisLevel == 0 ) { pNext = pch; result = result_missingLeftParenthesis; return false; }

        flags = _pCurrStackLvl->openPar.flags;


        // 2.1 External function definition (not a call), OR array parameter definition, closing parenthesis ?
        // -------------------------------------------------------------------

        if ( _isExtFunctionCmd ) {
            if ( _parenthesisLevel == 1 ) {          // function definition closing parenthesis
                // stack level will not change until closing parenthesis (because within definition, no nesting of parenthesis is possible)
                // stack min & max values: current range of args counts that occured in previous calls (not initialized if no earlier calls occured)

                // if empty function parameter list, then do not increment parameter count (function taking no parameters)
                bool emptyParamList = (_lastTokenType == tok_isLeftParenthesis);            // ok because no nesting allowed
                _pCurrStackLvl->openPar.actualArgsOrDims += (emptyParamList ? 0 : 1);

                // check order of mandatory and optional arguments, check if max. n° not exceeded
                if ( !emptyParamList ) { if ( !checkExtFunctionArguments( result, extFunctionDef_minArgCounter, extFunctionDef_maxArgCounter ) ) { pNext = pch; return false; }; }

                int funcIndex = _pCurrStackLvl->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
                // if previous calls, check if range of actual argument counts that occured in previous calls corresponds to mandatory and optional arguments defined now
                bool previousCalls = (_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) != extFunctionFirstOccurFlag;
                if ( previousCalls ) {                                                      // stack contains current range of actual args occured in previous calls
                    if ( ((int) _pCurrStackLvl->openPar.minArgs < extFunctionDef_minArgCounter) ||
                        (int) _pCurrStackLvl->openPar.maxArgs > extFunctionDef_maxArgCounter ) {
                        pNext = pch; result = result_prevCallsWrongArgCount; return false;  // argument count in previous calls to this function does not correspond 
                    }
                }

                // store min required & max allowed n° of arguments in identifier storage
                // this replaces the range of actual argument counts that occured in previous calls (if any)
                _pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1] = (extFunctionDef_minArgCounter << 4) | (extFunctionDef_maxArgCounter);

                // check that order of arrays and scalar variables is consistent with previous callsand function definition
                if ( !checkFuncArgArrayPattern( result, true ) ) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
            }
        }


        // 2.2 Array definition dimension spec closing parenthesis ?
        // ---------------------------------------------------------

        else if ( _isAnyVarCmd ) {                        // note: parenthesis level is 1 (because no inner parenthesis allowed)
            if ( !checkArrayDimCountAndSize( result, arrayDef_dims, array_dimCounter ) ) { pNext = pch; return false; }

            int varNameIndex = _pCurrStackLvl->openPar.identifierIndex;
            uint8_t varQualifier = _pCurrStackLvl->openPar.variableQualifier;

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
                peek = pNext;
                while ( peek [0] == ' ' ) { peek++; }
                bool arrayHasInitializer = (peek [0] == ':');                                                // scalar or matrix variable ? 
                if ( !arrayHasInitializer ) {                    // no explicit initializer 
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
            bool emptyArgList = (_lastTokenType == tok_isLeftParenthesis);            // ok because no nesting allowed
            _pCurrStackLvl->openPar.actualArgsOrDims += (emptyArgList ? 0 : 1);
            int actualArgs = (int) _pCurrStackLvl->openPar.actualArgsOrDims;

            // call to not yet defined external function ? (there might be previous calls)
            bool callToNotYetDefinedFunc = ((flags & (_pcalculator->extFunctionBit | _pcalculator->extFunctionPrevDefinedBit)) == _pcalculator->extFunctionBit);
            if ( callToNotYetDefinedFunc ) {
                // check that max argument count is not exceeded (number must fit in 4 bits)
                if ( actualArgs > extFunctionMaxArgs ) { pNext = pch; result = result_functionDefMaxArgsExceeded; return false; }

                // if at least one previous call (maybe a nested call) is completely parsed, retrieve current range of actual args that occured in these previous calls
                // and update this range with the argument count of the current external function call that is at its closing parenthesis
                int funcIndex = _pCurrStackLvl->openPar.identifierIndex;            // of current function call: stored in stack for current PARENTHESIS level
                bool prevExtFuncCompletelyParsed = (_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) != extFunctionFirstOccurFlag;
                if ( prevExtFuncCompletelyParsed ) {
                    _pCurrStackLvl->openPar.minArgs = ((_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) >> 4) & 0x0F;
                    _pCurrStackLvl->openPar.maxArgs = (_pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1]) & 0x0F;
                    if ( (int) _pCurrStackLvl->openPar.minArgs > actualArgs ) { _pCurrStackLvl->openPar.minArgs = actualArgs; }
                    if ( (int) _pCurrStackLvl->openPar.maxArgs < actualArgs ) { _pCurrStackLvl->openPar.maxArgs = actualArgs; }
                }
                // no previous call: simply set this range to the argument count of the current external function call that is at its closing parenthesis
                else { _pCurrStackLvl->openPar.minArgs = actualArgs; _pCurrStackLvl->openPar.maxArgs = actualArgs; }

                // store the up to date range of actual argument counts in identifier storage
                _pcalculator->extFunctionNames [funcIndex][_maxIdentifierNameLen + 1] = (_pCurrStackLvl->openPar.minArgs << 4) | (_pCurrStackLvl->openPar.maxArgs);
            }

            // if call to previously defined external function, to an internal function, or if open parenthesis, then check argument count 
            else {
                bool isOpenParenthesis = (flags & _pcalculator->openParenthesisBit);
                if ( isOpenParenthesis ) { _pCurrStackLvl->openPar.minArgs = 1; _pCurrStackLvl->openPar.maxArgs = 1; }
                bool argCountWrong = ((actualArgs < (int) _pCurrStackLvl->openPar.minArgs) ||
                    (actualArgs > ( int ) _pCurrStackLvl->openPar.maxArgs));
                if ( argCountWrong ) { pNext = pch; result = result_wrong_arg_count; return false; }
            }

            // external functions only: check that order of arrays and scalar variables is consistent with previous calls and function definition
            // note: internal functions only accept scalars
            bool extFunction = flags & _pcalculator->extFunctionBit;
            if ( extFunction ) {
                if ( !checkFuncArgArrayPattern( result, true ) ) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
            }
        }


        // 2.4 Array element spec closing parenthesis ?
        // --------------------------------------------

        else if ( flags & _pcalculator->arrayBit ) {
            // check if array dimension count corresponds (individual dimension adherence can only be checked at runtime)
            // if previous token is left parenthesis (' () '), then do not increment argument count
            if ( _lastTokenType != tok_isLeftParenthesis ) { _pCurrStackLvl->openPar.actualArgsOrDims++; }

            int varNameIndex = _pCurrStackLvl->openPar.identifierIndex;
            uint8_t varQualifier = _pCurrStackLvl->openPar.variableQualifier;
            bool isParam = (varQualifier == _pcalculator->var_isParamInFunc);            // but not function parameter definitions
            int actualDimCount = _pCurrStackLvl->openPar.actualArgsOrDims;
            if ( actualDimCount == 0 ) { pNext = pch; result = result_arrayUseNoDims; return false; } // dim count too high: already handled when preceding comma was parsed
            if ( !isParam ) {
                if ( actualDimCount != (int) _pCurrStackLvl->openPar.arrayDimCount ) { pNext = pch; result = result_arrayUseWrongDimCount; return false; }
            }
        }

        else {}     // for documentation only: all cases handled


        // token is a right parenthesis, and it's allowed here

        _arrayElemAssignmentAllowed = (flags & _pcalculator->arrayElemAssignmentAllowedBit);          // assignment possible next ? (to array element)
        myStack.deleteListElement( nullptr );                                           // decrement open parenthesis stack counter and delete corresponding list element
        _parenthesisLevel--;

        // set pointer to currently last element in stack
        if ( _blockLevel + _parenthesisLevel > 0 ) { _pCurrStackLvl = (LE_stack*) myStack.getLastListElement(); }
        tokenType = tok_isRightParenthesis;                                             // remember: token is a right parenthesis
        break;
    }


    case ',': {
        // ------------------------------------
        // Case 3: is token a comma separator ?
        // ------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is comma separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_5_2) ) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

        // within commands: skip this test (if '_isCommand' is true, then test expression is false)
        // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
        if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_separatorNotAllowedHere; return false; ; }

        // if no open parenthesis, a comma can only occur to separate command parameters
        if ( (_parenthesisLevel == 0) && !_isCommand ) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

        flags = (_parenthesisLevel > 0) ? _pCurrStackLvl->openPar.flags : 0;


        // 3.1 External function definition (not a call) parameter separator ? 
        // -------------------------------------------------------------------

        if ( _isExtFunctionCmd ) {
            if ( _parenthesisLevel == 1 ) {          // not an array parameter (would be parenthesis level 2)
                _pCurrStackLvl->openPar.actualArgsOrDims++;
                // check order of mandatory and optional arguments, check if max. n° not exceeded
                if ( !checkExtFunctionArguments( result, extFunctionDef_minArgCounter, extFunctionDef_maxArgCounter ) ) { pNext = pch; return false; };

                // Check order of mandatory and optional arguments (function: parenthesis levels > 0)
                if ( !checkFuncArgArrayPattern( result, false ) ) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
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
            _pCurrStackLvl->openPar.actualArgsOrDims++;           // include argument before the comma in argument count     
            int actualArgs = (int) _pCurrStackLvl->openPar.actualArgsOrDims;

            // call to not yet defined external function ? (because there might be previous calls as well)
            bool callToNotYetDefinedFunc = ((_pCurrStackLvl->openPar.flags & (_pcalculator->extFunctionBit | _pcalculator->extFunctionPrevDefinedBit)) == _pcalculator->extFunctionBit);
            if ( callToNotYetDefinedFunc ) {
                // check that max argument count is not exceeded (number must fit in 4 bits)
                if ( actualArgs > extFunctionMaxArgs ) { pNext = pch; result = result_functionDefMaxArgsExceeded; return false; }
            }

            // if call to previously defined external function, to an internal function, or if open parenthesis, then check argument count 
            else {
                bool isOpenParenthesis = (flags & _pcalculator->openParenthesisBit);
                if ( isOpenParenthesis ) { _pCurrStackLvl->openPar.minArgs = 1; _pCurrStackLvl->openPar.maxArgs = 1; }
                bool argCountWrong = (actualArgs >= (int) _pCurrStackLvl->openPar.maxArgs);       // check against allowed maximum number of arguments for this function
                if ( argCountWrong ) { pNext = pch; result = isOpenParenthesis ? result_missingRightParenthesis : result_wrong_arg_count; return false; }
            }

            // external functions only: check that order of arrays and scalar variables is consistent with previous calls and function definition
            // note: internal functions only accept scalars
            bool extFunction = flags & _pcalculator->extFunctionBit;
            if ( extFunction ) {
                if ( !checkFuncArgArrayPattern( result, false ) ) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
            }
        }


        // 3.4 Array element spec separator ?
        // ----------------------------------

        else if ( flags & _pcalculator->arrayBit ) {
            // check if array dimension count corresponds (individual dimension adherence can only be checked at runtime)
            _pCurrStackLvl->openPar.actualArgsOrDims++;
            if ( (int) _pCurrStackLvl->openPar.actualArgsOrDims == (int) _pCurrStackLvl->openPar.arrayDimCount ) { pNext = pch; result = result_arrayUseWrongDimCount; return false; }
        }

        else {}     // for documentation only: all cases handled


        // token is a comma separator, and it's allowed here
        tokenType = tok_isCommaSeparator;                                               // remember: token is a comma separator
        break; }


    case ';': {
        // ----------------------------------------
        // Case 4: is token a semicolon separator ?
        // ----------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is semicolon separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( _parenthesisLevel > 0 ) { pNext = pch; result = result_missingRightParenthesis; return false; }
        if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_5_2_1) ) { pNext = pch; result = result_expressionNotComplete; return false; }

        // token is a semicolon separator, and it's allowed here
        tokenType = tok_isSemiColonSeparator;                                           // remember: token is a semicolon separator
        break; }


    default: {
        // -------------------------------------------------
        // Case 5: token is a one- or two-character operator
        // -------------------------------------------------

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is an operator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_5_2) ) { pNext = pch; result = result_operatorNotAllowedHere; return false; }

        // within commands: skip this test (if '_isCommand' is true, then test expression is false)
        // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
        if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_operatorNotAllowedHere; return false; ; }

        if ( _isProgramCmd || _isDeleteVarCmd ) { pNext = pch; result = result_operatorNotAllowedHere; return false; }

        // if assignment, check whether it's allowed here 
        if ( pch [0] == ':' ) {
            bool isSecondSubExpressionToken = ((_previousTokenType == tok_no_token) || (_previousTokenType == tok_isSemiColonSeparator) ||
                (_previousTokenType == tok_isLeftParenthesis) || (_previousTokenType == tok_isCommaSeparator) || (_previousTokenType == tok_isReservedWord));
            bool assignmentToScalarVarOK = ((_lastTokenType == tok_isVariable) && isSecondSubExpressionToken);
            bool assignmentToArrayElemOK = ((_lastTokenType == tok_isRightParenthesis) && _arrayElemAssignmentAllowed && (!_isExtFunctionCmd));
            if ( !(assignmentToScalarVarOK || assignmentToArrayElemOK) ) { pNext = pch; result = result_assignmNotAllowedHere; return false; }
        }

        else {      // not an assignment
            if ( _isExtFunctionCmd || _isAnyVarCmd ) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
            // if less than or greater than, it can also be a two-character operator, depending on next character 
            if ( ((pch [0] == '<') || (pch [0] == '>')) && (pch [1] == '=') ) { pNext++; singleCharIndex = strlen( singleCharTokens ) + ((pch [0] == '<') ? 0 : 1); }
            else if ( (pch [0] == '<') && (pch [1] == '>') ) { pNext++; singleCharIndex = strlen( singleCharTokens ) + 2; }
        }
        // token is an operator, and it's allowed here
        // check if it is a two-character operator ('<>' or '<=' or '>=')
        tokenType = tok_isOperator;                                                     // remember: token is an operator
        _tokenIndex = singleCharIndex;                                                  // needed in case in a command and current command parameter needs a variable
    }
}

    // create token
    TokenIsTerminal* pToken = (TokenIsTerminal*) _pcalculator->_programCounter;
    pToken->tokenTypeAndIndex = tokenType | (singleCharIndex << 4);     // terminal tokens only: token type character includes token index too 

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = tokenType;

    _pcalculator->_programCounter += sizeof( TokenIsTerminal );
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

    for ( funcIndex = _functionNo - 1; funcIndex >= 0; funcIndex-- ) {                  // for all defined function names: check against alphanumeric token (NOT ending by '\0')
        if ( strlen( _functions [funcIndex].funcName ) != pNext - pch ) { continue; }   // token has correct length ? If not, skip remainder of loop ('continue')                            
        if ( strncmp( _functions [funcIndex].funcName, pch, pNext - pch ) != 0 ) { continue; }      // token corresponds to function name ? If not, skip remainder of loop ('continue')    

        if ( _pcalculator->_programCounter == _pcalculator->_programStorage ) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is a function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_functionNotAllowedHere; return false; }

        // within commands: skip this test (if '_isCommand' is true, then test expression is false)
        // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
        if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

        if ( _isExtFunctionCmd ) { pNext = pch; result = result_redefiningIntFunctionNotAllowed; return false; }
        if ( _isAnyVarCmd ) { pNext = pch; result = result_variableNameExpected; return false; }        // is a variable declaration: internal function name not allowed

        // token is function, and it's allowed here
        _minFunctionArgs = _functions [funcIndex].minArgs;                       // set min & max for allowed argument count (note: minimum is 0)
        _maxFunctionArgs = _functions [funcIndex].maxArgs;

        TokenIsIntFunction* pToken = (TokenIsIntFunction*) _pcalculator->_programCounter;
        pToken->tokenType = tok_isInternFunction | (sizeof( TokenIsIntFunction ) << 4);
        pToken->tokenIndex = funcIndex;

        _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
        _lastTokenType = tok_isInternFunction;

        _pcalculator->_programCounter += sizeof( TokenIsIntFunction );
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

    if ( peek1 [0] != '(' ) { pNext = pch; return true; }   // not an external function 
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
    if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_functionNotAllowedHere; return false; }

    // within commands: skip this test (if '_isCommand' is true, then test expression is false)
    // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
    if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

    // if function name is too long, reset pointer to first character to parse, indicate error and return
    if ( pNext - pch > _maxIdentifierNameLen ) { pNext = pch; result = result_identifierTooLong;  return false; }

    // if in immediate mode: the function must be defined earlier (in a program)
    if ( !_pcalculator->_programMode ) {
        createNewName = false;                                                              // only check if function is defined, do NOT YET create storage for it
        index = getIdentifier( _pcalculator->extFunctionNames, _pcalculator->_extFunctionCount, _pcalculator->MAX_EXT_FUNCS, pch, pNext - pch, createNewName );
        if ( index == -1 ) { pNext = pch; result = result_undefinedFunction; return false; }
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
        funcName [_maxIdentifierNameLen + 1] = extFunctionFirstOccurFlag;                          // max (bits 7654) < (bits 3210): indicates value is not yet updated by parsing previous calls closing parenthesis
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
        // variable name usage array: reset in-procedure reference flags to be able to keep track of in-procedure variable types used
        // KEEP all other settings
        for ( int i = 0; i < _pcalculator->_programVarNameCount; i++ ) { _pcalculator->globalVarType [i] = (_pcalculator->globalVarType [i] & ~_pcalculator->var_qualifierMask) | _pcalculator->var_qualToSpecify; }
        _pcalculator->_localVarCountInFunction = 0;             // reset local and parameter variable count in function
        _pcalculator->extFunctionData [index].localVarCountInFunction = 0;

        _pFunctionDefStackLvl = _pCurrStackLvl;               // stack level for FUNCTION definition block
        _pFunctionDefStackLvl->openBlock.fcnBlock_functionIndex = index;  // store in BLOCK stack level: only if function def

    }

    // if function was defined prior to this occurence (which is then a call), retrieve min & max allowed arguments for checking actual argument count
    // if function not yet defined: retrieve current state of min & max of actual argument count found in COMPLETELY PARSED previous calls to same function 
    // if no previous occurences at all: data is not yet initialized (which is ok)
    _minFunctionArgs = ((funcName [_maxIdentifierNameLen + 1]) >> 4) & 0x0F;         // use only for passing to parsing stack
    _maxFunctionArgs = (funcName [_maxIdentifierNameLen + 1]) & 0x0F;
    _extFunctionIndex = index;


    // 4. Store token in program memory
    // --------------------------------

    TokenIsExtFunction* pToken = (TokenIsExtFunction*) _pcalculator->_programCounter;
    pToken->tokenType = tok_isExternFunction | (sizeof( TokenIsExtFunction ) << 4);
    pToken->identNameIndex = index;

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = tok_isExternFunction;

    _pcalculator->_programCounter += sizeof( TokenIsExtFunction );
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
    if ( !(_lastTokenGroup_sequenceCheck & lastTokenGroups_4_1_0) ) { pNext = pch; result = result_variableNotAllowedHere; return false; }

    // within commands: skip this test (if '_isCommand' is true, then test expression is false)
    // allow if in immediate mode or inside a function ( '(!_pcalculator->_programMode) || _extFunctionBlockOpen' is true, so test expression is false)  
    if ( !(_isCommand || (!_pcalculator->_programMode) || _extFunctionBlockOpen) ) { pNext = pch; result = result_variableNotAllowedHere; return false; ; }

    // scalar or matrix variable ? (could still be function 'array' argument; this will be detected further below)
    char* peek1 = pNext; while ( peek1 [0] == ' ' ) { peek1++; }                                                // peek next character: is it a left parenthesis ?
    char* peek2; if ( peek1 [0] == '(' ) { peek2 = peek1 + 1; while ( peek2 [0] == ' ' ) { peek2++; } }         // also find the subsequent character
    bool isArray = (peek1 [0] == '(');
    if ( _isExtFunctionCmd ) {                                     // only (array) parameter allowed now
        if ( _parenthesisLevel == 0 ) { pNext = pch; result = result_functionDefExpected; return false; }           // is not an array parameter declaration
        if ( isArray && (_parenthesisLevel == 1) && (peek2 [0] != ')') ) { pNext = pch; result = result_arrayParamExpected; return false; }           // is not an array parameter declaration
    }

    if ( _isAnyVarCmd ) {
        if ( _varDefAssignmentFound ) { pNext = pch; result = result_constantValueExpected; return false; }
    }

    // Note: in a declaration statement, operators other than assignment are not allowed, which is detected in terminal token parsing
    // -> if previous token was operator: it's an assignment
    bool isParamDecl = (_isExtFunctionCmd);                                          // parameter declarations: initialising ONLY with a constant, not with a variable
    if ( isParamDecl && (_lastTokenType == tok_isOperator) ) { pNext = pch; result = result_variableNotAllowedHere; return false; } // if operator: is assignment

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
    Calculator::Val* varValues [2]; varValues [0] = _pcalculator->globalVarValues; varValues [1] = _pcalculator->userVarValues;

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
        // variable name is new: clear all variable type flags and indicate 'qualifier not determined yet'
        // variable type (array, float or string) will be set later, if it will appear to be a global variable
        if ( createNewName )varType [primaryNameRange][varNameIndex] = _pcalculator->var_qualToSpecify;
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


    // 4. The variable NAME exists now, but we still need to check whether storage space for the variable itself has been created
    //    Note: local variable storage is created at runtime
    // --------------------------------------------------------------------------------------------------------------------------

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
                if ( !isArray ) { _pcalculator->staticVarValues [_pcalculator->_staticVarCount].numConst = 0.; }           // initialize variable (if initializer and/or array: will be overwritten)
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

                // ext. function index: in stack level for FUNCTION definition command
                int fcnIndex = _pFunctionDefStackLvl->openBlock.fcnBlock_functionIndex;
                _pcalculator->extFunctionData [fcnIndex].localVarCountInFunction = _pcalculator->_localVarCountInFunction;        // after incrementing count
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
            if ( !isArray ) { varValues [activeNameRange][varNameIndex].numConst = 0.; }                  // initialize variable (if initializer and/or array: will be overwritten)
            varType [activeNameRange][varNameIndex] = varType [activeNameRange][varNameIndex] | _pcalculator->var_isFloat;         // init (for scalar and array)
            varType [activeNameRange][varNameIndex] = varType [activeNameRange][varNameIndex] | (isProgramVar ? _pcalculator->var_hasGlobalValue : _pcalculator->var_isUser);   // set 'has global value' bit
            varType [activeNameRange][varNameIndex] = (varType [activeNameRange][varNameIndex] & ~_pcalculator->var_isArray); // init (array flag will be added when storage is created) 
        }

        else {  // the global variable exists already: check for double definition
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
            bool isPartOfFuncCallArgument = (_parenthesisLevel > 0) ? _pCurrStackLvl->openPar.flags & _pcalculator->extFunctionBit : false;
            if ( isPartOfFuncCallArgument && existingArray ) {
                // if NOT followed by an array element enclosed in parenthesis, it references the complete array
                // this is only allowed if not part of an expression: check
                bool isFuncCallArgument = (((_lastTokenType == tok_isLeftParenthesis) || (_lastTokenType == tok_isCommaSeparator))
                    && ((peek1 [0] == ',') || (peek1 [0] == ')')));
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
        if ( (_lastTokenType = tok_isReservedWord) && (_blockLevel > 1) ) {     // minimum 1 other (outer) open block
            TokPnt prgmCnt;
            prgmCnt.pToken = _pcalculator->_programStorage + _lastTokenStep;  // address of reserved word
            int tokenIndex = prgmCnt.pResW->tokenIndex;
            CmdBlockDef cmdBlockDef = _resWords [tokenIndex].cmdBlockDef;

            // variable is a control variable of a FOR loop ?
            if ( cmdBlockDef.blockType == block_for ) {

                // check if control variable is in use by a FOR outer loop
                LE_stack* pStackLvl = (LE_stack*) myStack.getLastListElement();        // current open block level
                do {
                    pStackLvl = (LE_stack*) myStack.getPrevListElement( pStackLvl );    // an outer block stack level
                    if ( pStackLvl == nullptr ) { break; }
                    if ( pStackLvl->openBlock.cmdBlockDef.blockType == block_for ) {    // outer block is FOR loop as well
                        // find token for control variable for this outer loop
                        uint16_t tokenStep;
                        memcpy( &tokenStep, pStackLvl->openBlock.tokenStep, sizeof( char [2] ) );
                        tokenStep = tokenStep + sizeof( TokenIsResWord );  // now pointing to control variable of outer loop

                        // compare variable qualifier, name index and value index of outer and inner loop control variable
                        prgmCnt.pToken = _pcalculator->_programStorage + tokenStep;  // address of outer loop control variable
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

    TokenIsVariable* pToken = (TokenIsVariable*) _pcalculator->_programCounter;
    pToken->tokenType = tok_isVariable | (sizeof( TokenIsVariable ) << 4);
    pToken->identInfo = varQualifier | (isArray ? _pcalculator->var_isArray : 0);              // qualifier, array flag ? (fixed -> store in token)
    pToken->identNameIndex = varNameIndex;
    pToken->identValueIndex = valueIndex;                      // points to storage area element for the variable  

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastVariableTokenStep = _lastTokenStep;
    _lastTokenType = tok_isVariable;

    _pcalculator->_programCounter += sizeof( TokenIsVariable );
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

    TokenIsAlphanumCst* pToken = (TokenIsAlphanumCst*) _pcalculator->_programCounter;
    pToken->tokenType = tok_isGenericName | (sizeof( TokenIsAlphanumCst ) << 4);
    memcpy( pToken->pAlphanumConst, &pProgramName, sizeof( pProgramName ) );            // pointer not necessarily aligned with word size: copy memory instead
    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && (_lastTokenType == tok_isOperator));

    _lastTokenStep = _pcalculator->_programCounter - _pcalculator->_programStorage;
    _lastTokenType = tok_isGenericName;

    _pcalculator->_programCounter += sizeof( TokenIsAlphanumCst );
    *_pcalculator->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// -----------------------------------------
// *   pretty print a parsed instruction   *
// -----------------------------------------
void MyParser::prettyPrintInstructions() {
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
    bool isUserVar;
    TokPnt prgmCnt;
    prgmCnt.pToken = _pcalculator->_programStart;
    int tokenType = *prgmCnt.pToken & 0x0F;
    char pTokenStepPointedTo [2];
    uint16_t toTokenStep;
    TokenIsResWord* pToken;

    while ( tokenType != tok_no_token ) {                                                                    // for all tokens in token list
        uint16_t tokenStep = (uint16_t) (prgmCnt.pToken - _pcalculator->_programStorage);
        strcpy( s, "" );
        strcpy( prettyToken, "" );

        switch ( tokenType ) {
        case tok_isReservedWord:
            pToken = (TokenIsResWord*) prgmCnt.pToken;
            sprintf( s, "%s ", _resWords [prgmCnt.pResW->tokenIndex]._resWordName );
            break;

        case tok_isInternFunction:
            strcpy( s, _functions [prgmCnt.pIntFnc->tokenIndex].funcName );
            break;

        case tok_isExternFunction:
            identNameIndex = (int) prgmCnt.pExtFnc->identNameIndex;   // external function list element
            identifierName = _pcalculator->extFunctionNames [identNameIndex];
            strcpy( s, identifierName );
            break;

        case tok_isVariable:
            identNameIndex = (int) (prgmCnt.pVar->identNameIndex);
            varQualifier = prgmCnt.pVar->identInfo;
            isUserVar = (prgmCnt.pVar->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isUser;
            identifierName = isUserVar ? _pcalculator->userVarNames [identNameIndex] : _pcalculator->programVarNames [identNameIndex];
            strcpy( s, identifierName );
            break;

        case tok_isNumConst:
            memcpy( &f, prgmCnt.pFloat->numConst, sizeof( f ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "%.3G", f );
            break;

        case tok_isAlphaConst:
        case tok_isGenericName:
            memcpy( &pAnum, prgmCnt.pAnumP->pAlphanumConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "\"%s\"", pAnum );
            break;

        default:
            len = strlen( singleCharTokens );
            index = (prgmCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F;
            if ( index < len ) { pch [0] = singleCharTokens [index]; pch [1] = '\0'; }
            else {
                strcat( pch, ((index == len) ? "<=" : (index == len + 1) ? ">=" : "<>") );
            }
            strcpy( s, pch );
            if ( tokenType == tok_isSemiColonSeparator ) { strcat( s, " " ); }
            break;

        }

        // append pretty printed token to character string (if still place left)
        bool isSemiColon = (tokenType == tok_isSemiColonSeparator);                             // remember 
        int tokenLength = (tokenType >= tok_isOperator) ? 1 : (*prgmCnt.pToken >> 4) & 0x0F;        // fetch next token 
        prgmCnt.pToken += tokenLength;
        tokenType = *prgmCnt.pToken & 0x0F;                                                     // next token type

        int len = strlen( s );
        if ( tokenType == tok_no_token ) { len -= 2; s [len] = '\0'; }                                               // remove final semicolon and space
        if ( len <= maxCharsPretty ) { strcat( prettyToken, s ); }
        if ( strlen( prettyToken ) > 0 ) { _pcalculator->_pConsole->print( prettyToken ); }
    }
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

    TokPnt prgmCnt;
    prgmCnt.pToken = _pcalculator->_programStart;
    int tokenType = *prgmCnt.pToken & 0x0F;
    char pTokenStepPointedTo [2];
    uint16_t toTokenStep;
    TokenIsResWord* pToken;

    while ( tokenType != '\0' ) {                                                                    // for all tokens in token list
        uint16_t tokenStep = (uint16_t) (prgmCnt.pToken - _pcalculator->_programStorage);
        strcpy( prettyToken, "" );

        switch ( tokenType ) {
        case tok_isReservedWord:
            pToken = (TokenIsResWord*) prgmCnt.pToken;
            hasTokenStep = (_resWords [prgmCnt.pResW->tokenIndex].cmdBlockDef.blockType != block_none);
            if ( hasTokenStep ) {
                memcpy( &toTokenStep, pToken->toTokenStep, sizeof( char [2] ) );
                sprintf( s, "(step %d) resW: %s, points to step %d", tokenStep, _resWords [prgmCnt.pResW->tokenIndex]._resWordName, toTokenStep );
            }
            else { sprintf( s, "(step %d) resW: %s", tokenStep, _resWords [prgmCnt.pResW->tokenIndex]._resWordName ); }
            break;

        case tok_isInternFunction:
            sprintf( s, "(step %d) int func: %s", tokenStep, _functions [prgmCnt.pIntFnc->tokenIndex].funcName );
            break;

        case tok_isExternFunction:
            identNameIndex = (int) prgmCnt.pExtFnc->identNameIndex;   // external function list element
            identifierName = _pcalculator->extFunctionNames [identNameIndex];
            funcStart = (uint32_t) _pcalculator->extFunctionData [identNameIndex].pExtFunctionStartToken;
            if ( funcStart != 0 ) { funcStart -= (uint32_t) _pcalculator->_programStorage; }
            sprintf( s, "(step %d) ext func nr %d: %s, start: %lu", tokenStep, identNameIndex, identifierName, funcStart );
            break;

        case tok_isVariable:
            identNameIndex = (int) (prgmCnt.pVar->identNameIndex);
            valueIndex = (int) (prgmCnt.pVar->identValueIndex);

            identifierName = _pcalculator->programVarNames [identNameIndex];
            tokenInfo = prgmCnt.pVar->identInfo;
            isArray = (tokenInfo & _pcalculator->var_isArray);
            varQualifier = (tokenInfo & _pcalculator->var_qualifierMask);

            //// aanpassen:
            ////isUserVar = (prgmCnt.pVar->identInfo & _pcalculator->var_qualifierMask) == _pcalculator->var_isUser;
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
                    if ( varQualifier == _pcalculator->var_isGlobal ) { varStrValue = _pcalculator->globalVarValues [identNameIndex].pAlphanumConst; } // also ok for array pointer
                    else if ( varQualifier == _pcalculator->var_isStaticInFunc ) { varStrValue = _pcalculator->staticVarValues [valueIndex].pAlphanumConst; }
                    else { varStrValue = nullptr; }
                    sprintf( s, "(step %d) %s string: %s, AN cst: <%s>", tokenStep, qual, identifierName, (varStrValue == nullptr) ? "" : varStrValue );
                }

                else {
                    if ( varQualifier == _pcalculator->var_isGlobal ) { f = _pcalculator->globalVarValues [identNameIndex].numConst; }
                    else if ( varQualifier == _pcalculator->var_isStaticInFunc ) { f = _pcalculator->staticVarValues [valueIndex].numConst; }
                    else { f = 0. + valueIndex; }      // no local variable storage yet (test value only)
                    sprintf( s, "(step %d) %s float: %s, Num: %.3G", tokenStep, qual, identifierName, f );
                }
            }
            break;

        case tok_isNumConst:
            memcpy( &f, prgmCnt.pFloat->numConst, sizeof( f ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "(step %d) Num: %.3G", tokenStep, f );
            break;

        case tok_isAlphaConst:
            memcpy( &pAnum, prgmCnt.pAnumP->pAlphanumConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "(step %d) AN cst: <%s>", tokenStep, pAnum );
            break;

        case tok_isGenericName:
            memcpy( &pAnum, prgmCnt.pAnumP->pAlphanumConst, sizeof( pAnum ) );                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf( s, "(step %d) Identifier name: %s", tokenStep, pAnum );
            break;

        case tok_isOperator:
            len = strlen( singleCharTokens );
            index = (prgmCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F;
            if ( index < len ) { pch [0] = singleCharTokens [index]; pch [1] = '\0'; }
            else {
                strcat( pch, ((index == len) ? "<=" : (index == len + 1) ? ">=" : "<>") );
            }
            sprintf( s, "(step %d) Op: %s", tokenStep, pch );
            break;

        case tok_isCommaSeparator:
            sprintf( s, "(step %d) Sep: %c", tokenStep, singleCharTokens [(prgmCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        case tok_isSemiColonSeparator:
            sprintf( s, "(step %d) Sep: %c", tokenStep, singleCharTokens [(prgmCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        case tok_isLeftParenthesis:
            sprintf( s, "(step %d) Par: %c", tokenStep, singleCharTokens [(prgmCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        case tok_isRightParenthesis:
            sprintf( s, "(step %d) Par.: %c", tokenStep, singleCharTokens [(prgmCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F] );
            break;

        }

        // append pretty printed token to character string (if still place left)
        if ( strlen( s ) <= maxCharsPretty ) { strcat( prettyToken, s ); }
        ////if ( strlen( prettyToken ) > 0 ) { Serial.println( prettyToken ); }
        int tokenLength = (tokenType >= tok_isOperator) ? 1 : (*prgmCnt.pToken >> 4) & 0x0F;
        prgmCnt.pToken += tokenLength;
        tokenType = *prgmCnt.pToken & 0x0F;
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

    else  if ( (result == result_undefinedFunction) && _pcalculator->_programMode ) {     // in program mode only (because function can be defined after a call)
        sprintf( parsingInfo, "\r\nError %d: function: %s", result, _pcalculator->extFunctionNames [funcNotDefIndex] );
    }

    else {                                                                              // error
        int len = _pcalculator->_isPrompt ? _pcalculator->_promptLength : 0;
        char point [pErrorPos - pInstruction + 2 +  len];                               // 2 extra positions for '^' and '\0' characters
        memset( point, ' ', pErrorPos - pInstruction + len );
        point [pErrorPos - pInstruction + len] = '^';
        point [pErrorPos - pInstruction + len + 1] = '\0';
        _pcalculator->_pConsole->println( pInstruction );
        _pcalculator->_pConsole->println( point );
        if ( _pcalculator->_programMode ) { sprintf( parsingInfo, "Error %d: statement ending at line %d", result, lineCount ); }
        else { sprintf( parsingInfo, "Error %d\r\n", result ); }
    }

    if ( strlen( parsingInfo ) > 0 ) { _pcalculator->_pConsole->println( parsingInfo ); _pcalculator->_isPrompt = false; }
};
