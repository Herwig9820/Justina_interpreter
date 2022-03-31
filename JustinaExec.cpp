#include "Justina.h"

#define printCreateDeleteHeapObjects 0
#define debugPrint 0

// -----------------------------------
// *   execute parsed instructions   *
// -----------------------------------

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
        bool skipStatement;
        LE_calcStack* pstackLvl, * pPreviousStackLvl;
        int argCount;
        bool isLeftParenthesis;
        bool isSimpleParenthesisPair;
        Val operand;
        bool opReal;

        switch ( tokenType ) {

        case tok_isReservedWord:
            // ---------------------------------
            // Case: process reserved word token
            // ---------------------------------

            // compile time statements (VAR, LOCAL, STATIC): skip for execution
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
            // -------------------------------------------------
            // Case: process internal or external function token
            // -------------------------------------------------

            saveLastResult = true;
            pushFunctionName( tokenType );
            break;


        case tok_isRealConst:
        case tok_isStringConst:
        case tok_isVariable:
            // -----------------------------------------------------------
            // Case: process real or string constant token, variable token
            // -----------------------------------------------------------

            Serial.print( "operand: stack level " ); Serial.println( _calcStackLvl );

            // push constant value token or variable name token to stack
            if ( tokenType == tok_isVariable ) {
                pushVariable( tokenType );
                if ( pendingTokenType == tok_isLeftParenthesis ) {   // array name followed by element spec (to be processed)
                    _pCalcStackTop->varOrConst.arrayAttributes = var_isArrayNeedingElement; // not a plain array name (as function argument)
                }
            }
            else { pushConstant( tokenType ); }

            // set flag to save the current value as 'last value', in case the expression does not contain any operation or function to execute (this value only) 
            saveLastResult = true;

            // check if (an) operation(s) can be executed. 
            // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
            execAllProcessedInfixOperations( pPendingStep );
            if ( execResult != result_execOK ) { return execResult; }

            break;


        case tok_isOperator:
            // -------------------------------------------------------------------------------------------------------------------------------------
            // Case: process operator token (operator will NOT be executed yet, because operand following operator still missing (not yet processed)
            // -------------------------------------------------------------------------------------------------------------------------------------
            Serial.print( "operator: stack level " ); Serial.println( _calcStackLvl );

            PushTerminalToken( tokenType );
            break;


        case tok_isLeftParenthesis:
            // ------------------------------------
            // Case: process left parenthesis token
            // ------------------------------------
            Serial.print( "left parenthesis: stack level " ); Serial.println( _calcStackLvl );

            PushTerminalToken( tokenType );
            break;


        case tok_isCommaSeparator:
            // -----------------------------
            // Case: process comma separator 
            // -----------------------------
            break;                                              // nothing to do


        case tok_isRightParenthesis:
            // -------------------------------------
            // Case: process right parenthesis token
            // -------------------------------------

            Serial.print( "comma or right parenthesis: stack level " ); Serial.println( _calcStackLvl );

            // note: last expression may not have been evaluated at this point

            argCount = 0;                                                // init number of supplied arguments to 0 argument (empty parenthesis)
            pstackLvl = _pCalcStackTop;     // stack level of last argument before right parenthesis, or left parenthesis (if function call and no arguments supplied)
            while ( pstackLvl->genericToken.tokenType != tok_isLeftParenthesis ) {
                pstackLvl = (LE_calcStack*) execStack.getPrevListElement( pstackLvl );     // stack top level: right parenthesis
                argCount++;
            }
            pPreviousStackLvl = (LE_calcStack*) execStack.getPrevListElement( pstackLvl );     // stack level PRECEDING left parenthesis (or null pointer)

            // remove left parenthesis stack level
            pstackLvl = (LE_calcStack*) execStack.deleteListElement( pstackLvl );                            // now first argument                                                          

            ////  reeds nodig hier ? wordt na delete stack levels nog eens goed gezet
            _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );                 // correct previous stack levels (now wrong, if only one or 2 arguments)
            _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus2 );
            _calcStackLvl--;                                                                                    // left parenthesis level removed, argument(s) still there

                // perform internal or external function, calculate array element address or simply make an expression result within parentheses an intermediate constant
            if ( pPreviousStackLvl == nullptr ) { makeIntermediateConstant( _pCalcStackTop ); }                     // simple parenthesis pair
            else if ( pPreviousStackLvl->genericToken.tokenType == tok_isInternFunction ) {}
            else if ( pPreviousStackLvl->genericToken.tokenType == tok_isExternFunction ) {}
            else if ( pPreviousStackLvl->genericToken.tokenType == tok_isVariable ) {
                if ( pPreviousStackLvl->varOrConst.arrayAttributes == var_isArrayNeedingElement ) {
                    {
                        Serial.println( "array element spec" );
                        void* pArray = *pPreviousStackLvl->varOrConst.value.ppArray;
                        int elemSpec [4] = { 0 ,0,0,0 };
                        do {
                            opReal = (pstackLvl->varOrConst.valueType == var_isFloat);
                            Serial.print( "is real:" ); Serial.println( opReal );
                            if ( opReal ) {
                                elemSpec [elemSpec [3]] = (pstackLvl->varOrConst.tokenType == tok_isVariable) ? (*pstackLvl->varOrConst.value.pRealConst) : pstackLvl->varOrConst.value.realConst;
                                Serial.print( "dim " ); Serial.println( elemSpec [3] );
                                Serial.print( "elem spec " ); Serial.println( elemSpec [elemSpec [3]] );

                            }        // only used here as a counter
                            else { Serial.println( "error **************" ); return result_numberExpected; } //// eerst alle temp strings verwijderen

                            pstackLvl = (LE_calcStack*) execStack.getNextListElement( pstackLvl );
                        } while ( ++elemSpec [3] < argCount );

                        void* pArrayElem = arrayElemAddress( pArray, elemSpec );
                        if ( pArrayElem == nullptr ) { Serial.println( "error" ); }
                        pPreviousStackLvl->varOrConst.value.pVariable = pArrayElem;
                        pPreviousStackLvl->varOrConst.arrayAttributes = 0x00;       // other data does not change (value type, token type, intermediate constant, variable type address)

                        // delete intermediates strings
                        ////

                        // cleanup stack
                        pstackLvl = pPreviousStackLvl;
                        while ( pstackLvl != nullptr ) {
                            pstackLvl = (LE_calcStack*) execStack.getNextListElement( pstackLvl );
                            execStack.deleteListElement( pstackLvl );
                        };
                        _pCalcStackTop = pPreviousStackLvl;
                        _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
                        _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );
                        _calcStackLvl -= argCount;
                    }

                }
                else { makeIntermediateConstant( _pCalcStackTop ); }                                                // simple parenthesis pair
            }
            // with the left argument removed, check if additional operators preceding the left parenthesis can now be executed. 
            // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
            execAllProcessedInfixOperations( pPendingStep );
            if ( execResult != result_execOK ) { return execResult; }

            break;


        case tok_isSemiColonSeparator:
            // -----------------------------
            // Case: process semicolon token
            // -----------------------------

            if ( _calcStackLvl > 0 ) {
                if ( saveLastResult ) {
                    if ( (_lastCalcResult.isIntermediateResult) && (_lastCalcResult.valueType == var_isStringPointer) )   // a lsat result exists already, and it's a string: delete
                    {
#if printCreateDeleteHeapObjects
                        Serial.print( "delete 'previous' last result: " ); Serial.println( _lastCalcResult.value.pStringConst );
#endif 
                        if ( _lastCalcResult.value.pStringConst != nullptr ) { delete [] _lastCalcResult.value.pStringConst; }
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
#if printCreateDeleteHeapObjects
                    Serial.print( "delete remaining interm.cst string: " ); Serial.println( pstackLvl->varOrConst.value.pStringConst );
#endif 
                    if ( pstackLvl->varOrConst.value.pStringConst != nullptr ) { delete [] pstackLvl->varOrConst.value.pStringConst; }
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


        default:
            break;
        }

        // advance to next token
        _programCounter = pPendingStep;
        tokenType = *_programCounter & 0x0F;                                                     // next token type
    }

    // store last result in FIFO (if available) and return
    return result_execOK;
};


// -----------------------------------------------------
// *   turn stack operand into intermediate constant   *
// -----------------------------------------------------

void Interpreter::makeIntermediateConstant( LE_calcStack* pCalcStackLvl ) {
    // if a (scalar) variable: replace by a constant

    Val operand, opResult;                                                               // operands and result
    bool opReal = (pCalcStackLvl->varOrConst.valueType == var_isFloat);

    // if an intermediate constant, leave it as such. If not, make it an intermediate constant
    if ( pCalcStackLvl->varOrConst.isIntermediateResult != 0x01 ) {                    // not an intermediate constant
        if ( opReal ) { operand.realConst = (pCalcStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pCalcStackLvl->varOrConst.value.pRealConst) : pCalcStackLvl->varOrConst.value.realConst; }
        else { operand.pStringConst = (pCalcStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pCalcStackLvl->varOrConst.value.ppStringConst) : pCalcStackLvl->varOrConst.value.pStringConst; }

        // if the value (parsed constant or variable value) is a non-empty string value, make a copy of the character string and store a pointer to this copy as result
        // as the operand is not an intermediate constant, NO intermediate string object (if it's a string) needs to be deleted
        if ( !opReal && (operand.pStringConst != nullptr) ) {
            int stringlen = strlen( operand.pStringConst );
            opResult.pStringConst = new char [stringlen + 1];
            strcpy( opResult.pStringConst, operand.pStringConst );
#if printCreateDeleteHeapObjects
            Serial.print( "created copy of string: " ); Serial.println( opResult.pStringConst );
#endif
            operand = opResult;
        }

        pCalcStackLvl->varOrConst.value = operand;                        // float or pointer to string (type: no change)
        pCalcStackLvl->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        pCalcStackLvl->varOrConst.isIntermediateResult = 0x01;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        pCalcStackLvl->varOrConst.arrayAttributes = 0x00;                  // not an array, not an array element (it's a constant) 
    }
}


// ---------------------------------------------------------------
// Delete any intermediate result string objects used as arguments 
// ---------------------------------------------------------------

void temp() {  ////
/*
    pstackLvl = _pCalcStackTop;     // stack level of last argument before right parenthesis, or left parenthesis (if function call and no arguments supplied)

    do {
        isLeftParenthesis = (pstackLvl->genericToken.tokenType == tok_isLeftParenthesis);
        if ( pstackLvl->genericToken.tokenType == tok_isConstant ) {
            if ( (pstackLvl->varOrConst.isIntermediateResult == 0x01) && (pstackLvl->varOrConst.valueType == var_isStringPointer) )
            {
#if printCreateDeleteHeapObjects
                Serial.print( "delete argument interm.cst string: " ); Serial.println( _pCalcStackTop->varOrConst.value.pStringConst );
#endif
                if (pstackLvl->varOrConst.value.pStringConst != nullptr){delete [] pstackLvl->varOrConst.value.pStringConst;}
            }
        }
    } while ( !isLeftParenthesis );
*/
}


// ----------------------------------------------
// *   execute all processed infix operations   *
// ----------------------------------------------

Interpreter::execResult_type  Interpreter::execAllProcessedInfixOperations( char* pPendingStep ) {

    // _pCalcStackTop should point to an operand on entry

    int pendingTokenType, pendingTokenIndex;
    int pendingTokenPriorityLvl;
    bool currentOpHasPriority;
    execResult_type execResult = result_execOK;                                                     // init

    // check if (an) operation(s) can be executed 
    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
    while ( _calcStackLvl >= 3 ) {                                                      // a previous operand and operator might exist

        if ( _pCalcStackMinus1->genericToken.tokenType == tok_isOperator ) {
            // check pending (not yet processed) token (always present and always a terminal token after a variable or constant token)
            // pending token can be any terminal token: operator, left or right parenthesis, comma or semicolon 
            pendingTokenType = *pPendingStep & 0x0F;                                    // there's always minimum one token pending (even if it is a semicolon)
            pendingTokenIndex = (*pPendingStep >> 4) & 0x0F;                            // terminal token only: index stored in high 4 bits of token type 

            // if a pending operator has higher priority, or, it has equal priority and operator is right-to-left associative, do not execute operator yet 
            // note that a PENDING LEFT PARENTHESIS also has priority over the preceding operator
            pendingTokenPriorityLvl = MyParser::operatorPriority [pendingTokenIndex];
            currentOpHasPriority = (_pCalcStackMinus1->terminal.priority >= pendingTokenPriorityLvl);
            if ( (_pCalcStackMinus1->terminal.associativity == '1') && (_pCalcStackMinus1->terminal.priority == pendingTokenPriorityLvl) ) { currentOpHasPriority = false; }

            if ( !currentOpHasPriority ) { break; }   // exit while() loop

            execResult = execInfixOperation();                                          // execute operator 

            if ( execResult != result_execOK ) { return execResult; }
        }

        // token preceding the operand is a left parenthesis: exit while loop if it is (nothing to do for now)
        else { break; }
    }

    return execResult;

}


// -------------------------------
// *   execute infix operation   *
// -------------------------------

Interpreter::execResult_type  Interpreter::execInfixOperation() {

    execResult_type execResult = result_execOK;                                                     // init

    // --------------------------------------
    // Fetch operands and operands value type
    // --------------------------------------

    // variabbles for intermediate storage of operands (constants, variable values or intermediate results from previous calculations) and result
    Val operand1, operand2, opResult;                                                               // operands and result

    // value type of operands
    bool op1real = (_pCalcStackMinus2->varOrConst.valueType == var_isFloat);
    bool op2real = (_pCalcStackTop->varOrConst.valueType == var_isFloat);

    // check if operands or compatible with operator: real for all operators except string concatenation
    if ( (int) _pCalcStackMinus1->terminal.index != 2 ) {                                           // not an assignment ?
        if ( ((int) _pCalcStackMinus1->terminal.index == 6) && (op1real || op2real) ) { execResult = result_stringExpected; return execResult; }
        else if ( ((int) _pCalcStackMinus1->terminal.index != 6) && (!op1real || !op2real) ) { execResult = result_numberExpected; return execResult; }
    }
    else {                                                                                  // assignment 
        if ( true/* //// is array element */ ) {                                                                        // asignment to array element: value needs to be of same type as array
            if ( op1real != op2real ) { Serial.println( "error: array type fixed" ); execResult = result_arrayTypeIsFixed; return execResult; }
        }
    }

    // mixed operands not allowed; 2 x real -> real; 2 x string -> string: set result value type to operand 2 value type (assignment: current operand 1 value type is not relevant)
    bool opResultReal = op2real;                                                                    // do NOT set to operand 1 value type (would not work in case of assignment)

    // fetch operands: real constants or pointers to character strings
    if ( op1real ) { operand1.realConst = (_pCalcStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.pRealConst) : _pCalcStackMinus2->varOrConst.value.realConst; }
    else { operand1.pStringConst = (_pCalcStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackMinus2->varOrConst.value.ppStringConst) : _pCalcStackMinus2->varOrConst.value.pStringConst; }
    if ( op2real ) { operand2.realConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.pRealConst) : _pCalcStackTop->varOrConst.value.realConst; }
    else { operand2.pStringConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.ppStringConst) : _pCalcStackTop->varOrConst.value.pStringConst; }

    int stringlen;                                                                                  // define outside switch statement

    switch ( _pCalcStackMinus1->terminal.index ) {                                                  // operation to execute


    case 2:
        // -----------------------------------------------------------------------------------------------
        // Case: execute assignment (only possible if first operand is a variable: checked during parsing)
        // -----------------------------------------------------------------------------------------------

        if ( !op1real )                                                                             // if receiving variable currently holds a string, delete current char string object
        {
#if printCreateDeleteHeapObjects
            Serial.print( "delete previous variable string value: " ); Serial.println( *_pCalcStackMinus2->varOrConst.value.ppStringConst );
            Serial.print( "                           address is: " ); Serial.println( (uint32_t) _pCalcStackMinus2->varOrConst.value.pVariable - RAMSTART );
#endif
            if ( *_pCalcStackMinus2->varOrConst.value.ppStringConst != nullptr ) { delete [] * _pCalcStackMinus2->varOrConst.value.ppStringConst; }
        }

        // if the value to be assigned is real (float): simply assign the value (not a heap object)
        if ( op2real || (!op2real && (operand2.pStringConst == nullptr)) ) {
            opResult = operand2;
        }
        // the value (constant, variable value or intermediate result) to be assigned to the receiving variable is a non-empty string value
        else {
            // make a copy of the character stringand store a pointer to this copy as result
            stringlen = strlen( operand2.pStringConst );
            opResult.pStringConst = new char [stringlen + 1];
            strcpy( opResult.pStringConst, operand2.pStringConst );

#if printCreateDeleteHeapObjects
            Serial.print( "created copy of string: " ); Serial.println( opResult.pStringConst );
#endif

        }

        // store value and value type in variable and adapt variable value type
        if ( opResultReal ) { *_pCalcStackMinus2->varOrConst.value.pRealConst = opResult.realConst; }
        else { *_pCalcStackMinus2->varOrConst.value.ppStringConst = opResult.pStringConst; }
        *_pCalcStackMinus2->varOrConst.varTypeAddress = (*_pCalcStackMinus2->varOrConst.varTypeAddress & ~var_typeMask) | (opResultReal ? var_isFloat : var_isStringPointer);
        break;


        // -----------------------------------------------------
        // Next cases: execute infix operators taking 2 operands 
        // -----------------------------------------------------

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
        // concatenate two operand strings objects and store pointer to it in result
        stringlen = 0;                                  // is both operands are empty strings
        if ( operand1.pStringConst != nullptr ) { stringlen = strlen( operand1.pStringConst ); }
        if ( operand2.pStringConst != nullptr ) { stringlen += strlen( operand2.pStringConst ); }
        if ( stringlen == 0 ) { opResult.pStringConst = nullptr; }                                // empty strings are represented by a nullptr (conserve heap space)
        else {
            opResult.pStringConst = new char [stringlen + 1];
            opResult.pStringConst [0] = '\0';                                // in case first operand is nullptr
            if ( operand1.pStringConst != nullptr ) { strcpy( opResult.pStringConst, operand1.pStringConst ); }
            if ( operand2.pStringConst != nullptr ) { strcat( opResult.pStringConst, operand2.pStringConst ); }
#if printCreateDeleteHeapObjects
            Serial.print( "Create string object: " ); Serial.println( opResult.pStringConst );
#endif
        }
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
    }


    // --------------------------------------------------------------
    // Delete any intermediate result string objects used as operands 
    // --------------------------------------------------------------

    // operand 2 is an intermediate constant AND it is a string ? delete char string object
    if ( (_pCalcStackTop->varOrConst.isIntermediateResult == 0x01) && !op2real )
    {
#if printCreateDeleteHeapObjects
        Serial.print( "delete operand 2 interm.cst string: " ); Serial.println( _pCalcStackTop->varOrConst.value.pStringConst );
#endif
        if ( _pCalcStackTop->varOrConst.value.pStringConst != nullptr ) { delete [] _pCalcStackTop->varOrConst.value.pStringConst; }
    }
    // operand 1 is an intermediate constant AND it is a string ? delete char string object
    // note: if assignment, operand 1 refers to a variable value, so it will not be deleted (and 'op1real' still indicates the OLD variable value type, before assignment)
    if ( (_pCalcStackMinus2->varOrConst.isIntermediateResult == 0x01) && !op1real )
    {
#if printCreateDeleteHeapObjects
        Serial.print( "delete operand 1 interm.cst string: " ); Serial.println( _pCalcStackMinus2->varOrConst.value.pStringConst );
#endif
        if ( _pCalcStackMinus2->varOrConst.value.pStringConst != nullptr ) { delete [] _pCalcStackMinus2->varOrConst.value.pStringConst; }
    }


    // ---------------
    //  clean up stack
    // ---------------

    int ind = _pCalcStackMinus1->terminal.index;                            // remember index, before dropping stack

    // drop highest 2 stack levels( operator and operand 2 ) 
    execStack.deleteListElement( _pCalcStackTop );                          // operand 2 
    execStack.deleteListElement( _pCalcStackMinus1 );                       // operator
    _pCalcStackTop = _pCalcStackMinus2;
    _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
    _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );
    _calcStackLvl -= 2;


    // ----------------------
    //  store result in stack
    // ----------------------

    // set value type
    _pCalcStackTop->varOrConst.valueType = opResultReal ? var_isFloat : var_isStringPointer;

    // if assignment: the top of stack contains the variable ADDRESS, token type 'variable', 'not an isIntermediate result' and array attributes (not an array, could be array element) 
    if ( ind != 2 ) {                                                       // not an assignment
        // store result in stack (replaces operand 1)
        _pCalcStackTop->varOrConst.value = opResult;                        // float or pointer to string
        _pCalcStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        _pCalcStackTop->varOrConst.isIntermediateResult = 0x01;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        _pCalcStackTop->varOrConst.arrayAttributes = 0x00;                  // not an array, not an array element (it's a constant) 
    }

    return execResult;
}


// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Interpreter::varBaseAddress( TokenIsVariable* pVarToken, char*& varTypeAddress, char& valueType, char& variableAttributes ) {

    // pVarToken token argument must be a pointer to a variable name token stored in program memory
    // upon return, valueType and variableAttributes will contain current variable value type (float or string; which is fixed for arrays) and array flag, respectively
    // varTypeAddress will point to variable value type address (where variable value type and other attributes are maintained)
    // return pointer will point to variable base address

    int varNameIndex = pVarToken->identNameIndex;
    uint8_t varQualifier = pVarToken->identInfo & ~(var_isArray | var_isArrayNeedingElement);

    variableAttributes = pVarToken->identInfo & var_isArray;                            // address of scalar or base address of array (itself pointing to array start in memory) - not an array element
    bool isUserVar = (varQualifier == var_isUser);
    bool isGlobalVar = (varQualifier == var_isGlobal);
    bool isStaticVar = (varQualifier == var_isStaticInFunc);
    bool isLocalVar = (varQualifier == var_isLocalInFunc);
    bool isParam = (varQualifier == var_isParamInFunc);

    int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : programVarValueIndex [varNameIndex];

    if ( isUserVar ) {
        varTypeAddress = userVarType + valueIndex;
        valueType = userVarType [valueIndex] & var_typeMask;
        return &userVarValues [valueIndex];                                             // pointer to float, pointer to pointer to array or pointer to pointer to string
    }
    else if ( isGlobalVar ) {
        varTypeAddress = globalVarType + valueIndex;
        valueType = globalVarType [valueIndex] & var_typeMask;
        return &globalVarValues [valueIndex];
    }
    else if ( isStaticVar ) {
        varTypeAddress = staticVarType + valueIndex;
        valueType = staticVarType [valueIndex] & var_typeMask;
        return &staticVarValues [valueIndex];
    }

    //// local variables and parameters: to do
}



// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------


void* Interpreter::arrayElemAddress( void* varBaseAddress, int* elemSpec ) {

    // varBaseAddress argument must be base address of an array variable (containing itself a pointer to the array)
    // elemSpec array must specify an array element (max. 3 dimensions)
    // return pointer will point to a float or a string pointer (both can be array elements) - nullptr if outside boundaries

    void* pArray = varBaseAddress;                                                      // will point to float or string pointer (both can be array elements)
    int arrayDimCount = ((char*) pArray) [3];

    int arrayElement { 0 };
    for ( int i = 0; i < arrayDimCount; i++ ) {
        int arrayDim = ((char*) pArray) [i];
        if ( (elemSpec [i] < 1) || (elemSpec [i] > arrayDim) ) { return nullptr; }      // is outside array boundaries

        int arrayNextDim = (i < arrayDimCount - 1) ? ((char*) pArray) [i + 1] : 1;
        arrayElement = (arrayElement + (elemSpec [i] - 1)) * arrayNextDim;
    }
    arrayElement++;                                                                     // add one (first array element contains dimensions and dimension count)
    return (float*) pArray + arrayElement;                                              // pointer to a 4-byte array element, which can be a float or pointer to string
}


// ---------------------------------------------------
// *   push reserved word token to execution stack   *
// ---------------------------------------------------

bool Interpreter::pushResWord( int& tokenType ) {                                       // reserved word token is assumed

    // push reserved word to stack
    _flowCtrlStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pFlowCtrlStack = (LE_flowControlStack*) flowCtrlStack.appendListElement( sizeof( *_pFlowCtrlStack ) );
    _pFlowCtrlStack->tokenType = tok_isReservedWord;
    _pFlowCtrlStack->index = ((TokenIsResWord*) _programCounter)->tokenIndex;

    int toTokenStep { 0 };                                                                              // must be initialised to set high bytes to zero (internally, a 16 bit int reserves a 32 bit word !)
    memcpy( &toTokenStep, ((TokenIsResWord*) (_programCounter))->toTokenStep, sizeof( char [2] ) );     // token step token not necessarily aligned with word size: copy memory instead
    _pFlowCtrlStack->pToNextToken = _programStorage + toTokenStep;
};


// -----------------------------------------------
// *   push terminal token to execution stack   *
// -----------------------------------------------

bool Interpreter::PushTerminalToken( int& tokenType ) {                                 // terminal token is assumed

    // push internal or external function index to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->terminal ) );
    _pCalcStackTop->terminal.tokenType = tokenType;
    _pCalcStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;                                            // terminal token only: index stored in high 4 bits of token type 
    _pCalcStackTop->terminal.priority = MyParser::operatorPriority [_pCalcStackTop->terminal.index];
    _pCalcStackTop->terminal.associativity = MyParser::operatorAssociativity [_pCalcStackTop->terminal.index];  // operator priority and associativity 
};


// ------------------------------------------------------------------------
// *   push internal or external function name token to execution stack   *
// ------------------------------------------------------------------------

bool Interpreter::pushFunctionName( int& tokenType ) {                                  // function name is assumed (internal or external)

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


// -------------------------------------------------------------
// *   push real or string constant token to execution stack   *
// -------------------------------------------------------------

bool Interpreter::pushConstant( int& tokenType ) {                                              // float or string constant token is assumed

    // push real or string constant, value type and array flag (false) to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->varOrConst ) );
    _pCalcStackTop->varOrConst.tokenType = tok_isConstant;          // use generic constant type

    if ( tokenType == tok_isRealConst ) {
        float f;
        memcpy( &f, ((TokenIsRealCst*) _programCounter)->realConst, sizeof( float ) );          // float  not necessarily aligned with word size: copy memory instead
        _pCalcStackTop->varOrConst.value.realConst = f;                                         // store float in stack, NOT the pointer to float 
    }
    else {
        char* pAnum;
        memcpy( &pAnum, ((TokenIsStringCst*) _programCounter)->pStringConst, sizeof( pAnum ) ); // char pointer not necessarily aligned with word size: copy memory instead
        _pCalcStackTop->varOrConst.value.pStringConst = pAnum;                                  // store char* in stack, NOT the pointer to float 
    }

    _pCalcStackTop->varOrConst.valueType = (tokenType == tok_isRealConst) ? var_isFloat : var_isStringPointer;
    _pCalcStackTop->varOrConst.arrayAttributes = 0x00;
    _pCalcStackTop->varOrConst.isIntermediateResult = 0x00;
};


// ----------------------------------------------
// *   push variable token to execution stack   *
// ----------------------------------------------

bool Interpreter::pushVariable( int& tokenType ) {                                              // variable name token is assumed

    // push variable base address, variable value type (real, string) and array flag to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->varOrConst ) );
    _pCalcStackTop->varOrConst.tokenType = tokenType;
    void* varAddress = varBaseAddress( (TokenIsVariable*) _programCounter, _pCalcStackTop->varOrConst.varTypeAddress, _pCalcStackTop->varOrConst.valueType, _pCalcStackTop->varOrConst.arrayAttributes );
    _pCalcStackTop->varOrConst.value.pVariable = varAddress;                                    // base address of variable
    _pCalcStackTop->varOrConst.isIntermediateResult = 0x00;

    /* access scalar variable or array element data: both can contain float or pointer to string
    //// test
    if ( _pCalcStackTop->varOrConst.arrayAttributes == var_isArray ) {   // address of scalar variable or base address of array ? (itself pointing to array start in memory) - not an array element

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


// ----------------------------------------------------------
// *   put last calculation result (if available) in FIFO   *
// ----------------------------------------------------------

 //// in dev

bool Interpreter::pushLastCalcResultToFIFO() {

    if ( _lastResultCount == MAX_RESULT_DEPTH );

    if ( (_pCalcStackTop->varOrConst.isIntermediateResult == 0x01) && (_pCalcStackTop->varOrConst.valueType == var_isStringPointer) )
    {
#if printCreateDeleteHeapObjects
        Serial.print( "delete operand 2 interm.cst string: " ); Serial.println( _pCalcStackTop->varOrConst.value.pStringConst );
#endif
        if ( _pCalcStackTop->varOrConst.value.pStringConst != nullptr ) { delete [] _pCalcStackTop->varOrConst.value.pStringConst; }
    }

}


