#include "Justina.h"

#define printCreateDeleteHeapObjects 0
#define debugPrint 0

// -----------------------------------
// *   execute parsed instructions   *
// -----------------------------------

Interpreter::execResult_type  Interpreter::exec() {

    // init
    int tokenType = *_programStart & 0x0F;
    int tokenIndex;

    bool saveLastResult = false;
    execResult_type execResult = result_execOK;
    char* lastInstructionStart = _programStart;

    _programCounter = _programStart;
    _calcStackLvl = 0;
    _pCalcStackTop = nullptr;
    _pCalcStackMinus2 = nullptr;
    _pCalcStackMinus1 = nullptr;


    while ( tokenType != tok_no_token ) {                                                                    // for all tokens in token list
        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*_programCounter >> 4) & 0x0F;        // fetch next token 

        // if terminal token, determine which terminal type
        bool isTerminal = ((tokenType == Interpreter::tok_isTerminalGroup1) || (tokenType == Interpreter::tok_isTerminalGroup2) || (tokenType == Interpreter::tok_isTerminalGroup3));
        if ( isTerminal ) {
            tokenIndex = ((((TokenIsTerminal*) _programCounter)->tokenTypeAndIndex >> 4) & 0x0F);
            tokenIndex += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
        }

        bool isOperator = (isTerminal ? (MyParser::_terminals [tokenIndex].terminalCode <= MyParser::termcod_opRangeEnd) : false);
        bool isSemicolon = (isTerminal ? (MyParser::_terminals [tokenIndex].terminalCode == MyParser::termcod_semicolon) : false);
        bool isComma = (isTerminal ? (MyParser::_terminals [tokenIndex].terminalCode == MyParser::termcod_comma) : false);
        bool isLeftPar = (isTerminal ? (MyParser::_terminals [tokenIndex].terminalCode == MyParser::termcod_leftPar) : false);
        bool isRightPar = (isTerminal ? (MyParser::_terminals [tokenIndex].terminalCode == MyParser::termcod_rightPar) : false);

        // pending token
        char* pPendingStep = _programCounter + tokenLength;
        int pendingTokenType = *pPendingStep & 0x0F;          

        int pendingTokenIndex;
        bool isPendingTerminal = ((pendingTokenType == Interpreter::tok_isTerminalGroup1) || (pendingTokenType == Interpreter::tok_isTerminalGroup2) || (pendingTokenType == Interpreter::tok_isTerminalGroup3));
        if ( isPendingTerminal ) {
            pendingTokenIndex = ((((TokenIsTerminal*) pPendingStep)->tokenTypeAndIndex >> 4) & 0x0F);
            pendingTokenIndex += ((pendingTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (pendingTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
        }

        if ( lastInstructionStart == nullptr ) { lastInstructionStart = _programCounter; }

        switch ( tokenType ) {

        case tok_isReservedWord:
            // ---------------------------------
            // Case: process reserved word token
            // ---------------------------------

            // compile time statements (VAR, LOCAL, STATIC): skip for execution

        {   // start block (required for variable definitions inside)
            tokenIndex = ((TokenIsResWord*) _programCounter)->tokenIndex;
            bool skipStatement = ((_pmyParser->_resWords [tokenIndex].restrictions & MyParser::cmd_skipDuringExec) != 0);
            if ( skipStatement ) {
                bool isPendingSemicolon = (isPendingTerminal ? (MyParser::_terminals [pendingTokenIndex].terminalCode == MyParser::termcod_semicolon) : false);

                while ( !isPendingSemicolon ) {     // find semicolon (always there)
                    // move to next token
                    int nextTokenLength = (pendingTokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*pPendingStep >> 4) & 0x0F;
                    pPendingStep = pPendingStep + nextTokenLength;
                    pendingTokenType = *pPendingStep & 0x0F;          // there's always minimum one token pending (even if it is a semicolon)

                    isPendingTerminal = ((pendingTokenType == Interpreter::tok_isTerminalGroup1) || (pendingTokenType == Interpreter::tok_isTerminalGroup2) || (pendingTokenType == Interpreter::tok_isTerminalGroup3));
                    if ( isPendingTerminal ) {
                        pendingTokenIndex = ((((TokenIsTerminal*) pPendingStep)->tokenTypeAndIndex >> 4) & 0x0F);
                        pendingTokenIndex += ((pendingTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (pendingTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
                    }
                    isPendingSemicolon = (isPendingTerminal ? (MyParser::_terminals [pendingTokenIndex].terminalCode == MyParser::termcod_semicolon) : false);
                };
            }
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
#if debugPrint
            Serial.print( "operand: stack level " ); Serial.println( _calcStackLvl );
#endif
            saveLastResult = true;

            // push constant value token or variable name token to stack
            if ( tokenType == tok_isVariable ) {
                pushVariable( tokenType );

                bool isPendingLeftPar = (isPendingTerminal ? (MyParser::_terminals [pendingTokenIndex].terminalCode == MyParser::termcod_leftPar) : false);
                if ( isPendingLeftPar ) {   // array name followed by element spec (to be processed)
                    _pCalcStackTop->varOrConst.arrayAttributes |= var_isArrayElement; // not a plain array name (array function argument) - array element still to be processed
                }
            }
            else { pushConstant( tokenType ); }

            // set flag to save the current value as 'last value', in case the expression does not contain any operation or function to execute (this value only) 

            // check if (an) operation(s) can be executed. 
            // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)

            execResult = execAllProcessedOperators( pPendingStep );
            if ( execResult != result_execOK ) { break; }

            break;


            // ----------------------------
            // Case: process terminal token 
            // ----------------------------
        case tok_isTerminalGroup1:
        case tok_isTerminalGroup2:
        case tok_isTerminalGroup3:

            if ( isOperator || isLeftPar ) {
                // --------------------------------------------
                // Process operators and left parenthesis token
                // --------------------------------------------
#if debugPrint
                Serial.print( tok_isOperator ? "\r\n** operator: stack level " : "\r\n** left parenthesis: stack level " ); Serial.println( _calcStackLvl );
#endif

                // terminal tokens: only operators and left parentheses are pushed on the stack
                PushTerminalToken( tokenType );

            }

            else if ( isComma || isSemicolon ) {
                // -----------------------
                // Case: process separator
                // -----------------------
                if ( isSemicolon ) { lastInstructionStart = nullptr; }
            }

            else if ( isRightPar ) {
                // -------------------------------------
                // Case: process right parenthesis token
                // -------------------------------------

#if debugPrint
                Serial.print( "right parenthesis: stack level " ); Serial.println( _calcStackLvl );
#endif
                {   // start block (required for variable definitions inside)
                    int argCount = 0;                                                // init number of supplied arguments (or array subscripts) to 0
                    LE_calcStack* pstackLvl = _pCalcStackTop;     // stack level of last argument / array subscript before right parenthesis, or left parenthesis (if function call and no arguments supplied)

                    while ( (pstackLvl->genericToken.tokenType != tok_isTerminalGroup1) && (pstackLvl->genericToken.tokenType != tok_isTerminalGroup2) && (pstackLvl->genericToken.tokenType != tok_isTerminalGroup3) ) {
                        // terminal found: continue until left parenthesis
                        if ( MyParser::_terminals [pstackLvl->terminal.index].terminalCode == MyParser::termcod_leftPar ) { break; }   // continue until left parenthesis found
                        pstackLvl = (LE_calcStack*) execStack.getPrevListElement( pstackLvl );
                        argCount++;
                    }
                    LE_calcStack* pPrecedingStackLvl = (LE_calcStack*) execStack.getPrevListElement( pstackLvl );     // stack level PRECEDING left parenthesis (or null pointer)

                    // remove left parenthesis stack level
                    pstackLvl = (LE_calcStack*) execStack.deleteListElement( pstackLvl );                            // pstackLvl now pointing to first dim spec argument
                    _calcStackLvl--;                                                                                    // left parenthesis level removed, argument(s) still there

                    // correct pointers (now wrong, if only one or 2 arguments)
                    _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
                    _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );

                    // execute internal or external function, calculate array element address or remove parenthesis around single argument (if no function or array)
                    execResult = execParenthesisPair( pPrecedingStackLvl, pstackLvl, argCount, pPendingStep );
                    if ( execResult != result_execOK ) { break; }

                    // the left parenthesis and the argument(s) are now removed and replaced by a single scalar (function result, array element, single argument)
                    // check if additional operators preceding the left parenthesis can now be executed.
                    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
                    execResult = execAllProcessedOperators( pPendingStep );
                    if ( execResult != result_execOK ) { break; }
                }
            }

            break;

        } // end 'switch (tokenType)'


        // if execution error: print current instruction being executed, signal error and exit
        // -----------------------------------------------------------------------------------

        if ( execResult != result_execOK ) {
            int sourceErrorPos;
            _pConsole->print( "\r\n  " );
            _pmyParser->prettyPrintInstructions( true, lastInstructionStart, _errorProgramCounter, &sourceErrorPos );
            _pConsole->print( "  " ); for ( int i = 1; i <= sourceErrorPos; i++ ) { _pConsole->print( " " ); }
            char parsingInfo [30];
            sprintf( parsingInfo, "^ Exec error %d\r\n", execResult );   //// if within program: indicate function where error occured
            _pConsole->print( parsingInfo );
            clearExecStack();
            return execResult;              // return result, in case it's needed by caller
        }

        // advance to next token
        _programCounter = pPendingStep;         // note: will be altered when calling an external function and upon return of a called function
        tokenType = *pPendingStep & 0x0F;                                                               // next token type
    }                                                                                            // end 'while ( tokenType != tok_no_token )'


    // All tokens processed: finalize
    // ------------------------------

    if ( saveLastResult ) {             // did the execution produce a result ?
        saveLastValue();                // save last result in FIFO( if available )

        // print lastresult
        char s [MyParser::_maxAlphaCstLen + 10];  // note: with small '_maxAlphaCstLen' values, make sure string is also long enough to print real values
        if ( lastResultTypeFiFo [0] == var_isFloat ) { sprintf( s, "  %.3G", lastResultValueFiFo [0].realConst ); }
        else { sprintf( s, "  %s", lastResultValueFiFo [0].pStringConst ); }    // immediate mode: print evaluation result
        _pConsole->println( s );
    }

    // Delete any intermediate result string objects used as arguments, delete remaining stack level objects 
    clearExecStack();

    return result_execOK;
};


// ------------------------------------------------
// Save last value for future reuse by calculations 
// ------------------------------------------------

void Interpreter::saveLastValue() {
    if ( _calcStackLvl > 0 ) {           // safety

        // is FIFO full ?
        if ( _lastResultCount == MAX_LAST_RESULT_DEPTH ) {

            // if the oldest result is a string: delete heap object
            if ( lastResultTypeFiFo [MAX_LAST_RESULT_DEPTH - 1] == var_isStringPointer ) {

#if printCreateDeleteHeapObjects
                Serial.print( "\r\n===== delete 'previous' last result: " ); Serial.println( lastResultValueFiFo [MAX_LAST_RESULT_DEPTH - 1].pStringConst ); //// OK
#endif 
                    // note: this is always an intermediate string
                if ( lastResultValueFiFo [MAX_LAST_RESULT_DEPTH - 1].pStringConst != nullptr ) { delete [] lastResultValueFiFo [MAX_LAST_RESULT_DEPTH - 1].pStringConst; }
            }
        }
        else {
            _lastResultCount++;
        }

        // move older last results one place up in FIFO
        if ( _lastResultCount > 1 ) {       // if 'new' last result count is 1, no old results need to be moved  
            for ( int i = _lastResultCount - 1; i > 0; i-- ) {
                lastResultValueFiFo [i] = lastResultValueFiFo [i - 1];
                lastResultTypeFiFo [i] = lastResultTypeFiFo [i - 1];
            }
        }

        // store new last value
        VarOrConstLvl lastvalue;
        bool lastValueReal = (_pCalcStackTop->varOrConst.valueType == var_isFloat);
        if ( lastValueReal ) { lastvalue.value.realConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.pRealConst) : _pCalcStackTop->varOrConst.value.realConst; }
        else { lastvalue.value.pStringConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.ppStringConst) : _pCalcStackTop->varOrConst.value.pStringConst; }

        // new last value is a string: make a copy of the string and store a reference to this new string
        if ( !lastValueReal ) {
            int stringlen = min( strlen( lastvalue.value.pStringConst ), MyParser::_maxAlphaCstLen );        // excluding terminating \0
            lastResultValueFiFo [0].pStringConst = new char [stringlen + 1];
            memcpy( lastResultValueFiFo [0].pStringConst, lastvalue.value.pStringConst, stringlen );        // copy the actual string (not the pointer); do not use strcpy
            lastResultValueFiFo [0].pStringConst [stringlen] = '\0';                                         // add terminating \0
        }
        else { lastResultValueFiFo [0] = lastvalue.value; }

        // store new last value type
        lastResultTypeFiFo [0] = _pCalcStackTop->varOrConst.valueType;               // value type

    }
    return;
}

// -----------------------
// Clear calculation stack  
// -----------------------

void Interpreter::clearExecStack() {

    if ( _calcStackLvl == 0 ) { return; }           // nothing to clean up

    // delete any intermediate result string objects used as arguments
    LE_calcStack* pstackLvl = _pCalcStackTop;
    do {
        if ( (pstackLvl->genericToken.tokenType == tok_isConstant) || (pstackLvl->genericToken.tokenType == tok_isVariable) ) {  //// variable toch niet ??? (int. constant)
            if ( (pstackLvl->varOrConst.isIntermediateResult == 0x01) && (pstackLvl->varOrConst.valueType == var_isStringPointer) )
            {
#if printCreateDeleteHeapObjects
                Serial.print( "\r\n===== delete remaining interm.cst string: " ); Serial.println( pstackLvl->varOrConst.value.pStringConst );
#endif 
                if ( pstackLvl->varOrConst.value.pStringConst != nullptr ) { delete [] pstackLvl->varOrConst.value.pStringConst; }
            }
        }
        pstackLvl = (LE_calcStack*) execStack.getPrevListElement( pstackLvl );
    } while ( pstackLvl != nullptr );


    // delete all remaining stack level objects 
#if printCreateDeleteHeapObjects
    Serial.println( "\r\n>>>>> delete remaining list levels" );
#endif
    execStack.deleteList();
    _pCalcStackTop = nullptr;
    _pCalcStackMinus1 = nullptr;
    _pCalcStackMinus2 = nullptr;
    _calcStackLvl = 0;

    return;
}


// ------------------------------------------------------------------------------
// Remove operands / function arguments / array subscripts from calculation stack
// ------------------------------------------------------------------------------

void Interpreter::deleteStackArguments( LE_calcStack* pPrecedingStackLvl, int argCount, bool includePreceding ) {

    // Delete any intermediate result string objects used as operands 
    // --------------------------------------------------------------

    LE_calcStack* pStackLvl = (LE_calcStack*) execStack.getNextListElement( pPrecedingStackLvl );   // array subscripts or function arguments (NOT the preceding list element) 
    do {
        // stack levels contain variables and (interim) constants only
        if ( (pStackLvl->varOrConst.isIntermediateResult == 0x01) && (pStackLvl->varOrConst.valueType == var_isStringPointer) ) {
#if printCreateDeleteHeapObjects
            Serial.print( "\r\n===== delete interim cst string between parenthesis: " ); Serial.println( pStackLvl->varOrConst.value.pStringConst ); // to be checked
#endif
            if ( pStackLvl->varOrConst.value.pStringConst != nullptr ) { delete [] pStackLvl->varOrConst.value.pStringConst; }
        }
        pStackLvl = (LE_calcStack*) execStack.getNextListElement( pStackLvl );  // next dimspec or null pointer 

    } while ( pStackLvl != nullptr );


    // cleanup stack
    // -------------

    // set pointer to either first token (value) after opening parenthesis (includePreceding = false -> used if array subscripts), or
    // last token (function name) before opening parenthesis (includePreceding = true -> used if calling function)
    // note that the left parenthesis is already removed from stack at this stage
    pStackLvl = includePreceding ? pPrecedingStackLvl : (LE_calcStack*) execStack.getNextListElement( pPrecedingStackLvl );
    _pCalcStackTop = pPrecedingStackLvl;                                                        // note down before deleting list levels
    _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
    _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );
    while ( pStackLvl != nullptr ) { pStackLvl = (LE_calcStack*) execStack.deleteListElement( pStackLvl ); }

    _calcStackLvl -= (argCount + (includePreceding ? 1 : 0));

    return;
}


// --------------------------------------------------------------------------------------------------------------------------
// *   execute internal or external function, calculate array element address or remove parenthesis around single argument  *
// --------------------------------------------------------------------------------------------------------------------------

Interpreter::execResult_type Interpreter::execParenthesisPair( LE_calcStack*& pPrecedingStackLvl, LE_calcStack*& firstArgStackLvl, int argCount, char*& pPendingStep ) {
    // perform internal or external function, calculate array element address or simply make an expression result within parentheses an intermediate constant
    if ( pPrecedingStackLvl == nullptr ) { makeIntermediateConstant( _pCalcStackTop ); }                 // result of simple parenthesis pair is always an intermediate constant
    else if ( pPrecedingStackLvl->genericToken.tokenType == tok_isInternFunction ) {
        execResult_type execResult = result_execOK;//// vervang door call naar int func exec
        if ( execResult != result_execOK ) { return execResult; }
    }
    else if ( pPrecedingStackLvl->genericToken.tokenType == tok_isExternFunction ) {
        execResult_type execResult = execExternalFunction( pPrecedingStackLvl, firstArgStackLvl, argCount, pPendingStep );
        if ( execResult != result_execOK ) { return execResult; }
    }
    else if ( pPrecedingStackLvl->genericToken.tokenType == tok_isVariable ) {
        // stack level preceding left parenthesis is an array name requiring an array element ?
        // (if not, then it can only to be an array name used as previous argument in a function call)
        if ( (pPrecedingStackLvl->varOrConst.arrayAttributes & var_isArrayElement) == var_isArrayElement ) {
            execResult_type execResult = arrayAndSubscriptsToarrayElement( pPrecedingStackLvl, firstArgStackLvl, argCount );
            if ( execResult != result_execOK ) { return execResult; }
        }
        // stack level preceding left parenthesis is NOT an internal or external function name, or an array name requiring an array element:
        // simple parenthesis pair, which forms an expression -> result is always an intermediate constant
        else { makeIntermediateConstant( _pCalcStackTop ); }        // left parenthesis already removed from calculation stack
    }
    return result_execOK;
}

// ------------------------------------------------------------------------------------------------------------------
// *   replace array variable base address and subscripts with the array element address on the calculation stack   *
// ------------------------------------------------------------------------------------------------------------------

Interpreter::execResult_type Interpreter::arrayAndSubscriptsToarrayElement( LE_calcStack*& pPrecedingStackLvl, LE_calcStack*& pStackLvl, int argCount ) {
    void* pArray = *pPrecedingStackLvl->varOrConst.value.ppArray;
    _errorProgramCounter = pPrecedingStackLvl->varOrConst.tokenAddress;                // token adress of array name (in the event of an error)

    int elemSpec [4] = { 0 ,0,0,0 };
    do {
        bool opReal = (pStackLvl->varOrConst.valueType == var_isFloat);
        if ( opReal ) {
            //note: elemSpec [3] (last array element) only used here as a counter
            elemSpec [elemSpec [3]] = (pStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pStackLvl->varOrConst.value.pRealConst) : pStackLvl->varOrConst.value.realConst;
        }
        else { return result_numberExpected; }

        pStackLvl = (LE_calcStack*) execStack.getNextListElement( pStackLvl );
    } while ( ++elemSpec [3] < argCount );


    // calculate array element address and replace array base address with it in stack
    // -------------------------------------------------------------------------------

    void* pArrayElem = arrayElemAddress( pArray, elemSpec );
    if ( pArrayElem == nullptr ) { return result_array_outsideBounds; }
    pPrecedingStackLvl->varOrConst.value.pVariable = pArrayElem;
    // note: other data does not change (array attributes, value type, token type, intermediate constant, variable type address)


    // Remove array subscripts from calculation stack
    // ----------------------------------------------

    deleteStackArguments( pPrecedingStackLvl, argCount, false );

    return result_execOK;
}

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
            Serial.print( "\r\n===== created copy of string for interim constant: " ); Serial.println( opResult.pStringConst ); //// OK
#endif
            operand = opResult;
        }

        pCalcStackLvl->varOrConst.value = operand;                        // float or pointer to string (type: no change)
        pCalcStackLvl->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        pCalcStackLvl->varOrConst.isIntermediateResult = 0x01;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        pCalcStackLvl->varOrConst.arrayAttributes = 0x00;                  // not an array, not an array element (it's a constant) 
    }
}


// ----------------------------------------------
// *   execute all processed infix operations   *
// ----------------------------------------------

Interpreter::execResult_type  Interpreter::execAllProcessedOperators( char* pPendingStep ) {            // prefix and infix

    // _pCalcStackTop should point to an operand on entry (parsed constant, variable, expression result)

    int pendingTokenType, pendingTokenIndex;
    int pendingTokenPriorityLvl;
    bool currentOpHasPriority;

#if debugPrint
    Serial.print( "** exec processed infix operators -stack levels: " ); Serial.println( _calcStackLvl );
#endif
    // check if (an) operation(s) can be executed 
    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
    while ( _calcStackLvl >= 2 ) {                                                      // a previous operator might exist

        bool minus1IsTerminal = (_pCalcStackMinus1->genericToken.tokenType == tok_isTerminalGroup1) || (_pCalcStackMinus1->genericToken.tokenType == tok_isTerminalGroup2)
            || (_pCalcStackMinus1->genericToken.tokenType == tok_isTerminalGroup2);
        bool minus1IsOperator = (MyParser::_terminals [_pCalcStackMinus1->terminal.index].terminalCode <= MyParser::termcod_opRangeEnd);

        if ( minus1IsOperator ) {
            // check pending (not yet processed) token (always present and always a terminal token after a variable or constant token)
            // pending token can be any terminal token: infix operator, left or right parenthesis, comma or semicolon 
            // it can not be a prefix operator because it follows an operand (on top of stack)
            pendingTokenType = *pPendingStep & 0x0F;                                    // there's always minimum one token pending (even if it is a semicolon)
            pendingTokenIndex = (*pPendingStep >> 4) & 0x0F;                            // terminal token only: index stored in high 4 bits of token type 
            pendingTokenIndex += ((pendingTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (pendingTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);

            // infix operation ?
            bool isPrefixOperator = true;             // init as prefix operation
            if ( _calcStackLvl >= 2 ) {         // already a token on the stack ?
                bool minus2IsTerminal = ((_pCalcStackMinus2->genericToken.tokenType == tok_isTerminalGroup1) ||
                    (_pCalcStackMinus2->genericToken.tokenType == tok_isTerminalGroup2) || (_pCalcStackMinus2->genericToken.tokenType == tok_isTerminalGroup3));
                bool minus2IsRightPar = (MyParser::_terminals [_pCalcStackMinus2->terminal.index].terminalCode == MyParser::termcod_rightPar);
                isPrefixOperator = !((_pCalcStackMinus2->genericToken.tokenType == tok_isConstant) || (_pCalcStackMinus2->genericToken.tokenType == tok_isVariable)
                    || minus2IsRightPar);
            };

            int priority = _pmyParser->_terminals [_pCalcStackMinus1->terminal.index].priority;
            if ( isPrefixOperator ) { priority = priority >> 4; }
            priority &= 0x0F;

            int associativity = _pmyParser->_terminals [_pCalcStackMinus1->terminal.index].associativity;
            if ( isPrefixOperator ) { associativity = associativity >> 4; }
            associativity &= _pmyParser->trm_assocRtoL;

            Serial.print( "*** read terminal index: " ); Serial.print( (int) _pCalcStackMinus1->terminal.index );
            Serial.print( ", priority " ); Serial.print( (int) priority );
            Serial.print( ", associativity " ); Serial.print( (int) associativity );
            Serial.println( isPrefixOperator ? " - exec prefix" : " - exec infix" );

            // if a pending operator has higher priority, or, it has equal priority and operator is right-to-left associative, do not execute operator yet 
            // note that a PENDING LEFT PARENTHESIS also has priority over the preceding operator
            pendingTokenPriorityLvl = _pmyParser->_terminals [pendingTokenIndex].priority & 0x0F;      // pending terminal is never a prefix operator 
            currentOpHasPriority = (priority >= pendingTokenPriorityLvl);
            if ( (associativity == MyParser::trm_assocRtoL) && (priority == pendingTokenPriorityLvl) ) { currentOpHasPriority = false; }
            if ( !currentOpHasPriority ) { break; }   // exit while() loop

            // execute operator
            execResult_type execResult = (isPrefixOperator) ? execPrefixOperation() : execInfixOperation();
            if ( execResult != result_execOK ) { return execResult; }
        }

        // token preceding the operand is a left parenthesis ? exit while loop (nothing to do for now)
        else { break; }
    }

    return result_execOK;
}


// -------------------------------
// *   execute prefix operation   *
// -------------------------------

Interpreter::execResult_type  Interpreter::execPrefixOperation() {

    // check that operand is real, fetch operand and execute prefix operator
    // ---------------------------------------------------------------------

    Val operand;                                                               // operand and result
    _errorProgramCounter = _pCalcStackMinus1->terminal.tokenAddress;                // in the event of an error
    bool opreal = (_pCalcStackTop->varOrConst.valueType == var_isFloat);
    if ( _pCalcStackTop->varOrConst.valueType != var_isFloat ) { return result_numberExpected; }
    operand.realConst = (_pCalcStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pCalcStackTop->varOrConst.value.pRealConst) : _pCalcStackTop->varOrConst.value.realConst;
    if ( _pmyParser->_terminals [_pCalcStackMinus1->terminal.index].terminalCode == _pmyParser->termcod_minus ) { operand.realConst = -operand.realConst; } // prefix '-' ('+': no change in value)

    // negation of a floating point value can not produce an error: no checks needed

    //  store result in stack (if not yet, becomes an intermediate constant now)
    _pCalcStackTop->varOrConst.value = operand;
    _pCalcStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
    _pCalcStackTop->varOrConst.isIntermediateResult = 0x01;
    _pCalcStackTop->varOrConst.arrayAttributes = 0x00;                  // not an array, not an array element (it's a constant) 


    //  clean up stack (drop prefix operator)
    // --------------------------------------

    execStack.deleteListElement( _pCalcStackMinus1 );
    _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
    _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );
    _calcStackLvl -= 1;


    return result_execOK;
}


// -------------------------------
// *   execute infix operation   *
// -------------------------------

Interpreter::execResult_type  Interpreter::execInfixOperation() {

    // Fetch operands and operands value type
    // --------------------------------------

    // variables for intermediate storage of operands (constants, variable values or intermediate results from previous calculations) and result
    Val operand1, operand2, opResult;                                                               // operands and result

    // value type of operands
    bool op1real = (_pCalcStackMinus2->varOrConst.valueType == var_isFloat);
    bool op2real = (_pCalcStackTop->varOrConst.valueType == var_isFloat);

    int operatorCode = _pmyParser->_terminals [_pCalcStackMinus1->terminal.index].terminalCode;

    // check if operands are compatible with operator: real for all operators except string concatenation
    _errorProgramCounter = _pCalcStackMinus1->terminal.tokenAddress;                // in the event of an error
    if ( operatorCode != _pmyParser->termcod_assign ) {                                           // not an assignment ?
        if ( (operatorCode == _pmyParser->termcod_concat) && (op1real || op2real) ) { return result_stringExpected; }
        else if ( (operatorCode != _pmyParser->termcod_concat) && (!op1real || !op2real) ) { return result_numberExpected; }
    }
    else {                                                                                  // assignment 
        if ( _pCalcStackMinus2->varOrConst.arrayAttributes & var_isArrayElement ) {        // asignment to array element: value needs to be of same type as array
            if ( op1real != op2real ) { return result_arrayTypeIsFixed; }
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

    switch ( operatorCode ) {                                                  // operation to execute


    case MyParser::termcod_assign:
        // Case: execute assignment (only possible if first operand is a variable: checked during parsing)
        // -----------------------------------------------------------------------------------------------

        if ( !op1real )                                                                             // if receiving variable currently holds a string, delete current char string object
        {
#if printCreateDeleteHeapObjects
            Serial.print( "\r\n===== delete previous variable string value: " ); Serial.println( *_pCalcStackMinus2->varOrConst.value.ppStringConst );               //// OK
            Serial.print( "                                 address is: " ); Serial.println( (uint32_t) _pCalcStackMinus2->varOrConst.value.pVariable - RAMSTART );
#endif
            // delete variable string object
            if ( *_pCalcStackMinus2->varOrConst.value.ppStringConst != nullptr ) { delete [] * _pCalcStackMinus2->varOrConst.value.ppStringConst; }
        }

        // if the value to be assigned is real (float): simply assign the value (not a heap object)
        if ( op2real || (!op2real && (operand2.pStringConst == nullptr)) ) {
            opResult = operand2;
        }
        // the value (constant, variable value or intermediate result) to be assigned to the receiving variable is a non-empty string value
        else {
            // make a copy of the character string and store a pointer to this copy as result
            // because the value will be stored in a variable, limit to the maximum allowed string length
            stringlen = min( strlen( operand2.pStringConst ), MyParser::_maxAlphaCstLen );
            opResult.pStringConst = new char [stringlen + 1];
            memcpy( opResult.pStringConst, operand2.pStringConst, stringlen );        // copy the actual string (not the pointer); do not use strcpy
            opResult.pStringConst [stringlen] = '\0';                                         // add terminating \0

#if printCreateDeleteHeapObjects
            Serial.print( "\r\n===== created copy of string for assignment to variable : " ); Serial.println( opResult.pStringConst ); //// OK
#endif

        }

        // store value and value type in variable and adapt variable value type
        if ( opResultReal ) { *_pCalcStackMinus2->varOrConst.value.pRealConst = opResult.realConst; }
        else { *_pCalcStackMinus2->varOrConst.value.ppStringConst = opResult.pStringConst; }
        *_pCalcStackMinus2->varOrConst.varTypeAddress = (*_pCalcStackMinus2->varOrConst.varTypeAddress & ~var_typeMask) | (opResultReal ? var_isFloat : var_isStringPointer);
        break;


        // Next cases: execute infix operators taking 2 operands 
        // -----------------------------------------------------

    case MyParser::termcod_lt:
        opResult.realConst = operand1.realConst < operand2.realConst;
        break;
    case MyParser::termcod_gt:
        opResult.realConst = operand1.realConst > operand2.realConst;
        break;
    case MyParser::termcod_eq:
        opResult.realConst = operand1.realConst == operand2.realConst;
        break;
    case MyParser::termcod_concat:
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
            Serial.print( "\r\n===== created string object for string operator: " ); Serial.println( opResult.pStringConst ); //// OK
#endif
        }
        break;
    case MyParser::termcod_plus:
        opResult.realConst = operand1.realConst + operand2.realConst;
        break;
    case MyParser::termcod_minus:
        opResult.realConst = operand1.realConst - operand2.realConst;
        break;
    case MyParser::termcod_mult:
        opResult.realConst = operand1.realConst * operand2.realConst;
        if ( (operand1.realConst != 0) && (operand2.realConst != 0) && (!isnormal( opResult.realConst )) ) { return result_underflow; }
        break;
    case MyParser::termcod_div:
        opResult.realConst = operand1.realConst / operand2.realConst;
        if ( (operand1.realConst != 0) && (!isnormal( opResult.realConst )) ) { return result_underflow; }
        break;
    case MyParser::termcod_pow:
        if ( (operand1.realConst == 0) && (operand2.realConst == 0) ) { return result_undefined; } // C++ pow() provides 1 as result
        opResult.realConst = pow( operand1.realConst, operand2.realConst );
        break;
    case MyParser::termcod_ltoe:
        opResult.realConst = operand1.realConst <= operand2.realConst;
        break;
    case MyParser::termcod_gtoe:
        opResult.realConst = operand1.realConst >= operand2.realConst;
        break;
    case MyParser::termcod_ne:
        opResult.realConst = operand1.realConst != operand2.realConst;
        break;
    }

    if ( (opResultReal) && (operatorCode != _pmyParser->termcod_assign) ) {     // check error (not for assignment)
        if ( isnan( opResult.realConst ) ) { return result_undefined; }
        else if ( !isfinite( opResult.realConst ) ) { return result_overflow; }
    }

    // Delete any intermediate result string objects used as operands 
    // --------------------------------------------------------------

    // operand 2 is an intermediate constant AND it is a string ? delete char string object
    if ( (_pCalcStackTop->varOrConst.isIntermediateResult == 0x01) && !op2real )
    {
#if printCreateDeleteHeapObjects
        Serial.print( "\r\n===== delete operand 2 interm.cst string: " ); Serial.println( _pCalcStackTop->varOrConst.value.pStringConst ); //// OK
#endif
        if ( _pCalcStackTop->varOrConst.value.pStringConst != nullptr ) { delete [] _pCalcStackTop->varOrConst.value.pStringConst; }
    }
    // operand 1 is an intermediate constant AND it is a string ? delete char string object
    // note: if assignment, operand 1 refers to a variable value, so it will not be deleted (and 'op1real' still indicates the OLD variable value type, before assignment)
    if ( (_pCalcStackMinus2->varOrConst.isIntermediateResult == 0x01) && !op1real )
    {
#if printCreateDeleteHeapObjects
        Serial.print( "\r\n===== delete operand 1 interm.cst string: " ); Serial.println( _pCalcStackMinus2->varOrConst.value.pStringConst ); //// OK
#endif
        if ( _pCalcStackMinus2->varOrConst.value.pStringConst != nullptr ) { delete [] _pCalcStackMinus2->varOrConst.value.pStringConst; }
    }


    //  clean up stack
    // ---------------

    // drop highest 2 stack levels( operator and operand 2 ) 
    execStack.deleteListElement( _pCalcStackTop );                          // operand 2 
    execStack.deleteListElement( _pCalcStackMinus1 );                       // operator
    _pCalcStackTop = _pCalcStackMinus2;
    _pCalcStackMinus1 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackTop );
    _pCalcStackMinus2 = (LE_calcStack*) execStack.getPrevListElement( _pCalcStackMinus1 );
    _calcStackLvl -= 2;


    //  store result in stack
    // ----------------------

    // set value type
    _pCalcStackTop->varOrConst.valueType = opResultReal ? var_isFloat : var_isStringPointer;

    // if assignment: the top of stack contains the variable ADDRESS, token type 'variable', 'not an isIntermediate result' and array attributes (not an array, could be array element) 
    if ( operatorCode != _pmyParser->termcod_assign ) {                                                       // not an assignment
        _pCalcStackTop->varOrConst.value = opResult;                        // float or pointer to string
        _pCalcStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        _pCalcStackTop->varOrConst.isIntermediateResult = 0x01;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        _pCalcStackTop->varOrConst.arrayAttributes = 0x00;                  // not an array, not an array element (it's a constant) 
    }


    return result_execOK;
}


// ---------------------------------
// *   execute internal function   *
// ---------------------------------

Interpreter::execResult_type  Interpreter::execInternalFunction( LE_calcStack*& pPrecedingStackLvl, LE_calcStack*& pStackLvl, int argCount ) {

    int functionIndex = ((TokenIsIntFunction*) _programCounter)->tokenIndex & 0x0F;
    char functionCode = MyParser::_functions [functionIndex].functionCode;

    // variables for intermediate storage of operands (constants, variable values or intermediate results from previous calculations) and result
    LE_calcStack* pArgStackLvl = pStackLvl;
    Val operands [8], opResult;                                                               // operands and result
    bool opIsReal [8], opResultReal;

    // value type of operands
    if ( argCount > 0 ) {
        for ( int i = 0; i < argCount; i++ ) {
            opIsReal [i] = (pArgStackLvl->varOrConst.valueType == var_isFloat);

            // fetch operands: real constants or pointers to character strings
            if ( opIsReal [i] ) { operands [i].realConst = (pArgStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pArgStackLvl->varOrConst.value.pRealConst) : pArgStackLvl->varOrConst.value.realConst; }
            else { operands [i].pStringConst = (pArgStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pArgStackLvl->varOrConst.value.ppStringConst) : pArgStackLvl->varOrConst.value.pStringConst; }
        }
    }

    // check if operands are compatible with operator: real for all operators except string concatenation
    _errorProgramCounter = pPrecedingStackLvl->terminal.tokenAddress;                // in the event of an error




    switch ( functionIndex ) {

    case MyParser::fnccod_and:

        // Fetch operands and operands value type
        // --------------------------------------





        break;
    }

    return result_execOK;
}


// ---------------------------------
// *   execute external function   *
// ---------------------------------

Interpreter::execResult_type  Interpreter::execExternalFunction( LE_calcStack*& pPrecedingStackLvl, LE_calcStack*& pArgStackLvl, int argCount, char*& pPendingStep ) {

    int functionIndex = pPrecedingStackLvl->function.index;

    // temporarily hold current local storage pointer values (for caller) until stack is ready to receive these values
    LocalStoragePointers holdCallersStoragePointers = localStoragePointers;

    // create local variable storage for external function to be called
    int localVarCount = extFunctionData [functionIndex].localVarCountInFunction;
    localStoragePointers._pLocalVarValues = new Val [localVarCount];
    localStoragePointers._pSourceVarTypes = new char* [localVarCount];      // variables or array elements passed by reference, only: references to variable types 
    localStoragePointers._pLocalVarTypes = new char [localVarCount];        // local float, local string, reference


    // save function caller's arguments to local storage

    // value type of operands
    if ( argCount > 0 ) {
        LE_calcStack* pStackLvl = pArgStackLvl;         // pointing to first argument on stack
        for ( int i = 0; i < argCount; i++ ) {
            bool operandIsRef = (pStackLvl->varOrConst.valueType == var_isVarRef);
            bool operandIsReal = (pStackLvl->varOrConst.valueType == var_isFloat);
            bool operandIsVariable = (pStackLvl->varOrConst.tokenType == tok_isVariable);

            localStoragePointers._pLocalVarValues [i] = pStackLvl->varOrConst.value;
            localStoragePointers._pLocalVarTypes [i] = pStackLvl->varOrConst.valueType;

            if ( operandIsRef ) {
                localStoragePointers._pLocalVarValues [i].pVariable = pStackLvl->varOrConst.value.pVariable;  // local variable is reference to original variable
                localStoragePointers._pSourceVarTypes [i] = pStackLvl->varOrConst.varTypeAddress;             // reference to original variable's value type
                localStoragePointers._pLocalVarTypes [i] = pStackLvl->varOrConst.valueType;                   // reference
            }
            else {      // float or string, variable or constant, but NOT a variable reference itself
                if ( pStackLvl->varOrConst.tokenType == tok_isVariable ) {      // operand is variable: store a reference
                    localStoragePointers._pLocalVarValues [i].pVariable = pStackLvl->varOrConst.value.pVariable;  // local variable is reference to original variable
                    localStoragePointers._pSourceVarTypes [i] = pStackLvl->varOrConst.varTypeAddress;             // reference to original variable's value type
                    localStoragePointers._pLocalVarTypes [i] = var_isVarRef;                                      // local variable stores REFERENCE to original variable
                }
                else {      // parsed or intermediate constant passed as value
                    if ( operandIsReal ) {                                                      // operand is float constant
                        localStoragePointers._pLocalVarValues [i].realConst = pStackLvl->varOrConst.value.realConst;   // store a local copy
                        localStoragePointers._pLocalVarTypes [i] = var_isFloat;
                    }
                    else {                      // operand is string constant: create a local copy
                        int stringlen = strlen( pStackLvl->varOrConst.value.pStringConst );
                        localStoragePointers._pLocalVarValues [i].pStringConst = new char [stringlen + 1];
                        strcpy( localStoragePointers._pLocalVarValues [i].pStringConst, pStackLvl->varOrConst.value.pStringConst );
                        localStoragePointers._pLocalVarTypes [i] = var_isFloat;
                    }
                }
            }
        }
    }

    _errorProgramCounter = pPrecedingStackLvl->terminal.tokenAddress;                // in the event of an error

    // delete caller function's arguments AND function name token from calculation stack
    deleteStackArguments( pPrecedingStackLvl, argCount, true );


    // push current local variable storage pointers (for CALLER) and return address (for CALLER) on FLOW CONTROL stack //// mag hoger (separate stack); hold vars niet meer nodig
    _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
    _pFlowCtrlStackTop = (LocalStoragePointers*) flowCtrlStack.appendListElement( sizeof( LocalStoragePointers ) );
    *((LocalStoragePointers*) _pFlowCtrlStackTop) = holdCallersStoragePointers;
    ((LocalStoragePointers*) _pFlowCtrlStackTop)->_returnAddress = pPendingStep;        // add return address
    _flowCtrlStackLvl++;


    Serial.print( "local func index:" ); Serial.println( functionIndex );
    localVarCount = extFunctionData [functionIndex].localVarCountInFunction;
    int paramCount = extFunctionData [functionIndex].paramOnlyCountInFunction;
    Serial.print( "function name:   " ); Serial.println( extFunctionNames [functionIndex] );
    Serial.print( "local var count: " ); Serial.println( localVarCount );
    Serial.print( "param count:     " ); Serial.println( paramCount );
    Serial.print( "arg count:       " ); Serial.println( argCount );
    Serial.print( "func starts at   " ); Serial.println( extFunctionData [functionIndex].pExtFunctionStartToken - _programStorage );
    Serial.print( "return at:       " ); Serial.println( pPendingStep - _programStorage );

    // set next step to start of called function
    ////pPendingStep = extFunctionData [functionIndex].pExtFunctionStartToken;    // address of function name token




    // *** execute function (na 'return' nu)

    // ...

    //  store result in stack (if not yet, becomes an intermediate constant now)   //// example : float 999
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->varOrConst ) );
    _calcStackLvl++;

    _pCalcStackTop->varOrConst.value.realConst = 999;
    _pCalcStackTop->varOrConst.valueType = var_isFloat;
    _pCalcStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
    _pCalcStackTop->varOrConst.isIntermediateResult = 0x01;
    _pCalcStackTop->varOrConst.arrayAttributes = 0x00;                  // not an array, not an array element (it's a constant) 



    // *** NA called function (separate routine):


    // delete local variable arrays and strings

    // release local variable storage for function that has been called
    delete [] localStoragePointers._pLocalVarValues;
    delete [] localStoragePointers._pLocalVarTypes;
    delete [] localStoragePointers._pSourceVarTypes;


    // load local storage pointers again for caller function and restore pending step for caller function
    localStoragePointers = *(LocalStoragePointers*) _pFlowCtrlStackTop;          // top level contains called function result

    pPendingStep = ((LocalStoragePointers*) _pFlowCtrlStackTop)->_returnAddress;

    //// delete FLOW CONTROL stack level that contained caller function storage pointers and return address (all just retrieved)
    (LocalStoragePointers*) flowCtrlStack.deleteListElement( _pFlowCtrlStackTop );
    _pFlowCtrlStackTop = _pFlowCtrlStackMinus1;
    _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement( _pFlowCtrlStackTop );
    _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement( _pFlowCtrlStackMinus1 );
    _flowCtrlStackLvl--;

    return  result_execOK;
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
    uint8_t varQualifier = pVarToken->identInfo & ~(var_isArray | var_isArrayElement);

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
    else {      // local values (incl.) parameters    
        varTypeAddress = localStoragePointers._pLocalVarTypes + valueIndex;
        valueType = localStoragePointers._pLocalVarTypes [valueIndex] & var_typeMask;

        if ( valueType == var_isVarRef ) {
            varTypeAddress = (char*) *varTypeAddress;
            return   ((Val**)localStoragePointers._pLocalVarValues) [valueIndex];
        }
        else {              // local float or string
            return (Val*) & localStoragePointers._pLocalVarValues [valueIndex];
        }
    }
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
// *   push reserved word token to calculation stack   *
// ---------------------------------------------------
/*
bool Interpreter::pushResWord( int& tokenType ) {                                       // reserved word token is assumed

    // push reserved word to stack
    _flowCtrlStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;//// fout: flow control stack
    _pFlowCtrlStackTop = (LE_flowControlStack*) flowCtrlStack.appendListElement( sizeof( *_pFlowCtrlStackTop ) );
    _pFlowCtrlStackTop->tokenType = tok_isReservedWord;
    _pFlowCtrlStackTop->index = ((TokenIsResWord*) _programCounter)->tokenIndex;

    int toTokenStep { 0 };                                                                              // must be initialised to set high bytes to zero (internally, a 16 bit int reserves a 32 bit word !)
    memcpy( &toTokenStep, ((TokenIsResWord*) (_programCounter))->toTokenStep, sizeof( char [2] ) );     // token step token not necessarily aligned with word size: copy memory instead
    _pFlowCtrlStackTop->pToNextToken = _programStorage + toTokenStep;
};
*/

// -----------------------------------------------
// *   push terminal token to calculation stack   *
// -----------------------------------------------

bool Interpreter::PushTerminalToken( int& tokenType ) {                                 // terminal token is assumed

    // push internal or external function index to stack
    _calcStackLvl++;

    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;

    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->terminal ) );
    _pCalcStackTop->terminal.tokenType = tokenType;
    _pCalcStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;                                            // terminal token only: calculate from partial index stored in high 4 bits of token type 
    _pCalcStackTop->terminal.index += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);


    _pCalcStackTop->terminal.tokenAddress = _programCounter;                                                    // only for finding source error position during unparsing (for printing)
};


// ------------------------------------------------------------------------
// *   push internal or external function name token to calculation stack   *
// ------------------------------------------------------------------------

bool Interpreter::pushFunctionName( int& tokenType ) {                                  // function name is assumed (internal or external)

    // push internal or external function index to stack
    _calcStackLvl++;
    _pCalcStackMinus2 = _pCalcStackMinus1; _pCalcStackMinus1 = _pCalcStackTop;
    _pCalcStackTop = (LE_calcStack*) execStack.appendListElement( sizeof( _pCalcStackTop->function ) );
    _pCalcStackTop->function.tokenType = tokenType;
    _pCalcStackTop->function.index = ((TokenIsIntFunction*) _programCounter)->tokenIndex;
    _pCalcStackTop->function.tokenAddress = _programCounter;                                    // only for finding source error position during unparsing (for printing)
    /*
    if ( tokenType == tok_isInternFunction ) {
        int fIndex = (int) _pCalcStackTop->function.index;
        Serial.println( _pmyParser->_functions [fIndex].funcName );
    }
    else {
        Serial.println( extFunctionNames [_pCalcStackTop->function.index] );
    }
    */
};


// -------------------------------------------------------------
// *   push real or string constant token to calculation stack   *
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
    _pCalcStackTop->varOrConst.tokenAddress = _programCounter;                                  // only for finding source error position during unparsing (for printing)
};


// ----------------------------------------------
// *   push variable token to calculation stack   *
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
    _pCalcStackTop->varOrConst.tokenAddress = _programCounter;
}
