#include "Justina.h"

#define printCreateDeleteHeapObjects 1
#define debugPrint 0

// -----------------------------------
// *   execute parsed instructions   *
// -----------------------------------

Interpreter::execResult_type  Interpreter::exec() {

    // init
    int tokenType = *_programStart & 0x0F;
    int tokenIndex;
    bool lastValueIsStored = false;
    bool nextIsNewInstructionStart = false;                     // false, because this is already the start of a new instruction
    char activeCmd_ResWordCode = MyParser::cmdcod_none;        // no command is being executed
    char* activeCmd_resWordTokenAddress = nullptr;
    execResult_type execResult = result_execOK;

    _pEvalStackTop = nullptr;   _pEvalStackMinus2 = nullptr; _pEvalStackMinus1 = nullptr;
    _pFlowCtrlStackTop = nullptr;   _pFlowCtrlStackMinus2 = nullptr; _pFlowCtrlStackMinus1 = nullptr;

    _programCounter = _programStart;
    _activeFunctionData.functionIndex = 0;                  // main program level
    _activeFunctionData.callerEvalStackLevels = 0;          // this is the highest program level
    _activeFunctionData.errorStatementStartStep = _programCounter;
    _activeFunctionData.errorProgramCounter = _programCounter;

    intermediateStringObjectCount = 0;      // reset at the start of execution
    localVarStringObjectCount = 0;
    localArrayObjectCount = 0;

    while ( tokenType != tok_no_token ) {                                                                    // for all tokens in token list
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

        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*_programCounter >> 4) & 0x0F;        // fetch next token 
        _activeFunctionData.pNextStep = _programCounter + tokenLength;                                  // look ahead

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
                findTokenStep( tok_isTerminalGroup1, MyParser::termcod_semicolon, _activeFunctionData.pNextStep );  // find semicolon (always match)
                int tokIndx = ((((TokenIsTerminal*) _activeFunctionData.pNextStep)->tokenTypeAndIndex >> 4) & 0x0F);
                break;
            }

            // commands are executed when processing final semicolon statement (note: activeCmd_ResWordCode identifies individual commands; not command blocks)
            activeCmd_ResWordCode = _pmyParser->_resWords [tokenIndex].resWordCode;       // store command for now
            activeCmd_resWordTokenAddress = _programCounter;                       // only for finding source error position during unparsing (for printing)

        }
        break;


        case tok_isInternFunction:
        case tok_isExternFunction:
            // -------------------------------------------------
            // Case: process internal or external function token
            // -------------------------------------------------

            pushFunctionName( tokenType );
            break;


        case tok_isRealConst:
        case tok_isStringConst:
        case tok_isVariable:
            // -----------------------------------------------------------
            // Case: process real or string constant token, variable token
            // -----------------------------------------------------------
#if debugPrint
            Serial.print( "operand: stack level " ); Serial.println( evalStack.getElementCount() );
#endif
            {   // start block (required for variable definitions inside)

                // push constant value token or variable name token to stack
                if ( tokenType == tok_isVariable ) {
                    pushVariable( tokenType );

                    // next token
                    int nextTokenType = *_activeFunctionData.pNextStep & 0x0F;
                    int nextTokenIndex;
                    bool nextIsTerminal = ((nextTokenType == Interpreter::tok_isTerminalGroup1) || (nextTokenType == Interpreter::tok_isTerminalGroup2) || (nextTokenType == Interpreter::tok_isTerminalGroup3));
                    if ( nextIsTerminal ) {
                        nextTokenIndex = ((((TokenIsTerminal*) _activeFunctionData.pNextStep)->tokenTypeAndIndex >> 4) & 0x0F);
                        nextTokenIndex += ((nextTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (nextTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
                    }

                    bool nextIsLeftPar = (nextIsTerminal ? (MyParser::_terminals [nextTokenIndex].terminalCode == MyParser::termcod_leftPar) : false);
                    if ( nextIsLeftPar ) {                                                           // array variable name (this token) is followed by subscripts (to be processed)
                        _pEvalStackTop->varOrConst.variableAttributes |= var_isArray_pendingSubscripts;    // flag that array element still needs to be processed
                    }
                }
                else { pushConstant( tokenType ); }

                // set flag to save the current value as 'last value', in case the expression does not contain any operation or function to execute (this value only) 

                // check if (an) operation(s) can be executed. 
                // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)

                execResult = execAllProcessedOperators();
                if ( execResult != result_execOK ) { break; }
            }
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
                Serial.print( tok_isOperator ? "\r\n** operator: stack level " : "\r\n** left parenthesis: stack level " ); Serial.println( evalStack.getElementCount() );
#endif

                // terminal tokens: only operators and left parentheses are pushed on the stack
                PushTerminalToken( tokenType );

            }

            else if ( isComma ) {
                // -----------------------
                // Process comma separator
                // -----------------------
            }

            else if ( isSemicolon ) {
                // -----------------
                // Process separator
                // -----------------

                nextIsNewInstructionStart = true;         // for pretty print only   

                if ( activeCmd_ResWordCode == MyParser::cmdcod_none ) {       // currently not executing a command, but a simple expression
                    if ( evalStack.getElementCount() > _activeFunctionData.callerEvalStackLevels + 1 ) {
                        Serial.print( "*** Evaluation stack error. Remaining stack levels for current program level: " ); Serial.println( evalStack.getElementCount() - (_activeFunctionData.callerEvalStackLevels + 1) );
                    }

                    // did the last expression produce a result ?  
                    else if ( evalStack.getElementCount() == _activeFunctionData.callerEvalStackLevels + 1 ) {

                        // in main program level ? store as last value (for now, we don't know if it will be followed by other 'last' values)
                        if ( _programCounter >= _programStart ) { saveLastValue( lastValueIsStored ); }                // save last result in FIFO and delete stack level
                        else { clearEvalStackLevels( 1 ); } // NOT main program level: we don't need to keep the statement result
                    }
                }

                // command with optional expression(s) processed ? Execute command
                else {
                    execResult = execprocessedCommand( activeCmd_ResWordCode, activeCmd_resWordTokenAddress );
                    if ( execResult != result_execOK ) { break; }
                    activeCmd_ResWordCode = MyParser::cmdcod_none;        // no command is being executed
                }
            }

            else if ( isRightPar ) {
                // -------------------------------------
                // Process right parenthesis token
                // -------------------------------------

#if debugPrint
                Serial.print( "right parenthesis: stack level " ); Serial.println( evalStack.getElementCount() );
#endif
                {   // start block (required for variable definitions inside)
                    int argCount = 0;                                                // init number of supplied arguments (or array subscripts) to 0
                    LE_evalStack* pstackLvl = _pEvalStackTop;     // stack level of last argument / array subscript before right parenthesis, or left parenthesis (if function call and no arguments supplied)

                    while ( (pstackLvl->genericToken.tokenType != tok_isTerminalGroup1) && (pstackLvl->genericToken.tokenType != tok_isTerminalGroup2) && (pstackLvl->genericToken.tokenType != tok_isTerminalGroup3) ) {
                        // terminal found: continue until left parenthesis
                        if ( MyParser::_terminals [pstackLvl->terminal.index].terminalCode == MyParser::termcod_leftPar ) { break; }   // continue until left parenthesis found
                        pstackLvl = (LE_evalStack*) evalStack.getPrevListElement( pstackLvl );
                        argCount++;
                    }
                    LE_evalStack* pPrecedingStackLvl = (LE_evalStack*) evalStack.getPrevListElement( pstackLvl );     // stack level PRECEDING left parenthesis (or null pointer)

                    // remove left parenthesis stack level
                    pstackLvl = (LE_evalStack*) evalStack.deleteListElement( pstackLvl );                            // pstackLvl now pointing to first function argument or array subscript

                    // correct pointers (now wrong, if only one or 2 arguments)
                    _pEvalStackMinus1 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackTop );
                    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );

                    // execute internal or external function, calculate array element address or remove parenthesis around single argument (if no function or array)
                    execResult = execParenthesesPair( pPrecedingStackLvl, pstackLvl, argCount );
                    if ( execResult != result_execOK ) { break; }

                    // the left parenthesis and the argument(s) are now removed and replaced by a single scalar (function result, array element, single argument)
                    // check if additional operators preceding the left parenthesis can now be executed.
                    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
                    execResult = execAllProcessedOperators();
                    if ( execResult != result_execOK ) { break; }

                }
            }

            break;

        }   // end 'switch (tokenType)'


        // if execution error: print current instruction being executed, signal error and exit
        // -----------------------------------------------------------------------------------

        if ( execResult != result_execOK ) {
            int sourceErrorPos;
            _pConsole->print( "\r\n  " );
            _pmyParser->prettyPrintInstructions( true, _activeFunctionData.errorStatementStartStep, _activeFunctionData.errorProgramCounter, &sourceErrorPos );
            _pConsole->print( "  " ); for ( int i = 1; i <= sourceErrorPos; i++ ) { _pConsole->print( " " ); }
            char execInfo [100];
            if ( _programCounter >= _programStart ) { sprintf( execInfo, "^ Exec error %d\r\n", execResult ); }     // in main program level 
            else { sprintf( execInfo, "^ Exec error %d in user function %s\r\n", execResult, extFunctionNames [_activeFunctionData.functionIndex] ); }
            _pConsole->print( execInfo );
            lastValueIsStored = false;              // prevent printing last result (if any)
            break;
        }

        // advance to next token
        _programCounter = _activeFunctionData.pNextStep;         // note: will be altered when calling an external function and upon return of a called function
        tokenType = *_activeFunctionData.pNextStep & 0x0F;                                                               // next token type (could be token within caller, if returning now)
        if ( nextIsNewInstructionStart ) {
            _activeFunctionData.errorStatementStartStep = _programCounter;
            _activeFunctionData.errorStatementStartStep = _programCounter;

            nextIsNewInstructionStart = false;
        }  // statement start (for pretty print only)    

    }   // end 'while ( tokenType != tok_no_token )'                                                                                       // end 'while ( tokenType != tok_no_token )'


    // All tokens processed: finalize
    // ------------------------------
    if ( lastValueIsStored ) {             // did the execution produce a result ?
        // print last result
        char s [MyParser::_maxAlphaCstLen + 10];  // note: with small '_maxAlphaCstLen' values, make sure string is also long enough to print real values
        if ( lastResultTypeFiFo [0] == value_isFloat ) { sprintf( s, "  %.3G", lastResultValueFiFo [0].realConst ); }
        else { sprintf( s, "  %s", (lastResultValueFiFo [0].pStringConst == nullptr) ? "" : lastResultValueFiFo [0].pStringConst ); }    // immediate mode: print evaluation result
        _pConsole->println( s );
    }

    // Delete any intermediate result string objects used as arguments, delete remaining evaluation stack level objects 
    clearEvalStack();               // and intermediate strings
    clearFlowCtrlStack();           // and remaining local storage + local variable string and array values

    return execResult;   // return result, in case it's needed by caller
};


// -----------------------------------
// *   execute a processed command   *
// -----------------------------------

Interpreter::execResult_type Interpreter::execprocessedCommand( char activeCmd_ResWordCode, char* activeCmd_resWordTokenAddress ) {

    execResult_type execResult = result_execOK;
    int cmdParamCount = evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels;
    switch ( activeCmd_ResWordCode ) {

    case MyParser::cmdcod_if:                                                                           // 'if' command
        _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
        _pFlowCtrlStackTop = (IfBlockData*) flowCtrlStack.appendListElement( sizeof( IfBlockData ) );
        ((IfBlockData*) _pFlowCtrlStackTop)->blockType = MyParser::block_if;                            // start of 'if...end' block

    // no break here: from here on, subsequent execution is common for 'if', 'elseif' and 'else'

    case MyParser::cmdcod_else:
    case MyParser::cmdcod_elseif:
    {
        bool testClauseCondition { false }, fail { false };// init
        if ( activeCmd_ResWordCode == MyParser::cmdcod_if ) { testClauseCondition = true; }
        else { testClauseCondition = (bool) (((IfBlockData*) _pFlowCtrlStackTop)->testResult); }                     // retrieve result of previous test (in preceding 'if' or 'elseif' clause)

        fail = !testClauseCondition;                                                                                    // init: assume test in preceding clause passed ('if' or 'elseif'), so this clause needs to be skipped 
        if ( activeCmd_ResWordCode != MyParser::cmdcod_else ) {
            if ( testClauseCondition ) {                                                                                // result of test in preceding 'if' or 'elseif' clause FAILED ? Check this clause
                Val operand;                                                                                            // operand and result
                bool operandIsVarRef = (_pEvalStackTop->varOrConst.valueType == value_isVarRef);
                char valueType = operandIsVarRef ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
                if ( valueType != value_isFloat ) { execResult = result_testexpr_numberExpected; return execResult; }
                bool operandIsReal = (valueType == value_isFloat);
                operand.realConst = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable) ? *_pEvalStackTop->varOrConst.value.pRealConst : _pEvalStackTop->varOrConst.value.realConst;

                _activeFunctionData.errorProgramCounter = _pEvalStackTop->terminal.tokenAddress;                     // in the event of an error
                fail = (operand.realConst == 0);                                                                        // current test (elseif clause)
                ((IfBlockData*) _pFlowCtrlStackTop)->testResult = (char) fail;                                          // remember test result (true -> 0x1)
            }
        }

        Interpreter::TokenIsResWord* pToken;
        int toTokenStep { 0 };
        if ( fail ) {                                                                                  // skip this clause ? (either a preceding tets passed, or it failed but the curreent test failed as well)
            pToken = (Interpreter::TokenIsResWord*) activeCmd_resWordTokenAddress;
            memcpy( &toTokenStep, pToken->toTokenStep, sizeof( char [2] ) );
            _activeFunctionData.pNextStep = _programStorage + toTokenStep;              // prepare jump to else, elseif or end command
        }

        clearEvalStackLevels( cmdParamCount );      // clear evaluation stack
    }
    break;


    case MyParser::cmdcod_end:              // determine currently open block
    {
        char blockType = *(char*) _pFlowCtrlStackTop;
        if ( (blockType == MyParser::block_if) || (blockType == MyParser::block_while) || (blockType == MyParser::block_for) ) {
            flowCtrlStack.deleteListElement( _pFlowCtrlStackTop );
            _pFlowCtrlStackTop = _pFlowCtrlStackMinus1;
            _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement( _pFlowCtrlStackTop );
            _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement( _pFlowCtrlStackMinus1 );
            break;
        }
    }

    // no break here: from here on, subsequent execution is the same for 'end' and for 'return'

    case MyParser::cmdcod_return:
    {
        bool returnWithZero = (cmdParamCount == 0);                    // RETURN statement without expression, or END statement: return a zero
        execResult = terminateExternalFunction( returnWithZero );
        if ( execResult != result_execOK ) { return execResult; }
    }
    break;

    }

    return result_execOK;
}

// -----------------------------------------------------------------------------------------------
// *   jump n token steps, return token type and (for terminals and reserved words) token code   *
// -----------------------------------------------------------------------------------------------

// optional parameter not allowed with reference parameter: create separate entry
int Interpreter::jumpTokens( int n ) {
    int tokenCode;
    char* pStep;
    return jumpTokens( n, pStep, tokenCode );
}

int Interpreter::jumpTokens( int n, char*& pStep ) {
    int tokenCode;
    return jumpTokens( n, pStep, tokenCode );
}

int Interpreter::jumpTokens( int n, char*& pStep, int& tokenCode ) {

    // pStep: pointer to first token to test versus token group and (if applicable) token code
    // n: number of tokens to jump
    // return 'tok_no_token' if not enough tokens are present 

    int tokenType = tok_no_token;

    for ( int i = 1; i <= n; i++ ) {
        tokenType = *pStep & 0x0F;
        if ( tokenType == tok_no_token ) { return tok_no_token; }               // end of program reached
        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*pStep >> 4) & 0x0F;        // fetch next token 
        pStep = pStep + tokenLength;
    }

    tokenType = *pStep & 0x0F;
    int tokenIndex;

    switch ( tokenType ) {
    case tok_isReservedWord:
        tokenIndex = (((TokenIsResWord*) pStep)->tokenIndex);
        tokenCode = MyParser::_resWords [tokenIndex].resWordCode;
        break;

    case tok_isTerminalGroup1:
    case tok_isTerminalGroup2:
    case tok_isTerminalGroup3:
        tokenIndex = ((((TokenIsTerminal*) pStep)->tokenTypeAndIndex >> 4) & 0x0F);
        tokenIndex += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
        tokenCode = MyParser::_terminals [tokenIndex].terminalCode;
        break;

    default:
        break;

    }

    return tokenType;
}

// ------------------------------------
// *   advance until specific token   *
// ------------------------------------

int Interpreter::findTokenStep( int tokenTypeToFind, char tokenCodeToFind, char*& pStep ) {

    // pStep: pointer to first token to test versus token group and (if applicable) token code
    // tokenType: if 'tok_isTerminalGroup1', test for the three terminal groups !

    // exclude current token step
    int tokenType = *pStep & 0x0F;
    int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*pStep >> 4) & 0x0F;        // fetch next token 
    pStep = pStep + tokenLength;

    do {
        tokenType = *pStep & 0x0F;
        bool tokenTypeMatch = (tokenTypeToFind == tokenType);
        if ( tokenTypeToFind == tok_isTerminalGroup1 ) { tokenTypeMatch = tokenTypeMatch || (tokenType == tok_isTerminalGroup2) || (tokenType == tok_isTerminalGroup3); }
        if ( tokenTypeMatch ) {
            bool tokenCodeMatch { false };
            int tokenIndex;

            switch ( tokenTypeToFind ) {
            case tok_isReservedWord:
                tokenIndex = (((TokenIsResWord*) pStep)->tokenIndex);
                tokenCodeMatch = MyParser::_resWords [tokenIndex].resWordCode == tokenCodeToFind;
                break;

            case tok_isTerminalGroup1:       // actual token can be part of any of the three terminal groups
                tokenIndex = ((((TokenIsTerminal*) pStep)->tokenTypeAndIndex >> 4) & 0x0F);
                tokenIndex += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
                tokenCodeMatch = MyParser::_terminals [tokenIndex].terminalCode == tokenCodeToFind;
                break;

            case tok_no_token:
                return tokenType;       // token not found
                break;

            default:
                break;
            }
            if ( tokenCodeMatch ) { return tokenType; }      // if terminal, then return exact group (entry: use terminalGroup1) 
        }

        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? 1 : (*pStep >> 4) & 0x0F;        // fetch next token 
        pStep = pStep + tokenLength;
    } while ( true );
}


// ------------------------------------------------
// Save last value for future reuse by calculations 
// ------------------------------------------------

void Interpreter::saveLastValue( bool& overWritePrevious ) {
    if ( !(evalStack.getElementCount() > _activeFunctionData.callerEvalStackLevels) ) { return; }           // safety: data available ?

    // if overwrite 'previous' last result, then replace first item if there is one, otherwise replace last item if FiFo full (-1 if nothing to replace)
    int itemToRemove = overWritePrevious ? ((_lastResultCount >= 1) ? 0 : -1) :
        ((_lastResultCount == MAX_LAST_RESULT_DEPTH) ? MAX_LAST_RESULT_DEPTH - 1 : -1);

    // remove a previous item ?
    if ( itemToRemove != -1 ) {
        // if item to remove is a string: delete heap object
        if ( lastResultTypeFiFo [itemToRemove] == value_isStringPointer ) {

            if ( lastResultValueFiFo [itemToRemove].pStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
                Serial.print( "----- (FiFo string) " );   Serial.println( (uint32_t) lastResultValueFiFo [itemToRemove].pStringConst - RAMSTART );
#endif 
                // note: this is always an intermediate string
                delete [] lastResultValueFiFo [itemToRemove].pStringConst;
                lastValuesStringObjectCount--;
            }
        }
    }
    else {
        _lastResultCount++;     // only adding an item, without removing previous one
    }


    // move older last results one place up in FIFO, except when just overwriting 'previous' last result
    if ( !overWritePrevious && (_lastResultCount > 1) ) {       // if 'new' last result count is 1, no old results need to be moved  
        for ( int i = _lastResultCount - 1; i > 0; i-- ) {
            lastResultValueFiFo [i] = lastResultValueFiFo [i - 1];
            lastResultTypeFiFo [i] = lastResultTypeFiFo [i - 1];
        }
    }



    // store new last value
    VarOrConstLvl lastvalue;
    bool lastValueReal = (_pEvalStackTop->varOrConst.valueType == value_isFloat);
    bool lastValueIntermediate = ((_pEvalStackTop->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate);

    if ( lastValueReal ) { lastvalue.value.realConst = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pEvalStackTop->varOrConst.value.pRealConst) : _pEvalStackTop->varOrConst.value.realConst; }
    else { lastvalue.value.pStringConst = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pEvalStackTop->varOrConst.value.ppStringConst) : _pEvalStackTop->varOrConst.value.pStringConst; }

    if ( (lastValueReal) || (!lastValueReal && (lastvalue.value.pStringConst == nullptr)) ) {
        lastResultValueFiFo [0] = lastvalue.value;
    }
    // new last value is a non-empty string: make a copy of the string and store a reference to this new string
    else {
        int stringlen = min( strlen( lastvalue.value.pStringConst ), MyParser::_maxAlphaCstLen );        // excluding terminating \0
        lastResultValueFiFo [0].pStringConst = new char [stringlen + 1];
        lastValuesStringObjectCount++;
        memcpy( lastResultValueFiFo [0].pStringConst, lastvalue.value.pStringConst, stringlen );        // copy the actual string (not the pointer); do not use strcpy
        lastResultValueFiFo [0].pStringConst [stringlen] = '\0';
#if printCreateDeleteHeapObjects
        Serial.print( "+++++ (FiFo string) " );   Serial.println( (uint32_t) lastResultValueFiFo [0].pStringConst - RAMSTART );
#endif            

        if ( lastValueIntermediate ) {
#if printCreateDeleteHeapObjects
            Serial.print( "----- (intermd str) " );   Serial.println( (uint32_t) lastvalue.value.pStringConst - RAMSTART );
#endif
            delete [] lastvalue.value.pStringConst;
            intermediateStringObjectCount--;
        }
    }

    // store new last value type
    lastResultTypeFiFo [0] = _pEvalStackTop->varOrConst.valueType;               // value type

    // delete the stack level containing the result
    evalStack.deleteListElement( _pEvalStackTop );
    _pEvalStackTop = _pEvalStackMinus1;
    _pEvalStackMinus1 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackTop );
    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );

    overWritePrevious = true;

    return;
}

// --------------------------------------------------------------------------
// Clear n evaluation stack levels and associated intermediate string objects  
// --------------------------------------------------------------------------

void Interpreter::clearEvalStackLevels( int n ) {

    if ( n == 0 ) { return; }             // nothing to do

    LE_evalStack* pstackLvl;
    for ( int i = 1; i <= n; i++ ) {
        pstackLvl = _pEvalStackTop;
        if ( ((pstackLvl->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate) &&
            (pstackLvl->varOrConst.valueType == value_isStringPointer) ) {
            if ( pstackLvl->varOrConst.value.pStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
                Serial.print( "----- (intermd str) " );   Serial.println( (uint32_t) _pEvalStackTop->varOrConst.value.pStringConst - RAMSTART );
#endif
                delete [] pstackLvl->varOrConst.value.pStringConst;
                intermediateStringObjectCount--;
            }
        }

        evalStack.deleteListElement( pstackLvl );
        _pEvalStackTop = _pEvalStackMinus1;
        _pEvalStackMinus1 = _pEvalStackMinus2;
    }

    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );
    return;

    // delete the stack level containing the result
    _pEvalStackTop = _pEvalStackMinus1;
    _pEvalStackMinus1 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackTop );
    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );
    return;
}

// -----------------------
// Clear evaluation stack  
// -----------------------

void Interpreter::clearEvalStack() {                // and intermediate strings

    // clear evaluation stack and all associated temporary objects

    // delete any intermediate result string objects used as arguments
    LE_evalStack* pstackLvl = _pEvalStackTop;
    while ( pstackLvl != nullptr ) {
        if ( pstackLvl->genericToken.tokenType == tok_isConstant ) {            // needed to exclude non-value tokens (terminals, reserved words, functions, ...)
            if ( ((pstackLvl->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate) && (pstackLvl->varOrConst.valueType == value_isStringPointer) )
            {
                if ( pstackLvl->varOrConst.value.pStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
                    Serial.print( "----- (Intermd str) " );   Serial.println( (uint32_t) pstackLvl->varOrConst.value.pStringConst - RAMSTART );
#endif 
                    delete [] pstackLvl->varOrConst.value.pStringConst;
                    intermediateStringObjectCount--;
                }
            }
        }
        pstackLvl = (LE_evalStack*) evalStack.getPrevListElement( pstackLvl );
    };

    // error if not all intermediate string objects deleted (points to an internal Justina issue)
    if ( intermediateStringObjectCount != 0 ) {
        Serial.print( "*** Intermediate string cleanup error. Remaining: " ); Serial.println( intermediateStringObjectCount );
    }

    // delete all remaining stack level objects 

    evalStack.deleteList();
    _pEvalStackTop = nullptr;  _pEvalStackMinus1 = nullptr; _pEvalStackMinus2 = nullptr;

    return;
}


// -----------------------
// Clear evaluation stack  
// -----------------------

void Interpreter::clearFlowCtrlStack() {                // and remaining local storage + local variable string and array values for open functions

    void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;

    while ( pFlowCtrlStackLvl != nullptr ) {
        char blockType = *(char*) _pFlowCtrlStackTop;

        if ( blockType == MyParser::block_extFunction ) {               // open function
            int localVarCount = extFunctionData [_activeFunctionData.functionIndex].localVarCountInFunction;

            if ( localVarCount > 0 ) {
                _pmyParser->deleteArrayElementStringObjects( _activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true );
                _pmyParser->deleteVariableValueObjects( _activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true );

                // release local variable storage for function that has been called
                delete [] _activeFunctionData.pLocalVarValues;
                delete [] _activeFunctionData.pVariableAttributes;
                delete [] _activeFunctionData.ppSourceVarTypes;
            }

            pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement( pFlowCtrlStackLvl );
            if ( pFlowCtrlStackLvl == nullptr ) { break; }         // all done

            // load local storage pointers again for deepest CALLER function and restore pending step & active function information for caller function
            _activeFunctionData = *(FunctionData*) _pFlowCtrlStackTop;
        }
        else { pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement( pFlowCtrlStackLvl ); }
    }

    flowCtrlStack.deleteList();
    _pFlowCtrlStackTop = nullptr;   _pFlowCtrlStackMinus2 = nullptr; _pFlowCtrlStackMinus1 = nullptr;
}


// ------------------------------------------------------------------------------
// Remove operands / function arguments / array subscripts from evaluation stack
// ------------------------------------------------------------------------------

void Interpreter::deleteStackArguments( LE_evalStack* pPrecedingStackLvl, int argCount, bool includePreceding ) {

    // Delete any intermediate result string objects used as operands 
    // --------------------------------------------------------------

    LE_evalStack* pStackLvl = (LE_evalStack*) evalStack.getNextListElement( pPrecedingStackLvl );   // array subscripts or function arguments (NOT the preceding list element) 
    do {
        // stack levels contain variables and (interim) constants only
        if ( ((pStackLvl->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate) && (pStackLvl->varOrConst.valueType == value_isStringPointer) ) {
            if ( pStackLvl->varOrConst.value.pStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
                Serial.print( "----- (Intermd str) " );   Serial.println( (uint32_t) pStackLvl->varOrConst.value.pStringConst - RAMSTART );
#endif
                delete [] pStackLvl->varOrConst.value.pStringConst;
                intermediateStringObjectCount--;
            }
        }
        pStackLvl = (LE_evalStack*) evalStack.getNextListElement( pStackLvl );  // next dimspec or null pointer 

    } while ( pStackLvl != nullptr );


    // cleanup stack
    // -------------

    // set pointer to either first token (value) after opening parenthesis (includePreceding = false -> used if array subscripts), or
    // last token (function name) before opening parenthesis (includePreceding = true -> used if calling function)
    // note that the left parenthesis is already removed from stack at this stage
    pStackLvl = includePreceding ? pPrecedingStackLvl : (LE_evalStack*) evalStack.getNextListElement( pPrecedingStackLvl );
    _pEvalStackTop = pPrecedingStackLvl;                                                        // note down before deleting list levels
    _pEvalStackMinus1 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackTop );
    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );
    while ( pStackLvl != nullptr ) { pStackLvl = (LE_evalStack*) evalStack.deleteListElement( pStackLvl ); }

    return;
}


// --------------------------------------------------------------------------------------------------------------------------
// *   execute internal or external function, calculate array element address or remove parenthesis around single argument  *
// --------------------------------------------------------------------------------------------------------------------------

Interpreter::execResult_type Interpreter::execParenthesesPair( LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& firstArgStackLvl, int argCount ) {
    // perform internal or external function, calculate array element address or simply make an expression result within parentheses an intermediate constant

    // no lower stack levels before left parenthesis (removed in the meantime) ? Is a simple parentheses pair
    if ( pPrecedingStackLvl == nullptr ) {
            makeIntermediateConstant( _pEvalStackTop );                     // left parenthesis already removed from evaluation stack
            return result_execOK;
    }

    // stack level preceding left parenthesis is internal function ? execute function
    else if ( pPrecedingStackLvl->genericToken.tokenType == tok_isInternFunction ) {
        execResult_type execResult = result_execOK;//// vervang door call naar int func exec
        return execResult;
    }

    // stack level preceding left parenthesis is external function ? execute function
    else if ( pPrecedingStackLvl->genericToken.tokenType == tok_isExternFunction ) {
        execResult_type execResult = launchExternalFunction( pPrecedingStackLvl, firstArgStackLvl, argCount );    // exec only when next instruction is fetched !
        return execResult;
    }

    // stack level preceding left parenthesis is an array variable name AND it requires an array element ?
    // (if it is a variable name, it can still be an array name used as previous argument in a function call)
    else if ( pPrecedingStackLvl->genericToken.tokenType == tok_isVariable ) {
        if ( (pPrecedingStackLvl->varOrConst.variableAttributes & var_isArray_pendingSubscripts) == var_isArray_pendingSubscripts ) {
            execResult_type execResult = arrayAndSubscriptsToarrayElement( pPrecedingStackLvl, firstArgStackLvl, argCount );
            return execResult;
        }
    }

    // none of the te above: simple parenthesis pair ? If variable inside, make it an intermediate constant on the stack 
        makeIntermediateConstant( _pEvalStackTop );                     // left parenthesis already removed from evaluation stack
        return result_execOK;
}

// ------------------------------------------------------------------------------------------------------------------
// *   replace array variable base address and subscripts with the array element address on the evaluation stack   *
// ------------------------------------------------------------------------------------------------------------------

Interpreter::execResult_type Interpreter::arrayAndSubscriptsToarrayElement( LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pStackLvl, int argCount ) {
    void* pArray = *pPrecedingStackLvl->varOrConst.value.ppArray;
    _activeFunctionData.errorProgramCounter = pPrecedingStackLvl->varOrConst.tokenAddress;                // token adress of array name (in the event of an error)

    int elemSpec [4] = { 0 ,0,0,0 };
    do {
        bool opReal = (pStackLvl->varOrConst.valueType == value_isFloat);
        if ( opReal ) {
            //note: elemSpec [3] (last array element) only used here as a counter
            elemSpec [elemSpec [3]] = (pStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pStackLvl->varOrConst.value.pRealConst) : pStackLvl->varOrConst.value.realConst;
        }
        else { return result_numberExpected; }

        pStackLvl = (LE_evalStack*) evalStack.getNextListElement( pStackLvl );
    } while ( ++elemSpec [3] < argCount );


    // calculate array element address and replace array base address with it in stack
    // -------------------------------------------------------------------------------

    // dim count test only needed for function parameters receiving arrays: dimension count not yet known during parsing (should always equal caller's array dim count) 

    int arrayDimCount = ((char*) pArray) [3];
    if ( arrayDimCount != argCount ) { return result_array_paramUseWrongDimCount; }

    void* pArrayElem = arrayElemAddress( pArray, elemSpec );
    if ( pArrayElem == nullptr ) { return result_array_outsideBounds; }

    pPrecedingStackLvl->varOrConst.value.pVariable = pArrayElem;
    pPrecedingStackLvl->varOrConst.variableAttributes &= ~var_isArray_pendingSubscripts;           // remove 'pending subscripts' flag 
    // note: other data does not change (array attributes, value type, token type, intermediate constant, variable type address)


    // Remove array subscripts from evaluation stack
    // ----------------------------------------------

    deleteStackArguments( pPrecedingStackLvl, argCount, false );

    return result_execOK;
}

// -----------------------------------------------------
// *   turn stack operand into intermediate constant   *
// -----------------------------------------------------

void Interpreter::makeIntermediateConstant( LE_evalStack* pEvalStackLvl ) {
    // if a (scalar) variable or a parsed constant: replace by an intermediate constant

    if ( (pEvalStackLvl->varOrConst.valueAttributes & constIsIntermediate) == 0 ) {                    // not an intermediate constant (variable or parsed constant)
        Val operand, result;                                                               // operands and result
        bool operandIsVarRef = (pEvalStackLvl->varOrConst.valueType == value_isVarRef);
        char valueType = operandIsVarRef ? (*pEvalStackLvl->varOrConst.varTypeAddress & value_typeMask) : pEvalStackLvl->varOrConst.valueType;
        bool opReal = (valueType == value_isFloat);
        if ( opReal ) { operand.realConst = (pEvalStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pEvalStackLvl->varOrConst.value.pRealConst) : pEvalStackLvl->varOrConst.value.realConst; }
        else { operand.pStringConst = (pEvalStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pEvalStackLvl->varOrConst.value.ppStringConst) : pEvalStackLvl->varOrConst.value.pStringConst; }

        // if the value (parsed constant or variable value) is a non-empty string value, make a copy of the character string and store a pointer to this copy as result
        // as the operand is not an intermediate constant, NO intermediate string object (if it's a string) needs to be deleted
        if ( opReal || (!opReal && ((operand.pStringConst == nullptr))) ) {
            result = operand;
        }
        else {
            int stringlen = strlen( operand.pStringConst );
            result.pStringConst = new char [stringlen + 1];
            intermediateStringObjectCount++;
            strcpy( result.pStringConst, operand.pStringConst );        // copy the actual strings 
#if printCreateDeleteHeapObjects
            Serial.print( "+++++ (Intermd str) " );   Serial.println( (uint32_t) result.pStringConst - RAMSTART );

#endif
        }
        pEvalStackLvl->varOrConst.value = result;                        // float or pointer to string (type: no change)
        pEvalStackLvl->varOrConst.valueType=valueType;
        pEvalStackLvl->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        pEvalStackLvl->varOrConst.valueAttributes = constIsIntermediate;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        pEvalStackLvl->varOrConst.variableAttributes = 0x00;                  // not an array, not an array element (it's a constant) 
    }
}


// ----------------------------------------------
// *   execute all processed infix operations   *
// ----------------------------------------------

Interpreter::execResult_type  Interpreter::execAllProcessedOperators() {            // prefix and infix

    // _pEvalStackTop should point to an operand on entry (parsed constant, variable, expression result)

    int pendingTokenType, pendingTokenIndex;
    int pendingTokenPriorityLvl;
    bool currentOpHasPriority;

#if debugPrint
    Serial.print( "** exec processed infix operators -stack levels: " ); Serial.println( evalStack.getElementCount() );
#endif
    // check if (an) operation(s) can be executed 
    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
    while ( evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 2 ) {                                                      // a previous operator might exist

        bool minus1IsTerminal = (_pEvalStackMinus1->genericToken.tokenType == tok_isTerminalGroup1) || (_pEvalStackMinus1->genericToken.tokenType == tok_isTerminalGroup2)
            || (_pEvalStackMinus1->genericToken.tokenType == tok_isTerminalGroup2);
        bool minus1IsOperator = (MyParser::_terminals [_pEvalStackMinus1->terminal.index].terminalCode <= MyParser::termcod_opRangeEnd);

        if ( minus1IsOperator ) {
            // check pending (not yet processed) token (always present and always a terminal token after a variable or constant token)
            // pending token can be any terminal token: infix operator, left or right parenthesis, comma or semicolon 
            // it can not be a prefix operator because it follows an operand (on top of stack)
            pendingTokenType = *_activeFunctionData.pNextStep & 0x0F;                                    // there's always minimum one token pending (even if it is a semicolon)
            pendingTokenIndex = (*_activeFunctionData.pNextStep >> 4) & 0x0F;                            // terminal token only: index stored in high 4 bits of token type 
            pendingTokenIndex += ((pendingTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (pendingTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);

            // infix operation ?
            bool isPrefixOperator = true;             // init as prefix operation
            if ( evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 2 ) {         // already a token on the stack ?               
                bool minus2IsTerminal = ((_pEvalStackMinus2->genericToken.tokenType == tok_isTerminalGroup1) ||
                    (_pEvalStackMinus2->genericToken.tokenType == tok_isTerminalGroup2) || (_pEvalStackMinus2->genericToken.tokenType == tok_isTerminalGroup3));
                bool minus2IsRightPar = (MyParser::_terminals [_pEvalStackMinus2->terminal.index].terminalCode == MyParser::termcod_rightPar);
                isPrefixOperator = !((_pEvalStackMinus2->genericToken.tokenType == tok_isConstant) || (_pEvalStackMinus2->genericToken.tokenType == tok_isVariable)
                    || minus2IsRightPar);
            }

            // check priority and associativity
            int priority = _pmyParser->_terminals [_pEvalStackMinus1->terminal.index].priority;
            if ( isPrefixOperator ) { priority = priority >> 4; }
            priority &= 0x0F;

            int associativity = _pmyParser->_terminals [_pEvalStackMinus1->terminal.index].associativity;
            if ( isPrefixOperator ) { associativity = associativity >> 4; }
            associativity &= _pmyParser->trm_assocRtoL;

            /*
                        Serial.print( "*** read terminal index: " ); Serial.print( (int) _pEvalStackMinus1->terminal.index );
                        Serial.print( ", priority " ); Serial.print( (int) priority );
                        Serial.print( ", associativity " ); Serial.print( (int) associativity );
                        Serial.println( isPrefixOperator ? " - exec prefix" : " - exec infix" );
            */
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
    _activeFunctionData.errorProgramCounter = _pEvalStackMinus1->terminal.tokenAddress;                // in the event of an error

    bool operandIsVarRef = (_pEvalStackTop->varOrConst.valueType == value_isVarRef);
    char valueType = operandIsVarRef ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
    bool operandIsReal = (valueType == value_isFloat);
    if ( valueType != value_isFloat ) { return result_numberExpected; }

    operand.realConst = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable) ? *_pEvalStackTop->varOrConst.value.pRealConst : _pEvalStackTop->varOrConst.value.realConst;

    if ( _pmyParser->_terminals [_pEvalStackMinus1->terminal.index].terminalCode == _pmyParser->termcod_minus ) { operand.realConst = -operand.realConst; } // prefix '-' ('+': no change in value)

    // negation of a floating point value can not produce an error: no checks needed

    //  store result in stack (if not yet, then it becomes an intermediate constant now)
    _pEvalStackTop->varOrConst.value = operand;
    _pEvalStackTop->varOrConst.valueType = valueType;                   // real or string
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
    _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    _pEvalStackTop->varOrConst.variableAttributes = 0x00;                  // not an array, not an array element (it's a constant) 


    //  clean up stack (drop prefix operator)
    // --------------------------------------

    evalStack.deleteListElement( _pEvalStackMinus1 );
    _pEvalStackMinus1 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackTop );
    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );


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
    bool operand1IsVarRef = (_pEvalStackMinus2->varOrConst.valueType == value_isVarRef);
    char operandValueType = operand1IsVarRef ? (*_pEvalStackMinus2->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackMinus2->varOrConst.valueType;
    bool op1real = ((uint8_t) operandValueType == value_isFloat);

    bool operand2IsVarRef = (_pEvalStackTop->varOrConst.valueType == value_isVarRef);
    operandValueType = operand2IsVarRef ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
    bool op2real = ((uint8_t) operandValueType == value_isFloat);

    int operatorCode = _pmyParser->_terminals [_pEvalStackMinus1->terminal.index].terminalCode;

    bool isUserVar, isGlobalVar, isStaticVar, isLocalVar;

    // check if operands are compatible with operator: real for all operators except string concatenation
    _activeFunctionData.errorProgramCounter = _pEvalStackMinus1->terminal.tokenAddress;                // in the event of an error

    if ( operatorCode != _pmyParser->termcod_assign ) {                                           // not an assignment ?
        if ( (operatorCode == _pmyParser->termcod_concat) && (op1real || op2real) ) { return result_stringExpected; }
        else if ( (operatorCode != _pmyParser->termcod_concat) && (!op1real || !op2real) ) { return result_numberExpected; }
    }
    else {                                                                                  // assignment 
        if ( _pEvalStackMinus2->varOrConst.variableAttributes & var_isArray ) {        // asignment to array element: value type cannot change
            if ( op1real != op2real ) { return result_array_valueTypeIsFixed; }
        }

        // note that for reference variables, the variable type fetched is the SOURCE variable type
        isUserVar = ((_pEvalStackMinus2->varOrConst.variableAttributes & var_scopeMask) == var_isUser);
        isGlobalVar = ((_pEvalStackMinus2->varOrConst.variableAttributes & var_scopeMask) == var_isGlobal);
        isStaticVar = ((_pEvalStackMinus2->varOrConst.variableAttributes & var_scopeMask) == var_isStaticInFunc);
        isLocalVar = ((_pEvalStackMinus2->varOrConst.variableAttributes & var_scopeMask) == var_isLocalInFunc);            // but not function parameter definitions
    }

    // mixed operands not allowed; 2 x real -> real; 2 x string -> string: set result value type to operand 2 value type (assignment: current operand 1 value type is not relevant)
    bool opResultReal = op2real;                                                                    // do NOT set to operand 1 value type (would not work in case of assignment)

    // fetch operands: real constants or pointers to character strings
    if ( op1real ) { operand1.realConst = (_pEvalStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pEvalStackMinus2->varOrConst.value.pRealConst) : _pEvalStackMinus2->varOrConst.value.realConst; }
    else { operand1.pStringConst = (_pEvalStackMinus2->varOrConst.tokenType == tok_isVariable) ? (*_pEvalStackMinus2->varOrConst.value.ppStringConst) : _pEvalStackMinus2->varOrConst.value.pStringConst; }
    if ( op2real ) { operand2.realConst = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pEvalStackTop->varOrConst.value.pRealConst) : _pEvalStackTop->varOrConst.value.realConst; }
    else { operand2.pStringConst = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable) ? (*_pEvalStackTop->varOrConst.value.ppStringConst) : _pEvalStackTop->varOrConst.value.pStringConst; }

    bool op1emptyString = op1real ? false : (operand1.pStringConst == nullptr);
    bool op2emptyString = op2real ? false : (operand2.pStringConst == nullptr);

    int stringlen;                                                                                  // define outside switch statement

    switch ( operatorCode ) {                                                  // operation to execute


    case MyParser::termcod_assign:
        // Case: execute assignment (only possible if first operand is a variable: checked during parsing)
        // -----------------------------------------------------------------------------------------------

        // determine variable scope

        if ( !op1real )                                                                             // if receiving variable currently holds a string, delete current char string object
        {
            // delete variable string object
            if ( *_pEvalStackMinus2->varOrConst.value.ppStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
                Serial.print( isUserVar ? "----- (usr var str) " : (isGlobalVar || isStaticVar) ? "----- (var string ) " : "----- (loc var str) " );
                Serial.println( (uint32_t) *_pEvalStackMinus2->varOrConst.value.ppStringConst - RAMSTART );
#endif
                delete [] * _pEvalStackMinus2->varOrConst.value.ppStringConst;
                isUserVar ? userVarStringObjectCount-- : (isGlobalVar || isStaticVar) ? globalStaticVarStringObjectCount-- : localVarStringObjectCount--;
            }
        }

        // if the value to be assigned is real (float) OR an empty string: simply assign the value (not a heap object)

        if ( op2real || op2emptyString ) {
            opResult = operand2;
        }
        // the value (parsed constant, variable value or intermediate result) to be assigned to the receiving variable is a non-empty string value
        else {
            // make a copy of the character string and store a pointer to this copy as result (even if operand string is already an intermediate constant)
            // because the value will be stored in a variable, limit to the maximum allowed string length
            stringlen = min( strlen( operand2.pStringConst ), MyParser::_maxAlphaCstLen );
            opResult.pStringConst = new char [stringlen + 1];
            isUserVar ? userVarStringObjectCount++ : (isGlobalVar || isStaticVar) ? globalStaticVarStringObjectCount++ : localVarStringObjectCount++;
            memcpy( opResult.pStringConst, operand2.pStringConst, stringlen );        // copy the actual string (not the pointer); do not use strcpy
            opResult.pStringConst [stringlen] = '\0';                                         // add terminating \0
#if printCreateDeleteHeapObjects
            Serial.print( isUserVar ? "+++++ (usr var str) " : (isGlobalVar || isStaticVar) ? "+++++ (var string ) " : "+++++ (loc var str) " );
            Serial.println( (uint32_t) opResult.pStringConst - RAMSTART );
#endif
        }

        // store value and value type in variable and adapt variable value type
        if ( opResultReal ) { *_pEvalStackMinus2->varOrConst.value.pRealConst = opResult.realConst; }
        else { *_pEvalStackMinus2->varOrConst.value.ppStringConst = opResult.pStringConst; }
        // save resulting var type (in case it changed)
        *_pEvalStackMinus2->varOrConst.varTypeAddress = (*_pEvalStackMinus2->varOrConst.varTypeAddress & ~value_typeMask) | (opResultReal ? value_isFloat : value_isStringPointer);

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
    {
        // concatenate two operand strings objects and store pointer to it in result
        stringlen = 0;                                  // is both operands are empty strings
        if ( !op1emptyString ) { stringlen = strlen( operand1.pStringConst ); }
        if ( !op2emptyString ) { stringlen += strlen( operand2.pStringConst ); }
        if ( stringlen == 0 ) { opResult.pStringConst = nullptr; }                                // empty strings are represented by a nullptr (conserve heap space)
        else {
            opResult.pStringConst = new char [stringlen + 1];
            intermediateStringObjectCount++;
            opResult.pStringConst [0] = '\0';                                // in case first operand is nullptr
            if ( !op1emptyString ) { strcpy( opResult.pStringConst, operand1.pStringConst ); }
            if ( !op2emptyString ) { strcat( opResult.pStringConst, operand2.pStringConst ); }
#if printCreateDeleteHeapObjects
            Serial.print( "+++++ (Intermd str) " );   Serial.println( (uint32_t) opResult.pStringConst - RAMSTART );
#endif
        }
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
    if ( ((_pEvalStackTop->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate) && !op2real )
    {
        if ( _pEvalStackTop->varOrConst.value.pStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
            Serial.print( "----- (Intermd str) " );   Serial.println( (uint32_t) _pEvalStackTop->varOrConst.value.pStringConst - RAMSTART );
#endif
            delete [] _pEvalStackTop->varOrConst.value.pStringConst;
            intermediateStringObjectCount--;
        }
    }
    // operand 1 is an intermediate constant AND it is a string ? delete char string object
    if ( ((_pEvalStackMinus2->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate) && !op1real )
    {
        if ( _pEvalStackMinus2->varOrConst.value.pStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
            Serial.print( "----- (Intermd str) " );   Serial.println( (uint32_t) _pEvalStackMinus2->varOrConst.value.pStringConst - RAMSTART );
#endif
            delete [] _pEvalStackMinus2->varOrConst.value.pStringConst;
            intermediateStringObjectCount--;
        }
    }


    //  clean up stack
    // ---------------

    // drop highest 2 stack levels( operator and operand 2 ) 
    evalStack.deleteListElement( _pEvalStackTop );                          // operand 2 
    evalStack.deleteListElement( _pEvalStackMinus1 );                       // operator
    _pEvalStackTop = _pEvalStackMinus2;
    _pEvalStackMinus1 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackTop );
    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );


    //  store result in stack
    // ----------------------

    // set value type
    _pEvalStackTop->varOrConst.value = opResult;                        // float or pointer to string
    _pEvalStackTop->varOrConst.valueType = opResultReal ? value_isFloat : value_isStringPointer;     // value type of second operand  
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
    _pEvalStackTop->varOrConst.valueAttributes = (operatorCode == MyParser::termcod_assign) ? 0x00 : constIsIntermediate;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
    _pEvalStackTop->varOrConst.variableAttributes = 0x00;                  // not an array, not an array element (it's a constant) 

    return result_execOK;
}


// ---------------------------------
// *   execute internal function   *
// ---------------------------------

Interpreter::execResult_type  Interpreter::execInternalFunction( LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pStackLvl, int argCount ) {

    int functionIndex = ((TokenIsIntFunction*) _programCounter)->tokenIndex & 0x0F;
    char functionCode = MyParser::_functions [functionIndex].functionCode;

    // variables for intermediate storage of operands (constants, variable values or intermediate results from previous calculations) and result
    LE_evalStack* pArgStackLvl = pStackLvl;
    Val operands [8], opResult;                                                               // operands and result
    bool opIsReal [8], opResultReal;

    // value type of operands
    if ( argCount > 0 ) {
        for ( int i = 0; i < argCount; i++ ) {
            opIsReal [i] = (pArgStackLvl->varOrConst.valueType == value_isFloat);

            // fetch operands: real constants or pointers to character strings
            if ( opIsReal [i] ) { operands [i].realConst = (pArgStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pArgStackLvl->varOrConst.value.pRealConst) : pArgStackLvl->varOrConst.value.realConst; }
            else { operands [i].pStringConst = (pArgStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pArgStackLvl->varOrConst.value.ppStringConst) : pArgStackLvl->varOrConst.value.pStringConst; }
        }
    }

    // check if operands are compatible with operator: real for all operators except string concatenation
    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;                // in the event of an error  




    switch ( functionIndex ) {

    case MyParser::fnccod_and:

        // Fetch operands and operands value type
        // --------------------------------------





        break;
    }

    return result_execOK;
}


// --------------------------------
// *   launch external function   *
// --------------------------------

Interpreter::execResult_type  Interpreter::launchExternalFunction( LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pArgStackLvl, int suppliedArgCount ) {

    // note current token (function token) position, in case an error happens IN THE CALLER immediately upon return from function to be called
    // ---------------------------------------------------------------------------------------------------------------------------------------

    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;  // before pushing to stack   


    // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
    // ----------------------------------------------------------------------------------------------

    _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
    _pFlowCtrlStackTop = (FunctionData*) flowCtrlStack.appendListElement( sizeof( FunctionData ) );
    *((FunctionData*) _pFlowCtrlStackTop) = _activeFunctionData;                // push caller function data


    // function to be called: create storage and init local variables with supplied arguments (populate _activeFunctionData)
    // --------------------------------------------------------------------------------------------------------------------

    _activeFunctionData.functionIndex = pFunctionStackLvl->function.index;     // index of external function to call
    _activeFunctionData.blockType = MyParser::block_extFunction;

    // create local variable storage for external function to be called
    int localVarCount = extFunctionData [_activeFunctionData.functionIndex].localVarCountInFunction;
    int paramCount = extFunctionData [_activeFunctionData.functionIndex].paramOnlyCountInFunction;

    if ( localVarCount > 0 ) {
        _activeFunctionData.pLocalVarValues = new Val [localVarCount];              // local variable value: real, pointer to string or array, or (if reference): pointer to 'source' (referenced) variable
        _activeFunctionData.ppSourceVarTypes = new char* [localVarCount];           // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        _activeFunctionData.pVariableAttributes = new char [localVarCount];         // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) 

        // save function caller's arguments to function's local storage and remove them from evaluation stack
        if ( suppliedArgCount > 0 ) {
            LE_evalStack* pStackLvl = pArgStackLvl;         // pointing to first argument on stack
            for ( int i = 0; i < suppliedArgCount; i++ ) {
                int valueType = pStackLvl->varOrConst.valueType;
                bool operandIsRef = (valueType == value_isVarRef);
                bool operandIsReal = (valueType == value_isFloat);
                bool operandIsVariable = (pStackLvl->varOrConst.tokenType == tok_isVariable);

                // variable (could be an array) passed ?
                if ( pStackLvl->varOrConst.tokenType == tok_isVariable ) {                                      // argument is a variable => local value is a reference to 'source' variable
                    _activeFunctionData.pLocalVarValues [i].pVariable = pStackLvl->varOrConst.value.pVariable;  // pointer to 'source' variable
                    _activeFunctionData.ppSourceVarTypes [i] = pStackLvl->varOrConst.varTypeAddress;            // pointer to 'source' variable value type
                    _activeFunctionData.pVariableAttributes [i] = value_isVarRef |                              // local variable value type (reference) ...
                        (pStackLvl->varOrConst.variableAttributes & var_scopeMask);                             // ... and SOURCE variable scope (user, global, static; local, param)
                }
                else {      // parsed, or intermediate, constant passed as value
                    if ( operandIsReal ) {                                                      // operand is float constant
                        _activeFunctionData.pLocalVarValues [i].realConst = pStackLvl->varOrConst.value.realConst;   // store a local copy
                        _activeFunctionData.pVariableAttributes [i] = value_isFloat;
                    }
                    else {                      // operand is string constant: create a local copy
                        _activeFunctionData.pLocalVarValues [i].pStringConst = nullptr;             // init (if empty string)
                        _activeFunctionData.pVariableAttributes [i] = value_isStringPointer;
                        if ( pStackLvl->varOrConst.value.pStringConst != nullptr ) {
                            int stringlen = strlen( pStackLvl->varOrConst.value.pStringConst );
                            _activeFunctionData.pLocalVarValues [i].pStringConst = new char [stringlen + 1];
                            localVarStringObjectCount++;
                            strcpy( _activeFunctionData.pLocalVarValues [i].pStringConst, pStackLvl->varOrConst.value.pStringConst );
#if printCreateDeleteHeapObjects
                            Serial.print( "+++++ (loc var str) " );   Serial.println( (uint32_t) _activeFunctionData.pLocalVarValues [i].pStringConst - RAMSTART );
#endif
                        }
                    };
                }

                if ( ((pStackLvl->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate) && !operandIsReal ) {
                    if ( pStackLvl->varOrConst.value.pStringConst != nullptr ) {
#if printCreateDeleteHeapObjects
                        Serial.print( "----- (intermd str) " );   Serial.println( (uint32_t) pStackLvl->varOrConst.value.pStringConst - RAMSTART );
#endif
                        delete [] pStackLvl->varOrConst.value.pStringConst;
                        intermediateStringObjectCount--;
                    }
                }

                pStackLvl = (LE_evalStack*) evalStack.deleteListElement( pStackLvl );       // argument saved: remove argument from stack and point to next argument
            }
        }
    }

    // also delete function name token from evaluation stack
    _pEvalStackTop = (LE_evalStack*) evalStack.getPrevListElement( pFunctionStackLvl );
    _pEvalStackMinus1 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackTop );
    _pEvalStackMinus2 = (LE_evalStack*) evalStack.getPrevListElement( _pEvalStackMinus1 );
    (LE_evalStack*) evalStack.deleteListElement( pFunctionStackLvl );

    _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                          // store evaluation stack levels in use by callers (call stack)



    // init local variables for non_supplied arguments (scalar parameters with default values) and local (non-parameter) variables
    // ---------------------------------------------------------------------------------------------------------------------------

    char* calledFunctionTokenStep = extFunctionData [_activeFunctionData.functionIndex].pExtFunctionStartToken;
    initFunctionDefaultParamVariables( calledFunctionTokenStep, suppliedArgCount, paramCount );      // return with first token after function definition
    initFunctionLocalNonParamVariables( calledFunctionTokenStep, paramCount, localVarCount );       // and create storage for local array variables


    // set next step to start of called function
    // -----------------------------------------

    _activeFunctionData.pNextStep = calledFunctionTokenStep;                     // first step in first statement in called function
    _activeFunctionData.errorStatementStartStep = calledFunctionTokenStep;
    _activeFunctionData.errorProgramCounter = calledFunctionTokenStep;

    /*
    Serial.print( "==== launching function " ); Serial.print( extFunctionNames [_activeFunctionData.functionIndex] );
    Serial.print( ", function index: " ); Serial.println( (int) _activeFunctionData.functionIndex );
    Serial.print( "     first token step: " ); Serial.println( _activeFunctionData.pNextStep - _programStorage );
    Serial.print( "     error statement start token step: " ); Serial.println( _activeFunctionData.errorStatementStartStep - _programStorage );
    Serial.print( "     error token step: " ); Serial.println( _activeFunctionData.errorProgramCounter - _programStorage );
    Serial.print( "     caller stack eval levels: " ); Serial.println( (int) _activeFunctionData.callerEvalStackLevels );

    Serial.print( "     flow control stack levels: " ); Serial.println( (int) flowCtrlStack.getElementCount() );
    */

    return  result_execOK;
}


// -----------------------------------------------------------------------------------------------
// *   init local variables for non_supplied arguments (scalar parameters with default values)   *
// -----------------------------------------------------------------------------------------------

void Interpreter::initFunctionDefaultParamVariables( char*& pStep, int suppliedArgCount, int paramCount ) {
    int tokenType = *pStep & 0x0F;                                                          // function name token of called function

    if ( suppliedArgCount < paramCount ) {      // missing arguments: use parameter default values to init local variables
        int count = 0, terminalCode = 0;
        tokenType = jumpTokens( 1, pStep );
        // now positioned at opening parenthesis in called function (after FUNCTION token)
        // find n-th argument separator (comma), with n is number of supplied arguments (stay at left parenthesis if none provided)
        while ( count < suppliedArgCount ) { tokenType = findTokenStep( tok_isTerminalGroup1, MyParser::termcod_comma, pStep ); count++; }

        // now positioned before first parameter for non-supplied scalar argument. It always has an initializer
        // we only need the constant value, because we know the variable value index already (count): skip variable and assignment 
        while ( count < paramCount ) {
            tokenType = jumpTokens( ((count == suppliedArgCount) ? 3 : 4), pStep );

            // now positioned at constant initializer
            bool operandIsReal = (tokenType == tok_isRealConst);
            if ( operandIsReal ) {                                                      // operand is float constant
                float f{0.};
                memcpy( &f, ((TokenIsRealCst*) pStep)->realConst, sizeof( float ) );
                _activeFunctionData.pLocalVarValues [count].realConst = f;  // store a local copy
                _activeFunctionData.pVariableAttributes [count] = value_isFloat;                // default value: always scalar
            }
            else {                      // operand is parsed string constant: create a local copy and store in variable
                char* s{nullptr};
                memcpy( &s, ((TokenIsStringCst*) pStep)->pStringConst, sizeof( char* ) );  // copy the pointer, NOT the string  

                _activeFunctionData.pLocalVarValues [count].pStringConst = nullptr;   // init (if empty string)
                _activeFunctionData.pVariableAttributes [count] = value_isStringPointer;                // default value: always scalar
                if ( s != nullptr ) {
                    int stringlen = strlen( s );
                    _activeFunctionData.pLocalVarValues [count].pStringConst = new char [stringlen + 1];
                    localVarStringObjectCount++;
                    strcpy( _activeFunctionData.pLocalVarValues [count].pStringConst, s );
#if printCreateDeleteHeapObjects
                    Serial.print( "+++++ (loc var str) " );   Serial.println( (uint32_t) _activeFunctionData.pLocalVarValues [count].pStringConst - RAMSTART );
#endif
                }
            }
            count++;
        }
    }

    // skip (remainder of) function definition
    findTokenStep( tok_isTerminalGroup1, MyParser::termcod_semicolon, pStep );
};



// --------------------------------------------
// *   init local variables (non-parameter)   *
// --------------------------------------------

void Interpreter::initFunctionLocalNonParamVariables( char* pStep, int paramCount, int localVarCount ) {
    // upon entry, positioned at first token after FUNCTION statement

    int tokenType, terminalCode;

    int count = paramCount;         // sum of mandatory and optional parameters

    while ( count != localVarCount ) {
        findTokenStep( tok_isReservedWord, MyParser::cmdcod_local, pStep );     // find 'LOCAL' keyword (always there)

        int terminalCode;
        do {
            // in case variable is not an array and it does not have an initializer: init as zero (float)
            _activeFunctionData.pLocalVarValues [count].realConst = 0;
            _activeFunctionData.pVariableAttributes [count] = value_isFloat;        // for now, assume scalar

            tokenType = jumpTokens( 2, pStep, terminalCode );            // either left parenthesis, assignment, comma or semicolon separator (always a terminal)


            // handle array definition dimensions 
            // ----------------------------------

            int dimCount = 0, arrayElements = 1;
            int arrayDims [MAX_ARRAY_DIMS] { 0 };

            if ( terminalCode == MyParser::termcod_leftPar ) {
                do {
                    tokenType = jumpTokens( 1, pStep );         // dimension

                    // increase dimension count and calculate elements (checks done during parsing)
                    float f{0.};
                    memcpy( &f, ((TokenIsRealCst*) pStep)->realConst, sizeof( float ) );
                    arrayElements *= f;
                    arrayDims [dimCount] = f;
                    dimCount++;

                    tokenType = jumpTokens( 1, pStep, terminalCode );         // comma (dimension separator) or right parenthesis
                } while ( terminalCode != MyParser::termcod_rightPar );

                // create array (init later)
                float* pArray = new float [arrayElements + 1];
                localArrayObjectCount++;
#if printCreateDeleteHeapObjects
                Serial.print( "+++++ (loc ar stor) " ); Serial.println( (uint32_t) pArray - RAMSTART );
#endif
                _activeFunctionData.pLocalVarValues [count].pArray = pArray;
                _activeFunctionData.pVariableAttributes [count] |= var_isArray;             // set array bit

                // store dimensions in element 0: char 0 to 2 is dimensions; char 3 = dimension count 
                for ( int i = 0; i < MAX_ARRAY_DIMS; i++ ) {
                    ((char*) pArray) [i] = arrayDims [i];
                }
                ((char*) pArray) [3] = dimCount;        // (note: for param arrays, set to max dimension count during parsing)

                tokenType = jumpTokens( 1, pStep, terminalCode );       // assignment, comma or semicolon
            }


            // handle initialisation (if initializer provided)
            // -----------------------------------------------

            if ( terminalCode == MyParser::termcod_assign ) {
                tokenType = jumpTokens( 1, pStep );       // constant

                // fetch constant
                tokenType = *pStep & 0x0F;

                float f { 0. };        // last token is a number constant: dimension spec
                char* pString { nullptr };
                bool isNumberCst = (tokenType == tok_isRealConst);

                if ( isNumberCst ) { memcpy( &f, ((TokenIsRealCst*) pStep)->realConst, sizeof( float ) ); }
                else { memcpy( &pString, ((TokenIsStringCst*) pStep)->pStringConst, sizeof( pString ) ); }     // copy pointer to string (not the string itself)
                int length = isNumberCst ? 0 : (pString == nullptr) ? 0 : strlen( pString );       // only relevant for strings
                if ( !isNumberCst ) {
                    _activeFunctionData.pVariableAttributes [count] =
                        (_activeFunctionData.pVariableAttributes [count] & ~value_typeMask) | value_isStringPointer;
                }    // was initialised to float

// array: initialize (note: test for non-empty string done during parsing
                if ( (_activeFunctionData.pVariableAttributes [count] & var_isArray) == var_isArray ) {
                    void* pArray = ((void**) _activeFunctionData.pLocalVarValues) [count];        // void pointer to an array 
                    // fill up with numeric constants or (empty strings:) null pointers
                    if ( isNumberCst ) { for ( int elem = 1; elem <= arrayElements; elem++ ) { ((float*) pArray) [elem] = f; } }
                    else { for ( int elem = 1; elem <= arrayElements; elem++ ) { ((char**) pArray) [elem] = nullptr; } }
                }
                // scalar: initialize
                else {
                    if ( isNumberCst ) { _activeFunctionData.pLocalVarValues [count].realConst = f; }      // store numeric constant
                    else {
                        if ( length == 0 ) { _activeFunctionData.pLocalVarValues [count].pStringConst = nullptr; }       // an empty string does not create a heap object
                        else { // create string object and store string
                            char* pVarString = new char [length + 1];          // create char array on the heap to store alphanumeric constant, including terminating '\0'
                            // store alphanumeric constant in newly created character array
                            strcpy( pVarString, pString );              // including terminating \0
                            _activeFunctionData.pLocalVarValues [count].pStringConst = pVarString;       // store pointer to string
                            localVarStringObjectCount++;
#if printCreateDeleteHeapObjects
                            Serial.print( "+++++ (loc var str) " ); Serial.println( (uint32_t) pVarString - RAMSTART );
#endif
                        }
                    }
                }

                tokenType = jumpTokens( 1, pStep, terminalCode );       // comma or semicolon
            }

            count++;

        } while ( terminalCode == MyParser::termcod_comma );

    }
};


// -----------------------------------
// *   terminate external function   *
// -----------------------------------

Interpreter::execResult_type Interpreter::terminateExternalFunction( bool addZeroReturnValue ) {

    if ( addZeroReturnValue ) {
        _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;
        _pEvalStackTop = (LE_evalStack*) evalStack.appendListElement( sizeof( VarOrConstLvl ) );
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;          // use generic constant type
        _pEvalStackTop->varOrConst.value.realConst = 0.;                // default return value
        _pEvalStackTop->varOrConst.valueType = value_isFloat;
        _pEvalStackTop->varOrConst.variableAttributes = 0x00;
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    }
    else {makeIntermediateConstant( _pEvalStackTop );  }             // if not already an intermediate constant
  

    // delete local variable arrays and strings (only if local variable is not a reference)

    int localVarCount = extFunctionData [_activeFunctionData.functionIndex].localVarCountInFunction;      // of function to be terminated

    if ( localVarCount > 0 ) {
        _pmyParser->deleteArrayElementStringObjects( _activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true );
        _pmyParser->deleteVariableValueObjects( _activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true );

        // release local variable storage for function that has been called
        delete [] _activeFunctionData.pLocalVarValues;
        delete [] _activeFunctionData.pVariableAttributes;
        delete [] _activeFunctionData.ppSourceVarTypes;
    }

    char blockType = MyParser::block_none;
    do {
        blockType = *(char*) _pFlowCtrlStackTop;            // always at least one open function (because returning to caller from it)
        if ( blockType == MyParser::block_extFunction ) {
            // load local storage pointers again for caller function and restore pending step & active function information for caller function
            _activeFunctionData = *(FunctionData*) _pFlowCtrlStackTop;

            /*
            if ( flowCtrlStack.getElementCount() == 1 ) {   // caller is main   //// fout: niet 0 indien ope blocks in main (FOR, ...)
                Serial.println( "==== ending function; going back to main" );
            }
            else {
                Serial.print( "==== ending function; going back to caller " ); Serial.print( extFunctionNames [_activeFunctionData.functionIndex] );
                Serial.print( ", function index: " ); Serial.println( (int) _activeFunctionData.functionIndex );
            }

            Serial.print( "     caller token step: " ); Serial.println( _activeFunctionData.pNextStep - _programStorage );
            Serial.print( "     error statement start token step: " ); Serial.println( _activeFunctionData.errorStatementStartStep - _programStorage );
            Serial.print( "     error token step: " ); Serial.println( _activeFunctionData.errorProgramCounter - _programStorage );
            Serial.print( "     higher level caller eval stack levels: " ); Serial.println( (int) _activeFunctionData.callerEvalStackLevels );

            Serial.print( "     flow control stack levels: " ); Serial.println( (int) flowCtrlStack.getElementCount() );
            */
        }

        // delete FLOW CONTROL stack level that contained caller function storage pointers and return address (all just retrieved to _activeFunctionData)
        flowCtrlStack.deleteListElement( _pFlowCtrlStackTop );
        _pFlowCtrlStackTop = _pFlowCtrlStackMinus1;
        _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement( _pFlowCtrlStackTop );
        _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement( _pFlowCtrlStackMinus1 );

    } while ( blockType != MyParser::block_extFunction );

    ////Serial.print( "--- Terminate: next step is " ); Serial.println( _activeFunctionData.pNextStep - _programStorage );

    if ( _activeFunctionData.pNextStep >= _programStart ) {   // not within a function        
        if ( localVarStringObjectCount != 0 ) {
            Serial.print( "*** Local variable string objects cleanup error. Remaining: " ); Serial.println( localVarStringObjectCount );
            localVarStringObjectCount = 0;
        }

        if ( localArrayObjectCount != 0 ) {
            Serial.print( "*** Local array objects cleanup error. Remaining: " ); Serial.println( localArrayObjectCount );
            localArrayObjectCount = 0;
        }
    }

    execResult_type execResult = execAllProcessedOperators();     // in caller !!!

    return execResult;
}


// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Interpreter::fetchVarBaseAddress( TokenIsVariable* pVarToken, char*& sourceVarTypeAddress, char& localValueType, char& variableAttributes, char& valueAttributes ) {

    // pVarToken argument must point to a variable token in Justina PROGRAM memory (containing variable type, index and attributes - NOT the actual variable's address)
    // upon return:
    // - localValueType and variableAttributes arguments will contain current variable value type (float or string; which is fixed for arrays) and array flag, respectively
    // - sourceVarTypeAddress will point to (contain the address of) the variable value type (where variable value type and other attributes are maintained) in Justina memory allocated to variables
    // - return pointer will point to (contain the address of) the variable base address (containing the value (float or char*) OR an address (for arrays and referenced variables)

    int varNameIndex = pVarToken->identNameIndex;
    // identInfo may only contains variable scope (parameter, local, static, global) and 'is array' flag 
    uint8_t varScope = pVarToken->identInfo & var_scopeMask;                                // global, user, local, static or parameter
    bool isUserVar = (varScope == var_isUser);
    bool isGlobalVar = (varScope == var_isGlobal);
    bool isStaticVar = (varScope == var_isStaticInFunc);

    // init source variable scope (if the current variable is a reference variable, this will be changed to the source variable scope later)
    valueAttributes = 0;                                                                                // not an intermediate constant                                         

    int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : programVarValueIndex [varNameIndex];   // value index in allocated Justina data memory for this variable

    if ( isUserVar ) {
        localValueType = userVarType [valueIndex] & value_typeMask;                                     // value type (indicating float or string)
        sourceVarTypeAddress = userVarType + valueIndex;                                                // pointer to value type and the 'is array' flag          
        variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

        return &userVarValues [valueIndex];                                                             // pointer to value (float, char* or (array variables only) pointer to array start in memory)
    }
    else if ( isGlobalVar ) {
        localValueType = globalVarType [valueIndex] & value_typeMask;                                   // value type (indicating float or string)
        sourceVarTypeAddress = globalVarType + valueIndex;                                              // pointer to value type and the 'is array' flag
        variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

        return &globalVarValues [valueIndex];                                                           // pointer to value (float, char* or (array variables only) pointer to array start in memory)
    }
    else if ( isStaticVar ) {
        localValueType = staticVarType [valueIndex] & value_typeMask;                                   // value type (indicating float or string)
        sourceVarTypeAddress = staticVarType + valueIndex;                                              // pointer to value type and the 'is array' flag
        variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

        return &staticVarValues [valueIndex];                                                           // pointer to value (float, char* or (array variables only) pointer to array start in memory)
    }

    // local variables (including parameters)    
    else {
        // note (function parameter variables only): when a function is called with a variable argument (always passed by reference), 
        // the parameter value type has been set to 'reference' when the function was called
        localValueType = _activeFunctionData.pVariableAttributes [valueIndex] & value_typeMask;         // local variable value type (indicating float or string or REFERENCE)

        if ( localValueType == value_isVarRef ) {                                                       // local value is a reference to 'source' variable                                                         
            sourceVarTypeAddress = _activeFunctionData.ppSourceVarTypes [valueIndex];                   // pointer to 'source' variable value type
            // local variable value type (reference); SOURCE variable scope (user, global, static; local, param), 'is array' flag
            variableAttributes = _activeFunctionData.pVariableAttributes [valueIndex] | (pVarToken->identInfo & var_isArray);

            return   ((Val**) _activeFunctionData.pLocalVarValues) [valueIndex];                       // pointer to 'source' variable value 
        }

        // local variable OR parameter variable that received the result of an expression (or constant) as argument (passed by value) OR optional parameter variable that received no value (default initialization) 
        else {
            sourceVarTypeAddress = _activeFunctionData.pVariableAttributes + valueIndex;               // pointer to local variable value type and 'is array' flag
            // local variable value type (reference); local variable scope (user, global, static; local, param), 'is array' flag
            variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

            return (Val*) &_activeFunctionData.pLocalVarValues [valueIndex];                           // pointer to local variable value 
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


// -----------------------------------------------
// *   push terminal token to evaluation stack   *
// -----------------------------------------------

void Interpreter::PushTerminalToken( int& tokenType ) {                                 // terminal token is assumed

    // push internal or external function index to stack

    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*) evalStack.appendListElement( sizeof( TerminalTokenLvl ) );
    _pEvalStackTop->terminal.tokenType = tokenType;
    _pEvalStackTop->terminal.tokenAddress = _programCounter;                                                    // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;                                            // terminal token only: calculate from partial index stored in high 4 bits of token type 
    _pEvalStackTop->terminal.index += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
};


// ------------------------------------------------------------------------
// *   push internal or external function name token to evaluation stack   *
// ------------------------------------------------------------------------

void Interpreter::pushFunctionName( int& tokenType ) {                                  // function name is assumed (internal or external)

    // push internal or external function index to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*) evalStack.appendListElement( sizeof( FunctionLvl ) );
    _pEvalStackTop->function.tokenType = tokenType;
    _pEvalStackTop->function.tokenAddress = _programCounter;                                    // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->function.index = ((TokenIsIntFunction*) _programCounter)->tokenIndex;
    /*
    if ( tokenType == tok_isInternFunction ) {
        int fIndex = (int) _pEvalStackTop->function.index;
        Serial.println( _pmyParser->_functions [fIndex].funcName );
    }
    else {
        Serial.println( extFunctionNames [_pEvalStackTop->function.index] );
    }
    */
};


// -------------------------------------------------------------
// *   push real or string constant token to evaluation stack   *
// -------------------------------------------------------------

void Interpreter::pushConstant( int& tokenType ) {                                              // float or string constant token is assumed

    // push real or string parsed constant, value type and array flag (false) to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*) evalStack.appendListElement( sizeof( VarOrConstLvl ) );
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;          // use generic constant type
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;                                  // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->varOrConst.valueType = (tokenType == tok_isRealConst) ? value_isFloat : value_isStringPointer;
    _pEvalStackTop->varOrConst.variableAttributes = 0x00;
    _pEvalStackTop->varOrConst.valueAttributes = 0x00;

    if ( tokenType == tok_isRealConst ) {
        float f{0.};
        memcpy( &f, ((TokenIsRealCst*) _programCounter)->realConst, sizeof( float ) );          // float  not necessarily aligned with word size: copy memory instead
        _pEvalStackTop->varOrConst.value.realConst = f;                                         // store float in stack, NOT the pointer to float 
    }
    else {
        char* pAnum{nullptr};
        memcpy( &pAnum, ((TokenIsStringCst*) _programCounter)->pStringConst, sizeof( pAnum ) ); // char pointer not necessarily aligned with word size: copy memory instead
        _pEvalStackTop->varOrConst.value.pStringConst = pAnum;                                  // store char* in stack, NOT the pointer to float 
    }

};


// ----------------------------------------------
// *   push variable token to evaluation stack   *
// ----------------------------------------------

void Interpreter::pushVariable( int& tokenType ) {                                              // variable name token is assumed

    // push variable base address, variable value type (real, string) and array flag to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*) evalStack.appendListElement( sizeof( VarOrConstLvl ) );
    _pEvalStackTop->varOrConst.tokenType = tokenType;
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;

    // note: _pEvalStackTop->varOrConst.valueType is a value ONLY containing the value type of the variable pushed on the stack (float, string, reference)
    //       _pEvalStackTop->varOrConst.varTypeAddress is a pointer to the SOURCE variable's variable info (either a referenced variable or the variable itself), with ...
    //       the source variable info containing the value type of the variable AND the 'is array' flag 

    void* varAddress = fetchVarBaseAddress( (TokenIsVariable*) _programCounter, _pEvalStackTop->varOrConst.varTypeAddress, _pEvalStackTop->varOrConst.valueType,
        _pEvalStackTop->varOrConst.variableAttributes, _pEvalStackTop->varOrConst.valueAttributes );
    _pEvalStackTop->varOrConst.value.pVariable = varAddress;                                    // base address of variable
}
