#include "Justina.h"

#define printCreateDeleteHeapObjects 0

// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Interpreter::varBaseAddress( TokenIsVariable* pVarToken, char*& varTypeAddress, char& varType, char& variableAttributes ) {

    // pVarToken token argument must be a variable reference token
    // upon return, valueType and isArray will contain current variable type (float or string; which is fixed for arrays) and array flag, respectively
    // return pointer will point to variable base address

    int varNameIndex = pVarToken->identNameIndex;
    uint8_t varQualifier = pVarToken->identInfo & ~(var_isArray | var_isArrayElement);

    variableAttributes = pVarToken->identInfo & var_isArray;      // address of scalar or base address of array (itself pointing to array start in memory) - not an array element
    bool isUserVar = (varQualifier == var_isUser);
    bool isGlobalVar = (varQualifier == var_isGlobal);
    bool isStaticVar = (varQualifier == var_isStaticInFunc);
    bool isLocalVar = (varQualifier == var_isLocalInFunc);            // but not function parameter definitions

    int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : programVarValueIndex [varNameIndex];

    if ( isUserVar ) {
        varTypeAddress = userVarType + valueIndex;
        varType = userVarType [valueIndex] & var_typeMask;
        return &userVarValues [valueIndex];             // pointer to float, pointer to pointer to array or pointer to pointer to string
    }
    else if ( isGlobalVar ) {
        varTypeAddress = globalVarType + valueIndex;
        varType = globalVarType [valueIndex] & var_typeMask;
        return &globalVarValues [valueIndex];
    }
    else if ( isStaticVar ) {
        varTypeAddress = staticVarType + valueIndex;
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
        int pendingTokenType = *pPendingStep & 0x0F;          // there's always minimum one token pending (even if it is a semicolon);

        // defined outside case labels
        bool currentOpHasPriority ;
        int pendingTokenLength;
        int pendingTokenIndex;
        int pendingTokenPriority;
        bool op1real;
        bool op2real;
        Val operand1, operand2, result;
        char* pOp1string, pOp2string;
        bool skipStatement;

        switch ( tokenType ) {
        case tok_isReservedWord:
            // compile time statements VAR, LOCAL, STATIC: skip
            
            pendingTokenIndex =((TokenIsResWord*) _programCounter)->tokenIndex;     
            skipStatement = ((_pmyParser->_resWords [pendingTokenIndex].restrictions & MyParser::cmd_skipDuringExec) != 0);
            if ( skipStatement ) {
                do {
                    // move to next token
                    pendingTokenLength = (pendingTokenType >= Interpreter::tok_isOperator) ? 1 : (*pPendingStep >> 4) & 0x0F;
                    pPendingStep = pPendingStep + pendingTokenLength;
                    pendingTokenType = *pPendingStep & 0x0F;          // there's always minimum one token pending (even if it is a semicolon)
                } while ( pendingTokenType != tok_isSemiColonSeparator );
                break;                      // switch (tokenType)
            }
            
            
            ////if ( !pushResWord( tokenType ) );
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
            while ( _calcStackLvl >= 3 ) {                     // a previous operand and operator might exist
                if ( _pCalcStackMinus1->terminal.tokenType == tok_isOperator ) {
                    // check pending token (always present and always a terminal token after a variable or constant token)
                    // pending token can any terminal token: operator, left or right parenthesis, comma or semicolon 
                    pendingTokenType = *pPendingStep & 0x0F;          // there's always minimum one token pending (even if it is a semicolon)
                    pendingTokenIndex = (*pPendingStep >> 4) & 0x0F;        // terminal token only: index stored in high 4 bits of token type 
                    pendingTokenPriority = MyParser::operatorPriority [pendingTokenIndex];        // terminal token only: index stored in high 4 bits of token type 

                        // if a pending operator has higher priority, or, it has equal priority and operator is right-to-left associative, do not execute operator yet 
                    currentOpHasPriority = (_pCalcStackMinus1->terminal.priority >= pendingTokenPriority);
                    if ( (_pCalcStackMinus1->terminal.associativity == '1') && (_pCalcStackMinus1->terminal.priority == pendingTokenPriority) ) { currentOpHasPriority = false; }

                    // execute operation if available and allowed (priority and associativity with next)
                    if ( !currentOpHasPriority ) { break; }   // exit while() loop
                    
                    op1real = (_pCalcStackMinus2->varOrConst.valueType == var_isFloat);
                    op2real = (_pCalcStackTop->varOrConst.valueType == var_isFloat);

                    if ( op1real ) { operand1.realConst = (_pCalcStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.pRealConst) : _pCalcStackMinus2->varOrConst.value.realConst; }
                    else { operand1.pStringConst = (tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.ppStringConst) : _pCalcStackMinus2->varOrConst.value.pStringConst; }
                    if ( op2real ) { operand2.realConst = (tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.pRealConst) : _pCalcStackTop->varOrConst.value.realConst; }
                    else { operand2.pStringConst = (tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.ppStringConst) : _pCalcStackTop->varOrConst.value.pStringConst; }

                    switch ( _pCalcStackMinus1->terminal.index ) {
                    case 2:
                        result.realConst = operand2.realConst;                          // assignment (only possible if first operand is a variable: checked during parsing)
                        *_pCalcStackMinus2->varOrConst.value.pRealConst = result.realConst;  // store in variable (or array element)
                        *_pCalcStackMinus2->varOrConst.varTypeAddress = (*_pCalcStackMinus2->varOrConst.varTypeAddress & ~var_typeMask) | var_isFloat;   // adapt variable type 
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
                        result.realConst = pow( operand1.realConst, operand2.realConst );
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
                    /*
                    Serial.print( "++++++++++ nan   : " ); Serial.println( isnan( result.realConst ) );
                    Serial.print( "++++++++++ oper 1: " ); Serial.println( operand1.realConst );
                    Serial.print( "++++++++++ oper 2: " ); Serial.println( operand2.realConst );
                    Serial.print( "++++++++++ result: " ); Serial.println( result.realConst );
                    */

                    // store result in stack (replaces operand 1)
                    _pCalcStackMinus2->varOrConst.value.realConst = result.realConst;
                    _pCalcStackMinus2->varOrConst.tokenType = true ? tok_isRealConst : tok_isStringConst;
                    _pCalcStackMinus2->varOrConst.valueType = true ? var_isFloat : var_isStringPointer; //// replace 'true'
                    _pCalcStackMinus2->varOrConst.arrayAttributes = 0;                  // is a constant
                    _pCalcStackMinus2->varOrConst.isIntermediateResult = 1;             // is an intermediate result (intermediate constant strings must be deleted)

                    // drop highest 2 stack levels( operator and operand 2 ) 
                    execStack.deleteListElement( _pCalcStackTop );
                    execStack.deleteListElement( _pCalcStackMinus1 );
                    _pCalcStackTop = _pCalcStackMinus2;
                    _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
                    _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );
                    _calcStackLvl -= 2;

                    /*
                    Serial.print( "calculate: top  is " ); Serial.println( (uint32_t) _pCalcStackTop - RAMSTART );
                    Serial.print( "calculate: min1 is " ); Serial.println( (uint32_t) _pCalcStackMinus1 - RAMSTART );
                    Serial.print( "calculate: min2 is " ); Serial.println( (uint32_t) _pCalcStackMinus2 - RAMSTART );
                    */

                    //// handle assignment =OK=, string & temp. string delete **NOK**; store result in stack (replace operand 1) =OK=; remove 2 upper stack levels (operator and operand 2) =OK=
                    //// pending operator is left parenthesis: var is array: HOLD operator execution. Right parenthesis, comma, semicolon: exec. operator  **NOK**
                }
            }
            break;

        case tok_isGenericName:

            break;

        default:
            pendingTokenIndex = (*_programCounter >> 4) & 0x0F;
            if(pendingTokenIndex == 1 ) {
                //// save result voor display op console (overwrite vorige)
                break;}            // semicolon
            
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

    /*
    Serial.print( ((char*) (&toTokenStep)) [0], HEX ); Serial.print( " " );
    Serial.println( ((char*) (&toTokenStep)) [1], HEX );
    Serial.print( "next token step: " ); Serial.println( toTokenStep );

    int fIndex = (int) _pFlowCtrlStack->index;
    Serial.println( _pmyParser->_resWords [fIndex]._resWordName );
    */
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

    /*
    Serial.print( "push operator: top  is " ); Serial.println( (uint32_t) _pCalcStackTop - RAMSTART );
    Serial.print( "push operator: min1 is " ); Serial.println( (uint32_t) _pCalcStackMinus1 - RAMSTART );
    Serial.print( "push operator: min2 is " ); Serial.println( (uint32_t) _pCalcStackMinus2 - RAMSTART );
    */
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
    _pCalcStackTop->varOrConst.arrayAttributes = 0;
    _pCalcStackTop->varOrConst.isIntermediateResult = 0;
    /*
    Serial.print( "push constant: top  is " ); Serial.println( (uint32_t) _pCalcStackTop - RAMSTART );
    Serial.print( "push constant: min1 is " ); Serial.println( (uint32_t) _pCalcStackMinus1 - RAMSTART );
    Serial.print( "push constant: min2 is " ); Serial.println( (uint32_t) _pCalcStackMinus2 - RAMSTART );
    */
};


bool Interpreter::pushVariable( int& tokenType ) {
    // push variable base address, variable type (real, string) and array flag to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->varOrConst ) );
    _pCalcStackTop->varOrConst.tokenType = tokenType;
    void* varAddress = varBaseAddress( (TokenIsVariable*) _programCounter, _pCalcStackTop->varOrConst.varTypeAddress, _pCalcStackTop->varOrConst.valueType, _pCalcStackTop->varOrConst.arrayAttributes );
    _pCalcStackTop->varOrConst.value.pVariable = varAddress;

    /*
    if ( _pCalcStackTop->varOrConst.arrayAttributes == var_isArray ) {   // address of scalar variable or base address of array ? (itself pointing to array start in memory) - not an array element

        //// test
        void* pArray = *_pCalcStackTop->varOrConst.value.ppArray;
        int elemSpec [4] = { 1,1,1,1 };
        void* pArrayElem = arrayElemAddress( pArray, elemSpec );

        if ( _pCalcStackTop->varOrConst.valueType == var_isFloat ) { Serial.println( ((float*) pArrayElem) [1] ); }
        else if ( _pCalcStackTop->varOrConst.valueType == var_isStringPointer ) { Serial.println( ((char**) pArrayElem) [1] ); }
    }

    else if ( _pCalcStackTop->varOrConst.valueType == var_isFloat ) { Serial.println( *_pCalcStackTop->varOrConst.value.pRealConst ); }
    else if ( _pCalcStackTop->varOrConst.valueType == var_isStringPointer ) { Serial.println( *_pCalcStackTop->varOrConst.value.ppStringConst ); }
    */
}



