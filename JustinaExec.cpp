#include "Justina.h"

#define printCreateDeleteHeapObjects 0

// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Interpreter::varBaseAddress( TokenIsVariable* pVarToken, char& varType, char& isArray ) {

    // pVarToken token argument must be a variable reference token
    // upon return, valueType and isArray will contain current variable type (float or string; which is fixed for arrays) and array flag, respectively
    // return pointer will point to variable base address

    int varNameIndex = pVarToken->identNameIndex;
    uint8_t varQualifier = pVarToken->identInfo & ~Interpreter::var_isArray;

    isArray = (char) pVarToken->identInfo & Interpreter::var_isArray;
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

    void* pArray = varBaseAddress;                                                                        // will point to float or string pointer (both can be array elements)
    int arrayDimCount = ((char*) pArray) [3];

    int arrayElement { 0 };
    for ( int i = 0; i < arrayDimCount; i++ ) {
        int arrayDim = ((char*) pArray) [i];
        if ( (elemSpec [i] < 1) || (elemSpec [i] > arrayDim) ) { return nullptr; }                                             // outside array boundaries

        int arrayNextDim = (i < arrayDimCount - 1) ? ((char*) pArray) [i + 1] : 1;
        arrayElement = (arrayElement + (elemSpec [i] - 1)) * arrayNextDim;
    }
    arrayElement++;                                                                                                    // add one (
    return (float*) pArray + arrayElement;                                                           // pointer to 4-byte elements
}


// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------

Interpreter::execResult_type  Interpreter::exec() {

    _programCounter = _programStart;
    _calcStackLvl = 0;
    int tokenLength { 0 };
    char* pPendingStep { 0 };

    int tokenType = *_programCounter & 0x0F;

    while ( tokenType != tok_no_token ) {                                                                    // for all tokens in token list
        uint16_t tokenStep = (uint16_t) (_programCounter - _programStorage);
        tokenLength = (tokenType >= Interpreter::tok_isOperator) ? 1 : (*_programCounter >> 4) & 0x0F;        // fetch next token 
        pPendingStep = _programCounter + tokenLength;

        // defined outside case labels
        bool previousOpHasPriority = false;
        int pendingTokenType;
        int pendingTokenIndex;
        int pendingTokenPriority;
        bool op1real;
        bool op2real;
        Val operand1, operand2, result;
        char* pOp1string, pOp2string;

        switch ( tokenType ) {
        case tok_isReservedWord:
            // compile time statements VAR, LOCAL, STATIC: skip

            if ( !pushResWord( tokenType ) );
            break;

        case tok_isInternFunction:
        case tok_isExternFunction:
            if ( !pushFunctionName( tokenType ) );
            break;

        case tok_isRealConst:
        case tok_isStringConst:
        case tok_isVariable:
            // push to stack
            if ( tokenType == tok_isVariable ) { pushVariable( tokenType ); }
            else { pushConstant( tokenType ); }

            // check if an operation can be executed
            if ( _calcStackLvl >= 3 ) {                     // a previous operand and operator might exist
                // check pending token (always present and always a terminal token after a variable or constant token)
                // pending token can any terminal token: operator, left or right parenthesis, comma or semicolon 
                pendingTokenType = *pPendingStep & 0x0F;          // there's always minimum one token pending (even if it is a semicolon)
                pendingTokenIndex = (*pPendingStep >> 4) & 0x0F;        // terminal token only: index stored in high 4 bits of token type 
                pendingTokenPriority = MyParser::operatorPriority [pendingTokenIndex];        // terminal token only: index stored in high 4 bits of token type 

                if ( _pCalcStackMinus1->terminal.tokenType == tok_isOperator ) {
                    // if a pending operator has higher priority, or, it has equal priority and operator is right-to-left associative, do not execute operator yet 
                    previousOpHasPriority = (_pCalcStackMinus1->terminal.priority >= pendingTokenPriority);
                    if ( (_pCalcStackMinus1->terminal.associativity == 1) && (_pCalcStackMinus1->terminal.priority == pendingTokenPriority) ) { previousOpHasPriority = false; }

                    // execute operation if available and allowed (priority and associativity with next)
                    if ( previousOpHasPriority ) {
                        op1real = (_pCalcStackMinus2->varOrConst.valueType == var_isFloat);
                        op2real = (_pCalcStackTop->varOrConst.valueType == var_isFloat);

                        if ( op1real ) { operand1.realConst = (_pCalcStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.pRealConst) : _pCalcStackMinus2->varOrConst.value.realConst; }
                        else { operand1.pStringConst = (tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.ppStringConst) : _pCalcStackMinus2->varOrConst.value.pStringConst; }
                        if ( op2real ) { operand2.realConst = (tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.pRealConst) : _pCalcStackTop->varOrConst.value.realConst; }
                        else { operand2.pStringConst = (tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.ppStringConst) : _pCalcStackTop->varOrConst.value.pStringConst; }

                        // perform operation //// :<>=+-*/^
                        switch ( _pCalcStackMinus1->terminal.index ) {
                        case 2:
                            result.realConst = operand2.realConst;                          // assignment
                            break;
                        case 3:
                            result.realConst = operand1.realConst < operand2.realConst;
                            break;
                        case 4:
                            result.realConst = operand1.realConst > operand2.realConst;
                            break;
                        case 5:
                            result.realConst = operand1.realConst == operand2.realConst;     // equality
                            break;
                        case 6:
                            result.realConst = operand1.realConst + operand2.realConst;
                            break;
                        case 7:
                            result.realConst = operand1.realConst - operand2.realConst;
                            break;
                        case 8:
                            result.realConst = operand1.realConst * operand2.realConst;
                            break;
                        case 9:
                            result.realConst = operand1.realConst / operand2.realConst;
                            break;
                        case 10:
                            result.realConst = pow(operand1.realConst,  operand2.realConst);
                            break;
                        case 13:
                            result.realConst = operand1.realConst <= operand2.realConst;
                            break;
                        case 14:
                            result.realConst = operand1.realConst >= operand2.realConst;
                            break;
                        case 15:
                            result.realConst = operand1.realConst != operand2.realConst;
                            break;


                        default:
                            break;
                        }
                        Serial.print( "++++++++++ nan   : " ); Serial.println( isnan(result.realConst) );
                        Serial.print( "++++++++++ oper 1: " ); Serial.println( operand1.realConst );
                        Serial.print( "++++++++++ oper 2: " ); Serial.println( operand2.realConst );
                        Serial.print( "++++++++++ result: " ); Serial.println( result.realConst );

                        //// handle string; temp. string delete; store result in stack (replace operand 1); remove 2 stack levels (operator and operand 2); move higher stack levels two positions (needed ???) 
                    }
                }
            }
            break;

        case tok_isGenericName:

            break;

        default:
            if ( !PushTerminalToken( tokenType ) );
            break;


        }

        _programCounter = pPendingStep;
        tokenType = *_programCounter & 0x0F;                                                     // next token type

    }
};


bool Interpreter::pushResWord( int& tokenType ) {
    // push reserved word to stack
    _flowCtrlStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pFlowCtrlStack = (LE_flowControlStack*) flowCtrlStack.appendListElement( sizeof( *_pFlowCtrlStack ) );
    _pFlowCtrlStack->tokenType = tok_isReservedWord;
    _pFlowCtrlStack->index = ((TokenIsResWord*) _programCounter)->tokenIndex;

    int toTokenStep { 0 };                         // needed because internally 4 bytes are used and high bytes need to be set to zero
    memcpy( &toTokenStep, ((TokenIsResWord*) (_programCounter))->toTokenStep, sizeof( char [2] ) );
    _pFlowCtrlStack->pToNextToken = _programStorage + toTokenStep;

    Serial.print( ((char*) (&toTokenStep)) [0], HEX ); Serial.print( " " );
    Serial.println( ((char*) (&toTokenStep)) [1], HEX );
    Serial.print( "next token step: " ); Serial.println( toTokenStep );

    int fIndex = (int) _pFlowCtrlStack->index;
    Serial.println( _pmyParser->_resWords [fIndex]._resWordName );

};


bool Interpreter::PushTerminalToken( int& tokenType ) {
    // push internal or external function index to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->terminal ) );
    _pCalcStackTop->terminal.tokenType = tokenType;
    _pCalcStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;        // terminal token only: index stored in high 4 bits of token type 
    _pCalcStackTop->terminal.priority = MyParser::operatorPriority [_pCalcStackTop->terminal.index];        // terminal token only: index stored in high 4 bits of token type 
    _pCalcStackTop->terminal.associativity = MyParser::operatorAssociativity [_pCalcStackTop->terminal.index];        // terminal token only: index stored in high 4 bits of token type 
};


bool Interpreter::pushFunctionName( int& tokenType ) {
    // push internal or external function index to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->function ) );
    _pCalcStackTop->function.tokenType = tokenType;
    _pCalcStackTop->function.index = ((TokenIsIntFunction*) _programCounter)->tokenIndex;

    if ( tokenType == tok_isInternFunction ) {
        int fIndex = (int) _pCalcStackTop->function.index;
        Serial.println( _pmyParser->_functions [fIndex].funcName );
    }
    else {
        Serial.println( extFunctionNames [_pCalcStackTop->function.index] );
    }
};


bool Interpreter::pushConstant( int& tokenType ) {
    // push real or string constant, variable type and array flag (false) to stack

    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->varOrConst ) );
    _pCalcStackTop->varOrConst.tokenType = tokenType;

    if ( tokenType == tok_isRealConst ) {
        float f;
        memcpy( &f, ((TokenIsRealCst*) _programCounter)->realConst, sizeof( float ) );        // copy float (boundary alignment)
        _pCalcStackTop->varOrConst.value.realConst = f;                                          // store float in stack, NOT the pointer to float (boundary alignment)
    }
    else {
        char* pAnum;
        memcpy( &pAnum, ((TokenIsStringCst*) _programCounter)->pStringConst, sizeof( pAnum ) );      // copy char* (boundary alignment)
        _pCalcStackTop->varOrConst.value.pStringConst = pAnum;                                            // store char* in stack, NOT the pointer to float (boundary alignment)
    }

    _pCalcStackTop->varOrConst.valueType = (tokenType == tok_isRealConst) ? var_isFloat : var_isStringPointer;
    _pCalcStackTop->varOrConst.isArray = 0;
    _pCalcStackTop->varOrConst.isIntermediateResult = 0;
};


bool Interpreter::pushVariable( int& tokenType ) {
    // push variable base address, variable type (real, string) and array flag to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->varOrConst ) );
    _pCalcStackTop->varOrConst.tokenType = tokenType;
    void* varAddress = varBaseAddress( (TokenIsVariable*) _programCounter, _pCalcStackTop->varOrConst.valueType, _pCalcStackTop->varOrConst.isArray );
    _pCalcStackTop->varOrConst.value.pVariable = varAddress;


    if ( _pCalcStackTop->varOrConst.isArray ) {
        void* pArray = *_pCalcStackTop->varOrConst.value.ppArray;
        int elemSpec [4] = { 1,1,1,1 };
        void* pArrayElem = arrayElemAddress( pArray, elemSpec );

        if ( _pCalcStackTop->varOrConst.valueType == var_isFloat ) { Serial.println( ((float*) pArrayElem) [1] ); }
        else if ( _pCalcStackTop->varOrConst.valueType == var_isStringPointer ) { Serial.println( ((char**) pArrayElem) [1] ); }
    }
    else if ( _pCalcStackTop->varOrConst.valueType == var_isFloat ) { Serial.println( *_pCalcStackTop->varOrConst.value.pRealConst ); }
    else if ( _pCalcStackTop->varOrConst.valueType == var_isStringPointer ) { Serial.println( *_pCalcStackTop->varOrConst.value.ppStringConst ); }
}



