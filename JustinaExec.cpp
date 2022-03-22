#include "Justina.h"

#define printCreateDeleteHeapObjects 0
#define debugPrint 0
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


// ----------------------------------------------------------
// *   put last calculation result (if available) in FIFO   *
// ----------------------------------------------------------

bool Interpreter::pushLastCalcResultToFIFO() {

    if ( _lastResultCount == MAX_RESULT_DEPTH );

    if ( (_pCalcStackTop->varOrConst.isIntermediateResult == 0x01) && (_pCalcStackTop->varOrConst.valueType == var_isStringPointer) )
    {
        Serial.print( "delete operand 2 interm.cst string: " ); Serial.println( _pCalcStackTop->varOrConst.value.pStringConst );
        delete [] _pCalcStackTop->varOrConst.value.pStringConst;
    }

}


// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------

Interpreter::execResult_type  Interpreter::exec() {

    int tokenType = *_programStart & 0x0F;
    int tokenLength { 0 };
    char* pPendingStep { 0 };
    bool saveLastResult = false;
    execResult_type execResult = result_execOK;             // init

    // init
    _programCounter = _programStart;
    _calcStackLvl = 0;
    _pCalcStackTop = nullptr;
    _pCalcStackMinus2 = nullptr;
    _pCalcStackMinus1 = nullptr;


    while ( tokenType != tok_no_token ) {                                                                    // for all tokens in token list
        uint16_t tokenStep = (uint16_t) (_programCounter - _programStorage);
        tokenLength = (tokenType >= Interpreter::tok_isOperator) ? 1 : (*_programCounter >> 4) & 0x0F;        // fetch next token 
        pPendingStep = _programCounter + tokenLength;
        int pendingTokenType = *pPendingStep & 0x0F;          // there's always minimum one token pending (even if it is a semicolon);

        // defined outside switch statement
        bool currentOpHasPriority;
        int pendingTokenLength;
        int pendingTokenIndex;
        int pendingTokenPriority;
        bool op1real, op2real, opResultReal;
        Val operand1, operand2, opResult;
        char* pOp1string, pOp2string;
        bool skipStatement;
        int stringlen;
        char* pOpResultString;
        int stackLvl;
        LE_calcStack* pstackLvl;

#if debugPrint 
        Serial.print( ">> loop: stack level " ); Serial.println( _calcStackLvl );

#endif

        switch ( tokenType ) {
        case tok_isReservedWord:
            // compile time statements VAR, LOCAL, STATIC: skip
#if debugPrint 
            Serial.println( "-- loop: is reserved word " );
#endif

            pendingTokenIndex = ((TokenIsResWord*) _programCounter)->tokenIndex;
            skipStatement = ((_pmyParser->_resWords [pendingTokenIndex].restrictions & MyParser::cmd_skipDuringExec) != 0);
            if ( skipStatement ) {
                while ( pendingTokenType != tok_isSemiColonSeparator ) {
                    // move to next token
                    pendingTokenLength = (pendingTokenType >= Interpreter::tok_isOperator) ? 1 : (*pPendingStep >> 4) & 0x0F;
                    pPendingStep = pPendingStep + pendingTokenLength;
                    pendingTokenType = *pPendingStep & 0x0F;          // there's always minimum one token pending (even if it is a semicolon)
                };
            }


            ////pushResWord( tokenType );//// enkel indien block start command, ...
            break;

        case tok_isInternFunction:
        case tok_isExternFunction:
            saveLastResult = true;
            pushFunctionName( tokenType );
            break;






        case tok_isRealConst:
        case tok_isStringConst:
        case tok_isVariable:

            // push to stack
            if ( tokenType == tok_isVariable ) { pushVariable( tokenType ); }
            else { pushConstant( tokenType ); }

            // set last result to this value, in case the expression does not contain any operation or function to execute (this value only) 
            saveLastResult = true;

            // check if an operation can be executed
            while (_calcStackLvl >= 3) {                     // a previous operand and operator might exist
#if debugPrint 
                Serial.println( "-- loop: constant or variable" );
#endif
                if ( _pCalcStackMinus1->terminal.tokenType == tok_isOperator ) {
#if debugPrint 
                    Serial.print( "-- loop: will execute operator, index " ); Serial.println( (int) _pCalcStackMinus1->terminal.index );
#endif
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

                    if ( (_pCalcStackMinus1->terminal.index == 6) && (op1real || op2real) ) { execResult = result_stringExpected; return execResult; }
                    else if ( (_pCalcStackMinus1->terminal.index != 6) && (!op1real || !op2real) ) { execResult = result_numberExpected; return execResult; }
                    opResultReal = op1real;             // because mixed operands not allowed; 2 x real -> real; 2 x string -> string

                    if ( op1real ) { operand1.realConst = (_pCalcStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.pRealConst) : _pCalcStackMinus2->varOrConst.value.realConst; }
                    else { operand1.pStringConst = (_pCalcStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.ppStringConst) : _pCalcStackMinus2->varOrConst.value.pStringConst; }
                    if ( op2real ) { operand2.realConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.pRealConst) : _pCalcStackTop->varOrConst.value.realConst; }
                    else { operand2.pStringConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.ppStringConst) : _pCalcStackTop->varOrConst.value.pStringConst; }

                    switch ( _pCalcStackMinus1->terminal.index ) {
                    case 2:                                                                     // assignment (only possible if first operand is a variable: checked during parsing)
                        opResult.realConst = operand2.realConst;
                        *_pCalcStackMinus2->varOrConst.value.pRealConst = opResult.realConst;  // store in variable (or array element)
                        *_pCalcStackMinus2->varOrConst.varTypeAddress = (*_pCalcStackMinus2->varOrConst.varTypeAddress & ~var_typeMask) | var_isFloat;   // adapt variable type 
                        break;
                    case 3:
                        opResult.realConst = operand1.realConst < operand2.realConst;
                        break;
                    case 4:
                        opResult.realConst = operand1.realConst > operand2.realConst;
                        break;
                    case 5:
                        opResult.realConst = operand1.realConst == operand2.realConst;
                        break;
                    case 6:
                        stringlen = strlen( operand1.pStringConst ) + strlen( operand2.pStringConst );
                        opResult.pStringConst = new char [stringlen + 1];
                        strcpy( opResult.pStringConst, operand1.pStringConst );
                        strcat( opResult.pStringConst, operand2.pStringConst );
                        Serial.print( "Create string object: " ); Serial.println( opResult.pStringConst );
                        break;
                    case 7:
                        opResult.realConst = operand1.realConst + operand2.realConst;
                        break;
                    case 8:
                        opResult.realConst = operand1.realConst - operand2.realConst;
                        break;
                    case 9:
                        opResult.realConst = operand1.realConst * operand2.realConst;
                        break;
                    case 10:
                        opResult.realConst = operand1.realConst / operand2.realConst;
                        break;
                    case 11:
                        opResult.realConst = pow( operand1.realConst, operand2.realConst );
                        break;
                    case 14:
                        opResult.realConst = operand1.realConst <= operand2.realConst;
                        break;
                    case 15:
                        opResult.realConst = operand1.realConst >= operand2.realConst;
                        break;
                    case 16:
                        opResult.realConst = operand1.realConst != operand2.realConst;
                        break;


                    default:
                        break;
                    }

                    // operand 2 is an intermediate constant AND it is a string ? delete char string object
                    if ( (_pCalcStackTop->varOrConst.isIntermediateResult == 0x01) && (_pCalcStackTop->varOrConst.valueType == var_isStringPointer) )
                    {
                        Serial.print( "delete operand 2 interm.cst string: " ); Serial.println( _pCalcStackTop->varOrConst.value.pStringConst );
                        delete [] _pCalcStackTop->varOrConst.value.pStringConst;
                    }
                    // operand 1 is an intermediate constant AND it is a string ? delete char string object
                    if ( (_pCalcStackMinus2->varOrConst.isIntermediateResult == 0x01) && (_pCalcStackMinus2->varOrConst.valueType == var_isStringPointer) )
                    {
                        Serial.print( "delete operand 1 interm.cst string: " ); Serial.println( _pCalcStackMinus2->varOrConst.value.pStringConst );
                        delete [] _pCalcStackMinus2->varOrConst.value.pStringConst;
                    }

                    // drop highest 2 stack levels( operator and operand 2 ) 
                    execStack.deleteListElement( _pCalcStackTop );                          // operand 2 
                    execStack.deleteListElement( _pCalcStackMinus1 );                       // operator
                    _pCalcStackTop = _pCalcStackMinus2;
                    _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
                    _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );
                    _calcStackLvl -= 2;


                    // store result in stack (replaces operand 1)
                    _pCalcStackTop->varOrConst.value = opResult;                           // number or string
                    _pCalcStackTop->varOrConst.tokenType = tok_isConstant;                      // use generic constant type
                    _pCalcStackTop->varOrConst.valueType = opResultReal ? var_isFloat : var_isStringPointer;
                    _pCalcStackTop->varOrConst.arrayAttributes = 0x00;                  // is a scalar constant
                    _pCalcStackTop->varOrConst.isIntermediateResult = 0x01;             // is an intermediate result (intermediate constant strings must be deleted)

                    //// handle assignment =OK=, store result in stack (replace operand 1) =OK=; remove 2 upper stack levels (operator and operand 2) =OK=
                    //// string & temp. string delete **NOK**; pending operator is left parenthesis: var is array: HOLD operator execution. Right parenthesis, comma, semicolon: exec. operator  **NOK**
                }
            }
            break;

        case tok_isOperator:
            PushTerminalToken( tokenType );
            break;



        case tok_isSemiColonSeparator:
            // store last result 
#if debugPrint 
            Serial.println( "-- loop: is semicolon " );
#endif



            if ( _calcStackLvl > 0 ) {
                if ( saveLastResult ) {

                    if ( (_lastCalcResult.isIntermediateResult) && (_lastCalcResult.valueType == var_isStringPointer) )   // a lsat result exists already, and it's a string: delete
                    {
                        delete [] _lastCalcResult.value.pStringConst;
                    }

                    _lastCalcResult.valueType = _pCalcStackTop->varOrConst.valueType;
                    _lastCalcResult.tokenType = tok_isConstant;
                    if ( _lastCalcResult.valueType == var_isFloat ) { _lastCalcResult.value.realConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.pRealConst) : _pCalcStackTop->varOrConst.value.realConst; }
                    else if ( _lastCalcResult.valueType == var_isStringPointer ) { _lastCalcResult.value.pStringConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.ppStringConst) : _pCalcStackTop->varOrConst.value.pStringConst; }

                    _lastCalcResult.isIntermediateResult = _pCalcStackTop->varOrConst.isIntermediateResult;
                }

            }


            // delete intermediate constant strings
            pstackLvl = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop ); // if stack top level is string: keep (saved as last value)
            while ( pstackLvl != nullptr ) {
                if ( (pstackLvl->varOrConst.isIntermediateResult == 0x01) && (pstackLvl->varOrConst.valueType == var_isStringPointer) )
                {
                    Serial.print( "delete remaining interm.cst string: " ); Serial.println( pstackLvl->varOrConst.value.pStringConst );
                    delete [] pstackLvl->varOrConst.value.pStringConst;
                }


                pstackLvl = (LE_calcStack*) execStack.getPrevListElement( pstackLvl );
            }

            // drop remaining stack levels( result ) 
            execStack.deleteList();
            _pCalcStackTop = nullptr;
            _pCalcStackMinus1 = nullptr;
            _pCalcStackMinus2 = nullptr;
            _calcStackLvl = 0;

            break;

        case tok_isLeftParenthesis:
#if debugPrint 
            Serial.println( "-- loop: is left parenthesis " );
#endif
            PushTerminalToken( tokenType );
            break;

        case tok_isCommaSeparator:
#if debugPrint 
            Serial.println( "-- loop: is comma " );
#endif
            PushTerminalToken( tokenType );
            break;

        case tok_isRightParenthesis:

            break;

        default:
            break;


        }

        _programCounter = pPendingStep;
        tokenType = *_programCounter & 0x0F;                                                     // next token type
    }

    // normal end: store last value in last results FIFO


    return execResult;
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
    _pCalcStackTop->varOrConst.tokenType = tok_isConstant;          // use generic constant type

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
    _pCalcStackTop->varOrConst.isIntermediateResult = 0;

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



