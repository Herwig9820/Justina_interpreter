#include "Justina.h"

#define printCreateDeleteHeapObjects 0
#define printParsedTokens 0
#define debugPrint 0


/***********************************************************
*                       class MyParser                     *
*             parse character string into tokens           *
***********************************************************/

// -------------------------------------------------
// *   // initialisation of static class members   *
// -------------------------------------------------


// commands (FUNCTION, FOR, ...): 'allowed command parameter' keys
// ---------------------------------------------------------------

const char
// command parameter spec name      param type and flags                param type and flags                            param type and flags                        param type and flags
// ---------------------------      --------------------                --------------------                            --------------------                        --------------------
MyParser::cmdPar_100[4]{ cmdPar_ident | cmdPar_multipleFlag,            cmdPar_none,                                    cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_101[4]{ cmdPar_ident,                                  cmdPar_expression | cmdPar_optionalFlag,        cmdPar_expression | cmdPar_optionalFlag,        cmdPar_expression | cmdPar_optionalFlag, },
MyParser::cmdPar_102[4]{ cmdPar_none,                                   cmdPar_none,                                    cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_103[4]{ cmdPar_ident,                                  cmdPar_none,                                    cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_104[4]{ cmdPar_expression,                             cmdPar_none,                                    cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_105[4]{ cmdPar_expression,                             cmdPar_expression,                              cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_106[4]{ cmdPar_expression | cmdPar_optionalFlag,       cmdPar_none,                                    cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_107[4]{ cmdPar_expression | cmdPar_multipleFlag,       cmdPar_none,                                    cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_108[4]{ cmdPar_extFunction,                            cmdPar_none,                                    cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_109[4]{ cmdPar_varOptAssignment,                       cmdPar_expression,                              cmdPar_expression | cmdPar_optionalFlag,        cmdPar_none },
MyParser::cmdPar_110[4]{ cmdPar_ident,                                  cmdPar_ident | cmdPar_multipleFlag,             cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_111[4]{ cmdPar_varOptAssignment,                       cmdPar_varOptAssignment | cmdPar_multipleFlag,  cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_112[4]{ cmdPar_expression,                             cmdPar_expression | cmdPar_multipleFlag,        cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_113[4]{ cmdPar_expression,                             cmdPar_varOptAssignment,                        cmdPar_varOptAssignment,                        cmdPar_none },
MyParser::cmdPar_114[4]{ cmdPar_expression,                             cmdPar_varOptAssignment | cmdPar_optionalFlag,  cmdPar_none,                                    cmdPar_none },
MyParser::cmdPar_999[4]{ cmdPar_varNoAssignment,                        cmdPar_none,                                    cmdPar_none,                                    cmdPar_none }////test
;

// commands: keywords with attributes
// ----------------------------------

const MyParser::ResWordDef MyParser::_resWords[]{
    //  name            id code             where allowed               padding (boundary alignment)    param key       control info
    //  ----            -------             -------------               ----------------------------    ---------      ------------   

    /* programs and functions */
    /* ---------------------- */

    {"Program",         cmdcod_program,     cmd_onlyProgramTop | cmd_skipDuringExec,            0,0,    cmdPar_103,     cmdProgram},
    {"Function",        cmdcod_function,    cmd_onlyInProgram | cmd_skipDuringExec,             0,0,    cmdPar_108,     cmdBlockExtFunction},


    /* variables */
    /* --------- */

    {"Var",             cmdcod_var,         cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,  0,0,    cmdPar_111,     cmdGlobalVar},
    {"Static",          cmdcod_static,      cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_111,     cmdStaticVar},
    {"Local",           cmdcod_local,       cmd_onlyInFunctionBlock | cmd_skipDuringExec,       0,0,    cmdPar_111,     cmdLocalVar},

    //// to do
    {"Delvar",          cmdcod_delete,      cmd_onlyImmediate | cmd_skipDuringExec,             0,0,    cmdPar_110,     cmdDeleteVar},
    {"Clearvars",       cmdcod_clear,       cmd_onlyImmediate | cmd_skipDuringExec,             0,0,    cmdPar_102,     cmdBlockNone},
    {"Vars",            cmdcod_vars,        cmd_onlyImmediate | cmd_skipDuringExec,             0,0,    cmdPar_102,     cmdBlockNone},
    {"Test",            cmdcod_test,        cmd_onlyImmediate | cmd_skipDuringExec,             0,0,    cmdPar_999,     cmdBlockNone},//// test


    /* flow control commands */
    /* --------------------- */

    {"For",             cmdcod_for,         cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_109,     cmdBlockFor},
    {"While",           cmdcod_while,       cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockWhile},
    {"If",              cmdcod_if,          cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockIf},
    {"Elseif",          cmdcod_elseif,      cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_104,     cmdBlockIf_elseIf},
    {"Else",            cmdcod_else,        cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockIf_else},

    {"Break",           cmdcod_break,       cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockOpenBlock_loop},        // allowed if at least one open loop block (any level) 
    {"Continue",        cmdcod_continue,    cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_102,     cmdBlockOpenBlock_loop },       // allowed if at least one open loop block (any level) 
    {"Return",          cmdcod_return,      cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_106,     cmdBlockOpenBlock_function},    // allowed if currently an open function definition block 

    {"End",             cmdcod_end,         cmd_noRestrictions,                                 0,0,    cmdPar_102,     cmdBlockGenEnd},                // closes inner open command block

    {"Quit",            cmdcod_quit,        cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_106,     cmdBlockNone},


    /* input and output commands */
    /* ------------------------- */

    {"Info",            cmdcod_info,        cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_114,     cmdBlockNone},
    {"Input",           cmdcod_input,       cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_113,     cmdBlockNone},
    {"Print",           cmdcod_print,       cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_107,     cmdBlockNone},
    {"Dispfmt",         cmdcod_dispfmt,     cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_112,     cmdBlockNone},
    {"Dispmod",         cmdcod_dispmod,     cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_105,     cmdBlockNone},
    {"Pause",           cmdcod_pause,       cmd_onlyInFunctionBlock,                            0,0,    cmdPar_104,     cmdBlockNone},
    {"Halt",            cmdcod_halt,        cmd_onlyInFunctionBlock,                            0,0,    cmdPar_102,     cmdBlockNone},


    /* debugging commands */
    /* ------------------ */

    {"Stop",            cmdcod_stop,        cmd_onlyInFunctionBlock,                            0,0,    cmdPar_102,     cmdBlockNone},
    {"Go",              cmdcod_go,          cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"Step",            cmdcod_step,        cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"Debug",           cmdcod_debug,       cmd_onlyImmediate,                                  0,0,    cmdPar_102,     cmdBlockNone},
    {"Nop",             cmdcod_nop,         cmd_onlyImmOrInsideFuncBlock | cmd_skipDuringExec,  0,0,    cmdPar_102,     cmdBlockNone},                  // insert two bytes in program, do nothing


    /* user callback functions */
    /* ----------------------- */

    {"DeclareCB",       cmdcod_decCBproc,   cmd_onlyOutsideFunctionBlock | cmd_skipDuringExec,  0,0,    cmdPar_100,     cmdBlockNone},
    {"Callback",        cmdcod_callback,    cmd_onlyImmOrInsideFuncBlock,                       0,0,    cmdPar_101,     cmdBlockNone},
};


// internal (intrinsic) functions
// ------------------------------

// the 8 array pattern bits indicate the order of arrays and scalars; bit b0 to bit b7 refer to parameter 1 to 8, if a bit is set, an array is expected as argument
// maximum number of parameters should be no more than 8

const MyParser::FuncDef MyParser::_functions[]{
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
    {"millis",      fnccod_millis,      0,0,    0b0},
    {"sqrt",        fnccod_sqrt,        1,1,    0b0},
    {"ubound",      fnccod_ubound,      2,2,    0b00000001},        // first parameter is array (LSB)
    {"dims",        fnccod_dims,        1,1,    0b00000001},
    {"valtype",     fnccod_valueType,   1,1,    0b0},
    {"last",        fnccod_last,        0,1,    0b0},
    {"asc",         fnccod_asc,         1,2,    0b0},
    {"char",        fnccod_char,        1,1,    0b0},
    {"len",         fnccod_len,         1,1,    0b0},
    {"nl",          fnccod_nl,          0,0,    0b0},
    {"ft",          fnccod_format,      1,6,    0b0},               // short label
    {"sysvar",      fnccod_sysVar,      1,1,    0b0}
};


// terminal tokens 
// ---------------

// priority: bits b43210 define priority if used as prefix, infix, postfix operator, respectively (0x1 = lowest, 0x1F = highest) 
// priority 0 means operator not available for use as use as postfix, prefix, infix operator
// bit b7 defines associativity for infix operators (bit set indicates 'right-to-left').
// prefix operators: always right-to-left. postfix operators: always left-to-right
// NOTE: table entries with names starting with same characters: shortest entries should come BEFORE longest (e.g. '!' before '!=', '&' before '&&')
// postfix operator names can only be shared with prefix operator names

const MyParser::TerminalDef MyParser::_terminals[]{

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

    {term_plus,             termcod_plus,               0x0C,               0x0A,                       0x00},      // strings: concatenate
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
    {term_bitShRightAssign, termcod_bitShRightAssign,   0x00,               0x01 | op_RtoL | op_long,   0x00},
};


// -------------------
// *   constructor   *
// -------------------

MyParser::MyParser(Interpreter* const pInterpreter) : _pInterpreter(pInterpreter) {
    _resWordCount = (sizeof(_resWords)) / sizeof(_resWords[0]);
    _functionCount = (sizeof(_functions)) / sizeof(_functions[0]);
    _terminalCount = (sizeof(_terminals)) / sizeof(_terminals[0]);

    _blockLevel = 0;
    _extFunctionBlockOpen = false;
}


// ---------------------
// *   deconstructor   *
// ---------------------

MyParser::~MyParser() {
    resetMachine(true);             // delete all objects created on the heap
}


// -----------------------------------------------------------------------------------------
// *   delete all identifier names (char strings)                                          *
// *   note: this excludes UNQUALIFIED identifier names stored as alphanumeric constants   *
// -----------------------------------------------------------------------------------------

void MyParser::deleteIdentifierNameObjects(char** pIdentNameArray, int identifiersInUse, bool isUserVar) {
    int index = 0;          // points to last variable in use
    while (index < identifiersInUse) {                       // points to variable in use
#if printCreateDeleteHeapObjects
        Serial.print(isUserVar ? "----- (usrvar name) " : "----- (ident name ) "); Serial.println((uint32_t) * (pIdentNameArray + index) - RAMSTART);
#endif
        delete[] * (pIdentNameArray + index);
        isUserVar ? _pInterpreter->userVarNameStringObjectCount-- : _pInterpreter->identifierNameStringObjectCount--;
        index++;
    }
}


// ----------------------------------------------------------------------------------------------
// *   delete variable heap objects: scalar variable strings and array variable array storage   *
// ----------------------------------------------------------------------------------------------

void MyParser::deleteArrayElementStringObjects(Interpreter::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue, bool isUserVar, bool isLocalVar) {
    int index = 0;
    while (index < varNameCount) {
        if (!checkIfGlobalValue || (varType[index] & (_pInterpreter->var_nameHasGlobalValue))) { // if only for global values: is it a global value ?
            if ((varType[index] & (_pInterpreter->var_isArray | _pInterpreter->value_typeMask)) ==
                (_pInterpreter->var_isArray | _pInterpreter->value_isStringPointer)) {              // array of strings

                void* pArrayStorage = varValues[index].pArray;        // void pointer to an array of string pointers; element 0 contains dimensions and dimension count
                int dimensions = (((char*)pArrayStorage)[3]);  // can range from 1 to MAX_ARRAY_DIMS
                int arrayElements = 1;                                  // determine array size
                for (int dimCnt = 0; dimCnt < dimensions; dimCnt++) { arrayElements *= (int)((((char*)pArrayStorage)[dimCnt])); }

                // delete non-empty strings
                for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) {  // array element 0 contains dimensions and count
                    char* pString = ((char**)pArrayStorage)[arrayElem];
                    uint32_t stringPointerAddress = (uint32_t) & (((char**)pArrayStorage)[arrayElem]);
                    if (pString != nullptr) {
#if printCreateDeleteHeapObjects
                        Serial.print(isUserVar ? "----- (usr arr str) " : isLocalVar ? "-----(loc arr str)" : "----- (arr string ) "); Serial.println((uint32_t)pString - RAMSTART);     // applicable to string and array (same pointer)
#endif
                        delete[]  pString;                                  // applicable to string and array (same pointer)
                        isUserVar ? _pInterpreter->userVarStringObjectCount-- : isLocalVar ? _pInterpreter->localVarStringObjectCount-- : _pInterpreter->globalStaticVarStringObjectCount--;
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

void MyParser::deleteVariableValueObjects(Interpreter::Val* varValues, char* varType, int varNameCount, bool checkIfGlobalValue, bool isUserVar, bool isLocalVar) {
    int index = 0;
    while (index < varNameCount) {
        if (!checkIfGlobalValue || (varType[index] & (_pInterpreter->var_nameHasGlobalValue))) { // global value ?
            // check for arrays before checking for strings (if both 'var_isArray' and 'value_isStringPointer' bits are set: array of strings, with strings already deleted)
            if (varType[index] & _pInterpreter->var_isArray) {       // variable is an array: delete array storage          
#if printCreateDeleteHeapObjects
                Serial.print(isUserVar ? "----- (usr ar stor) " : isLocalVar ? "----- (loc ar stor) " : "----- (array stor ) "); Serial.println((uint32_t)varValues[index].pStringConst - RAMSTART);
#endif
                delete[]  varValues[index].pArray;
                isUserVar ? _pInterpreter->userArrayObjectCount-- : isLocalVar ? _pInterpreter->localArrayObjectCount-- : _pInterpreter->globalStaticArrayObjectCount--;
            }
            else if ((varType[index] & _pInterpreter->value_typeMask) == _pInterpreter->value_isStringPointer) {       // variable is a scalar containing a string
                if (varValues[index].pStringConst != nullptr) {
#if printCreateDeleteHeapObjects
                    Serial.print(isUserVar ? "----- (usr var str) " : isLocalVar ? "----- (loc var str)" : "----- (var string ) "); Serial.println((uint32_t)varValues[index].pStringConst - RAMSTART);
#endif
                    delete[]  varValues[index].pStringConst;
                    isUserVar ? _pInterpreter->userVarStringObjectCount-- : isLocalVar ? _pInterpreter->localVarStringObjectCount-- : _pInterpreter->globalStaticVarStringObjectCount--;
                }
            }
        }
        index++;
    }
}


// ----------------------------------------------------------------------------------------------
// *   delete variable heap objects: scalar variable strings and array variable array storage   *
// ----------------------------------------------------------------------------------------------

void MyParser::deleteLastValueFiFoStringObjects() {

    if (_pInterpreter->_lastResultCount == 0) return;

    for (int i = 0; i < _pInterpreter->_lastResultCount; i++) {
        bool isNonEmptyString = (_pInterpreter->lastResultTypeFiFo[i] == _pInterpreter->value_isStringPointer) ? (_pInterpreter->lastResultValueFiFo[i].pStringConst != nullptr) : false;
        if (isNonEmptyString) {
#if printCreateDeleteHeapObjects
            Serial.print("----- (FiFo string) "); Serial.println((uint32_t)_pInterpreter->lastResultValueFiFo[i].pStringConst - RAMSTART);
#endif
            delete[] _pInterpreter->lastResultValueFiFo[i].pStringConst;
            _pInterpreter->lastValuesStringObjectCount--;
        }
    }
}



// -----------------------------------------------------------------------------------------
// *   delete all parsed alphanumeric constant value heap objects                          *
// *   note: this includes UNQUALIFIED identifier names stored as alphanumeric constants   *
// -----------------------------------------------------------------------------------------

// must be called before deleting tokens (list elements) 

void MyParser::deleteConstStringObjects(char* programStart) {
    char* pAnum;
    Interpreter::TokenPointer prgmCnt;

    prgmCnt.pTokenChars = programStart;
    uint8_t tokenType = *prgmCnt.pTokenChars & 0x0F;
    while (tokenType != '\0') {                                                                    // for all tokens in token list
        bool isStringConst = (tokenType == Interpreter::tok_isConstant) ? (((*prgmCnt.pTokenChars >> 4) & Interpreter::value_typeMask) == Interpreter::value_isStringPointer) : false;
        if (isStringConst || (tokenType == Interpreter::tok_isGenericName)) {
            memcpy(&pAnum, prgmCnt.pCstToken->cstValue.pStringConst, sizeof(pAnum));                         // pointer not necessarily aligned with word size: copy memory instead
            if (pAnum != nullptr) {
#if printCreateDeleteHeapObjects
                Serial.print("----- (parsed str ) ");   Serial.println((uint32_t)pAnum - RAMSTART);
#endif
                delete[] pAnum;
                _pInterpreter->parsedStringConstObjectCount--;
            }
        }
        uint8_t tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? sizeof(Interpreter::TokenIsTerminal) :
            (tokenType == Interpreter::tok_isConstant) ? sizeof(Interpreter::TokenIsConstant) : (*prgmCnt.pTokenChars >> 4) & 0x0F;
        prgmCnt.pTokenChars += tokenLength;
        tokenType = *prgmCnt.pTokenChars & 0x0F;
    }
}


// --------------------
// *   reset parser   *
// --------------------

void MyParser::resetMachine(bool withUserVariables) {
    // delete identifier name objects on the heap (variable names, external function names) 
    deleteIdentifierNameObjects(_pInterpreter->programVarNames, _pInterpreter->_programVarNameCount);
    deleteIdentifierNameObjects(_pInterpreter->extFunctionNames, _pInterpreter->_extFunctionCount);
    if (withUserVariables) { deleteIdentifierNameObjects(_pInterpreter->userVarNames, _pInterpreter->_userVarCount, true); }

    // delete variable heap objects: array variable element string objects
    deleteArrayElementStringObjects(_pInterpreter->globalVarValues, _pInterpreter->globalVarType, _pInterpreter->_programVarNameCount, true);
    deleteArrayElementStringObjects(_pInterpreter->staticVarValues, _pInterpreter->staticVarType, _pInterpreter->_staticVarCount, false);
    if (withUserVariables) {
        deleteArrayElementStringObjects(_pInterpreter->userVarValues, _pInterpreter->userVarType, _pInterpreter->_userVarCount, false, true);
        deleteLastValueFiFoStringObjects();
    }

    // delete variable heap objects: scalar variable strings and array variable array storage 
    deleteVariableValueObjects(_pInterpreter->globalVarValues, _pInterpreter->globalVarType, _pInterpreter->_programVarNameCount, true);
    deleteVariableValueObjects(_pInterpreter->staticVarValues, _pInterpreter->staticVarType, _pInterpreter->_staticVarCount, false);
    if (withUserVariables) { deleteVariableValueObjects(_pInterpreter->userVarValues, _pInterpreter->userVarType, _pInterpreter->_userVarCount, false, true); }

    // delete alphanumeric constants: before clearing program memory and immediate mode user instructon memory
    deleteConstStringObjects(_pInterpreter->_programStorage);
    deleteConstStringObjects(_pInterpreter->_programStorage + _pInterpreter->PROG_MEM_SIZE);

    parsingStack.deleteList();                                                               // delete list to keep track of open parentheses and open command blocks
    _blockLevel = 0;
    _extFunctionBlockOpen = false;

    // init interpreter variables: AFTER deleting heap objects
    _pInterpreter->_programsInDebug = 0;
    _pInterpreter->_programName[0] = '\0';
    _pInterpreter->_programVarNameCount = 0;
    _pInterpreter->_localVarCountInFunction = 0;
    _pInterpreter->_paramOnlyCountInFunction = 0;
    _pInterpreter->_localVarCount = 0;
    _pInterpreter->_staticVarCountInFunction = 0;
    _pInterpreter->_staticVarCount = 0;
    _pInterpreter->_extFunctionCount = 0;

    if (withUserVariables) { _pInterpreter->_userVarCount = 0; }
    else {
        int index = 0;          // clear user variable flag 'variable is used by program'
        while (index++ < _pInterpreter->_userVarCount) {
            _pInterpreter->userVarType[index] = _pInterpreter->userVarType[index] & ~_pInterpreter->var_userVarUsedByProgram;
        }
    }

    _pInterpreter->_lastResultCount = 0;                                       // current last result FiFo depth (values currently stored)

    _pInterpreter->_userCBprocAliasSet_count = 0;   // note: _userCBprocStartSet_count: only reset when starting interpreter

    // calculation result print
    _pInterpreter->_dispWidth = _pInterpreter->_defaultPrintWidth, _pInterpreter->_dispNumPrecision = _pInterpreter->_defaultNumPrecision;
    _pInterpreter->_dispCharsToPrint = _pInterpreter->_defaultCharsToPrint; _pInterpreter->_dispFmtFlags = _pInterpreter->_defaultPrintFlags;
    _pInterpreter->_dispNumSpecifier[0] = 'G'; _pInterpreter->_dispNumSpecifier[1] = '\0';
    _pInterpreter->_dispIsIntFmt = false;
    _pInterpreter->makeFormatString(_pInterpreter->_dispFmtFlags, false, _pInterpreter->_dispNumSpecifier, _pInterpreter->_dispNumberFmtString);       // for numbers
    strcpy(_pInterpreter->_dispStringFmtString, "%*.*s%n");                                                           // for strings

    // for print command
    _pInterpreter->_printWidth = _pInterpreter->_defaultPrintWidth, _pInterpreter->_printNumPrecision = _pInterpreter->_defaultNumPrecision;
    _pInterpreter->_printCharsToPrint = _pInterpreter->_defaultCharsToPrint, _pInterpreter->_printFmtFlags = _pInterpreter->_defaultPrintFlags;
    _pInterpreter->_printNumSpecifier[0] = 'G'; _pInterpreter->_printNumSpecifier[1] = '\0';

    // display output settings
    _pInterpreter->_promptAndEcho = 2, _pInterpreter->_printLastResult = true;


    _pInterpreter->_programStart = _pInterpreter->_programStorage + (_pInterpreter->_programMode ? 0 : _pInterpreter->PROG_MEM_SIZE);
    _pInterpreter->_programSize = _pInterpreter->_programSize + (_pInterpreter->_programMode ? _pInterpreter->PROG_MEM_SIZE : _pInterpreter->IMM_MEM_SIZE);
    _pInterpreter->_programCounter = _pInterpreter->_programStart;                          // start of 'immediate mode' program area

    *_pInterpreter->_programStorage = '\0';                                    //  current end of program 
    *_pInterpreter->_programStart = '\0';                                      //  current end of program (immediate mode)

    _pInterpreter->_callStackDepth = 0;
    _pInterpreter->_programsInDebug = 0;
    _pInterpreter->_doOneProgramStep = false;

    // perform consistency checks: verify that all objects created are destroyed again
    // note: intermediate string objects, function local storage, and function local variable strings and arrays exist solely during execution.
    //       count of function local variable strings and arrays is checked each time execution terminates 

    // parsing stack: no need to check if any elements were left (the list has just been deleted)
    // note: this stack does not contain any pointers to heap objects

    // string and array heap objects: any objects left ?
    if (_pInterpreter->identifierNameStringObjectCount != 0) {
        Serial.print("*** Variable / function name objects cleanup error. Remaining: "); Serial.println(_pInterpreter->identifierNameStringObjectCount); //// _pConsole ???
    }

    if (_pInterpreter->parsedStringConstObjectCount != 0) {
        Serial.print("*** Parsed constant string objects cleanup error. Remaining: "); Serial.println(_pInterpreter->parsedStringConstObjectCount);
    }

    if (_pInterpreter->globalStaticVarStringObjectCount != 0) {
        Serial.print("*** Variable string objects cleanup error. Remaining: "); Serial.println(_pInterpreter->globalStaticVarStringObjectCount);
    }

    if (_pInterpreter->globalStaticArrayObjectCount != 0) {
        Serial.print("*** Array objects cleanup error. Remaining: "); Serial.println(_pInterpreter->globalStaticArrayObjectCount);
    }

#if debugPrint
    Serial.print("\r\n** Reset stats\r\n    parsed strings "); Serial.print(_pInterpreter->parsedStringConstObjectCount);

    Serial.print(", prog name strings "); Serial.print(_pInterpreter->identifierNameStringObjectCount);
    Serial.print(", prog var strings "); Serial.print(_pInterpreter->globalStaticVarStringObjectCount);
    Serial.print(", prog arrays "); Serial.print(_pInterpreter->globalStaticArrayObjectCount);
#endif

    _pInterpreter->parsedStringConstObjectCount = 0;

    _pInterpreter->identifierNameStringObjectCount = 0;
    _pInterpreter->globalStaticVarStringObjectCount = 0;
    _pInterpreter->globalStaticArrayObjectCount = 0;

    if (withUserVariables) {
        if (_pInterpreter->userVarNameStringObjectCount != 0) {
            Serial.print("*** User variable name objects cleanup error. Remaining: "); Serial.println(_pInterpreter->userVarNameStringObjectCount);
        }

        if (_pInterpreter->userVarStringObjectCount != 0) {
            Serial.print("*** User variable string objects cleanup error. Remaining: "); Serial.println(_pInterpreter->userVarStringObjectCount);
        }

        if (_pInterpreter->userArrayObjectCount != 0) {
            Serial.print("*** User array objects cleanup error. Remaining: "); Serial.println(_pInterpreter->userArrayObjectCount);
        }

        if (_pInterpreter->lastValuesStringObjectCount != 0) {
            Serial.print("*** Last value FiFo string objects cleanup error. Remaining: "); Serial.print(_pInterpreter->lastValuesStringObjectCount);
        }

#if debugPrint
        Serial.print(", user var names "); Serial.print(_pInterpreter->userVarNameStringObjectCount);
        Serial.print(", user var strings "); Serial.print(_pInterpreter->userVarStringObjectCount);
        Serial.print(", user arrays "); Serial.print(_pInterpreter->userArrayObjectCount);

        Serial.print(", last value strings "); Serial.print(_pInterpreter->lastValuesStringObjectCount);
#endif

        _pInterpreter->userVarNameStringObjectCount = 0;
        _pInterpreter->userVarStringObjectCount = 0;
        _pInterpreter->userArrayObjectCount = 0;

        _pInterpreter->lastValuesStringObjectCount = 0;
    }
    Serial.println();       //// _pConsole ???

    // intermediateStringObjectCount, localVarStringObjectCount, localArrayObjectCount ...
    // ... is not tested, neither is it reset, here. It is a purely execution related object, tested at the end of execution

}


// -------------------------------------------------------------------------
// *   check if identifier storage exists already, optionally create new   *
// -------------------------------------------------------------------------

int MyParser::getIdentifier(char** pIdentNameArray, int& identifiersInUse, int maxIdentifiers, char* pIdentNameToCheck, int identLength, bool& createNewName, bool isUserVar) {

    char* pIdentifierName;
    int index = 0;          // points to last variable in use
    while (index < identifiersInUse) {                       // points to variable in use
        pIdentifierName = pIdentNameArray[index];
        if (strlen(pIdentifierName) == identLength) {                                    // identifier with name with same length found
            if (strncmp(pIdentifierName, pIdentNameToCheck, identLength) == 0) { break; } // storage for identifier name was created already 
        }
        index++;
    }
    if (index == identifiersInUse) { index = -1; }                                  // not found
    if (!createNewName) { return index; }                                                 // if check only: identNameIndex to identifier name or -1, createNewName = false

    createNewName = (index == -1);                                       // create new ?

    // create new identifier if it does not exist yet ?
    // upon return, createNew indicates whether new identifier storage NEEDED to be created ...
    // and if it was possible, identifiersInUse will be set to the new identifier count

    if (createNewName) {
        if (identifiersInUse == maxIdentifiers) { return index; }                // create identifier name failed: return -1 with createNewName = true
        pIdentifierName = new char[Interpreter::_maxIdentifierNameLen + 1 + 1];                      // create standard length char array on the heap, including '\0' and an extra character 
        isUserVar ? _pInterpreter->userVarNameStringObjectCount++ : _pInterpreter->identifierNameStringObjectCount++;
#if printCreateDeleteHeapObjects
        Serial.print(isUserVar ? "+++++ (usrvar name) " : "+++++ (ident name ) "); Serial.println((uint32_t)pIdentifierName - RAMSTART);
#endif
        strncpy(pIdentifierName, pIdentNameToCheck, identLength);                            // store identifier name in newly created character array
        pIdentifierName[identLength] = '\0';                                                 // string terminating '\0'
        pIdentNameArray[identifiersInUse] = pIdentifierName;
        identifiersInUse++;
        return identifiersInUse - 1;                                                   // identNameIndex to newly created identifier name
    }
}

// --------------------------------------------------------------
// *   initialize a variable or an array with (a) constant(s)   *
// --------------------------------------------------------------

bool MyParser::initVariable(uint16_t varTokenStep, uint16_t constTokenStep) {
    long l{ 0 };
    float f{ 0. };        // last token is a number constant: dimension spec
    char* pString{ nullptr };

    // parsing: initialize variables and arrays with a constant number or (arrays: empty) string

    // fetch variable location and attributes
    bool isArrayVar = ((Interpreter::TokenIsVariable*)(_pInterpreter->_programStorage + varTokenStep))->identInfo & _pInterpreter->var_isArray;
    bool isGlobalVar = (((Interpreter::TokenIsVariable*)(_pInterpreter->_programStorage + varTokenStep))->identInfo & _pInterpreter->var_scopeMask) == _pInterpreter->var_isGlobal;
    bool isUserVar = (((Interpreter::TokenIsVariable*)(_pInterpreter->_programStorage + varTokenStep))->identInfo & _pInterpreter->var_scopeMask) == _pInterpreter->var_isUser;
    int varValueIndex = ((Interpreter::TokenIsVariable*)(_pInterpreter->_programStorage + varTokenStep))->identValueIndex;
    void* pVarStorage = isGlobalVar ? _pInterpreter->globalVarValues : isUserVar ? _pInterpreter->userVarValues : _pInterpreter->staticVarValues;
    char* pVarTypeStorage = isGlobalVar ? _pInterpreter->globalVarType : isUserVar ? _pInterpreter->userVarType : _pInterpreter->staticVarType;
    void* pArrayStorage;        // array storage (if array) 

    // fetch constant (numeric or alphanumeric) 
    char valueType = (((Interpreter::TokenIsConstant*)(_pInterpreter->_programStorage + constTokenStep))->tokenType >> 4) & Interpreter::value_typeMask;
    bool isLongConst = (valueType == Interpreter::value_isLong);
    bool isFloatConst = (valueType == Interpreter::value_isFloat);
    bool isStringConst = (valueType == Interpreter::value_isStringPointer);

    if (isLongConst) { memcpy(&l, ((Interpreter::TokenIsConstant*)(_pInterpreter->_programStorage + constTokenStep))->cstValue.longConst, sizeof(l)); }        // copy float
    else if (isFloatConst) { memcpy(&f, ((Interpreter::TokenIsConstant*)(_pInterpreter->_programStorage + constTokenStep))->cstValue.floatConst, sizeof(f)); }        // copy float
    else { memcpy(&pString, ((Interpreter::TokenIsConstant*)(_pInterpreter->_programStorage + constTokenStep))->cstValue.pStringConst, sizeof(pString)); }     // copy pointer to string (not the string itself)
    int length = (!isStringConst) ? 0 : (pString == nullptr) ? 0 : strlen(pString);       // only relevant for strings

    if (isArrayVar) {
        pArrayStorage = ((void**)pVarStorage)[varValueIndex];        // void pointer to an array 
        int dimensions = (((char*)pArrayStorage)[3]);  // can range from 1 to MAX_ARRAY_DIMS
        int arrayElements = 1;                                  // determine array size
        for (int dimCnt = 0; dimCnt < dimensions; dimCnt++) { arrayElements *= (int)((((char*)pArrayStorage)[dimCnt])); }
        // fill up with numeric constants or (empty strings:) null pointers
        if (isLongConst) { for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) { ((long*)pArrayStorage)[arrayElem] = l; } }
        else if (isFloatConst) { for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) { ((float*)pArrayStorage)[arrayElem] = f; } }
        else {                                                      // alphanumeric constant
            if (length != 0) { return false; };       // to limit memory usage, no mass initialisation with non-empty strings
            for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) {
                ((char**)pArrayStorage)[arrayElem] = nullptr;
            }
        }
    }

    else {                                  // scalar
        if (isLongConst) { ((long*)pVarStorage)[varValueIndex] = l; }      // store numeric constant
        else if (isFloatConst) { ((float*)pVarStorage)[varValueIndex] = f; }      // store numeric constant
        else {                                                  // alphanumeric constant
            if (length == 0) {
                ((char**)pVarStorage)[varValueIndex] = nullptr;       // an empty string does not create a heap object
            }
            else { // create string object and store string
                char* pVarAlphanumValue = new char[length + 1];          // create char array on the heap to store alphanumeric constant, including terminating '\0'
                isUserVar ? _pInterpreter->userVarStringObjectCount++ : _pInterpreter->globalStaticVarStringObjectCount++;
#if printCreateDeleteHeapObjects
                Serial.print(isUserVar ? "+++++ (usr var str) " : "+++++ (var string ) "); Serial.println((uint32_t)pVarAlphanumValue - RAMSTART);
#endif
                // store alphanumeric constant in newly created character array
                strcpy(pVarAlphanumValue, pString);              // including terminating \0
                ((char**)pVarStorage)[varValueIndex] = pVarAlphanumValue;       // store pointer to string
            }
        }
    }


    pVarTypeStorage[varValueIndex] = (pVarTypeStorage[varValueIndex] & ~_pInterpreter->value_typeMask) |
        (isLongConst ? _pInterpreter->value_isLong : isFloatConst ? _pInterpreter->value_isFloat : _pInterpreter->value_isStringPointer);
    return true;
};


// --------------------------------------------------------------
// *   check if all external functions referenced are defined   *
// --------------------------------------------------------------

bool MyParser::allExternalFunctionsDefined(int& index) {
    index = 0;
    while (index < _pInterpreter->_extFunctionCount) {                       // points to variable in use
        if (_pInterpreter->extFunctionData[index].pExtFunctionStartToken == nullptr) { return false; }
        index++;
    }
    return true;
}


// ----------------------------------------------------------------------------------------------------------------------
// *   parse ONE instruction in a character string, ended by an optional ';' character and a '\0' mandatary character   *
// ----------------------------------------------------------------------------------------------------------------------

MyParser::parseTokenResult_type MyParser::parseInstruction(char*& pInputStart) {
    _lastTokenType_hold = Interpreter::tok_no_token;
    _lastTokenType = Interpreter::tok_no_token;                                                      // no token yet
    _lastTokenIsTerminal = false;
    _lastTokenIsPrefixOp = false;
    _lastTokenIsPostfixOp = false;
    _lastTokenIsPrefixIncrDecr = false;

    // expression syntax check
    _thisLvl_lastIsVariable = false;                               // init
    _thisLvl_assignmentStillPossible = true;                             // assume for now
    _thisLvl_lastOpIsIncrDecr = false;                                  // assume for now

    // command argument constraints check
    _lvl0_withinExpression = false;
    _lvl0_isPureVariable = false;
    _lvl0_isVarWithAssignment = false;

    _parenthesisLevel = 0;

    _isProgramCmd = false;

    _isDecCBprocCmd = false;
    _isCallbackCmd = false;

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

#if printParsedTokens
    Serial.println(); ////
#endif

    do {                                                                                // parse ONE token in an instruction
        bool isLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;
        bool isRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;
        bool isComma = _lastTokenIsTerminal ? (_lastTermCode == termcod_comma) : false;
        bool isSemicolon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
        bool isOperator = _lastTokenIsTerminal ? (_lastTermCode <= termcod_opRangeEnd) : false;

        bool isLongConst = false, isFloatConst = false, isStringConst = false;
        if (t == Interpreter::tok_isConstant) {
            char valueType = (((Interpreter::TokenIsConstant*)(_pInterpreter->_programStorage + _lastTokenStep))->tokenType >> 4) & Interpreter::value_typeMask;
            isStringConst = (valueType == Interpreter::value_isStringPointer);
        }

        // determine token group of last token parsed (bits b4 to b0): this defines which tokens are allowed as next token
        _lastTokenGroup_sequenceCheck_bit = isOperator ? lastTokenGroup_0 :
            isComma ? lastTokenGroup_1 :
            ((t == Interpreter::tok_no_token) || isSemicolon || (t == Interpreter::tok_isReservedWord) || (t == Interpreter::tok_isGenericName)) ? lastTokenGroup_2 :
            ((t == Interpreter::tok_isConstant) || isRightPar) ? lastTokenGroup_3 :
            ((t == Interpreter::tok_isInternFunction) || (t == Interpreter::tok_isExternFunction)) ? lastTokenGroup_4 :
            isLeftPar ? lastTokenGroup_5 : lastTokenGroup_6;     // token group 5: scalar or array variable name


        // a space may be required between last token and next token (not yet known), if one of them is a keyword
        // and the other token is either a keyword, an alphanumeric constant or a parenthesis
        // space check result is OK if a check is not required or if a space is present anyway
        _leadingSpaceCheck = ((t == Interpreter::tok_isReservedWord) || isStringConst || isRightPar) && (pNext[0] != ' ');

        // move to the first character of next token (within one instruction)
        while (pNext[0] == ' ') { pNext++; }                                         // skip leading spaces
        if (pNext[0] == '\0') { break; }                                              // safety: instruction was not ended by a semicolon (should never happen) 

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
            if ((_pInterpreter->_programCounter + sizeof(Interpreter::TokenIsConstant) + 1) > (_pInterpreter->_programStart + _pInterpreter->_programSize)) { result = result_progMemoryFull; break; };
            if (!parseAsResWord(pNext, result)) { break; } if (result == result_tokenFound) { continue; }             // check before checking for identifier  
            if (!parseTerminalToken(pNext, result)) { break; }  if (result == result_tokenFound) { continue; }       // check before checking for number
            if (!parseAsNumber(pNext, result)) { break; }  if (result == result_tokenFound) { continue; }
            if (!parseAsStringConstant(pNext, result)) { break; }  if (result == result_tokenFound) { continue; }
            if (!parseAsInternFunction(pNext, result)) { break; }  if (result == result_tokenFound) { continue; }     // check before checking for identifier (ext. function / variable) 
            if (!parseAsExternFunction(pNext, result)) { break; }  if (result == result_tokenFound) { continue; }     // check before checking for variable
            if (!parseAsVariable(pNext, result)) { break; }  if (result == result_tokenFound) { continue; }
            if (!parseAsIdentifierName(pNext, result)) { break; }  if (result == result_tokenFound) { continue; }     // at the end
            result = result_token_not_recognised;
        } while (false);

        // one token parsed (or error)
        if (result != result_tokenFound) { break; }                                   // exit loop if token error (syntax, ...). Checked before checking command syntax
        if (!checkCommandSyntax(result)) { pNext = pNext_hold; break; }             // exit loop if command syntax error (pNext altered: set correctly again)

    } while (true);

    // one instruction parsed (or error: no token found OR command syntax error OR semicolon encountered)


    // while parsing, periodically do a housekeeping callback (if function defined)
    // ----------------------------------------------------------------------------

    if (_pInterpreter->_housekeepingCallback != nullptr) {
        bool quitNow{ false };
        _pInterpreter->_currenttime = millis();
        _pInterpreter->_previousTime = _pInterpreter->_currenttime;                                                                           // keep up to date (needed during parsing and evaluation)
        // also handle millis() overflow after about 47 days
        if ((_pInterpreter->_lastCallBackTime + Interpreter::callbackPeriod < _pInterpreter->_currenttime) || (_pInterpreter->_currenttime < _pInterpreter->_previousTime)) {     // while parsing, limit calls to housekeeping callback routine 
            _pInterpreter->_lastCallBackTime = _pInterpreter->_currenttime;
            _pInterpreter->_housekeepingCallback(quitNow);
            if (quitNow) { pNext = pNext_hold; result = result_parse_kill; }
        }
    }

    pInputStart = pNext;                                                                // set to next character (if error: indicates error position)
    return result;
}


// --------------------------------------------------------------------------------------------
// *   if instruction is a command (starting with a keyword): apply additional checks   *
// *   this check is applied AFTER parsing each token and checking its syntax                 *
// --------------------------------------------------------------------------------------------

bool MyParser::checkCommandSyntax(parseTokenResult_type& result) {                    // command syntax checks
    static Interpreter::tokenType_type cmdSecondLastTokenType = Interpreter::tok_no_token;                           // type of last token parsed
    static bool cmdSecondLastIsLvl0CommaSep = false;
    static bool expressionStartsWithVarRef = false;
    static bool expressionStartsWithPrefixOp = false;
    static bool withinExpression = false;
    static uint8_t allowedParType = cmdPar_none;                                         // init

    // is the start of a new command ? Check token preceding the last parsed token 
    bool isInstructionStart = (_lastTokenType_hold == Interpreter::tok_no_token) || (_lastTokenIsTerminal_hold ? (_lastTermCode_hold == termcod_semicolon) : false);

    if (isInstructionStart) {
        _isCommand = (_lastTokenType == Interpreter::tok_isReservedWord);                            // keyword at start of instruction ? is a command
        _varDefAssignmentFound = false;

        // start of a command ?
        // --------------------

        if (_isCommand) {
            _pCmdAllowedParTypes = _resWords[_tokenIndex].pCmdAllowedParTypes;         // remember allowed parameter types
            _cmdParSpecColumn = 0;                                                          // reset actual command parameter counter
            _cmdArgNo = 0;
            _cmdExprArgTokenNo = 0;

            withinExpression = false;
            expressionStartsWithVarRef = false;                                               // scalar or array
            expressionStartsWithPrefixOp = false;

            cmdSecondLastTokenType = Interpreter::tok_isReservedWord;                              // ini: token sequence within current command (command parameters)
            cmdSecondLastIsLvl0CommaSep = false;

            // determine command and where allowed
            CmdBlockDef cmdBlockDef = _resWords[_tokenIndex].cmdBlockDef;

            _isExtFunctionCmd = _resWords[_tokenIndex].resWordCode == cmdcod_function;
            _isProgramCmd = _resWords[_tokenIndex].resWordCode == cmdcod_program;
            _isDecCBprocCmd = _resWords[_tokenIndex].resWordCode == cmdcod_decCBproc;
            _isCallbackCmd = _resWords[_tokenIndex].resWordCode == cmdcod_callback;
            _isGlobalOrUserVarCmd = _resWords[_tokenIndex].resWordCode == cmdcod_var;
            _isLocalVarCmd = _resWords[_tokenIndex].resWordCode == cmdcod_local;
            _isStaticVarCmd = _resWords[_tokenIndex].resWordCode == cmdcod_static;
            _isDeleteVarCmd = _resWords[_tokenIndex].resWordCode == cmdcod_delete;

            _isAnyVarCmd = _isGlobalOrUserVarCmd || _isLocalVarCmd || _isStaticVarCmd;      //  VAR, LOCAL, STATIC

            // is command allowed here ? Check restrictions
            char cmdRestriction = _resWords[_tokenIndex].restrictions & cmd_usageRestrictionMask;
            if (cmdRestriction == cmd_onlyProgramTop) {
                if (_lastTokenStep != 0) { result = result_onlyProgramStart; return false; }
            }
            else {
                if (_lastTokenStep == 0) { result = result_programCmdMissing; return false; }
            }
            if (_pInterpreter->_programMode && (cmdRestriction == cmd_onlyImmediate)) { result = result_onlyImmediateMode; return false; }
            if (!_pInterpreter->_programMode && (cmdRestriction == cmd_onlyInProgram)) { result = result_onlyInsideProgram; return false; }
            if (!_extFunctionBlockOpen && (cmdRestriction == cmd_onlyInFunctionBlock)) { result = result_onlyInsideFunction; return false; }
            if (_extFunctionBlockOpen && (cmdRestriction == cmd_onlyOutsideFunctionBlock)) { result = result_onlyOutsideFunction; return false; }
            if (((!_pInterpreter->_programMode) || _extFunctionBlockOpen) && (cmdRestriction == cmd_onlyInProgramOutsideFunctionBlock)) { result = result_onlyInProgOutsideFunction; return false; };
            if ((_pInterpreter->_programMode && !_extFunctionBlockOpen) && (cmdRestriction == cmd_onlyImmOrInsideFuncBlock)) { result = result_onlyImmediateOrInFunction; return false; };

            if (_extFunctionBlockOpen && _isExtFunctionCmd) { result = result_functionDefsCannotBeNested; return false; } // separate message to indicate 'no nesting'

            // not a block command: nothing more to do here 
            if (cmdBlockDef.blockType == block_none) { return true; }

            if (cmdBlockDef.blockPosOrAction == block_startPos) {                        // is a block start command ?                          
                _blockLevel++;                                                          // increment stack counter and create corresponding list element
                _pParsingStack = (LE_parsingStack*)parsingStack.appendListElement(sizeof(LE_parsingStack));
                _pParsingStack->openBlock.cmdBlockDef = cmdBlockDef;                // store in stack: block type, block position (start), n/a, n/a
                memcpy(_pParsingStack->openBlock.tokenStep, &_lastTokenStep, sizeof(char[2]));                      // store in stack: pointer to block start command token of open block
                _blockStartCmdTokenStep = _lastTokenStep;                                     // remember pointer to block start command token of open block
                _blockCmdTokenStep = _lastTokenStep;                                          // remember pointer to last block command token of open block
                _extFunctionBlockOpen = _extFunctionBlockOpen || _isExtFunctionCmd;    // open until block closing END command     
                return true;                                                         // nothing more to do
            }

            if (_blockLevel == 0) { result = result_noOpenBlock; return false; }      // not a block start and no open block: error

            if ((cmdBlockDef.blockType == block_alterFlow) && (_blockLevel > 0)) {
                // check for a compatible open block (e.g. a BREAK command can only occur if at least one open loop block exists)
                // parenthesis level is zero, because this is a block start command -> all stack levels are block levels
                LE_parsingStack* pStackLvl = _pParsingStack;                                   // start with current open block level
                while (pStackLvl != nullptr) {
                    if ((pStackLvl->openBlock.cmdBlockDef.blockType == block_extFunction) &&   // an open external function block has been found (call or definition)
                        (cmdBlockDef.blockPosOrAction == block_inOpenFunctionBlock)) {                // and current flow altering command is allowed in open function block
                        // store pointer from 'alter flow' token (command) to block start command token of compatible open block (from RETURN to FUNCTION token)
                        memcpy(((Interpreter::TokenIsResWord*)(_pInterpreter->_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.tokenStep, sizeof(char[2]));
                        break;                                                                      // -> applicable open block level found
                    }
                    if (((pStackLvl->openBlock.cmdBlockDef.blockType == block_for) ||
                        (pStackLvl->openBlock.cmdBlockDef.blockType == block_while)) &&         // an open loop block has been found (e.g. FOR ... END block)
                        (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock)) {                    // and current flow altering command is allowed in open loop block
                        // store pointer from 'alter flow' token (command) to block start command token of compatible open block (e.g. from BREAK to FOR token)
                        memcpy(((Interpreter::TokenIsResWord*)(_pInterpreter->_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.tokenStep, sizeof(char[2]));
                        break;                                                                      // -> applicable open block level found
                    }
                    pStackLvl = (LE_parsingStack*)parsingStack.getPrevListElement(pStackLvl);
                }
                if (pStackLvl == nullptr) { result = (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock) ? result_noOpenLoop : result_noOpenFunction; }
                return (pStackLvl != nullptr);
            }

            if ((cmdBlockDef.blockType != _pParsingStack->openBlock.cmdBlockDef.blockType) &&    // same block type as open block (or block type is generic block end) ?
                (cmdBlockDef.blockType != block_genericEnd)) {
                result = result_notAllowedInThisOpenBlock; return false;                // wrong block type: error
            }

            bool withinRange = (_pParsingStack->openBlock.cmdBlockDef.blockPosOrAction >= cmdBlockDef.blockMinPredecessor) &&     // sequence of block commands OK ?
                (_pParsingStack->openBlock.cmdBlockDef.blockPosOrAction <= cmdBlockDef.blockMaxPredecessor);
            if (!withinRange) { result = result_wrongBlockSequence; return false; }   // sequence of block commands (for current stack level) is not OK: error

            // pointer from previous open block token to this open block token (e.g. pointer from IF token to ELSEIF or ELSE token)
            memcpy(((Interpreter::TokenIsResWord*)(_pInterpreter->_programStorage + _blockCmdTokenStep))->toTokenStep, &_lastTokenStep, sizeof(char[2]));
            _blockCmdTokenStep = _lastTokenStep;                                              // remember pointer to last block command token of open block


            if (cmdBlockDef.blockPosOrAction == block_endPos) {                          // is this a block END command token ? 
                if (_pParsingStack->openBlock.cmdBlockDef.blockType == block_extFunction) { _extFunctionBlockOpen = false; }       // FUNCTON definition blocks cannot be nested
                memcpy(((Interpreter::TokenIsResWord*)(_pInterpreter->_programStorage + _lastTokenStep))->toTokenStep, &_blockStartCmdTokenStep, sizeof(char[2]));
                parsingStack.deleteListElement(nullptr);                                   // decrement stack counter and delete corresponding list element
                _blockLevel--;                                                          // also set pointer to currently last element in stack (if it exists)

                if (_blockLevel + _parenthesisLevel > 0) { _pParsingStack = (LE_parsingStack*)parsingStack.getLastListElement(); }
                if (_blockLevel > 0) {
                    // retrieve pointer to block start command token and last block command token of open block
                    memcpy(&_blockStartCmdTokenStep, _pParsingStack->openBlock.tokenStep, sizeof(char[2]));         // pointer to block start command token of open block       
                    uint16_t tokenStep = _blockStartCmdTokenStep;                            // init pointer to last block command token of open block
                    uint16_t tokenStepPointedTo;
                    memcpy(&tokenStepPointedTo, ((Interpreter::TokenIsResWord*)(_pInterpreter->_programStorage + tokenStep))->toTokenStep, sizeof(char[2]));
                    while (tokenStepPointedTo != 0xFFFF)
                    {
                        tokenStep = tokenStepPointedTo;
                        memcpy(&tokenStepPointedTo, ((Interpreter::TokenIsResWord*)(_pInterpreter->_programStorage + tokenStep))->toTokenStep, sizeof(char[2]));
                    }

                    _blockCmdTokenStep = tokenStep;                                        // pointer to last block command token of open block                       
                }
            }
            else { _pParsingStack->openBlock.cmdBlockDef = cmdBlockDef; }           // overwrite (block type (same or generic end), position, min & max predecessor)

            return true;
        };
    }


    // parsing a command parameter right now ? Apply additional command syntax rules
    // -----------------------------------------------------------------------------

    if (!_isCommand) { return true; }                                                 // not within a command                                                


    // init and adapt variables
    // ------------------------

    bool isResWord = (_lastTokenType == Interpreter::tok_isReservedWord);
    bool isGenIdent = (_lastTokenType == Interpreter::tok_isGenericName);
    bool isSemiColonSep = _lastTokenIsTerminal ? (_terminals[_tokenIndex].terminalCode == termcod_semicolon) : false;
    bool isLeftParenthesis = _lastTokenIsTerminal ? (_terminals[_tokenIndex].terminalCode == termcod_leftPar) : false;
    bool isLvl0CommaSep = _lastTokenIsTerminal ? ((_terminals[_tokenIndex].terminalCode == termcod_comma) && (_parenthesisLevel == 0)) : false;
    bool isAssignmentOp = _lastTokenIsTerminal ? ((_terminals[_tokenIndex].terminalCode == termcod_assign)
        || (_terminals[_tokenIndex].terminalCode == termcod_plusAssign) || (_terminals[_tokenIndex].terminalCode == termcod_minusAssign)
        || (_terminals[_tokenIndex].terminalCode == termcod_multAssign) || (_terminals[_tokenIndex].terminalCode == termcod_divAssign)) : false;
    bool isNonAssignmentOp = _lastTokenIsTerminal ? (((_terminals[_tokenIndex].terminalCode <= termcod_opRangeEnd)) && !isAssignmentOp) : false;
    bool isOperator = isAssignmentOp || isNonAssignmentOp;
    bool isIncrDecrOp = _lastTokenIsTerminal ? (_terminals[_tokenIndex].terminalCode == termcod_incr) || (_terminals[_tokenIndex].terminalCode == termcod_decr) : false;

    // is this token part of an expression ? 
    withinExpression = !(isResWord || isGenIdent || isLvl0CommaSep || isSemiColonSep);

    // start of expression: if within expression, AND the preceding token was a level 0 comma separator, keyword or generic name
    bool isExpressionFirstToken = withinExpression &&
        ((cmdSecondLastTokenType == Interpreter::tok_isReservedWord) || (cmdSecondLastTokenType == Interpreter::tok_isGenericName) || (cmdSecondLastIsLvl0CommaSep));

    // does this expression start with a variable reference ? (either a variable or a prefix increment / decrement operator, which returns a variable reference)
    if (isExpressionFirstToken) {
        expressionStartsWithVarRef = (_lastTokenType == Interpreter::tok_isVariable);
        expressionStartsWithPrefixOp = isIncrDecrOp;
    }

    // keep track of token index within expression (base 0)
    if (!withinExpression || isExpressionFirstToken) { _cmdExprArgTokenNo = 0; }
    else { _cmdExprArgTokenNo++; }

    // keep track of argument index within command
    if (isResWord || isGenIdent || isExpressionFirstToken) { _cmdArgNo++; }

    // if first token of a command parameter or a semicolon: check allowed argument types with respect to command definition (ecpression, identifier, ...) 
    bool multipleParameter = false, optionalParameter = false;
    if (isResWord || isGenIdent || isExpressionFirstToken || isSemiColonSep) {
        allowedParType = (_cmdParSpecColumn == sizeof(_pCmdAllowedParTypes)) ? cmdPar_none : (uint8_t)(_pCmdAllowedParTypes[_cmdParSpecColumn]);
        multipleParameter = (allowedParType & cmdPar_multipleFlag);
        optionalParameter = (allowedParType & cmdPar_optionalFlag);
        if (!multipleParameter) { _cmdParSpecColumn++; }                                   // increase parameter count, unless multiple parameters of this type are accepted  
        allowedParType = allowedParType & ~cmdPar_flagMask;
    }


    // if end of command, test for missing parameters and exit
    if (isSemiColonSep) {                                                             // semicolon: end of command                                                    
        if ((allowedParType != cmdPar_none) && !multipleParameter && !optionalParameter) {    // missing parameters ?
            result = result_cmdParameterMissing; return false;
        }

        if (_isDecCBprocCmd && (_cmdArgNo == 0)) { _pInterpreter->_userCBprocAliasSet_count = 0; }

        _isProgramCmd = false;
        _isDecCBprocCmd = false;
        _isCallbackCmd = false;
        _isExtFunctionCmd = false;
        _isAnyVarCmd = false;
        _isGlobalOrUserVarCmd = false;
        _isLocalVarCmd = false;
        _isStaticVarCmd = false;
        _isDeleteVarCmd = false;

        return true;                                                                    // nothing more to do for this command
    }


    // if command argument first token: check parameter validity (skip block if not first token) 
    // ------------------------------------------------------------------------------------------

    if (isResWord || isGenIdent || isExpressionFirstToken) {
        if (allowedParType == cmdPar_none) {
            result = result_cmdHasTooManyParameters; return false;
        }

        else if (allowedParType == cmdPar_resWord) {
            if (!isResWord) {
                result = result_resWordExpectedAsCmdPar; return false;
            }
        }

        else if (allowedParType == cmdPar_ident) {
            if (!isGenIdent) {
                result = result_identExpectedAsCmdPar; return false;
            }
        }

        else if (allowedParType == cmdPar_varNoAssignment) {
            if (!expressionStartsWithVarRef) {                                      // variable can be array as well
                result = result_variableExpectedAsCmdPar; return false;
            }
        }

        else if (allowedParType == cmdPar_varOptAssignment) {
            if (!expressionStartsWithVarRef && !expressionStartsWithPrefixOp) {  //// check prefix op                                    // variable can be array as well
                result = result_varRefExpectedAsCmdPar; return false;
            }
        }

        else if (allowedParType == cmdPar_expression) {
            if (isResWord) { result = result_expressionExpectedAsCmdPar; return false; }
        }
    }


    // check command argument constraints
    // ----------------------------------
    if (!isLvl0CommaSep) {                       // skip, because variables for command argument constraint checks have been reset there
        if ((allowedParType == cmdPar_varNoAssignment) && !_lvl0_isPureVariable) {
            result = isAssignmentOp ? (parseTokenResult_type)result_varWithoutAssignmentExpectedAsCmdPar : (parseTokenResult_type)result_variableExpectedAsCmdPar; return false;
        }
        
        if (allowedParType == cmdPar_varOptAssignment){
            Serial.print(">>>> is pure var: "); Serial.print(_lvl0_isPureVariable); Serial.print(", is var with assignment: "); Serial.println(_lvl0_isVarWithAssignment);
        }
        
        if ((allowedParType == cmdPar_varOptAssignment) && !_lvl0_isPureVariable && !_lvl0_isVarWithAssignment) {
            result = (parseTokenResult_type)result_varWithOptionalAssignmentExpectedAsCmdPar; return false;
        }
    }

    _varDefAssignmentFound = false;                     // to enable check for assignment to non-constants
    if (_isAnyVarCmd && isAssignmentOp) { _varDefAssignmentFound = true; }

    // remember past values
    // --------------------

    cmdSecondLastTokenType = _lastTokenType;                                               // within current command
    cmdSecondLastIsLvl0CommaSep = isLvl0CommaSep;
    return true;
}


// -------------------------------------------------------
// *   try to parse next characters as a keyword   *
// -------------------------------------------------------

bool MyParser::parseAsResWord(char*& pNext, parseTokenResult_type& result) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int resWordIndex;

    if (!isalpha(pNext[0])) { return true; }                                       // first character is not a letter ? Then it's not a keyword (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    for (resWordIndex = _resWordCount - 1; resWordIndex >= 0; resWordIndex--) {          // for all defined keywords: check against alphanumeric token (NOT ending by '\0')
        if (strlen(_resWords[resWordIndex]._resWordName) != pNext - pch) { continue; }          // token has correct length ? If not, skip remainder of loop ('continue')                            
        if (strncmp(_resWords[resWordIndex]._resWordName, pch, pNext - pch) != 0) { continue; } // token corresponds to keyword ? If not, skip remainder of loop ('continue') 

        // token is keyword, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (_parenthesisLevel > 0) { pNext = pch; result = result_resWordNotAllowedHere; return false; }
        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_2_0)) { pNext = pch; result = result_resWordNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_resWordNotAllowedHere; return false; }

        if (!_isCommand) {                                                             // already within a command: do not test here
            bool lastIsSemiColon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
            if (!lastIsSemiColon && (_lastTokenType != Interpreter::tok_no_token)) {
                pNext = pch; result = result_resWordNotAllowedHere; return false;       // keyword only at start of a statement (not within an expression)
            }
        }
        if (_leadingSpaceCheck) { pNext = pch; result = result_spaceMissing; return false; }

        _tokenIndex = resWordIndex;                                                     // needed in case it's the start of a command (to determine parameters)

        // token is a keyword, and it's allowed here

        // expression syntax check 
        _thisLvl_lastIsVariable = false;
        _thisLvl_assignmentStillPossible = true;                                                 // reset (expression may follow)                          

        // command argument constraints check
        _lvl0_withinExpression = false;                                                         // reset for next command parameter
        _lvl0_isPureVariable = false;
        _lvl0_isVarWithAssignment = false;

        // if NOT a block command, bytes for token step are not needed 
        bool hasTokenStep = (_resWords[resWordIndex].cmdBlockDef.blockType != block_none);

        Interpreter::TokenIsResWord* pToken = (Interpreter::TokenIsResWord*)_pInterpreter->_programCounter;
        pToken->tokenType = Interpreter::tok_isReservedWord | ((sizeof(Interpreter::TokenIsResWord) - (hasTokenStep ? 0 : 2)) << 4);
        pToken->tokenIndex = resWordIndex;
        if (hasTokenStep) { pToken->toTokenStep[0] = 0xFF; pToken->toTokenStep[1] = 0xFF; }                  // -1: no token ref. Because uint16_t not necessarily aligned with word size: store as two sep. bytes                            

        _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
        _lastTokenType = Interpreter::tok_isReservedWord;
        _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if printParsedTokens
        Serial.print("parsing keyword: address is "); Serial.print(_lastTokenStep); Serial.print(" ["); Serial.print(_resWords[resWordIndex]._resWordName);  Serial.println("]");
#endif

        _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsResWord) - (hasTokenStep ? 0 : 2);
        *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
        result = result_tokenFound;                                                     // flag 'valid token found'
        return true;
    }

    pNext = pch;                                                                        // reset pointer to first character to parse (because no token was found)
    return true;                                                                        // token is not a keyword (but can still be something else)
}


// ------------------------------------------------
// *   try to parse next characters as a number   *
// ------------------------------------------------

bool MyParser::parseAsNumber(char*& pNext, parseTokenResult_type& result) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    // all numbers will be positive, because leading '-' or '+' characters are parsed separately as prefix operators
    // this is important if next infix operator (power) has higher priority then this prefix operator: -2^4 <==> -(2^4) <==> -16, AND NOT (-2)^4 <==> 16 
    // exception: variable declarations with initializers: prefix operators are not parsed separately

    // check if number (if valid) will be stored as long or float

    char* pNumStart = pNext;
    float f{ 0 }; long l{ 0 };
    bool isLong{ false }, negate{ false };
    int i{ 0 };

    if (pNumStart[0] == '-') { negate = true; }
    if ((pNumStart[0] == '+') || (pNumStart[0] == '-')) { pNumStart++; };  // start with a plus or minus sign ? start looking for digits at next position 

    int base = ((pNumStart[0] == '0') && ((pNumStart[1] == 'x') || (pNumStart[1] == 'X'))) ? 16 : ((pNumStart[0] == '0') && ((pNumStart[1] == 'b') || (pNumStart[1] == 'B'))) ? 2 : 10;

    if (base == 10) {      // base 10
        while (isDigit(pNumStart[++i]));
        isLong = ((i > 0) && (pNumStart[i] != '.') && (pNumStart[i] != 'E') && (pNumStart[i] != 'e'));        // no decimal point, no exponent and minimum one digit
    }

    else {       // binary or hexadecimal
        pNumStart += 2;      // skip "0b" or "0x" and start looking for digits at next position
        while ((base == 16) ? isxdigit(pNumStart[++i]) : ((pNumStart[i] == '0') || (pNumStart[i] == '1'))) { ++i; }
        isLong = (i > 0);        // minimum one digit
        if (!isLong) { pNext = pch; result = result_numberInvalidFormat; return false; }  // not a long constant, but not a float either
    }

    if (isLong) {                                                       // token can be parsed as long ?
        l = strtoul(pNumStart, &pNext, base);                       // string to UNSIGNED long before assigning to (signed) long -> 0xFFFFFFFF will be stored as -1, as it should (all bits set)
        if (negate) { l = -l; }
    }
    else { f = strtof(pNumStart, &pNext); }                                                    // token can be parsed as float ?

    if (pNumStart == pNext) { return true; }                                                // token is not a number if pointer pNext was not moved


    // is valid number: continue processing

    if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command
    // token is a number constant, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_numConstNotAllowedHere; return false; }
    if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_numConstNotAllowedHere; return false; }

    // overflow ? (underflow is not detected with strtof() ) 
    if (!isLong) { if (!isfinite(f)) { pNext = pch; result = result_overflow; return false; } }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
    if (!tokenAllowed) { pNext = pch; result = result_numConstNotAllowedHere; return false; ; }

    // Note: in a (variable or parameter) declaration statement, operators other than assignment operators are not allowed, which is detected in terminal token parsing
    bool isParamDecl = (_isExtFunctionCmd);                                          // parameter declarations :  constant can ONLY FOLLOW an assignment operator
    bool isPureAssignmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;
    if (isParamDecl && !isPureAssignmentOp) { pNext = pch; result = result_numConstNotAllowedHere; return false; }

    // is a variable required instead of a constant ?
    bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
    if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }

    // array declaration: dimensions must be number constants (global, static, local arrays)
    bool isArrayDimSpec = (_isAnyVarCmd) && (_parenthesisLevel > 0);
    if (isArrayDimSpec) {
        if (isLong && (l < 1)) { pNext = pch; result = result_arrayDimNotValid; return false; }
        else if ((!isLong) && ((f != int(f)) || (f < 1))) { pNext = pch; result = result_arrayDimNotValid; return false; }
    }

    // token is a number, and it's allowed here

    // expression syntax check 
    _thisLvl_lastIsVariable = false;                                                               // any expression

    // command argument constraints check
    _lvl0_withinExpression = true;

    Interpreter::TokenIsConstant* pToken = (Interpreter::TokenIsConstant*)_pInterpreter->_programCounter;
    pToken->tokenType = Interpreter::tok_isConstant | ((isLong ? Interpreter::value_isLong : Interpreter::value_isFloat) << 4);
    if (isLong) { memcpy(pToken->cstValue.longConst, &l, sizeof(l)); }
    else { memcpy(pToken->cstValue.floatConst, &f, sizeof(f)); }                                           // float not necessarily aligned with word size: copy memory instead

    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && isPureAssignmentOp);

    _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
    _lastTokenType = Interpreter::tok_isConstant;
    _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if printParsedTokens
    Serial.print("parsing number : address is "); Serial.print(_lastTokenStep); Serial.print(" ["); if (isLong) { Serial.print(l); }
    else { Serial.print(f); }  Serial.println("]");
#endif

    if (doNonLocalVarInit) { initVariable(_lastVariableTokenStep, _lastTokenStep); }     // initialisation of global / static variable ? (operator: is always assignment)

    _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsConstant);
    *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ----------------------------------------------------------------
// *   try to parse next characters as an alphanumeric constant   *
// ----------------------------------------------------------------

bool MyParser::parseAsStringConstant(char*& pNext, parseTokenResult_type& result) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int escChars = 0;

    if ((pNext[0] != '\"')) { return true; }                                         // no opening quote ? Is not an alphanumeric cst (it can still be something else)
    pNext++;                                                                            // skip opening quote

    if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is an alphanumeric constant, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }
    if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
    if (!tokenAllowed) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; ; }

    // Note: in a (variable or parameter) declaration statement, operators other than assignment operators are not allowed, which is detected in terminal token parsing
    bool isParamDecl = (_isExtFunctionCmd);                                             // parameter declarations :  constant can ONLY FOLLOW an assignment operator
    bool isPureAssignmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;
    if (isParamDecl && !isPureAssignmentOp) { pNext = pch; result = result_alphaConstNotAllowedHere; return false; }

    // is a variable required instead of a constant ?
    bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
    if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }
    // array declaration: dimensions must be number constants (global, static, local arrays)
    bool isArrayDimSpec = (_isAnyVarCmd) && (_parenthesisLevel > 0);
    if (isArrayDimSpec) { pNext = pch; result = result_arrayDimNotValid; return false; }

    if (_leadingSpaceCheck) { pNext = pch; result = result_spaceMissing; return false; }

    while (pNext[0] != '\"') {                                                       // do until closing quote, if any
        // if no closing quote found, an invalid escape sequence or a control character detected, reset pointer to first character to parse, indicate error and return
        if (pNext[0] == '\0') { pNext = pch; result = result_alphaClosingQuoteMissing; return false; }
        if (pNext[0] < ' ') { pNext = pch; result = result_alphaNoCtrlCharAllowed; return false; }
        if (pNext[0] == '\\') {
            if ((pNext[1] == '\\') || (pNext[1] == '\"')) { pNext++; escChars++; }  // valid escape sequences: ' \\ ' (add backslash) and ' \" ' (add double quote)
            else { pNext = pch; result = result_alphaConstInvalidEscSeq; return false; }
        }
        pNext++;
    };

    // if alphanumeric constant is too long, reset pointer to first character to parse, indicate error and return
    if (pNext - (pch + 1) - escChars > _maxAlphaCstLen) { pNext = pch; result = result_alphaConstTooLong; return false; }

    char* pStringCst = nullptr;                 // init: is empty string (prevent creating a string object to conserve memory)
    if (pNext - (pch + 1) - escChars > 0) {    // not an empty string: create string object 

        // token is an alphanumeric constant, and it's allowed here
        pStringCst = new char[pNext - (pch + 1) - escChars + 1];                                // create char array on the heap to store alphanumeric constant, including terminating '\0'
        _pInterpreter->parsedStringConstObjectCount++;
#if printCreateDeleteHeapObjects
        Serial.print("+++++ (parsed str ) "); Serial.println((uint32_t)pStringCst - RAMSTART);
#endif
        // store alphanumeric constant in newly created character array
        pStringCst[pNext - (pch + 1) - escChars] = '\0';                                 // store string terminating '\0' (pch + 1 points to character after opening quote, pNext points to closing quote)
        char* pSource = pch + 1, * pDestin = pStringCst;                                  // pSource points to character after opening quote
        while (pSource + escChars < pNext) {                                              // store alphanumeric constant in newly created character array (terminating '\0' already added)
            if (pSource[0] == '\\') { pSource++; escChars--; }                           // if escape sequences found: skip first escape sequence character (backslash)
            pDestin++[0] = pSource++[0];
        }
    }
    pNext++;                                                                            // skip closing quote

    // expression syntax check 
    _thisLvl_lastIsVariable = false;

    // command argument constraints check
    _lvl0_withinExpression = true;


    Interpreter::TokenIsConstant* pToken = (Interpreter::TokenIsConstant*)_pInterpreter->_programCounter;
    pToken->tokenType = Interpreter::tok_isConstant | (Interpreter::value_isStringPointer << 4);
    memcpy(pToken->cstValue.pStringConst, &pStringCst, sizeof(pStringCst));            // pointer not necessarily aligned with word size: copy pointer instead

    bool isLocalVarInitCheck = (_isLocalVarCmd && isPureAssignmentOp);
    bool isArrayVar = ((Interpreter::TokenIsVariable*)(_pInterpreter->_programStorage + _lastVariableTokenStep))->identInfo & _pInterpreter->var_isArray;
    if (isLocalVarInitCheck && isArrayVar && (pStringCst != nullptr)) {
        pNext = pch; result = result_arrayInit_emptyStringExpected; return false;        // only check (init when function is called)
    }

    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && isPureAssignmentOp);          // (operator: is always assignment)

    _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
    _lastTokenType = Interpreter::tok_isConstant;
    _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if printParsedTokens
    Serial.print("parsing alphan : address is "); Serial.print(_lastTokenStep); Serial.print(" ['"); Serial.print(pStringCst);  Serial.println("']");
#endif

    if (doNonLocalVarInit) {                                     // initialisation of global / static variable ? 
        if (!initVariable(_lastVariableTokenStep, _lastTokenStep)) { pNext = pch; result = result_arrayInit_emptyStringExpected; return false; };
    }

    _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsConstant);
    *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ---------------------------------------------------------------------------------
// *   try to parse next characters as a terminal token (one- or two-characters)   *
// ---------------------------------------------------------------------------------

// Array parsing: check that max dimension count and maximum array size is not exceeded
// ------------------------------------------------------------------------------------

bool MyParser::checkArrayDimCountAndSize(parseTokenResult_type& result, int* arrayDef_dims, int& dimCnt) {

    bool lastIsLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;
    if (lastIsLeftPar) { result = result_arrayDefNoDims; return false; }

    dimCnt++;

    if (dimCnt > _pInterpreter->MAX_ARRAY_DIMS) { result = result_arrayDefMaxDimsExceeded; return false; }

    char valueType = (*(_pInterpreter->_programStorage + _lastTokenStep) >> 4) & Interpreter::value_typeMask;

    long l{};        // last token is a number constant: dimension spec
    float f{ 0 };
    if (valueType == (Interpreter::value_isLong)) {     // float
        memcpy(&l, ((Interpreter::TokenIsConstant*)(_pInterpreter->_programStorage + _lastTokenStep))->cstValue.longConst, sizeof(l));
    }
    else {
        memcpy(&f, ((Interpreter::TokenIsConstant*)(_pInterpreter->_programStorage + _lastTokenStep))->cstValue.floatConst, sizeof(f));
        l = int(f);
    }

    if (l < 1) { result = result_arrayDefNegativeDim; return false; }
    arrayDef_dims[dimCnt - 1] = l;
    int arrayElements = 1;
    for (int cnt = 0; cnt < dimCnt; cnt++) { arrayElements *= arrayDef_dims[cnt]; }
    if (arrayElements > _pInterpreter->MAX_ARRAY_ELEM) { result = result_arrayDefMaxElementsExceeded; return false; }
    return true;
}


// External function definition statement parsing: check order of mandatory and optional arguments, check if max. n° not exceeded
// -------------------------------------------------------------------------------------------------------------------------------

bool MyParser::checkExtFunctionArguments(parseTokenResult_type& result, int& minArgCnt, int& maxArgCnt) {
    bool lastIsRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;

    bool argWasMandatory = (_lastTokenType == Interpreter::tok_isVariable) || lastIsRightPar;         // variable without assignment to a constant or param array def. parenthesis
    bool alreadyOptArgs = (minArgCnt != maxArgCnt);
    if (argWasMandatory && alreadyOptArgs) { result = result_mandatoryArgFoundAfterOptionalArgs; return false; }
    if (argWasMandatory) { minArgCnt++; }
    maxArgCnt++;
    // check that max argument count is not exceeded (number must fit in 4 bits)
    if (maxArgCnt > c_extFunctionMaxArgs) { result = result_functionDefMaxArgsExceeded; return false; }
    return true;
}


// Internal function: check that order of arrays and scalar variables is consistent with function definition 
// ---------------------------------------------------------------------------------------------------------

bool MyParser::checkInternFuncArgArrayPattern(parseTokenResult_type& result) {
    int funcIndex = _pParsingStack->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
    char paramIsArrayPattern = _functions[funcIndex].arrayPattern;
    int argNumber = _pParsingStack->openPar.actualArgsOrDims;

    if (argNumber > 0) {
        bool isArray = false;
        if (_lastTokenType == Interpreter::tok_isVariable) {                                      // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((Interpreter::TokenIsVariable*)(_pInterpreter->_programStorage + _lastTokenStep))->identInfo) & _pInterpreter->var_isArray;
        }

        if (((paramIsArrayPattern >> (argNumber - 1)) & 0b1) != isArray) { result = isArray ? result_scalarArgExpected : result_arrayArgExpected; return false; }
    }
}


// External function: check that order of arrays and scalar variables is consistent with previous calls and function definition 
// ----------------------------------------------------------------------------------------------------------------------------

bool MyParser::checkExternFuncArgArrayPattern(parseTokenResult_type& result, bool isFunctionClosingParenthesis) {

    int funcIndex = _pParsingStack->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
    int argNumber = _pParsingStack->openPar.actualArgsOrDims;
    uint16_t paramIsArrayPattern{ 0 };
    memcpy(&paramIsArrayPattern, _pInterpreter->extFunctionData[funcIndex].paramIsArrayPattern, sizeof(char[2]));
    if (argNumber > 0) {

        bool isArray = false;
        bool lastIsRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;

        if (_isExtFunctionCmd) { isArray = lastIsRightPar; }  // function definition: if variable name followed by empty parameter list ' () ': array parameter
        else if (_lastTokenType == Interpreter::tok_isVariable) {                                      // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((Interpreter::TokenIsVariable*)(_pInterpreter->_programStorage + _lastTokenStep))->identInfo) & _pInterpreter->var_isArray;
        }

        uint16_t paramArrayMask = 1 << (argNumber - 1);
        if (paramIsArrayPattern & 0x8000) {                   // function not used yet (before it was defined now: no need to check, just set array bit)
            paramIsArrayPattern = paramIsArrayPattern | (isArray ? paramArrayMask : 0);
        }
        else {  // error message can not be more specific (scalar expected, array expected) because maybe function has not been defined yet
            if ((paramIsArrayPattern & paramArrayMask) != (isArray ? paramArrayMask : 0)) { result = result_fcnScalarAndArrayArgOrderNotConsistent; return false; }
        }
    }

    if (isFunctionClosingParenthesis) { paramIsArrayPattern = paramIsArrayPattern & ~0x8000; }    // function name used now: order of scalar and array parameters is now fixed
    memcpy(_pInterpreter->extFunctionData[funcIndex].paramIsArrayPattern, &paramIsArrayPattern, sizeof(char[2]));
    return true;
}


// --------------------------
// * Parse a terminal token * 
// --------------------------

bool MyParser::parseTerminalToken(char*& pNext, parseTokenResult_type& result) {

    // external function definition statement parsing: count number of mandatory and optional arguments in function definition for storage
    static int extFunctionDef_minArgCounter{ 0 };
    static int extFunctionDef_maxArgCounter{ 0 };

    // array definition statement parsing: record dimensions (if 1 dimension only: set dim 2 to zero) 
    static int array_dimCounter{ 0 };
    static int arrayDef_dims[_pInterpreter->MAX_ARRAY_DIMS]{ 0 };

    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int termIndex;

    for (termIndex = _terminalCount - 1; termIndex >= 0; termIndex--) {                  // for all defined terminal names: check against alphanumeric token (NOT ending by '\0')
        int len = strlen(_terminals[termIndex].terminalName);    // token has correct length ? If not, skip remainder of loop ('continue')                            
        // do not look for trailing space, to use strncmp() wih number of non-space characters found, because a space is not required after an operator
        if (strncmp(_terminals[termIndex].terminalName, pch, len) == 0) { break; }      // token corresponds to terminal name ? Then exit loop    
    }
    if (termIndex < 0) { return true; }                                                // token is not a one-character token (and it's not a two-char token, because these start with same character)
    pNext += strlen(_terminals[termIndex].terminalName);                                                                            // move to next character

    int nextTermIndex;  // peek: is next token a terminal ? nextTermIndex will be -1 if not
    char* peek = pNext;     // first character of next token (or '\0')
    while (peek[0] == ' ') { peek++; }
    for (nextTermIndex = _terminalCount - 1; nextTermIndex >= 0; nextTermIndex--) {                  // for all defined function names: check against alphanumeric token (NOT ending by '\0')
        int len = strlen(_terminals[nextTermIndex].terminalName);    // token has correct length ? If not, skip remainder of loop ('continue')                            
        // do not look for trailing space, to use strncmp() wih number of non-space characters found, because a space is not required after an operator
        if (strncmp(_terminals[nextTermIndex].terminalName, peek, len) == 0) { break; }      // token corresponds to terminal name ? Then exit loop   
    }


    Interpreter::tokenType_type tokenType;
    uint8_t flags{ B0 };

    switch (_terminals[termIndex].terminalCode) {

    case termcod_leftPar: {
        // -------------------------------------
        // Case 1: is token a left parenthesis ?
        // -------------------------------------

        if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is left parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_5_4_2_1_0)) { pNext = pch;  result = result_parenthesisNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
        if (!tokenAllowed) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }

        if (_isAnyVarCmd && (_parenthesisLevel > 0)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }     // no parenthesis nesting in array declarations
        // parenthesis nesting in function definitions, only to declare an array parameter AND only if followed by a closing parenthesis 
        if ((_isExtFunctionCmd) && (_parenthesisLevel > 0) && (_lastTokenType != Interpreter::tok_isVariable)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }
        if (_isProgramCmd || _isDeleteVarCmd || _isDecCBprocCmd) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }
        if (_isCallbackCmd && (_cmdArgNo == 0)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

        bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
        if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }

        if (_leadingSpaceCheck) { pNext = pch; result = result_spaceMissing; return false; }

        // token is a left parenthesis, and it's allowed here

        // store specific flags in stack, because if nesting functions or parentheses, values will be overwritten
        flags = (_lastTokenType == Interpreter::tok_isExternFunction) ? _pInterpreter->extFunctionBit :
            (_lastTokenType == Interpreter::tok_isInternFunction) ? _pInterpreter->intFunctionBit :
            (_lastTokenType == Interpreter::tok_isVariable) ? _pInterpreter->arrayBit : _pInterpreter->openParenthesisBit;     // is it following a(n internal or external) function name ?

        // external function (call or definition) opening parenthesis
        if (_lastTokenType == Interpreter::tok_isExternFunction) {
            if (_pInterpreter->extFunctionData[_functionIndex].pExtFunctionStartToken != nullptr) { flags = flags | _pInterpreter->extFunctionPrevDefinedBit; }
        }

        // expression syntax check 
        _thisLvl_lastIsVariable = false;       // currently open block
        if (_thisLvl_assignmentStillPossible) { flags = flags | _pInterpreter->varAssignmentAllowedBit; }          // remember if array element can be assigned to (after closing parenthesis)
        _thisLvl_assignmentStillPossible = true;                                                                   // array subscripts: reset assignment allowed flag (init)
        if (_thisLvl_lastOpIsIncrDecr) { flags = flags | _pInterpreter->varHasPrefixIncrDecrBit; }          // remember if array element has a prefix incr/decr operator (before opening parenthesis) 
        _thisLvl_lastOpIsIncrDecr = false;                                                                   // array subscripts: reset assignment allowed flag 

        // command argument constraints check
        _lvl0_withinExpression = true;

        // if function DEFINITION: initialize variables for counting of allowed mandatory and optional arguments (not an array parameter, would be parenthesis level 1)
        if (_isExtFunctionCmd && (_parenthesisLevel == 0)) {      // not an array parameter (would be parenthesis level 1)
            extFunctionDef_minArgCounter = 0;
            extFunctionDef_maxArgCounter = 0;            // init count; range from 0 to a hardcoded maximum 
        }

        // if LOCAL, STATIC or GLOBAL array DEFINITION or USE (NOT: parameter array): initialize variables for reading dimensions 
        if (flags & _pInterpreter->arrayBit) {                    // always count, also if not first definition (could happen for global variables)
            array_dimCounter = 0;
            for (int i = 0; i < _pInterpreter->MAX_ARRAY_DIMS; i++) { arrayDef_dims[i] = 0; }        // init dimensions (dimension count will result from dimensions being non-zero
        }

        // left parenthesis only ? (not a function or array opening parenthesis): min & max allowed argument count not yet initialised
        if (_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_5) {
            _minFunctionArgs = 1;                                    // initialize min & max allowed argument count to 1
            _maxFunctionArgs = 1;
        }

        // min & max argument count: either allowed range (if function previously defined), current range of actual args counts (if previous calls only), or not initialized
        _parenthesisLevel++;                                                            // increment stack counter and create corresponding list element
        _pParsingStack = (LE_parsingStack*)parsingStack.appendListElement(sizeof(LE_parsingStack));
        _pParsingStack->openPar.minArgs = _minFunctionArgs;
        _pParsingStack->openPar.maxArgs = _maxFunctionArgs;
        _pParsingStack->openPar.actualArgsOrDims = 0;
        _pParsingStack->openPar.arrayDimCount = _pInterpreter->_arrayDimCount;         // dimensions of previously defined array. If zero, then this array did not yet exist, or it's a scalar variable
        _pParsingStack->openPar.flags = flags;
        _pParsingStack->openPar.identifierIndex = (_lastTokenType == Interpreter::tok_isInternFunction) ? _functionIndex :
            (_lastTokenType == Interpreter::tok_isExternFunction) ? _functionIndex :
            (_lastTokenType == Interpreter::tok_isVariable) ? _variableNameIndex : 0;
        _pParsingStack->openPar.variableScope = _variableScope;

        _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;
        break; }


    case termcod_rightPar: {
        // --------------------------------------
        // Case 2: is token a right parenthesis ?
        // --------------------------------------

        if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is right parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_5_3_0)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
        if (!tokenAllowed) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }
        if (_parenthesisLevel == 0) { pNext = pch; result = result_missingLeftParenthesis; return false; }

        flags = _pParsingStack->openPar.flags;

        // expression syntax check 
        _thisLvl_lastIsVariable = (flags & _pInterpreter->arrayBit);
        _thisLvl_assignmentStillPossible = (flags & _pInterpreter->varAssignmentAllowedBit);                                                            // array subscripts: reset assignment allowed flag 
        _thisLvl_lastOpIsIncrDecr = (flags & _pInterpreter->varHasPrefixIncrDecrBit);


        // 2.1 External function definition (not a call), OR array parameter definition, closing parenthesis ?
        // -------------------------------------------------------------------

        if (_isExtFunctionCmd) {
            if (_parenthesisLevel == 1) {          // function definition closing parenthesis
                // stack level will not change until closing parenthesis (because within definition, no nesting of parenthesis is possible)
                // stack min & max values: current range of args counts that occured in previous calls (not initialized if no earlier calls occured)

                // if empty function parameter list, then do not increment parameter count (function taking no parameters)

                bool emptyParamList = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;            // ok because no nesting allowed
                _pParsingStack->openPar.actualArgsOrDims += (emptyParamList ? 0 : 1);

                // check order of mandatory and optional arguments, check if max. n� not exceeded
                if (!emptyParamList) { if (!checkExtFunctionArguments(result, extFunctionDef_minArgCounter, extFunctionDef_maxArgCounter)) { pNext = pch; return false; }; }

                int funcIndex = _pParsingStack->openPar.identifierIndex;            // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
                // if previous calls, check if range of actual argument counts that occured in previous calls corresponds to mandatory and optional arguments defined now
                bool previousCalls = (_pInterpreter->extFunctionNames[funcIndex][Interpreter::_maxIdentifierNameLen + 1]) != c_extFunctionFirstOccurFlag;
                if (previousCalls) {                                                      // stack contains current range of actual args occured in previous calls
                    if (((int)_pParsingStack->openPar.minArgs < extFunctionDef_minArgCounter) ||
                        (int)_pParsingStack->openPar.maxArgs > extFunctionDef_maxArgCounter) {
                        pNext = pch; result = result_prevCallsWrongArgCount; return false;  // argument count in previous calls to this function does not correspond 
                    }
                }

                // store min required & max allowed n� of arguments in identifier storage
                // this replaces the range of actual argument counts that occured in previous calls (if any)
                _pInterpreter->extFunctionNames[funcIndex][Interpreter::_maxIdentifierNameLen + 1] = (extFunctionDef_minArgCounter << 4) | (extFunctionDef_maxArgCounter);

                // check that order of arrays and scalar variables is consistent with previous callsand function definition
                if (!checkExternFuncArgArrayPattern(result, true)) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
            }
        }


        // 2.2 Array definition dimension spec closing parenthesis ?
        // ---------------------------------------------------------

        else if (_isAnyVarCmd) {                        // note: parenthesis level is 1 (because no inner parenthesis allowed)
            if (!checkArrayDimCountAndSize(result, arrayDef_dims, array_dimCounter)) { pNext = pch; return false; }

            int varNameIndex = _pParsingStack->openPar.identifierIndex;
            uint8_t varQualifier = _pParsingStack->openPar.variableScope;

            bool isUserVar = (varQualifier == _pInterpreter->var_isUser);
            bool isGlobalVar = (varQualifier == _pInterpreter->var_isGlobal);
            bool isStaticVar = (varQualifier == _pInterpreter->var_isStaticInFunc);
            bool isLocalVar = (varQualifier == _pInterpreter->var_isLocalInFunc);            // but not function parameter definitions

            float* pArray;
            int arrayElements = 1;              // init
            int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : _pInterpreter->programVarValueIndex[varNameIndex];

            // user, global and static arrays: create array on the heap. Array dimensions will be stored in array element 0
            if (isUserVar || isGlobalVar || isStaticVar) {
                for (int dimCnt = 0; dimCnt < array_dimCounter; dimCnt++) { arrayElements *= arrayDef_dims[dimCnt]; }
                pArray = new float[arrayElements + 1];
                isUserVar ? _pInterpreter->userArrayObjectCount++ : _pInterpreter->globalStaticArrayObjectCount++;
#if printCreateDeleteHeapObjects
                Serial.print(isUserVar ? "+++++ (usr ar stor) " : "+++++ (array stor ) "); Serial.println((uint32_t)pArray - RAMSTART);
#endif
                // only now, the array flag can be set, because only now the object exists
                if (isUserVar) {
                    _pInterpreter->userVarValues[valueIndex].pArray = pArray;
                    _pInterpreter->userVarType[varNameIndex] |= _pInterpreter->var_isArray;             // set array bit
                    // USER variables can only be created now to prevent inconsistency if an issue with array dimensions: sufficient to perform increment of _userVarCount here
                    _pInterpreter->_userVarCount++;                                                     // user array variable is now considered 'created'
                }
                else if (isGlobalVar) {
                    _pInterpreter->globalVarValues[valueIndex].pArray = pArray;
                    _pInterpreter->globalVarType[varNameIndex] |= _pInterpreter->var_isArray;             // set array bit
                }
                else if (isStaticVar) {
                    _pInterpreter->staticVarValues[valueIndex].pArray = pArray;
                    _pInterpreter->staticVarType[_pInterpreter->_staticVarCount - 1] |= _pInterpreter->var_isArray;             // set array bit
                }

                // global and static variables are initialized at parsing time. If no explicit initializer, initialize array elements to zero now
                bool arrayHasInitializer = false;
                arrayHasInitializer = (nextTermIndex < 0) ? false : _terminals[nextTermIndex].terminalCode == termcod_assign;
                if (!arrayHasInitializer) {                    // no explicit initializer: initialize now (as real) 
                    for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) { ((float*)pArray)[arrayElem] = 0.; }
                }
            }

            // local arrays (note: NOT for function parameter arrays): set pointer to dimension storage 
            // the array flag has been set when local variable was created (including function parameters, which are also local variables)
            // dimensions are not stored in array value array (because created at runtime) but are temporarily stored here during function parsing  
            else if (isLocalVar) {
                pArray = (float*)_pInterpreter->localVarDims[_pInterpreter->_localVarCountInFunction - 1];
            }

            // global, static and local arrays: store array dimensions (local arrays: temporary storage during parsing only)
            // store dimensions in element 0: char 0 to 2 is dimensions; char 3 = dimension count 
            for (int i = 0; i < _pInterpreter->MAX_ARRAY_DIMS; i++) {
                ((char*)pArray)[i] = arrayDef_dims[i];
            }
            ((char*)pArray)[3] = array_dimCounter;        // (note: for param arrays, set to max dimension count during parsing)
        }


        // 2.3 Internal or external function call, or parenthesis pair, closing parenthesis ?
        // ----------------------------------------------------------------------------------

        else if (flags & (_pInterpreter->intFunctionBit | _pInterpreter->extFunctionBit | _pInterpreter->openParenthesisBit)) {
            // if empty function call argument list, then do not increment argument count (function call without arguments)
            bool emptyArgList = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;            // ok because no nesting allowed
            _pParsingStack->openPar.actualArgsOrDims += (emptyArgList ? 0 : 1);
            int actualArgs = (int)_pParsingStack->openPar.actualArgsOrDims;

            // call to not yet defined external function ? (there might be previous calls)
            bool callToNotYetDefinedFunc = ((flags & (_pInterpreter->extFunctionBit | _pInterpreter->extFunctionPrevDefinedBit)) == _pInterpreter->extFunctionBit);
            if (callToNotYetDefinedFunc) {
                // check that max argument count is not exceeded (number must fit in 4 bits)
                if (actualArgs > c_extFunctionMaxArgs) { pNext = pch; result = result_functionDefMaxArgsExceeded; return false; }

                // if at least one previous call (maybe a nested call) is completely parsed, retrieve current range of actual args that occured in these previous calls
                // and update this range with the argument count of the current external function call that is at its closing parenthesis
                int funcIndex = _pParsingStack->openPar.identifierIndex;            // of current function call: stored in stack for current PARENTHESIS level
                bool prevExtFuncCompletelyParsed = (_pInterpreter->extFunctionNames[funcIndex][Interpreter::_maxIdentifierNameLen + 1]) != c_extFunctionFirstOccurFlag;
                if (prevExtFuncCompletelyParsed) {
                    _pParsingStack->openPar.minArgs = ((_pInterpreter->extFunctionNames[funcIndex][Interpreter::_maxIdentifierNameLen + 1]) >> 4) & 0x0F;
                    _pParsingStack->openPar.maxArgs = (_pInterpreter->extFunctionNames[funcIndex][Interpreter::_maxIdentifierNameLen + 1]) & 0x0F;
                    if ((int)_pParsingStack->openPar.minArgs > actualArgs) { _pParsingStack->openPar.minArgs = actualArgs; }
                    if ((int)_pParsingStack->openPar.maxArgs < actualArgs) { _pParsingStack->openPar.maxArgs = actualArgs; }
                }
                // no previous call: simply set this range to the argument count of the current external function call that is at its closing parenthesis
                else { _pParsingStack->openPar.minArgs = actualArgs; _pParsingStack->openPar.maxArgs = actualArgs; }

                // store the up to date range of actual argument counts in identifier storage
                _pInterpreter->extFunctionNames[funcIndex][Interpreter::_maxIdentifierNameLen + 1] = (_pParsingStack->openPar.minArgs << 4) | (_pParsingStack->openPar.maxArgs);
            }

            // if call to previously defined external function, to an internal function, or if open parenthesis, then check argument count 
            else {
                bool isOpenParenthesis = (flags & _pInterpreter->openParenthesisBit);
                if (isOpenParenthesis) { _pParsingStack->openPar.minArgs = 1; _pParsingStack->openPar.maxArgs = 1; }
                bool argCountWrong = ((actualArgs < (int)_pParsingStack->openPar.minArgs) ||
                    (actualArgs > (int) _pParsingStack->openPar.maxArgs));
                if (argCountWrong) { pNext = pch; result = result_wrong_arg_count; return false; }
            }

            // check that order of arrays and scalar variables is consistent with function definition and (external functions only: with previous calls) 
            if (flags & _pInterpreter->intFunctionBit) { if (!checkInternFuncArgArrayPattern(result)) { pNext = pch; return false; }; }
            else if (flags & _pInterpreter->extFunctionBit) { if (!checkExternFuncArgArrayPattern(result, true)) { pNext = pch; return false; }; }
        }


        // 2.4 Array element spec closing parenthesis ?
        // --------------------------------------------

        else if (flags & _pInterpreter->arrayBit) {
            // check if array dimension count corresponds (individual dimension adherence can only be checked at runtime)
            // for function parameters, array dimension count can only be checked at runtime as well
            // if previous token is left parenthesis (' () '), then do not increment argument count
            bool lastWasLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;            // ok because no nesting allowed
            if (!lastWasLeftPar) { _pParsingStack->openPar.actualArgsOrDims++; }

            int varNameIndex = _pParsingStack->openPar.identifierIndex;
            uint8_t varScope = _pParsingStack->openPar.variableScope;
            bool isParam = (varScope == _pInterpreter->var_isParamInFunc);
            int actualDimCount = _pParsingStack->openPar.actualArgsOrDims;
            if (actualDimCount == 0) { pNext = pch; result = result_arrayUseNoDims; return false; } // dim count too high: already handled when preceding comma was parsed
            if (!isParam) {
                if (actualDimCount != (int)_pParsingStack->openPar.arrayDimCount) { pNext = pch; result = result_arrayUseWrongDimCount; return false; }
            }
        }

        else {}     // for documentation only: all cases handled


        // token is a right parenthesis, and it's allowed here

        parsingStack.deleteListElement(nullptr);                                           // decrement open parenthesis stack counter and delete corresponding list element
        _parenthesisLevel--;

        // set pointer to currently last element in stack
        if (_blockLevel + _parenthesisLevel > 0) { _pParsingStack = (LE_parsingStack*)parsingStack.getLastListElement(); }

        _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;
        break;
    }


    case termcod_comma: {
        // ------------------------------------
        // Case 3: is token a comma separator ?
        // ------------------------------------

        if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is comma separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_0)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
        if (!tokenAllowed) { pNext = pch; result = result_separatorNotAllowedHere; return false; ; }

        // if no open parenthesis, a comma can only occur to separate command parameters
        if ((_parenthesisLevel == 0) && !_isCommand) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

        flags = (_parenthesisLevel > 0) ? _pParsingStack->openPar.flags : 0;

        // expression syntax check 
        _thisLvl_lastIsVariable = false;       // currently open block, new expression
        _thisLvl_assignmentStillPossible = true;            // init (start of (sub-)expression)
        _thisLvl_lastOpIsIncrDecr = false;

        // command argument constraints check
        if (_parenthesisLevel == 0) {
            _lvl0_withinExpression = false;                                                         // reset for next command argument
            _lvl0_isPureVariable = false;
            _lvl0_isVarWithAssignment = false;
        }


        // 3.1 External function definition (not a call) parameter separator ? 
        // -------------------------------------------------------------------

        if (_isExtFunctionCmd) {
            if (_parenthesisLevel == 1) {          // not an array parameter (would be parenthesis level 2)
                _pParsingStack->openPar.actualArgsOrDims++;
                // check order of mandatory and optional arguments, check if max. n� not exceeded
                if (!checkExtFunctionArguments(result, extFunctionDef_minArgCounter, extFunctionDef_maxArgCounter)) { pNext = pch; return false; };

                // Check order of mandatory and optional arguments (function: parenthesis levels > 0)
                if (!checkExternFuncArgArrayPattern(result, false)) { pNext = pch; return false; };       // verify that the order of scalar and array parameters is consistent with arguments
            }
        }


        // 3.2 Array definition dimension spec separator ? 
        // -----------------------------------------------

        else if (_isAnyVarCmd) {
            if (_parenthesisLevel == 1) {             // // parenthesis level 1: separator between array dimension specs (level 0: sep. between variables)             
                // Check dimension count and array size 
                if (!checkArrayDimCountAndSize(result, arrayDef_dims, array_dimCounter)) { pNext = pch; return false; }
            }
        }


        // 3.3 Internal or external function call argument separator ?
        // -----------------------------------------------------------

        else if (flags & (_pInterpreter->intFunctionBit | _pInterpreter->extFunctionBit | _pInterpreter->openParenthesisBit)) {
            // note that actual argument count is at least one more than actual argument count, because at least one more to go (after the comma)
            _pParsingStack->openPar.actualArgsOrDims++;           // include argument before the comma in argument count     
            int actualArgs = (int)_pParsingStack->openPar.actualArgsOrDims;

            // call to not yet defined external function ? (because there might be previous calls as well)
            bool callToNotYetDefinedFunc = ((_pParsingStack->openPar.flags & (_pInterpreter->extFunctionBit | _pInterpreter->extFunctionPrevDefinedBit)) == _pInterpreter->extFunctionBit);
            if (callToNotYetDefinedFunc) {
                // check that max argument count is not exceeded (number must fit in 4 bits)
                if (actualArgs > c_extFunctionMaxArgs) { pNext = pch; result = result_functionDefMaxArgsExceeded; return false; }
            }

            // if call to previously defined external function, to an internal function, or if open parenthesis, then check argument count 
            else {
                bool isOpenParenthesis = (flags & _pInterpreter->openParenthesisBit);
                if (isOpenParenthesis) { _pParsingStack->openPar.minArgs = 1; _pParsingStack->openPar.maxArgs = 1; }
                bool argCountWrong = (actualArgs >= (int)_pParsingStack->openPar.maxArgs);       // check against allowed maximum number of arguments for this function
                if (argCountWrong) { pNext = pch; result = isOpenParenthesis ? result_missingRightParenthesis : result_wrong_arg_count; return false; }
            }

            // check that order of arrays and scalar variables is consistent with function definition and (external functions only: with previous calls) 
            if (flags & _pInterpreter->intFunctionBit) { if (!checkInternFuncArgArrayPattern(result)) { pNext = pch; return false; }; }
            else if (flags & _pInterpreter->extFunctionBit) { if (!checkExternFuncArgArrayPattern(result, false)) { pNext = pch; return false; }; }
        }


        // 3.4 Array subscript separator ?
        // ----------------------------------

        else if (flags & _pInterpreter->arrayBit) {
            // check if array dimension count corresponds (individual boundary adherence can only be checked at runtime)
            _pParsingStack->openPar.actualArgsOrDims++;
            if ((int)_pParsingStack->openPar.actualArgsOrDims == (int)_pParsingStack->openPar.arrayDimCount) { pNext = pch; result = result_arrayUseWrongDimCount; return false; }
        }

        else {}     // for documentation only: all cases handled

        // token is a comma separator, and it's allowed here
        _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;
        break; }


    case termcod_semicolon: {
        // ----------------------------------------
        // Case 4: is token a semicolon separator ?
        // ----------------------------------------

        if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is semicolon separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (_parenthesisLevel > 0) { pNext = pch; result = result_missingRightParenthesis; return false; }
        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_2_0)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

        // token is a semicolon separator, and it's allowed here
        _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

        // expression syntax check 
        _thisLvl_lastIsVariable = false;       // currently open block
        _thisLvl_assignmentStillPossible = true;
        _thisLvl_lastOpIsIncrDecr = false;

        // command argument constraints check
        _lvl0_withinExpression = false;                                                         // reset for next command argument
        _lvl0_isPureVariable = false;
        _lvl0_isVarWithAssignment = false;

        break; }


    default:
    {
        // ----------------------------
        // Case 5: token is an operator
        // ----------------------------

        if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        // token is an operator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return

        // does last token type allow an operator as current token ?
        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_5_3_2_1_0)) { pNext = pch; result = result_operatorNotAllowedHere; return false; }

        // allow token (pending further tests) if within most commands, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
        if (!tokenAllowed) { pNext = pch; result = result_operatorNotAllowedHere; return false; ; }
        if (_isProgramCmd || _isDeleteVarCmd || _isDecCBprocCmd) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
        if (_isCallbackCmd && (_cmdArgNo == 0)) { pNext = pch; result = result_operatorNotAllowedHere; return false; }


        // find out if the provided operator type (prefix, infix or postfix) is allowed 
        // ----------------------------------------------------------------------------

        // does last token type limit allowable operators to infix and postfix ?
        bool tokenIsPrefixOp{ false }, tokenIsPostfixOp{ false };
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3) ||
            ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp)) {
            // infix and postfix operators are allowed: test that current operator is infix or postfix
            if ((_terminals[termIndex].infix_priority == 0) && (_terminals[termIndex].postfix_priority == 0)) { pNext = pch; result = result_prefixOperatorNotAllowedhere; return false; }
            tokenIsPrefixOp = false; tokenIsPostfixOp = (_terminals[termIndex].postfix_priority != 0);    // token is either infix or postfix
        }

        // prefix operators only are allowed
        else {
            //test that current operator is prefix
            if (_terminals[termIndex].prefix_priority == 0) { pNext = pch; result = result_invalidOperator; return false; }
            tokenIsPrefixOp = true; tokenIsPostfixOp = false;
        }


        // if current token is a prefix variable increment / decrement operator: note if subsequent assignment is allowed
        // ---------------------------------------------------------------------------------------------------------------

        bool isPrefixIncrDecr = (tokenIsPrefixOp && (_terminals[termIndex].terminalCode == termcod_incr) || (_terminals[termIndex].terminalCode == termcod_decr));
        bool isPostfixIncrDecr = (tokenIsPostfixOp && (_terminals[termIndex].terminalCode == termcod_incr) || (_terminals[termIndex].terminalCode == termcod_decr));

        if (isPostfixIncrDecr) {
            if (!_thisLvl_lastIsVariable) { Serial.println("*** 1"); pNext = pch; result = result_operatorNotAllowedHere; return false; }   // not a variable or array element
            if (_thisLvl_lastOpIsIncrDecr) { Serial.println("*** 2"); pNext = pch; result = result_operatorNotAllowedHere; return false; }
        }

        if (tokenIsPrefixOp && !isPrefixIncrDecr) {
            if (_thisLvl_lastOpIsIncrDecr) { Serial.println("*** 0"); pNext = pch; result = result_operatorNotAllowedHere; return false; }
        }

        _thisLvl_lastOpIsIncrDecr = (isPrefixIncrDecr || isPostfixIncrDecr);      // remember, because not allowed with postfix increment / decrement (has higher priority and does not return a variable reference)


        // if current token is an assignment operator, check whether it's allowed here
        // ---------------------------------------------------------------------------

        bool operatorContainsAssignment = ((_terminals[termIndex].terminalCode == termcod_assign)
            || (_terminals[termIndex].terminalCode == termcod_plusAssign) || (_terminals[termIndex].terminalCode == termcod_minusAssign)
            || (_terminals[termIndex].terminalCode == termcod_multAssign) || (_terminals[termIndex].terminalCode == termcod_divAssign));


        if (operatorContainsAssignment) {
            if (!_thisLvl_lastIsVariable) { Serial.println("*** 3"); pNext = pch; result = result_assignmNotAllowedHere; return false; }   // not a variable or array element
            if (!_thisLvl_assignmentStillPossible) { Serial.println("*** 4"); pNext = pch; result = result_assignmNotAllowedHere; return false; }
        }

        if (!(operatorContainsAssignment || isPrefixIncrDecr)) { _thisLvl_assignmentStillPossible = false; }   // further assignments at this expression level not possible any more






        // command argument constraints check
        if (_parenthesisLevel == 0) {
            if (_lvl0_isPureVariable) { _lvl0_isVarWithAssignment = true; }
        }
        _lvl0_withinExpression = true;                                                         // reset for next command argument
        _lvl0_isPureVariable = false;


        // numeric initializer with + or minus prefix: handle as part of number
        // --------------------------------------------------------------------

        if (_isExtFunctionCmd || _isAnyVarCmd) {
            if ((_terminals[termIndex].terminalCode == termcod_plus) || (_terminals[termIndex].terminalCode == termcod_minus)) {
                // normally, a prefix operator needs its own token (example: expression -2^2 evaluates as -(2^2) yielding -4, whereas a number -2 (stored as one token) ^2 would yield 4, which is incorrect
                // but initializers are pure constants: no prefix operators are allowed here, because this would create a constant expression
                // however negative numbers are legal as initialiser: discard the prefix operator, to make it part of the number token
                if (nextTermIndex >= 0) { pNext = pch; result = result_operatorNotAllowedHere; return false; } // next token is terminal as well. It risks to be another prefix operator
                else { pNext = pch; return true; }         // do not move input pointer
            }
            else if (_terminals[termIndex].terminalCode != termcod_assign) { pNext = pch; result = result_operatorNotAllowedHere; return false; }       // not a plus or minus prefix
        }


        // token is an operator, and it's allowed here
        _lastTokenIsPrefixOp = tokenIsPrefixOp;
        _lastTokenIsPostfixOp = tokenIsPostfixOp;
        _lastTokenIsPrefixIncrDecr = isPrefixIncrDecr;
    }
    }


    // create token
    // ------------

    // too many terminals for 1 terminal group: provide multiple groups
    tokenType = (termIndex <= 0x0F) ? Interpreter::tok_isTerminalGroup1 : (termIndex <= 0x1F) ? Interpreter::tok_isTerminalGroup2 : Interpreter::tok_isTerminalGroup3;                                              // remember: token is a left parenthesis
    _tokenIndex = termIndex;

    Interpreter::TokenIsTerminal* pToken = (Interpreter::TokenIsTerminal*)_pInterpreter->_programCounter;
    pToken->tokenTypeAndIndex = tokenType | ((termIndex & 0x0F) << 4);     // terminal tokens only: token type character includes token index too 

    _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
    _lastTokenType = tokenType;
    _lastTokenIsTerminal = true;
    _lastTermCode = (termin_code)_terminals[termIndex].terminalCode;

#if printParsedTokens
    Serial.print("parsing termin : address is "); Serial.print(_lastTokenStep); Serial.print(" [ "); Serial.print(_terminals[termIndex].terminalName);  Serial.println(" ]");
#endif

    _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsTerminal);
    *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// ----------------------------------------------------------------------------
// *   try to parse next characters as an internal (built in) function name   *
// ----------------------------------------------------------------------------

bool MyParser::parseAsInternFunction(char*& pNext, parseTokenResult_type& result) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int funcIndex;

    if (!isalpha(pNext[0])) { return true; }                                       // first character is not a letter ? Then it's not a function name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    for (funcIndex = _functionCount - 1; funcIndex >= 0; funcIndex--) {                  // for all defined function names: check against alphanumeric token (NOT ending by '\0')
        if (strlen(_functions[funcIndex].funcName) != pNext - pch) { continue; }   // token has correct length ? If not, skip remainder of loop ('continue')                            
        if (strncmp(_functions[funcIndex].funcName, pch, pNext - pch) != 0) { continue; }      // token corresponds to function name ? If not, skip remainder of loop ('continue')    

        // token is a function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_functionNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_functionNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
        if (!tokenAllowed) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

        if (_isExtFunctionCmd) { pNext = pch; result = result_redefiningIntFunctionNotAllowed; return false; }
        if (_isAnyVarCmd) { pNext = pch; result = result_variableNameExpected; return false; }        // is a variable declaration: internal function name not allowed

        bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
        if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }

        // token is function, and it's allowed here
        _minFunctionArgs = _functions[funcIndex].minArgs;                       // set min & max for allowed argument count (note: minimum is 0)
        _maxFunctionArgs = _functions[funcIndex].maxArgs;
        _functionIndex = funcIndex;

        // expression syntax check 
        _thisLvl_lastIsVariable = false;

        // command argument constraints check
        _lvl0_withinExpression = true;


        Interpreter::TokenIsIntFunction* pToken = (Interpreter::TokenIsIntFunction*)_pInterpreter->_programCounter;
        pToken->tokenType = Interpreter::tok_isInternFunction | (sizeof(Interpreter::TokenIsIntFunction) << 4);
        pToken->tokenIndex = funcIndex;

        _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
        _lastTokenType = Interpreter::tok_isInternFunction;
        _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if printParsedTokens
        Serial.print("parsing int fcn: address is "); Serial.print(_lastTokenStep); Serial.print(" ["); Serial.print(_functions[funcIndex].funcName);  Serial.println("]");
#endif

        _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsIntFunction);
        *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
        result = result_tokenFound;                                                     // flag 'valid token found'
        return true;
    }

    pNext = pch;                                                                        // reset pointer to first character to parse (because no token was found)
    return true;                                                                        // token is not a function name (but can still be something else)
}


// ------------------------------------------------------------------------
// *   try to parse next characters as an external (user) function name   *
// ------------------------------------------------------------------------

bool MyParser::parseAsExternFunction(char*& pNext, parseTokenResult_type& result) {

    if (_isProgramCmd || _isDeleteVarCmd) { return true; }                             // looking for an UNQUALIFIED identifier name; prevent it's mistaken for a variable name (same format)
    if (_isDecCBprocCmd || _isCallbackCmd) { return true; }

    // 1. Is this token a function name ? 
    // ----------------------------------

    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    if (!isalpha(pNext[0])) { return true; }                                       // first character is not a letter ? Then it's not an identifier name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    // peek next character: is it a left parenthesis ?
    char* peek1 = pNext; while (peek1[0] == ' ') { peek1++; }

    if (peek1[0] != term_leftPar[0]) { pNext = pch; return true; }   // not an external function 
    if ((_isExtFunctionCmd) && (_parenthesisLevel > 0)) { pNext = pch; return true; }        // only array parameter allowed now
    if (_isAnyVarCmd) { pNext = pch; return true; }                                   // is a variable declaration: not an external function

    // name already in use as global or user variable name ? Then it's not an external function
    bool createNewName = false;
    int index = getIdentifier(_pInterpreter->programVarNames, _pInterpreter->_programVarNameCount, _pInterpreter->MAX_PROGVARNAMES, pch, pNext - pch, createNewName);
    if (index != -1) { pNext = pch; return true; }                // is a variable
    index = getIdentifier(_pInterpreter->userVarNames, _pInterpreter->_userVarCount, _pInterpreter->MAX_USERVARNAMES, pch, pNext - pch, createNewName);
    if (index != -1) { pNext = pch; return true; }                // is a user variable


    // 2. Is a function name allowed here ? 
    // ------------------------------------

    if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is an external function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_functionNotAllowedHere; return false; }
    if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_functionNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
    if (!tokenAllowed) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

    // if function name is too long, reset pointer to first character to parse, indicate error and return
    if (pNext - pch > Interpreter::_maxIdentifierNameLen) { pNext = pch; result = result_identifierTooLong;  return false; }

    // if in immediate mode: the function must be defined earlier (in a program)
    if (!_pInterpreter->_programMode) {
        createNewName = false;                                                              // only check if function is defined, do NOT YET create storage for it
        index = getIdentifier(_pInterpreter->extFunctionNames, _pInterpreter->_extFunctionCount, _pInterpreter->MAX_EXT_FUNCS, pch, pNext - pch, createNewName);
        if (index == -1) { pNext = pch; result = result_undefinedFunctionOrArray; return false; }
    }

    bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
    if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }

    // token is an external function (definition or call), and it's allowed here


    // 3. Has function attribute storage already been created for this function ? (because of a previous function definition or a previous function call)
    // --------------------------------------------------------------------------------------------------------------------------------------------------

    createNewName = true;                                                              // if new external function, create storage for it
    index = getIdentifier(_pInterpreter->extFunctionNames, _pInterpreter->_extFunctionCount, _pInterpreter->MAX_EXT_FUNCS, pch, pNext - pch, createNewName);
    if (index == -1) { pNext = pch; result = result_maxExtFunctionsReached; return false; }
    char* funcName = _pInterpreter->extFunctionNames[index];                                    // either new or existing function name
    if (createNewName) {                                                                      // new function name
        // init max (bits 7654) & min (bits 3210) allowed n� OR actual n� of arguments; store in last position (behind string terminating character)
        funcName[Interpreter::_maxIdentifierNameLen + 1] = c_extFunctionFirstOccurFlag;                          // max (bits 7654) < (bits 3210): indicates value is not yet updated by parsing previous calls closing parenthesis
        _pInterpreter->extFunctionData[index].pExtFunctionStartToken = nullptr;                      // initialize. Pointer will be set when function definition is parsed (checked further down)
        _pInterpreter->extFunctionData[index].paramIsArrayPattern[1] = 0x80;                        // set flag to indicate a new function name is parsed (definition or call)
        _pInterpreter->extFunctionData[index].paramIsArrayPattern[0] = 0x00;                        // boundary alignment 
    }

    // if function storage was created already: check for double function definition
    else if (_isExtFunctionCmd) {                                                     // this is a function definition (not a call)
        // pointer to function starting token already defined: this is a double definition
        if (_pInterpreter->extFunctionData[index].pExtFunctionStartToken != nullptr) { pNext = pch; result = result_functionAlreadyDefinedBefore; return false; }
    }

    // Is this an external function definition( not a function call ) ?
    if (_isExtFunctionCmd) {
        _pInterpreter->extFunctionData[index].pExtFunctionStartToken = _pInterpreter->_programCounter;            // store pointer to function start token 
        // variable name usage array: reset in-procedure reference flags to be able to keep track of in-procedure variable value types used
        // KEEP all other settings
        for (int i = 0; i < _pInterpreter->_programVarNameCount; i++) { _pInterpreter->globalVarType[i] = (_pInterpreter->globalVarType[i] & ~_pInterpreter->var_scopeMask) | _pInterpreter->var_scopeToSpecify; }
        _pInterpreter->_paramOnlyCountInFunction = 0;             // reset local and parameter variable count in function 
        _pInterpreter->_localVarCountInFunction = 0;             // reset local and parameter variable count in function
        _pInterpreter->_staticVarCountInFunction = 0;             // reset static variable count in function
        _pInterpreter->extFunctionData[index].paramOnlyCountInFunction = 0;
        _pInterpreter->extFunctionData[index].localVarCountInFunction = 0;
        _pInterpreter->extFunctionData[index].staticVarCountInFunction = 0;

        // if function will define static variables, then storage area will start right after stoarage area for previously defined user function's static variable area (this is needed while in debugging only)
        _pInterpreter->extFunctionData[index].staticVarStartIndex = _pInterpreter->_staticVarCount;

        // if function will define local variables, although storage area is dynamic, this is needed while in debugging (only)
        _pInterpreter->extFunctionData[index].localVarNameRefs_startIndex = _pInterpreter->_localVarCount;

        _pFunctionDefStack = _pParsingStack;               // stack level for FUNCTION definition block
        _pFunctionDefStack->openBlock.fcnBlock_functionIndex = index;  // store in BLOCK stack level: only if function def

    }

    // if function was defined prior to this occurence (which is then a call), retrieve min & max allowed arguments for checking actual argument count
    // if function not yet defined: retrieve current state of min & max of actual argument count found in COMPLETELY PARSED previous calls to same function 
    // if no previous occurences at all: data is not yet initialized (which is ok)
    _minFunctionArgs = ((funcName[Interpreter::_maxIdentifierNameLen + 1]) >> 4) & 0x0F;         // use only for passing to parsing stack
    _maxFunctionArgs = (funcName[Interpreter::_maxIdentifierNameLen + 1]) & 0x0F;
    _functionIndex = index;

    // expression syntax check 
    _thisLvl_lastIsVariable = false;

    // command argument constraints check
    _lvl0_withinExpression = true;


    // 4. Store token in program memory
    // --------------------------------

    Interpreter::TokenIsExtFunction* pToken = (Interpreter::TokenIsExtFunction*)_pInterpreter->_programCounter;
    pToken->tokenType = Interpreter::tok_isExternFunction | (sizeof(Interpreter::TokenIsExtFunction) << 4);
    pToken->identNameIndex = index;

    _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
    _lastTokenType = Interpreter::tok_isExternFunction;
    _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if printParsedTokens
    Serial.print("parsing ext fcn: address is "); Serial.print(_lastTokenStep); Serial.print(" ["); Serial.print(_pInterpreter->extFunctionNames[_functionIndex]);  Serial.println("]");
#endif

    _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsExtFunction);
    *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// --------------------------------------------------
// *   try to parse next characters as a variable   *
// --------------------------------------------------

bool MyParser::parseAsVariable(char*& pNext, parseTokenResult_type& result) {


    if (_isProgramCmd || _isDeleteVarCmd || _isDecCBprocCmd) { return true; }                             // looking for an UNQUALIFIED identifier name; prevent it's mistaken for a variable name (same format)
    if (_isCallbackCmd && (_cmdArgNo == 0)) { return true; }

    // 1. Is this token a variable name ? 
    // ----------------------------------

    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    if (!isalpha(pNext[0])) { return true; }                                       // first character is not a letter ? Then it's not a variable name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')


    // 2. Is a variable name allowed here ? 
    // ------------------------------------

    if (_pInterpreter->_programCounter == _pInterpreter->_programStorage) { pNext = pch; result = result_programCmdMissing; return false; }  // program mode and no PROGRAM command

    // token is a variable, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_variableNotAllowedHere; return false; }
    if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_variableNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_pInterpreter->_programMode) || _extFunctionBlockOpen);
    if (!tokenAllowed) { pNext = pch; result = result_variableNotAllowedHere; return false; ; }

    // scalar or array variable ? (could still be function 'array' argument; this will be detected further below)
    char* peek1 = pNext; while (peek1[0] == ' ') { peek1++; }                                                // peek next character: is it a left parenthesis ?
    char* peek2; if (peek1[0] == term_leftPar[0]) { peek2 = peek1 + 1; while (peek2[0] == ' ') { peek2++; } }         // also find the subsequent character
    bool isArray = (peek1[0] == term_leftPar[0]);
    if (_isExtFunctionCmd) {                                     // only (array) parameter allowed now
        if (_parenthesisLevel == 0) { pNext = pch; result = result_functionDefExpected; return false; }           // is not an array parameter declaration
        if (isArray && (_parenthesisLevel == 1) && (peek2[0] != term_rightPar[0])) { pNext = pch; result = result_arrayParamExpected; return false; }           // is not an array parameter declaration
    }

    if (_isAnyVarCmd) {
        if (_varDefAssignmentFound) { pNext = pch; result = result_constantValueExpected; return false; }
    }

    // Note: in a (variable or parameter) declaration statement, operators other than assignment operators are not allowed, which is detected in terminal token parsing
    bool isParamDecl = (_isExtFunctionCmd);                                          // parameter declarations: initialising ONLY with a constant, not with a variable
    bool isPureAssgnmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;
    if (isParamDecl && isPureAssgnmentOp)                                                    // if operator: it is an assignment
    {
        pNext = pch; result = result_variableNotAllowedHere; return false;
    }

    bool isArrayDimSpec = (_isAnyVarCmd) && (_parenthesisLevel > 0);                    // array declaration: dimensions must be number constants (global, static, local arrays)
    if (isArrayDimSpec) { pNext = pch; result = result_variableNotAllowedHere; return false; }

    // if variable name is too long, reset pointer to first character to parse, indicate error and return
    if (pNext - pch > Interpreter::_maxIdentifierNameLen) { pNext = pch; result = result_identifierTooLong;  return false; }

    // name already in use as external function name ?
    bool createNewName{ false };
    int varNameIndex = getIdentifier(_pInterpreter->extFunctionNames, _pInterpreter->_extFunctionCount, _pInterpreter->MAX_EXT_FUNCS, pch, pNext - pch, createNewName);
    if (varNameIndex != -1) { pNext = pch; result = result_varNameInUseForFunction; return false; }

    // token is a variable NAME, and a variable is allowed here

    // 3. Check whether this name exists already for variables, and create if needed
    // -----------------------------------------------------------------------------------------------------------------------------------------------

    // note that multiple distinct program variables (global, static, local) and function parameters can all share the same name, which is only stored once 
    // user variable names are stored separately

    // set pointers to variable name storage areas for program variable names and user variable names, respectively
    char** pvarNames[2]; pvarNames[0] = _pInterpreter->programVarNames; pvarNames[1] = _pInterpreter->userVarNames;
    int* varNameCount[2]; varNameCount[0] = &_pInterpreter->_programVarNameCount; varNameCount[1] = &_pInterpreter->_userVarCount;
    int maxVarNames[2]; maxVarNames[0] = _pInterpreter->MAX_PROGVARNAMES; maxVarNames[1] = _pInterpreter->MAX_USERVARNAMES;
    char* varType[2]; varType[0] = _pInterpreter->globalVarType; varType[1] = _pInterpreter->userVarType;
    Interpreter::Val* varValues[2]; varValues[0] = _pInterpreter->globalVarValues; varValues[1] = _pInterpreter->userVarValues;

    // 0: program variable, 1: user variable
    int primaryNameRange = _pInterpreter->_programMode ? 0 : 1;
    int secondaryNameRange = _pInterpreter->_programMode ? 1 : 0;

    // init: program parsing: assume program variable name for now; immediate mode parsing: assume user variable name
    bool isProgramVar = _pInterpreter->_programMode;
    int activeNameRange = primaryNameRange;

    // check if variable exists already (program mode: as program variable; immediate mode: as user variable)
    // if a variable DEFINITION, then create variable name if it does not exist yet
    // note: this only concerns the NAME, not yet the actual variable (program variables: local, static, param and global variables can all share the same name)
    createNewName = _isExtFunctionCmd || _isAnyVarCmd;
    bool isUserVar = !_pInterpreter->_programMode;
    varNameIndex = getIdentifier(pvarNames[primaryNameRange], *varNameCount[primaryNameRange], maxVarNames[primaryNameRange], pch, pNext - pch, createNewName, isUserVar);

    if (_isExtFunctionCmd || _isAnyVarCmd) {               // variable or parameter DEFINITION: if name didn't exist, it should have been created now
        if (varNameIndex == -1) { pNext = pch; result = result_maxVariableNamesReached; return false; }      // name still does not exist: error
        // name exists (newly created or pre-existing)
        // variable name is new: clear all variable value type flags and indicate 'qualifier not determined yet'
        // variable value type (array, float or string) will be set later
        if (createNewName) {
            varType[primaryNameRange][varNameIndex] = _pInterpreter->var_scopeToSpecify;      // new name was created now
            // NEW user variables only: if array definition, then decrease variable count by 1 for now, and increase by 1 again when array dim spec is validated
            // this ensures that a scalar is not created when an error is encountered later within dim spec parsing
            if (!isProgramVar && isArray) { (*varNameCount[primaryNameRange])--; }    // the variable is not considered 'created' yet
        }
    }
    else { // not a variable definition, just a variable reference
        if (varNameIndex == -1) {
            // variable name does not exist in primary range (and no error produced, so it was not a variable definition):
            // check if the name is defined in the secondary name range
            varNameIndex = getIdentifier(pvarNames[secondaryNameRange], *varNameCount[secondaryNameRange], maxVarNames[secondaryNameRange], pch, pNext - pch, createNewName);
            if (varNameIndex == -1) { pNext = pch; result = result_varNotDeclared; return false; }  // if the name doesn't exist, the variable doesn't
            isProgramVar = !_pInterpreter->_programMode;                  // program parsing: is program variable; immediate mode: is user variable
            activeNameRange = secondaryNameRange;
        }

        // user variable referenced in program: set flag in user var types array (only; will not be copied in token info)
        if (_pInterpreter->_programMode && !isProgramVar) { varType[activeNameRange][varNameIndex] = varType[activeNameRange][varNameIndex] | _pInterpreter->var_userVarUsedByProgram; }
    }


    // 4. The variable NAME exists now, but we still need to check whether storage space for the variable itself has been created / allocated
    //    Note: LOCAL variable storage is created at runtime
    // --------------------------------------------------------------------------------------------------------------------------------------

    bool variableNotYetKnown = false;                                                                             // init
    bool isOpenFunctionStaticVariable{ false }, isOpenFunctionLocalVariable{ false };
    int openFunctionVar_valueIndex{};

    // 4.1 Currently parsing a FUNCTION...END block ? 
    // ----------------------------------------------

    // note: only while parsing program instructions
    if (_extFunctionBlockOpen) {
        // first use of a particular variable NAME in a function ?  (in a variable declaration, or just using the name in an expression)
        bool isFirstVarNameRefInFnc = (((uint8_t)varType[activeNameRange][varNameIndex] & _pInterpreter->var_scopeMask) == _pInterpreter->var_scopeToSpecify);
        if (isFirstVarNameRefInFnc) {                                                                         // variable not yet referenced within currently parsed procedure

            // determine variable qualifier
            // if a variable definition statement: set scope to parameter, local or static (global and usar variable definition: not possible in a function) 
            // if a variable reference: we will determine the qualifier in a moment 
            uint8_t varScope = _isExtFunctionCmd ? _pInterpreter->var_isParamInFunc : _isLocalVarCmd ? _pInterpreter->var_isLocalInFunc : _isStaticVarCmd ? _pInterpreter->var_isStaticInFunc : _pInterpreter->var_scopeToSpecify;
            varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~_pInterpreter->var_scopeMask) | varScope;     //set scope bits (will be stored in token AND needed during parsing current procedure)

            if (_isStaticVarCmd) {                                             // definition of NEW static variable for function
                variableNotYetKnown = true;
                if (_pInterpreter->_staticVarCount == _pInterpreter->MAX_STAT_VARS) { pNext = pch; result = result_maxStaticVariablesReached; return false; }



                _pInterpreter->programVarValueIndex[varNameIndex] = _pInterpreter->_staticVarCount;
                if (!isArray) { _pInterpreter->staticVarValues[_pInterpreter->_staticVarCount].floatConst = 0.; }           // initialize variable (if initializer and/or array: will be overwritten)

                _pInterpreter->staticVarType[_pInterpreter->_staticVarCount] = _pInterpreter->value_isFloat;                                         // init as float (for array or scalar)
                _pInterpreter->staticVarType[_pInterpreter->_staticVarCount] = (_pInterpreter->staticVarType[_pInterpreter->_staticVarCount] & ~_pInterpreter->var_isArray); // init (array flag will be added when storage is created)    



                // will only be used while in DEBUGGING mode: index of static variable name
                _pInterpreter->staticVarNameRef[_pInterpreter->_staticVarCount] = varNameIndex;

                _pInterpreter->_staticVarCountInFunction++;
                _pInterpreter->_staticVarCount++;

                // ext. function index: in parsing stack level for FUNCTION definition command
                int fcnIndex = _pFunctionDefStack->openBlock.fcnBlock_functionIndex;
                _pInterpreter->extFunctionData[fcnIndex].staticVarCountInFunction = _pInterpreter->_staticVarCountInFunction;
            }

            else if (_isExtFunctionCmd || _isLocalVarCmd) {               // definition of NEW parameter (in function definition) or NEW local variable for function
                variableNotYetKnown = true;
                if (_pInterpreter->_localVarCountInFunction == _pInterpreter->MAX_LOC_VARS_IN_FUNC) { pNext = pch; result = result_maxLocalVariablesReached; return false; }



                _pInterpreter->programVarValueIndex[varNameIndex] = _pInterpreter->_localVarCountInFunction;
                // param and local variables: array flag temporarily stored during function parsing       
                // storage space creation and initialisation will occur when function is called durig execution 
                _pInterpreter->localVarType[_pInterpreter->_localVarCountInFunction] = (_pInterpreter->localVarType[_pInterpreter->_localVarCountInFunction] & ~_pInterpreter->var_isArray) |
                    (isArray ? _pInterpreter->var_isArray : 0); // init (no storage needs to be created: set array flag here) 



                // will only be used while in DEBUGGING mode: index of local variable name
                _pInterpreter->localVarNameRef[_pInterpreter->_localVarCount] = varNameIndex;

                _pInterpreter->_localVarCountInFunction++;
                if (_isExtFunctionCmd) { _pInterpreter->_paramOnlyCountInFunction++; }
                _pInterpreter->_localVarCount++;

                // ext. function index: in stack level for FUNCTION definition command
                int fcnIndex = _pFunctionDefStack->openBlock.fcnBlock_functionIndex;
                _pInterpreter->extFunctionData[fcnIndex].localVarCountInFunction = _pInterpreter->_localVarCountInFunction;        // after incrementing count
                if (_isExtFunctionCmd) { _pInterpreter->extFunctionData[fcnIndex].paramOnlyCountInFunction = _pInterpreter->_paramOnlyCountInFunction; }
            }

            else {
                // not a variable definition:  CAN BE an EXISTING global or user variable, within a function
                // it CANNOT be a local or static variable, because this is the first reference of this variable name in the function and it's not a variable definition
                // if the variable name refers to a user variable, the variable exists, so it's known then
                variableNotYetKnown = isProgramVar ? (!(varType[activeNameRange][varNameIndex] & _pInterpreter->var_nameHasGlobalValue)) : false;
                // variable is NEW ? Variable has not been declared
                if (variableNotYetKnown) {              // undeclared global program variable                                                             
                    pNext = pch; result = result_varNotDeclared; return false;
                }
                // existing global or user variable
                varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~_pInterpreter->var_scopeMask) | (isProgramVar ? _pInterpreter->var_isGlobal : _pInterpreter->var_isUser);
            }                                                                                               // IS the use of an EXISTING global or user variable, within a function

        }

        else {  // if variable name already referenced before in function (global / user variable use OR param, local, static declaration), then it has been defined already
            bool isLocalDeclaration = (_isExtFunctionCmd || _isLocalVarCmd || _isStaticVarCmd); // local variable declaration ? (parameter, local, static)
            if (isLocalDeclaration) { pNext = pch; result = result_varRedeclared; return false; }
        }
    }


    // 4.2 NOT parsing FUNCTION...END block 
    // ------------------------------------

    // note: while parsing program instructions AND while parsing instructions entered in immediate mode
    else {
        // is global or user variable declared already ?
        variableNotYetKnown = !(varType[activeNameRange][varNameIndex] & (isProgramVar ? _pInterpreter->var_nameHasGlobalValue : _pInterpreter->var_isUser));
        // qualifier 'var_isGlobal' (program variables): set, because could be cleared by previously parsed function (will be stored in token)
        varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~_pInterpreter->var_scopeMask) | (isProgramVar ? _pInterpreter->var_isGlobal : _pInterpreter->var_isUser);

        // variable not yet declared as global or user variable
        if (variableNotYetKnown) {
            ////Serial.println("\r\n*** 4.2 - var not yet known");
            // but this can still be a global or user variable declaration 
            if (_isGlobalOrUserVarCmd) {                           // no, it's is a variable reference 
                ////Serial.println("*** 4.2 - is global");
                // is a declaration of a new program global variable (in program mode), or a new user user variable (in immediate mode) 
                // variable qualifier : don't care for now (global varables: reset at start of next external function parsing)
                if (!isArray) { varValues[activeNameRange][varNameIndex].floatConst = 0.; }                  // initialize variable (if initializer and/or array: will be overwritten)
                varType[activeNameRange][varNameIndex] = varType[activeNameRange][varNameIndex] | _pInterpreter->value_isFloat;         // init as float (for scalar and array)
                varType[activeNameRange][varNameIndex] = varType[activeNameRange][varNameIndex] | (isProgramVar ? _pInterpreter->var_nameHasGlobalValue : _pInterpreter->var_isUser);   // set 'has global value' or 'user var' bit
                varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~_pInterpreter->var_isArray); // init (array flag may only be added when storage is created) 
            }
            else {
                ////Serial.println("*** 4.2 - is not global");
                // it's neither a global or user variable declaration, nor a global or user variable reference. But the variable name exists,
                // so local or static function variables using this name have been defined already. 
                // in debug mode (program stopped), the name could refer to a local or static variable of a function in the call stack (open function) 
                // in debug mode now ? (if multiple programs in debug mode, only the last one stopped will be considered here
                if (_pInterpreter->_programsInDebug > 0) {
                    // check whether this is a local or static function variable reference of the deepest open function in the call stack

                    ////Serial.println("*** 4.2 - in debug mode");
                    int openFunctionIndex{};
                    void* pFlowCtrlStackLvl = _pInterpreter->_pFlowCtrlStackTop;                    int blockType = block_none;
                    do {
                        blockType = *(char*)pFlowCtrlStackLvl;
                        if (blockType != block_extFunction) {
                            pFlowCtrlStackLvl = _pInterpreter->flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                            continue;
                        };          // there is at least one open function in the call stack
                        openFunctionIndex = ((Interpreter::OpenFunctionData*)pFlowCtrlStackLvl)->functionIndex;    // function index of deepest function in call stack
                        break;
                    } while (true);

                    // is variable defined in this function, and is it local or static ?
                    int staticVarStartIndex = _pInterpreter->extFunctionData[openFunctionIndex].staticVarStartIndex;
                    int staticVarCountInFunction = _pInterpreter->extFunctionData[openFunctionIndex].staticVarCountInFunction;

                    //   is variable defined in this function as a static variable ?
                    int i{};

                    for (i = staticVarStartIndex; i <= staticVarStartIndex + staticVarCountInFunction - 1; ++i) {        // skip if count is zero
                        if (_pInterpreter->staticVarNameRef[i] == varNameIndex) { isOpenFunctionStaticVariable = true; openFunctionVar_valueIndex = i; break; }     // is a static variable of function and its value index is known
                    }
                    if (!isOpenFunctionStaticVariable) {
                        int localVarNameRefs_startIndex = _pInterpreter->extFunctionData[openFunctionIndex].localVarNameRefs_startIndex;
                        int localVarCountInFunction = _pInterpreter->extFunctionData[openFunctionIndex].localVarCountInFunction;

                        //   is variable defined in this function as a local variable ?
                        for (i = localVarNameRefs_startIndex; i <= localVarNameRefs_startIndex + localVarCountInFunction - 1; ++i) {        // skip if count is zero
                            if (_pInterpreter->localVarNameRef[i] == varNameIndex) { isOpenFunctionLocalVariable = true; openFunctionVar_valueIndex = i; break; }     // is a local variable of function and its value index is known
                        }
                    }
                    if (!isOpenFunctionStaticVariable && !isOpenFunctionLocalVariable) {
                        pNext = pch; result = result_varNotDeclared; return false;
                    }
                }

                else {
                    pNext = pch; result = result_varNotDeclared; return false;
                }
            }

        }

        else {  // the global or user variable exists already: check for double definition
            ////Serial.println("*** 4.2 - var IS known");
            if (_isGlobalOrUserVarCmd) {
                if (!(_pInterpreter->_programMode ^ isProgramVar)) { pNext = pch; result = result_varRedeclared; return false; }
            }
        }
    }


    ////Serial.println("*** 5");
    // 5. If NOT a new variable, check if it corresponds to the variable definition (scalar or array) and retrieve array dimension count (if array)
    //    If it is a FOR loop control variable, check that it is not in use by a FOR outer loop (in same function)
    // --------------------------------------------------------------------------------------------------------------------------------------------

    uint8_t varScope = isOpenFunctionStaticVariable ? _pInterpreter->var_isStaticInFunc :
        isOpenFunctionLocalVariable ? _pInterpreter->var_isLocalInFunc :
        varType[activeNameRange][varNameIndex] & _pInterpreter->var_scopeMask;  // may only contain variable scope info (parameter, local, static, global, user)

    bool isGlobalOrUserVar = (isOpenFunctionStaticVariable || isOpenFunctionLocalVariable) ? false :
        isProgramVar ?
        ((_extFunctionBlockOpen && (varScope == _pInterpreter->var_isGlobal)) ||                             // NOTE: outside a function, test against 'var_nameHasGlobalValue'
            (!_extFunctionBlockOpen && (varType[activeNameRange][varNameIndex] & _pInterpreter->var_nameHasGlobalValue))) :
        varType[activeNameRange][varNameIndex] & _pInterpreter->var_isUser;

    bool isStaticVar = isOpenFunctionStaticVariable ? true : (_extFunctionBlockOpen && (varScope == _pInterpreter->var_isStaticInFunc));
    bool isLocalVar = isOpenFunctionLocalVariable ? true : (_extFunctionBlockOpen && (varScope == _pInterpreter->var_isLocalInFunc));
    bool isParam = isOpenFunctionLocalVariable ? false : (_extFunctionBlockOpen && (varScope == _pInterpreter->var_isParamInFunc));    //// isOpenFunctionLocalVariable -> isOpenFunctionParamVariable, false -> true

    int valueIndex = (isOpenFunctionStaticVariable || isOpenFunctionLocalVariable) ? openFunctionVar_valueIndex :
        isGlobalOrUserVar ? varNameIndex : _pInterpreter->programVarValueIndex[varNameIndex];

    if (!variableNotYetKnown) {  // not a variable definition but a variable use
        bool existingArray = false;
        _pInterpreter->_arrayDimCount = 0;                  // init: if new variable (or no array), then set dimension count to zero

        existingArray = isGlobalOrUserVar ? (varType[activeNameRange][valueIndex] & _pInterpreter->var_isArray) :
            isStaticVar ? (_pInterpreter->staticVarType[valueIndex] & _pInterpreter->var_isArray) :
            (_pInterpreter->localVarType[valueIndex] & _pInterpreter->var_isArray);           // param or local

        // if not a function definition: array name does not have to be followed by a left parenthesis (passing the array and not an array element)
        if (!_isExtFunctionCmd) {
            // Is this variable part of a function call argument, without further nesting of parenthesis, and has it been defined as an array ? 
            bool isPartOfFuncCallArgument = (_parenthesisLevel > 0) ? (_pParsingStack->openPar.flags & (_pInterpreter->intFunctionBit | _pInterpreter->extFunctionBit)) : false;
            if (isPartOfFuncCallArgument && existingArray) {
                // if NOT followed by an array element enclosed in parenthesis, it references the complete array
                // this is only allowed if not part of an expression: check

                bool isFuncCallArgument = _lastTokenIsTerminal ? ((_lastTermCode == termcod_leftPar) || (_lastTermCode == termcod_comma)) : false;
                isFuncCallArgument = isFuncCallArgument && ((peek1[0] == term_comma[0]) || (peek1[0] == term_rightPar[0]));
                if (isFuncCallArgument) { isArray = true; }
            }
            if (existingArray ^ isArray) { pNext = pch; result = isArray ? result_varDefinedAsScalar : result_varDefinedAsArray; return false; }
        }


        // if existing array: retrieve dimension count against existing definition, for testing against definition afterwards
        if (existingArray) {
            void* pArray = nullptr;
            if (isStaticVar) { pArray = _pInterpreter->staticVarValues[valueIndex].pArray; }
            else if (isGlobalOrUserVar) { pArray = varValues[activeNameRange][valueIndex].pArray; }
            else if (isLocalVar) { pArray = (float*)_pInterpreter->localVarDims[valueIndex]; }   // dimensions and count are stored in a float
            // retrieve dimension count from array element 0, character 3 (char 0 to 2 contain the dimensions) 
            _pInterpreter->_arrayDimCount = isParam ? _pInterpreter->MAX_ARRAY_DIMS : ((char*)pArray)[3];
        }


        // if FOR loop control variable, check it is not in use by a FOR outer loop of same function  
        if ((_lastTokenType == Interpreter::tok_isReservedWord) && (_blockLevel > 1)) {     // minimum 1 other (outer) open block
            Interpreter::TokenPointer prgmCnt;
            prgmCnt.pTokenChars = _pInterpreter->_programStorage + _lastTokenStep;  // address of keyword
            int tokenIndex = prgmCnt.pResW->tokenIndex;
            CmdBlockDef cmdBlockDef = _resWords[tokenIndex].cmdBlockDef;

            // variable is a control variable of a FOR loop ?
            if (cmdBlockDef.blockType == block_for) {

                // check if control variable is in use by a FOR outer loop
                LE_parsingStack* pStackLvl = (LE_parsingStack*)parsingStack.getLastListElement();        // current open block level
                do {
                    pStackLvl = (LE_parsingStack*)parsingStack.getPrevListElement(pStackLvl);    // an outer block stack level
                    if (pStackLvl == nullptr) { break; }
                    if (pStackLvl->openBlock.cmdBlockDef.blockType == block_for) {    // outer block is FOR loop as well
                        // find token for control variable for this outer loop
                        uint16_t tokenStep{ 0 };
                        memcpy(&tokenStep, pStackLvl->openBlock.tokenStep, sizeof(char[2]));
                        tokenStep = tokenStep + sizeof(Interpreter::TokenIsResWord);  // now pointing to control variable of outer loop

                        // compare variable qualifier, name index and value index of outer and inner loop control variable
                        prgmCnt.pTokenChars = _pInterpreter->_programStorage + tokenStep;  // address of outer loop control variable
                        bool isSameControlVariable = ((varScope == uint8_t(prgmCnt.pVar->identInfo & _pInterpreter->var_scopeMask))
                            && ((int)prgmCnt.pVar->identNameIndex == varNameIndex)
                            && ((int)prgmCnt.pVar->identValueIndex == valueIndex));
                        if (isSameControlVariable) { pNext = pch; result = result_varControlVarInUse; return false; }
                    }
                } while (true);
            }
        }
    }

    _variableNameIndex = varNameIndex;          // will be pushed to parsing stack
    _variableScope = varScope;

    // expression syntax check 
    _thisLvl_lastIsVariable = true;

    // command argument constraints check
    if (!_lvl0_withinExpression) { _lvl0_isPureVariable = true; }
    _lvl0_withinExpression = true;                                                         // reset for next command parameter


    ////Serial.println("*** 6");
    // 6. Store token in program memory
    // --------------------------------

    Interpreter::TokenIsVariable* pToken = (Interpreter::TokenIsVariable*)_pInterpreter->_programCounter;
    pToken->tokenType = Interpreter::tok_isVariable | (sizeof(Interpreter::TokenIsVariable) << 4);
    // identInfo may only contain variable scope info (parameter, local, static, global) and 'is array' flag 
    pToken->identInfo = varScope | (isArray ? _pInterpreter->var_isArray : 0);              // qualifier, array flag ? (is fixed for a variable -> can be stored in token)  
    pToken->identNameIndex = varNameIndex;
    pToken->identValueIndex = valueIndex;                      // points to storage area element for the variable  

    _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
    _lastVariableTokenStep = _lastTokenStep;
    _lastTokenType = Interpreter::tok_isVariable;
    _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if printParsedTokens
    Serial.print("parsing var nam: address is "); Serial.print(_lastTokenStep); Serial.print(" ["); Serial.print(pvarNames[activeNameRange][varNameIndex]);  Serial.println("]");
#endif

    _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsVariable);
    *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'


    ////Serial.println("**** Var token stored");////
    return true;
}


// -----------------------------------------------------------------
// *   try to parse next characters as a generic identifier name   *
// -----------------------------------------------------------------

bool MyParser::parseAsIdentifierName(char*& pNext, parseTokenResult_type& result) {
    result = result_tokenNotFound;                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                  // pointer to first character to parse (any spaces have been skipped already)

    bool stay = (_isProgramCmd || _isDeleteVarCmd || _isDecCBprocCmd);
    stay = stay || (_isCallbackCmd && (_cmdArgNo == 0));
    if (!stay) { return true; }

    if (!isalpha(pNext[0])) { return true; }                                       // first character is not a letter ? Then it's not an identifier name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                   // do until first character after alphanumeric token (can be anything, including '\0')

    // token is a generic identifier, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (_parenthesisLevel > 0) { pNext = pch; result = result_identifierNotAllowedHere; return false; }
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_2_0)) { pNext = pch; result = result_identifierNotAllowedHere; return false; }

    // if variable name is too long, reset pointer to first character to parse, indicate error and return
    if (pNext - pch > Interpreter::Interpreter::_maxIdentifierNameLen) { pNext = pch; result = result_identifierTooLong;  return false; }

    // token is an identifier name, and it's allowed here
    char* pIdentifierName = new char[pNext - pch + 1];                    // create char array on the heap to store identifier name, including terminating '\0'
    _pInterpreter->parsedStringConstObjectCount++;
#if printCreateDeleteHeapObjects
    Serial.print("+++++ (parsed str ) "); Serial.println((uint32_t)pIdentifierName - RAMSTART);
#endif
    strncpy(pIdentifierName, pch, pNext - pch);                            // store identifier name in newly created character array
    pIdentifierName[pNext - pch] = '\0';                                                 // string terminating '\0'

    // Declaring program name or aliases ? Store 
    if (_isProgramCmd) {
        strcpy(_pInterpreter->_programName, pIdentifierName);
    }
    else if (_isDecCBprocCmd) {                                             // maximum 10 user functions     
        if (_pInterpreter->_userCBprocAliasSet_count >= _pInterpreter->_userCBprocStartSet_count) { pNext = pch; result = result_allUserCBAliasesSet;  return false; }
        for (int i = 0; i < _pInterpreter->_userCBprocAliasSet_count; i++) {
            if (strcmp(_pInterpreter->_callbackUserProcAlias[i], pIdentifierName) == 0) { pNext = pch; result = result_userCBAliasRedeclared;  return false; }
        }
        strcpy(_pInterpreter->_callbackUserProcAlias[_pInterpreter->_userCBprocAliasSet_count++], pIdentifierName);                           // maximum 10 user functions                                   
    }


    // expression syntax check 
    _thisLvl_lastIsVariable = false;

    // command argument constraints check
    _lvl0_withinExpression = false;                                                         // reset for next command parameter
    _lvl0_isPureVariable = false;
    _lvl0_isVarWithAssignment = false;


    Interpreter::TokenIsConstant* pToken = (Interpreter::TokenIsConstant*)_pInterpreter->_programCounter;
    pToken->tokenType = Interpreter::tok_isGenericName | (sizeof(Interpreter::TokenIsConstant) << 4);
    memcpy(pToken->cstValue.pStringConst, &pIdentifierName, sizeof(pIdentifierName));            // pointer not necessarily aligned with word size: copy memory instead

    _lastTokenStep = _pInterpreter->_programCounter - _pInterpreter->_programStorage;
    _lastTokenType = Interpreter::tok_isGenericName;
    _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if printParsedTokens
    Serial.print("parsing identif: address is "); Serial.print(_lastTokenStep); Serial.print(" ["); Serial.print(pIdentifierName);  Serial.println("]");
#endif

    _pInterpreter->_programCounter += sizeof(Interpreter::TokenIsConstant);
    *_pInterpreter->_programCounter = '\0';                                                 // indicates end of program
    result = result_tokenFound;                                                         // flag 'valid token found'
    return true;
}


// -----------------------------------------
// *   pretty print a parsed instruction   *
// -----------------------------------------
void MyParser::prettyPrintInstructions(int instructionCount, char* startToken, char* errorProgCounter, int* sourceErrorPos) {

    // input: stored tokens
    Interpreter::TokenPointer progCnt;
    progCnt.pTokenChars = (startToken == nullptr) ? _pInterpreter->_programStart : startToken;
    int tokenType = *progCnt.pTokenChars & 0x0F;
    int lastTokenType = Interpreter::tok_no_token;
    bool lastHasTrailingSpace = false, testForPostfix = false, testForPrefix = false;
    bool lastWasPostfixOperator = false, lastWasInfixOperator = false;
    bool allInstructions = (instructionCount == 0);
    bool isFirstInstruction = true;

    // output: printable token (text)
    const int maxCharsPretty{ 100 };           // must be long enough to hold one token in text (e.g. a variable name)
    int outputLength = 0;                       // init: first position

    while (tokenType != Interpreter::tok_no_token) {                                                                    // for all tokens in token list
        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? sizeof(Interpreter::TokenIsTerminal) :
            (tokenType == Interpreter::tok_isConstant) ? sizeof(Interpreter::TokenIsConstant) : (*progCnt.pTokenChars >> 4) & 0x0F;
        Interpreter::TokenPointer nextProgCnt;
        nextProgCnt.pTokenChars = progCnt.pTokenChars + tokenLength;
        int nextTokenType = *nextProgCnt.pTokenChars & 0x0F;                                                                // next token type (look ahead)
        bool tokenHasLeadingSpace = false, testNextForPostfix = false, isPostfixOperator = false, isInfixOperator = false;
        bool hasTrailingSpace = false;
        bool isSemicolon = false;

        char prettyToken[maxCharsPretty] = "";

        switch (tokenType) {
        case Interpreter::tok_isReservedWord:
        {
            Interpreter::TokenIsResWord* pToken = (Interpreter::TokenIsResWord*)progCnt.pTokenChars;
            bool nextIsTerminal = ((nextTokenType == Interpreter::tok_isTerminalGroup1) || (nextTokenType == Interpreter::tok_isTerminalGroup2) || (nextTokenType == Interpreter::tok_isTerminalGroup3));
            bool nextIsSemicolon = false;
            if (nextIsTerminal) {
                int nextTokenIndex = ((nextProgCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F);
                nextTokenIndex += ((nextTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (nextTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
                nextIsSemicolon = (_terminals[nextTokenIndex].terminalCode == termcod_semicolon);
            }

            sprintf(prettyToken, nextIsSemicolon ? "%s" : "%s ", _resWords[progCnt.pResW->tokenIndex]._resWordName);
            hasTrailingSpace = true;
            break;
        }

        case Interpreter::tok_isInternFunction:
            strcpy(prettyToken, _functions[progCnt.pIntFnc->tokenIndex].funcName);
            break;

        case Interpreter::tok_isExternFunction:
        {
            int identNameIndex = (int)progCnt.pExtFnc->identNameIndex;   // external function list element
            char* identifierName = _pInterpreter->extFunctionNames[identNameIndex];
            strcpy(prettyToken, identifierName);
            break;
        }

        case Interpreter::tok_isVariable:
        {
            int identNameIndex = (int)(progCnt.pVar->identNameIndex);
            uint8_t varQualifier = progCnt.pVar->identInfo;
            bool isUserVar = (progCnt.pVar->identInfo & _pInterpreter->var_scopeMask) == _pInterpreter->var_isUser;
            char* identifierName = isUserVar ? _pInterpreter->userVarNames[identNameIndex] : _pInterpreter->programVarNames[identNameIndex];
            strcpy(prettyToken, identifierName);
            testNextForPostfix = true;
            break;
        }

        case Interpreter::tok_isConstant:
        {
            char valueType = (*progCnt.pTokenChars >> 4) & Interpreter::value_typeMask;
            bool isLongConst = (valueType == Interpreter::value_isLong);
            bool isFloatConst = (valueType == Interpreter::value_isFloat);
            bool isStringConst = (valueType == Interpreter::value_isStringPointer);

            if (isLongConst) {
                long  l;
                memcpy(&l, progCnt.pCstToken->cstValue.longConst, sizeof(l));                         // pointer not necessarily aligned with word size: copy memory instead
                sprintf(prettyToken, "%ld", l);
                testNextForPostfix = true;
                break;   // and quit switch
            }

            else if (isFloatConst) {
                float f;
                memcpy(&f, progCnt.pCstToken->cstValue.floatConst, sizeof(f));                         // pointer not necessarily aligned with word size: copy memory instead
                sprintf(prettyToken, "%#.3G", f);
                testNextForPostfix = true;
                break;   // and quit switch
            }

            else { testNextForPostfix = true; }     // no break here: fall into generic name handling
        }

        case Interpreter::tok_isGenericName:
        {
            char* pAnum{ nullptr };
            memcpy(&pAnum, progCnt.pCstToken->cstValue.pStringConst, sizeof(pAnum));                         // pointer not necessarily aligned with word size: copy memory instead
            sprintf(prettyToken, (testNextForPostfix ? "\"%s\"" : "%s "), (pAnum == nullptr) ? "" : pAnum);
            hasTrailingSpace = !testNextForPostfix;

            break;
        }

        default:  // terminal
        {
            int index = (progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F;
            index += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
            char trailing[2] = "\0";      // init: empty string

            if (_terminals[index].terminalCode <= termcod_opRangeEnd) {      // operator 
                isPostfixOperator = testForPostfix ? (_terminals[index].postfix_priority != 0) : false;

                isInfixOperator = lastWasInfixOperator ? false : testForPostfix ? !isPostfixOperator : false;

                if (lastWasPostfixOperator && isPostfixOperator) {             // check if operator is postfix operator 
                    strcat(prettyToken, " ");          // leading space
                    tokenHasLeadingSpace = true;
                }

                if (!isPostfixOperator && !lastHasTrailingSpace) {             // check if operator is postfix operator 
                    strcat(prettyToken, " ");          // leading space
                    tokenHasLeadingSpace = true;
                }

                if ((isInfixOperator)) {
                    trailing[0] = ' ';      // single space (already terminated by '\0')
                    hasTrailingSpace = true;
                }

                testNextForPostfix = isPostfixOperator;
            }

            else if (_terminals[index].terminalCode == termcod_rightPar) {
                testNextForPostfix = true;
            }

            else if (_terminals[index].terminalCode == termcod_leftPar) {
                hasTrailingSpace = true;
                testNextForPostfix = false;
            }

            else if ((_terminals[index].terminalCode == termcod_comma) || (_terminals[index].terminalCode == termcod_semicolon)) {
                testNextForPostfix = false;
                trailing[0] = ' ';      // single space (already terminated by '\0')
                hasTrailingSpace = true;
            }

            strcat(prettyToken, _terminals[index].terminalName);         // concatenate with empty string or single-space string
            strcat(prettyToken, trailing);
            isSemicolon = (_terminals[index].terminalCode == termcod_semicolon);
            break; }
        }


        // print pretty token
        // ------------------

        int tokenSourceLength = strlen(prettyToken);
        if (isSemicolon) {
            if ((nextTokenType != Interpreter::tok_no_token) && allInstructions) { _pInterpreter->_pConsole->print(prettyToken); }
        }

        else { _pInterpreter->_pConsole->print(prettyToken); }              // not a semicolon

        // if printing one instruction, return output error position based on token where execution error was produced
        if (!allInstructions) {
            if (errorProgCounter == progCnt.pTokenChars) {
                *sourceErrorPos = outputLength + (tokenHasLeadingSpace ? 1 : 0);
            }
            if (isSemicolon) {
                if (--instructionCount == 0) { break; }     // all statements printed
                else if (!isFirstInstruction) { _pInterpreter->_pConsole->print("; "); }
            }
            outputLength += tokenSourceLength;
        }


        // advance to next token
        // ---------------------

        progCnt.pTokenChars = nextProgCnt.pTokenChars;
        lastTokenType = tokenType;
        tokenType = nextTokenType;                                                     // next token type
        testForPostfix = testNextForPostfix;
        lastHasTrailingSpace = hasTrailingSpace;
        lastWasInfixOperator = isInfixOperator;
        lastWasPostfixOperator = isPostfixOperator;

        isFirstInstruction = false;
    }

    // exit
    _pInterpreter->_pConsole->println(); _pInterpreter->_isPrompt = false;

}


// ----------------------------
// *   print parsing result   *
// ----------------------------

void MyParser::printParsingResult(parseTokenResult_type result, int funcNotDefIndex, char* const pInstruction, int lineCount, char* const pErrorPos) {
    char parsingInfo[_pInterpreter->_maxInstructionChars];
    if (result == result_tokenFound) {                                                // prepare message with parsing result
        strcpy(parsingInfo, _pInterpreter->_programMode ? "Program parsed without errors" : "");
    }

    else  if ((result == result_undefinedFunctionOrArray) && _pInterpreter->_programMode) {     // in program mode only 
        // during external function call parsing, it is not always known whether the function exists (because function can be defined after a call) 
        // -> a linenumber can not be given, but the undefined function can
        sprintf(parsingInfo, "\r\n  Parsing error %d: function: %s", result, _pInterpreter->extFunctionNames[funcNotDefIndex]);
    }

    else {                                                                              // parsing error
        // instruction not parsed (because of error): print source instruction where error is located (can not 'unparse' yet for printing instruction)
        char point[pErrorPos - pInstruction + 3];                               // 2 extra positions for 2 leading spaces, 2 for '^' and '\0' characters
        memset(point, ' ', pErrorPos - pInstruction + 2);
        point[pErrorPos - pInstruction + 2] = '^';
        point[pErrorPos - pInstruction + 3] = '\0';

        _pInterpreter->_pConsole->print("\r\n  "); _pInterpreter->_pConsole->println(pInstruction);
        _pInterpreter->_pConsole->println(point);
        if (_pInterpreter->_programMode) { sprintf(parsingInfo, "  Parsing error %d: statement ending at line %d", result, lineCount); }
        else { sprintf(parsingInfo, "  Parsing error %d", result); }
    }

    if (strlen(parsingInfo) > 0) { _pInterpreter->_pConsole->println(parsingInfo); _pInterpreter->_isPrompt = false; }
};