#include "Justina.h"

#define printCreateDeleteHeapObjects 0

// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Interpreter::varBaseAddress( TokenIsVariable* pVarToken, uint8_t& varType, bool& isArray ) {

    // pVarToken token argument must be a variable reference token
    // upon return, varType and isArray will contain current variable type (float or string; which is fixed for arrays) and array flag, respectively
    // return pointer will point to variable base address

    int varNameIndex = pVarToken->identNameIndex;
    uint8_t varQualifier = pVarToken->identInfo & ~Interpreter::var_isArray;

    isArray = pVarToken->identInfo & Interpreter::var_isArray;
    bool isUserVar = (varQualifier == var_isUser);
    bool isGlobalVar = (varQualifier == var_isGlobal);
    bool isStaticVar = (varQualifier == var_isStaticInFunc);
    bool isLocalVar = (varQualifier == var_isLocalInFunc);            // but not function parameter definitions

    int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : programVarValueIndex [varNameIndex];

    if ( isUserVar ) {
        varType = userVarType [valueIndex] & var_typeMask;
        return &userVarValues [valueIndex];             // pointer to float, pointer to pointer to array or pointer to pointer to string
    }
    else if ( isGlobalVar ) {
        varType = globalVarType [valueIndex] & var_typeMask;
        return &globalVarValues [valueIndex];
    }
    else if ( isStaticVar ) {
        varType = staticVarType [valueIndex] & var_typeMask;
        return &staticVarValues [valueIndex];
    }
}



// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------


void* Interpreter::arrayElemAddress( void* varBaseAddress, int* elemSpec ) {

    // varBaseAddress argument must be base address of an array variable
    // elemSpec array must specify array element (max. 3 dimensions)
    // return pointer will point to float or string pointer (both can be array elements) - nullptr if outside boundaries

    void* pArray =  varBaseAddress;                                                                        // will point to float or string pointer (both can be array elements)
    int arrayDimCount = ((char*) pArray) [3];

    int arrayElement { 0 };
    for ( int i = 0; i < arrayDimCount; i++ ) {
        int arrayDim = ((char*) pArray) [i];
        if ( (elemSpec [i] < 1) || (elemSpec [i] > arrayDim) ) { return nullptr; }                                             // outside array boundaries

        int arrayNextDim = (i < arrayDimCount - 1) ? ((char*) pArray) [i + 1] : 1;
        arrayElement = (arrayElement + (elemSpec [i] - 1)) * arrayNextDim;
    }
    arrayElement++;                                                                                                    // add one (
    return (float*)pArray + arrayElement;                                                           // pointer to 4-byte elements
}


// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------

Interpreter::execResult_type  Interpreter::exec() {

    TokPnt prgmCnt;
    _programCounter = _programStart;
    int tokenType = *_programCounter & 0x0F;

    while ( tokenType != tok_no_token ) {                                                                    // for all tokens in token list
        uint16_t tokenStep = (uint16_t) (_programCounter - _programStorage);

        switch ( tokenType ) {
        case Interpreter::tok_isReservedWord:
            if ( !execResWord() );
            break;

        case Interpreter::tok_isInternFunction:
            if ( !execInternFunction() );
            break;

        case Interpreter::tok_isExternFunction:
            if ( !execExternFunction() );
            break;

        case Interpreter::tok_isVariable:
            if ( !execVariable() );
            break;

        case Interpreter::tok_isRealConst:
            if ( !execNumber() );
            break;

        case Interpreter::tok_isStringConst:
            if ( !execStringConstant() );
            break;

        case Interpreter::tok_isGenericName:
            if ( !execIdentifierName() );
            break;

        default:
            if ( !execTerminalToken() );
            break;


        }

        int tokenLength = (tokenType >= Interpreter::tok_isOperator) ? 1 : (*_programCounter >> 4) & 0x0F;        // fetch next token 
        _programCounter += tokenLength;
        tokenType = *_programCounter & 0x0F;                                                     // next token type

    }
};
bool Interpreter::execResWord() {

};
bool Interpreter::execNumber() {
    // store numeric constant, variable type (real) and array flag (false) in stack
    float f;

    _execStackLvl++;
    _pExecStackLvl = (LE_execStack*) execStack.appendListElement( sizeof( _pExecStackLvl->varData ) );
    memcpy( &f, ((TokenIsRealCst*) _programCounter)->realConst, sizeof( float ) );        // copy float (boundary alignment)
    _pExecStackLvl->varData.value.realConst = f;                                          // store float in stack, NOT the pointer to float (boundary alignment)
    _pExecStackLvl->varData.varType = var_isFloat;
    _pExecStackLvl->varData.isArray = false;
};

bool Interpreter::execStringConstant() {
    // store constant string pointer, variable type (string) and array flag (false) in stack
    char* pAnum;

    _execStackLvl++;
    _pExecStackLvl = (LE_execStack*) execStack.appendListElement( sizeof( _pExecStackLvl->varData ) );
    memcpy( &pAnum, ((TokenIsStringCst*) _programCounter)->pStringConst, sizeof( pAnum ) );      // copy char* (boundary alignment)
    _pExecStackLvl->varData.value.pStringConst = pAnum;                                            // store char* in stack, NOT the pointer to float (boundary alignment)
    _pExecStackLvl->varData.varType = var_isStringPointer;
    _pExecStackLvl->varData.isArray = false;
};

bool Interpreter::execTerminalToken() {

};

bool Interpreter::execInternFunction() {

};

bool Interpreter::execExternFunction() {

};

bool Interpreter::execVariable() {
    // store variable base address, variable type (real, string) and array flag in stack
    _execStackLvl++;
    _pExecStackLvl = (LE_execStack*) execStack.appendListElement( sizeof( _pExecStackLvl->varData ) );
    void* varAddress = varBaseAddress( (TokenIsVariable*) _programCounter, _pExecStackLvl->varData.varType, _pExecStackLvl->varData.isArray );
    _pExecStackLvl->varData.value.pVarBaseAddress = varAddress;

    
    if ( _pExecStackLvl->varData.isArray ) {
        void*  pArray = *_pExecStackLvl->varData.value.ppArray;
        int elemSpec[4] ={1,1,1,1};
        void* pArrayElem = arrayElemAddress(pArray, elemSpec);

        if ( _pExecStackLvl->varData.varType == var_isFloat ) { Serial.println( ((float*) pArrayElem)[1]); }
        else if ( _pExecStackLvl->varData.varType == var_isStringPointer ) { Serial.println( ((char**) pArrayElem) [1] ); }
    }
    else if ( _pExecStackLvl->varData.varType == var_isFloat ) { Serial.println( *_pExecStackLvl->varData.value.pRealConst ); }
    else if ( _pExecStackLvl->varData.varType == var_isStringPointer ) { Serial.println( *_pExecStackLvl->varData.value.ppStringConst ); }
}


bool Interpreter::execIdentifierName() {

};

