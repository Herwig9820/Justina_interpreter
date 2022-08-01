#include "Justina.h"

#define printCreateDeleteHeapObjects 0
#define debugPrint 0

const char passCopyToCallback = 0x40;       // flag: string is an empty string 

// -----------------------------------
// *   execute parsed instructions   *
// -----------------------------------

Interpreter::execResult_type  Interpreter::exec() {

    // init
    int tokenType = *_programStart & 0x0F;
    int tokenIndex{ 0 };
    bool isFunctionReturn = false;
    bool precedingIsComma = false;                                      // used to detect prefix operators following a comma separator
    bool nextIsNewInstructionStart = false;                     // false, because this is already the start of a new instruction
    execResult_type execResult = result_execOK;

    _pEvalStackTop = nullptr;   _pEvalStackMinus2 = nullptr; _pEvalStackMinus1 = nullptr;
    _pFlowCtrlStackTop = nullptr;   _pFlowCtrlStackMinus2 = nullptr; _pFlowCtrlStackMinus1 = nullptr;

    _programCounter = _programStart;
    _activeFunctionData.functionIndex = 0;                  // main program level
    _activeFunctionData.callerEvalStackLevels = 0;          // this is the highest program level
    _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // no command is being executed
    _activeFunctionData.activeCmd_tokenAddress = nullptr;
    _activeFunctionData.errorStatementStartStep = _programCounter;
    _activeFunctionData.errorProgramCounter = _programCounter;
    _activeFunctionData.blockType = MyParser::block_extFunction;  // consider main as an 'external' function      

    _lastValueIsStored = false;

    intermediateStringObjectCount = 0;      // reset at the start of execution
    localVarStringObjectCount = 0;
    localArrayObjectCount = 0;

    while (tokenType != tok_no_token) {                                                                    // for all tokens in token list
        // if terminal token, determine which terminal type
        bool isTerminal = ((tokenType == Interpreter::tok_isTerminalGroup1) || (tokenType == Interpreter::tok_isTerminalGroup2) || (tokenType == Interpreter::tok_isTerminalGroup3));
        if (isTerminal) {
            tokenIndex = ((((TokenIsTerminal*)_programCounter)->tokenTypeAndIndex >> 4) & 0x0F);
            tokenIndex += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
        }

        bool isOperator = (isTerminal ? (MyParser::_terminals[tokenIndex].terminalCode <= MyParser::termcod_opRangeEnd) : false);
        bool isSemicolon = (isTerminal ? (MyParser::_terminals[tokenIndex].terminalCode == MyParser::termcod_semicolon) : false);
        bool isComma = (isTerminal ? (MyParser::_terminals[tokenIndex].terminalCode == MyParser::termcod_comma) : false);
        bool isLeftPar = (isTerminal ? (MyParser::_terminals[tokenIndex].terminalCode == MyParser::termcod_leftPar) : false);
        bool isRightPar = (isTerminal ? (MyParser::_terminals[tokenIndex].terminalCode == MyParser::termcod_rightPar) : false);

        // fetch next token (for some token types, the size is stored in the upper 4 bits of the token type byte)
        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) : (tokenType == Interpreter::tok_isConstant) ? sizeof(TokenIsConstant) : (*_programCounter >> 4) & 0x0F;
        _activeFunctionData.pNextStep = _programCounter + tokenLength;                                  // look ahead

        switch (tokenType) {

        case tok_isReservedWord:
            // ---------------------------------
            // Case: process keyword token
            // ---------------------------------

            // compile time statements (program, function, var, local, static, ...): skip for execution

        {   // start block (required for variable definitions inside)
            tokenIndex = ((TokenIsResWord*)_programCounter)->tokenIndex;
            bool skipStatement = ((_pmyParser->_resWords[tokenIndex].restrictions & MyParser::cmd_skipDuringExec) != 0);
            if (skipStatement) {
                findTokenStep(tok_isTerminalGroup1, MyParser::termcod_semicolon, _programCounter);  // find semicolon (always match)
                _activeFunctionData.pNextStep = _programCounter;
                break;
            }

            // commands are executed when processing final semicolon statement (note: activeCmd_ResWordCode identifies individual commands; not command blocks)
            _activeFunctionData.activeCmd_ResWordCode = _pmyParser->_resWords[tokenIndex].resWordCode;       // store command for now
            _activeFunctionData.activeCmd_tokenAddress = _programCounter;
        }

        break;


        case tok_isInternFunction:
        case tok_isExternFunction:
            // -------------------------------------------------
            // Case: process internal or external function token
            // -------------------------------------------------

            pushFunctionName(tokenType);
            break;


        case tok_isGenericName:
            pushGenericName(tokenType);
            break;


        case tok_isConstant:
        case tok_isVariable:
            // -----------------------------------------------------------
            // Case: process real or string constant token, variable token
            // -----------------------------------------------------------
#if debugPrint
            Serial.print("operand: stack level "); Serial.println(evalStack.getElementCount());
#endif
            {   // start block (required for variable definitions inside)
                _activeFunctionData.errorProgramCounter = _programCounter;

                // push constant value token or variable name token to stack
                if (tokenType == tok_isVariable) {
                    pushVariable(tokenType);

                    // next token
                    int nextTokenType = *_activeFunctionData.pNextStep & 0x0F;
                    int nextTokenIndex{ 0 };
                    bool nextIsTerminal = ((nextTokenType == Interpreter::tok_isTerminalGroup1) || (nextTokenType == Interpreter::tok_isTerminalGroup2) || (nextTokenType == Interpreter::tok_isTerminalGroup3));
                    if (nextIsTerminal) {
                        nextTokenIndex = ((((TokenIsTerminal*)_activeFunctionData.pNextStep)->tokenTypeAndIndex >> 4) & 0x0F);
                        nextTokenIndex += ((nextTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (nextTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
                    }

                    bool nextIsLeftPar = (nextIsTerminal ? (MyParser::_terminals[nextTokenIndex].terminalCode == MyParser::termcod_leftPar) : false);
                    if (nextIsLeftPar) {                                                           // array variable name (this token) is followed by subscripts (to be processed)
                        _pEvalStackTop->varOrConst.valueAttributes |= var_isArray_pendingSubscripts;    // flag that array element still needs to be processed
                    }
                }

                else {
                    pushConstant(tokenType);
                }
            }

            // check if (an) operation(s) can be executed. 
            // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
            execResult = execAllProcessedOperators();
            if (execResult != result_execOK) { break; }
            break;


            // ----------------------------
            // Case: process terminal token 
            // ----------------------------
        case tok_isTerminalGroup1:
        case tok_isTerminalGroup2:
        case tok_isTerminalGroup3:

            if (isOperator || isLeftPar) {
                // --------------------------------------------
                // Process operators and left parenthesis token
                // --------------------------------------------
#if debugPrint
                Serial.print(tok_isOperator ? "\r\n** operator: stack level " : "\r\n** left parenthesis: stack level "); Serial.println(evalStack.getElementCount());
#endif
                // terminal tokens: only operators and left parentheses are pushed on the stack
                PushTerminalToken(tokenType);

                if (precedingIsComma) { _pEvalStackTop->terminal.index |= 0x80;   break; }      // flag that preceding token is comma separator 

                if (evalStack.getElementCount() < _activeFunctionData.callerEvalStackLevels + 2) { break; }         // no preceding token exist on the stack               
                if (!(_pEvalStackMinus1->genericToken.tokenType == tok_isConstant) && !(_pEvalStackMinus1->genericToken.tokenType == tok_isVariable)) { break; };

                // previous token is constant or variable: check if current token is an infix or a postfix operator (it cannot be a prefix operator)
                // if postfix operation, execute it first (it always has highest priority)
                bool isPostfixOperator = (_pmyParser->_terminals[_pEvalStackTop->terminal.index & 0x7F].postfix_priority != 0);
                if (isPostfixOperator) {
                    execUnaryOperation(false);        // flag postfix operation
                    execResult = execAllProcessedOperators();
                    if (execResult != result_execOK) { break; }
                }
            }

            else if (isComma) {
                // -----------------------
                // Process comma separator
                // -----------------------

                // no action needed
            }

            else if (isRightPar) {
                // -------------------------------------
                // Process right parenthesis token
                // -------------------------------------

#if debugPrint
                Serial.print("right parenthesis: stack level "); Serial.println(evalStack.getElementCount());
#endif
                {   // start block (required for variable definitions inside)
                    int argCount = 0;                                                // init number of supplied arguments (or array subscripts) to 0
                    LE_evalStack* pstackLvl = _pEvalStackTop;     // stack level of last argument / array subscript before right parenthesis, or left parenthesis (if function call and no arguments supplied)

                    // set pointer to stack level for left parenthesis and pointer to stack level for preceding token (if any)
                    while (true) {
                        bool isTerminalLvl = ((pstackLvl->genericToken.tokenType == tok_isTerminalGroup1) || (pstackLvl->genericToken.tokenType == tok_isTerminalGroup2) || (pstackLvl->genericToken.tokenType == tok_isTerminalGroup3));
                        bool isLeftParLvl = isTerminalLvl ? (MyParser::_terminals[pstackLvl->terminal.index & 0x7F].terminalCode == MyParser::termcod_leftPar) : false;
                        if (isLeftParLvl) { break; }   // break if left parenthesis found
                        pstackLvl = (LE_evalStack*)evalStack.getPrevListElement(pstackLvl);
                        argCount++;
                    }

                    LE_evalStack* pPrecedingStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pstackLvl);     // stack level PRECEDING left parenthesis (or null pointer)

                    // remove left parenthesis stack level
                    pstackLvl = (LE_evalStack*)evalStack.deleteListElement(pstackLvl);                            // pstackLvl now pointing to first function argument or array subscript (or nullptr if none)

                    // correct pointers (now wrong, if only one or 2 arguments)
                    _pEvalStackTop = (LE_evalStack*)evalStack.getLastListElement();        // this line needed if no arguments
                    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
                    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

                    // execute internal or external function, calculate array element address or remove parenthesis around single argument (if no function or array)
                    execResult = execParenthesesPair(pPrecedingStackLvl, pstackLvl, argCount);

                    if (execResult != result_execOK) { break; }

                    // the left parenthesis and the argument(s) are now removed and replaced by a single scalar (function result, array element, single argument)
                    // check if additional operators preceding the left parenthesis can now be executed.
                    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
                    execResult = execAllProcessedOperators();
                    if (execResult != result_execOK) { break; }
                }
            }

            else if (isSemicolon) {
                // -----------------
                // Process separator
                // -----------------

                nextIsNewInstructionStart = true;         // for pretty print only   
                if (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_none) {       // currently not executing a command, but a simple expression
                    if (evalStack.getElementCount() > (_activeFunctionData.callerEvalStackLevels + 1)) {
                        Serial.print("*** Evaluation stack error. Remaining stack levels for current program level: "); Serial.println(evalStack.getElementCount() - (_activeFunctionData.callerEvalStackLevels + 1));
                    }

                    // did the last expression produce a result ?  
                    else if (evalStack.getElementCount() == _activeFunctionData.callerEvalStackLevels + 1) {
                        // in main program level ? store as last value (for now, we don't know if it will be followed by other 'last' values)
                        if (_programCounter >= _programStart) { saveLastValue(_lastValueIsStored); }                // save last result in FIFO and delete stack level
                        else { clearEvalStackLevels(1); } // NOT main program level: we don't need to keep the statement result
                    }
                }
                // command with optional expression(s) processed ? Execute command
                else {
                    execResult = execProcessedCommand(isFunctionReturn);
                    if (execResult != result_execOK) { break; }
                }
            }

            break;

        }   // end 'switch (tokenType)'


        // advance to next token
        _programCounter = _activeFunctionData.pNextStep;         // note: will be altered when calling an external function and upon return of a called function
        tokenType = *_activeFunctionData.pNextStep & 0x0F;                                                               // next token type (could be token within caller, if returning now)
        precedingIsComma = isComma;

        // if execution error: print current instruction being executed, signal error and exit
        // -----------------------------------------------------------------------------------

        if (execResult != result_execOK) {
            int sourceErrorPos{ 0 };
            if (!_atLineStart) { _pConsole->println(); _atLineStart = true; }
            _pConsole->print("\r\n  ");

            _pmyParser->prettyPrintInstructions(true, _activeFunctionData.errorStatementStartStep, _activeFunctionData.errorProgramCounter, &sourceErrorPos);
            _pConsole->print("  "); for (int i = 1; i <= sourceErrorPos; i++) { _pConsole->print(" "); }
            char execInfo[100];
            if (_programCounter >= _programStart) { sprintf(execInfo, "^ Exec error %d\r\n", execResult); }     // in main program level 
            else { sprintf(execInfo, "^ Exec error %d in user function %s\r\n", execResult, extFunctionNames[_activeFunctionData.functionIndex]); }
            _pConsole->print(execInfo);
            _lastValueIsStored = false;              // prevent printing last result (if any)
            break;
        }


        // finalize token processing
        // -------------------------

        if (nextIsNewInstructionStart) {
            if (!isFunctionReturn) {   // if returning from user function, error statement pointers retrieved from flow control stack 
                _activeFunctionData.errorStatementStartStep = _programCounter;
                _activeFunctionData.errorProgramCounter = _programCounter;
            }

            isFunctionReturn = false;
            nextIsNewInstructionStart = false;
        }

    }   // end 'while ( tokenType != tok_no_token )'                                                                                       // end 'while ( tokenType != tok_no_token )'


    // All tokens processed: finalize
    // ------------------------------

    if (!_atLineStart) { _pConsole->println(); _atLineStart = true; }

    if (_lastValueIsStored && _printLastResult) {             // did the execution produce a result ?

        // print last result
        bool isLong = (lastResultTypeFiFo[0] == value_isLong);
        bool isFloat = (lastResultTypeFiFo[0] == value_isFloat);
        int charsPrinted{ 0 };        // not used
        Val toPrint;
        char* fmtString = (isLong || isFloat) ? _dispNumberFmtString : _dispStringFmtString;

        printToString(_dispWidth, (isLong || isFloat) ? _dispNumPrecision : _maxCharsToPrint,
            (!isLong && !isFloat), _dispIsIntFmt, lastResultTypeFiFo, lastResultValueFiFo, fmtString, toPrint, charsPrinted);
        _pConsole->println(toPrint.pStringConst);


#if printCreateDeleteHeapObjects
        Serial.print("----- (Intermd str) "); Serial.println((uint32_t)toPrint.pStringConst - RAMSTART);
#endif
        if (toPrint.pStringConst != nullptr) {
            delete[] toPrint.pStringConst;
            intermediateStringObjectCount--;
        }

    }

    // Delete any intermediate result string objects used as arguments, delete remaining evaluation stack level objects 

    clearEvalStack();               // and intermediate strings
    clearFlowCtrlStack();           // and remaining local storage + local variable string and array values


    return execResult;   // return result, in case it's needed by caller
};


// -----------------------------------
// *   execute a processed command   *
// -----------------------------------

Interpreter::execResult_type Interpreter::execProcessedCommand(bool& isFunctionReturn) {

    // this function is called when the END of the command is encountered during execution, and all arguments are on the stack already

    isFunctionReturn = false;  // init
    execResult_type execResult = result_execOK;
    int cmdParamCount = evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels;

    // note supplied argument count and go to first argument (if any)
    LE_evalStack* pstackLvl = _pEvalStackTop;
    for (int i = 1; i < cmdParamCount; i++) {        // skipped if no arguments, or if one argument
        pstackLvl = (LE_evalStack*)evalStack.getPrevListElement(pstackLvl);      // go to first argument
    }

    _activeFunctionData.errorProgramCounter = _activeFunctionData.activeCmd_tokenAddress;

    switch (_activeFunctionData.activeCmd_ResWordCode) {                                                                      // command code 

    // --------------
    // Input a string
    // --------------

    // note: a DEFAULT value can not be displayed to be overtyped (command line only shows user input)

    case MyParser::cmdcod_input:
    {
        bool argIsVar[3];
        bool argIsArray[3];
        char valueType[3];
        Val args[3];

        bool allowCancel = true; // init
        copyValueArgsFromStack(pstackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);
        if (valueType[0] != value_isStringPointer) { return result_arg_stringExpected; }       // prompt 
        if ((argIsArray[1]) && (valueType[1] != value_isStringPointer)) { return result_array_valueTypeIsFixed; }       // an array cannot change type: it needs to be string
        if (cmdParamCount == 3) {
            if (((uint8_t)(valueType[2]) != value_isLong) && ((uint8_t)(valueType[2]) != value_isFloat)) { return result_arg_numValueExpected; }       // flag: allow Cancel 
            allowCancel = (((uint8_t)(valueType[2]) == value_isLong) ? args[2].longConst != 0 : args[2].floatConst != 0.);
        }
        _pConsole->println(allowCancel ? "***** Input (enter Escape character '1B' to cancel) *****" : "***** Input *****");
        _pConsole->print(args[0].pStringConst); _pConsole->print(" ");

        bool doCancel{ false };
        bool dummy{ false };
        char c;
        int length{ 0 };
        char input[_maxCharsToInput + 1] = "";     // init: empty string

        do {
            if (_callbackFcn != nullptr) { _callbackFcn(dummy); }
            if (_pConsole->available() > 0) {     // if terminal character available for reading
                c = _pConsole->read();
                if ((c == 0x1B) && allowCancel) { doCancel = true; }        // no break yet, we must still read new line character here
                if (c == '\n') { break; }               // read until new line characters
                if (c < ' ') { continue; }              // skip control-chars except new line (ESC is skipped here as well - flag already set)
                if (length >= _maxCharsToInput) { continue; }       // max. input length exceeded: drop character
                input[length] = c; input[++length] = '\0';
            }
        } while (true);

        if (doCancel) {
            _pConsole->println("(Input canceled)");
        }
        else {// save in variable
            _pConsole->println(input);      // echo input

            LE_evalStack* pStackLvl = (cmdParamCount == 3) ? _pEvalStackMinus1 : _pEvalStackTop;
            // if  variable currently holds a non-empty string (indicated by a nullptr), delete char string object
            execResult_type execResult = deleteVarStringObject(pStackLvl); if (execResult != result_execOK) { return execResult; }

            if (strlen(input) == 0) { args[1].pStringConst = nullptr; }         
            else {
                // note that for reference variables, the variable type fetched is the SOURCE variable type
                int varScope = pStackLvl->varOrConst.variableAttributes & var_scopeMask;
                int stringlen = min(strlen(input), MyParser::_maxAlphaCstLen);
                (varScope == var_isUser) ? userVarStringObjectCount++ : ((varScope==var_isGlobal) || (varScope==var_isStaticInFunc)) ? globalStaticVarStringObjectCount++ : localVarStringObjectCount++;

                args[1].pStringConst =new char [stringlen+1];
                memcpy(args[1].pStringConst,input, stringlen);        // copy the actual string (not the pointer); do not use strcpy
                args[1].pStringConst[stringlen]='\0';

#if printCreateDeleteHeapObjects
                Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
                Serial.println((uint32_t)args[1].pStringConst - RAMSTART);
#endif
            }
            *pStackLvl->varOrConst.value.ppStringConst = args[1].pStringConst;
            *pStackLvl->varOrConst.varTypeAddress = (*pStackLvl->varOrConst.varTypeAddress & ~value_typeMask) | value_isStringPointer;
        
            // if NOT a variable REFERENCE, then value type on the stack indicates the real value type and NOT 'variable reference' ...
            // but it does not need to be changed, because in the next step, the respective stack level will be deleted 
        }

        if (cmdParamCount == 3) {       // optional third (and last) argument serves a dual purpose: allow cancel (always) and signal 'canceled' (if variable)
            if (argIsVar[2]) {
                // store 'canceled' flag  in variable and adapt variable value type
                *_pEvalStackTop->varOrConst.value.pLongConst = doCancel;  // variable is already numeric: no variable string to delete
                *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isLong;

                // if NOT a variable REFERENCE, then value type on the stack indicates the real value type and NOT 'variable reference' ...
                // but it does not need to be changed, because in the next step, the respective stack level will be deleted 
            }
        }

        clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings

        _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // command execution ended
        _activeFunctionData.activeCmd_tokenAddress = nullptr;
    }
    break;


    // -------------------------------------------------------------------------------------------------------------------------------------------------------------
    // print all arguments (longs, floats and strings) in succession. Floats are printed in compact format with maximum 3 digits / decimals and an optional exponent
    // -------------------------------------------------------------------------------------------------------------------------------------------------------------

    // note: the print command does not take into account the display format set to print the last calculation result
    // to format output produced with the print command, use the formatting function provided (function code: fnccod_format) 

    case MyParser::cmdcod_print:
    {
        for (int i = 1; i <= cmdParamCount; i++) {
            bool operandIsVar = (pstackLvl->varOrConst.tokenType == tok_isVariable);
            char valueType = operandIsVar ? (*pstackLvl->varOrConst.varTypeAddress & value_typeMask) : pstackLvl->varOrConst.valueType;
            bool opIsLong = ((uint8_t)valueType == value_isLong);
            bool opIsFloat = ((uint8_t)valueType == value_isFloat);
            char* printString = nullptr;

            Val operand;
            if (opIsLong || opIsFloat) {
                char s[20];  // largely long enough to print long values, or float values with "G" specifier, without leading characters
                printString = s;    // pointer
                // next line is valid for long values as well (same memory locations are copied)
                operand.floatConst = (operandIsVar ? (*pstackLvl->varOrConst.value.pFloatConst) : pstackLvl->varOrConst.value.floatConst);
                if (opIsLong) { sprintf(s, "%ld", operand.longConst); }
                else { sprintf(s, "%.3G", operand.floatConst); }
            }
            else {
                operand.pStringConst = operandIsVar ? (*pstackLvl->varOrConst.value.ppStringConst) : pstackLvl->varOrConst.value.pStringConst;
                // no need to copy string - just print the original, directly from stack (it's still there)
                printString = operand.pStringConst;     // attention: null pointers not transformed into zero-length strings here
            }
            // NOTE that there is no limit on the number of characters printed here (_maxPrintFieldWidth not checked)
            if (printString != nullptr) {
                _pConsole->print(printString);         // test needed because zero length strings stored as nullptr
                if (strlen(printString) > 0) { _atLineStart = (printString[strlen(printString) - 1] == '\n'); }       // no change if empty string
            }
            pstackLvl = (LE_evalStack*)evalStack.getNextListElement(pstackLvl);
        }

        clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings

        _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // command execution ended
        _activeFunctionData.activeCmd_tokenAddress = nullptr;
    }
    break;


    // -------------------------------------------------------
    // Set display format for printing last calculation result
    // -------------------------------------------------------

    case MyParser::cmdcod_dispfmt:
    {
        // mandatory argument 1: width (used for both numbers and strings) 
        // optional arguments 2-4 (relevant for printing numbers only): [precision, [specifier (F:fixed, E:scientific, G:general, D: decimal, X:hex), ] flags]
        // note that specifier argument can be left out, flags argument taking its place

        bool argIsVar[4];
        bool argIsArray[4];
        char valueType[4];
        Val args[4];

        if (cmdParamCount > 4) { execResult = result_arg_tooManyArgs; return execResult; }
        copyValueArgsFromStack(pstackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

        // set format for numbers and strings

        execResult_type execResult = checkFmtSpecifiers(true, false, cmdParamCount, valueType, args, _dispNumSpecifier[0],
            _dispWidth, _dispNumPrecision, _dispFmtFlags);
        if (execResult != result_execOK) { return execResult; }

        _dispIsIntFmt = (_dispNumSpecifier[0] == 'X') || (_dispNumSpecifier[0] == 'x') || (_dispNumSpecifier[0] == 'd') || (_dispNumSpecifier[0] == 'D');
        makeFormatString(_dispFmtFlags, _dispIsIntFmt, _dispNumSpecifier, _dispNumberFmtString);       // for numbers

        _dispCharsToPrint = _dispWidth;
        strcpy(_dispStringFmtString, "%*.*s%n");                                                           // strings: set characters to print to display width

        clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings

        _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // command execution ended
        _activeFunctionData.activeCmd_tokenAddress = nullptr;
    }
    break;


    // ------------------------
    // set console display mode
    // ------------------------

    case MyParser::cmdcod_dispmod:      // takes two arguments: width & flags
    {
        // mandatory argument 1: 0 = do not print prompt and do not echo user input; 1 = print prompt but no not echo user input; 2 = print prompt and echo user input 
        // mandatory argument 2: 0 = do not print last result; 1 = print last result

        bool argIsVar[2];
        bool argIsArray[2];
        char valueType[2];               // 2 arguments
        Val args[2];

        copyValueArgsFromStack(pstackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

        for (int i = 0; i < cmdParamCount; i++) {           // always 2 parameters
            bool argIsLong = (valueType[i] == value_isLong);
            bool argIsFloat = (valueType[i] == value_isFloat);
            if (!(argIsLong || argIsFloat)) { execResult = result_arg_numValueExpected; return execResult; }

            if (argIsFloat) { args[i].longConst = (int)args[i].floatConst; }
            if ((args[i].longConst != 0) && (args[i].longConst != 1) && ((i == 0) ? (args[i].longConst != 2) : true)) { execResult = result_arg_invalid; return execResult; };
        }

        // if last result printing switched back on, then prevent printing pending last result (if any)
        _lastValueIsStored = false;               // prevent printing last result (if any)

        _promptAndEcho = args[0].longConst, _printLastResult = args[1].longConst;
        clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings

        _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // command execution ended
        _activeFunctionData.activeCmd_tokenAddress = nullptr;
    }
    break;


    // -------------------------- 
    // Call a user routine in C++
    // --------------------------

    case MyParser::cmdcod_callback:
    {
        // preprocess
        // ----------

        // determine callback routine, based upon alias (argument 1) 
        LE_evalStack* aliasStackLvl = pstackLvl;
        char* alias = aliasStackLvl->genericName.pStringConst;
        bool isDeclared = false;
        int index{};
        for (index = 0; index < _userCBprocAliasSet_count; index++) {                               // find alias in table (break if found)
            if (strcmp(_callbackUserProcAlias[index], alias) == 0) { isDeclared = true; break; }
        }
        if (!isDeclared) { execResult = result_aliasNotDeclared; return execResult; }

        LE_evalStack* pStackLvlFirstValueArg = (LE_evalStack*)evalStack.getNextListElement(pstackLvl);
        pstackLvl = pStackLvlFirstValueArg;

        // variable references to store (arguments 2[..4]) 
        const char isVariable = 0x80;                                                               // mask: is variable (not a constant) 

        Val args[3]{  };                                                                            // values to be passed to user routine
        char valueType[3]{ value_noValue,value_noValue,value_noValue };                             // value types (long, float, char string)
        char varScope[3]{};                                                                         // if variable: variable scope (user, program global, static, local)
        bool argIsVar[3]{};                                                                         // flag: is variable (scalar or aray)
        bool argIsArray[3]{};                                                                       // flag: is array element

        const void* values[3]{};                                                                    // to keep it simple for the c++ user writing the user routine, we simply pass const void pointers

        // any data to pass ? (optional arguments 2 to 4)
        if (cmdParamCount >= 2) {                                                                   // first argument (callback procedure) processed (but still on the stack)
            copyValueArgsFromStack(pstackLvl, cmdParamCount - 1, argIsVar, argIsArray, valueType, args, true);  // creates a NEW temporary string object if empty string OR or constant (non-variable) string 
            pstackLvl = pStackLvlFirstValueArg;     // set stack level again to first value argument
            for (int i = 0; i < cmdParamCount - 1; i++) {
                if (argIsVar[i]) {                                                                  // is this a variable ? (not a constant)
                    valueType[i] |= isVariable;                                                     // flag as variable (scalar or array element)
                    varScope[i] = (pstackLvl->varOrConst.variableAttributes & var_scopeMask);       // remember variable scope (user, program global, local, static) 
                }
                values[i] = args[i].pBaseValue;                                                     // set void pointer to: integer, float, char* 
                pstackLvl = (LE_evalStack*)evalStack.getNextListElement(pstackLvl);
            }
        }


        // call user routine
        // -----------------

        _callbackUserProcStart[index](values, valueType);                                           // call back user procedure


        // postprocess: check any strings RETURNED by callback procedure
        // -------------------------------------------------------------

        pstackLvl = pStackLvlFirstValueArg;                                                         // set stack level again to first value argument
        for (int i = 0; i < 3; i++) {
            if ((valueType[i] & value_typeMask) == value_isStringPointer) {

                // string COPY (or newly created empty variable string) passed to user routine ? (only if string passed is empty string OR or constant (non-variable) string)
                if (valueType[i] & passCopyToCallback) {
#if printCreateDeleteHeapObjects
                    Serial.print("----- (Intermd str) "); Serial.println((uint32_t)args[i].pStringConst - RAMSTART);
#endif
                    delete[] args[i].pStringConst;                                                  // delete temporary string
                    intermediateStringObjectCount--;
                }

                // callback routine changed non-empty VARIABLE string into empty variable string ("\0") ?
                else if (strlen(args[i].pStringConst) == 0) {

#if printCreateDeleteHeapObjects 
                    Serial.print((varScope[i] == var_isUser) ? "----- (usr var str) " : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
                    Serial.println((uint32_t)args[i].pStringConst - RAMSTART);
#endif
                    delete[]args[i].pStringConst;                                                   // delete variable string
                    (varScope[i] == var_isUser) ? userVarStringObjectCount-- : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? globalStaticVarStringObjectCount-- : localVarStringObjectCount--;

                    // set variable string pointer to null pointer
                    *pstackLvl->varOrConst.value.ppStringConst = nullptr;                           // change pointer to string (in variable) to null pointer
                }
            }
            pstackLvl = (LE_evalStack*)evalStack.getNextListElement(pstackLvl);
        }


        // finalize
        // --------

        clearEvalStackLevels(cmdParamCount);                                                        // clear evaluation stack and intermediate strings

        _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;                          // command execution ended
        _activeFunctionData.activeCmd_tokenAddress = nullptr;
    }
    break;


    // -----------------
    //
    // -----------------

    case MyParser::cmdcod_for:
    case MyParser::cmdcod_if:                                                                                                   // 'if' command
    case MyParser::cmdcod_while:                                                                                                // 'while' command
    {
        // start a new loop, or execute an existing loop ? 
        bool initNew{ true };        // IF...END: only one iteration (always new), FOR...END loop: always first itaration of a new loop, because only pass (command skipped for next iterations)
        if (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_while) {        // while block: start of an iteration
            if (flowCtrlStack.getElementCount() != 0) {                 // at least one open block exists ?
                char blockType = *(char*)_pFlowCtrlStackTop;
                if ((blockType == MyParser::block_for) || (blockType == MyParser::block_if)) { initNew = true; }
                else if (blockType == MyParser::block_while) {
                    // currently executing an iteration of an outer 'if', 'while' or 'for' loop ? Then this is the start of the first iteration of a new (inner) 'if' or 'while' loop
                    initNew = ((blockTestData*)_pFlowCtrlStackTop)->loopControl & withinIteration;      // 'within iteration' flag set ?
                }
            }
        }

        if (initNew) {
            _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
            _pFlowCtrlStackTop = (blockTestData*)flowCtrlStack.appendListElement(sizeof(blockTestData));
            ((blockTestData*)_pFlowCtrlStackTop)->blockType =
                (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_if) ? MyParser::block_if :
                (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_while) ? MyParser::block_while :
                MyParser::block_for;       // start of 'if...end' or 'while...end' block

            // FOR...END loops only: initialize ref to control variable, final value and step
            if (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_for) {

                // store variable reference, upper limit, optional increment / decrement (only once), address of token directly following 'FOR...; statement
                ((blockTestData*)_pFlowCtrlStackTop)->nextTokenAddress = _activeFunctionData.pNextStep;

                bool controlVarIsLong{ false }, finalValueIsLong{ false }, stepIsLong{ false };
                for (int i = 1; i <= cmdParamCount; i++) {        // skipped if no arguments
                    Val operand;                                                                                            // operand and result
                    bool operandIsVar = (pstackLvl->varOrConst.tokenType == tok_isVariable);
                    char valueType = operandIsVar ? (*pstackLvl->varOrConst.varTypeAddress & value_typeMask) : pstackLvl->varOrConst.valueType;
                    if ((valueType != value_isLong) && (valueType != value_isFloat)) { execResult = result_testexpr_numberExpected; return execResult; }
                    operand.floatConst = (operandIsVar ? *pstackLvl->varOrConst.value.pFloatConst : pstackLvl->varOrConst.value.floatConst);        // valid for long values as well

                    // store references to control variable and its value type
                    if (i == 1) {
                        controlVarIsLong = (valueType == value_isLong);         // remember
                        ((blockTestData*)_pFlowCtrlStackTop)->pControlVar = pstackLvl->varOrConst.value;      // pointer to variable (containing a long or float constant)
                        ((blockTestData*)_pFlowCtrlStackTop)->pControlValueType = pstackLvl->varOrConst.varTypeAddress;        // pointer to variable value type
                    }

                    // store final loop value
                    else if (i == 2) {
                        finalValueIsLong = (valueType == value_isLong);         // remember
                        ((blockTestData*)_pFlowCtrlStackTop)->finalValue = operand;
                    }

                    // store loop step
                    else {      // third parameter
                        stepIsLong = (valueType == value_isLong);         // remember
                        ((blockTestData*)_pFlowCtrlStackTop)->step = operand;
                    }                         // store loop increment / decrement 

                    pstackLvl = (LE_evalStack*)evalStack.getNextListElement(pstackLvl);
                }

                if (cmdParamCount < 3) {        // step not specified: init with default (1.)  
                    stepIsLong = false;
                    ((blockTestData*)_pFlowCtrlStackTop)->step.floatConst = 1.;     // init as float
                }

                // determine value type to use for loop tests, promote final value and step to float if value type to use for loop tests is float
                // the initial value type of the control variable and the value type of (constant) final value and step define the loop test value type
                ((blockTestData*)_pFlowCtrlStackTop)->testValueType = (controlVarIsLong && finalValueIsLong && stepIsLong ? value_isLong : value_isFloat);
                if (((blockTestData*)_pFlowCtrlStackTop)->testValueType == value_isFloat) {
                    if (finalValueIsLong) { ((blockTestData*)_pFlowCtrlStackTop)->finalValue.floatConst = (float)((blockTestData*)_pFlowCtrlStackTop)->finalValue.longConst; }
                    if (stepIsLong) { ((blockTestData*)_pFlowCtrlStackTop)->step.floatConst = (float)((blockTestData*)_pFlowCtrlStackTop)->step.longConst; }
                }

                ((blockTestData*)_pFlowCtrlStackTop)->loopControl |= forLoopInit;           // init at the start of initial FOR loop iteration
            }

            ((blockTestData*)_pFlowCtrlStackTop)->loopControl &= ~breakFromLoop;            // init at the start of initial iteration for any loop
        }

        ((blockTestData*)_pFlowCtrlStackTop)->loopControl |= withinIteration;               // init at the start of an iteration for any loop
    }

    // no break here: from here on, subsequent execution is common for 'if', 'elseif', 'else' and 'while'


    // -----------------
    //
    // -----------------

    case MyParser::cmdcod_else:
    case MyParser::cmdcod_elseif:

    {
        bool precedingTestFailOrNone{ true };  // init: preceding test failed ('elseif', 'else' command), or no preceding test ('if', 'for' command)
        // init: set flag to test condition of current 'if', 'while', 'elseif' command
        bool testClauseCondition = (_activeFunctionData.activeCmd_ResWordCode != MyParser::cmdcod_for);
        // 'else, 'elseif': if result of previous test (in preceding 'if' or 'elseif' clause) FAILED (fail = false), then CLEAR flag to test condition of current command (not relevant for 'else') 
        if ((_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_else) ||
            (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_elseif)) {
            precedingTestFailOrNone = (bool)(((blockTestData*)_pFlowCtrlStackTop)->loopControl & testFail);
        }
        testClauseCondition = precedingTestFailOrNone && (_activeFunctionData.activeCmd_ResWordCode != MyParser::cmdcod_for) && (_activeFunctionData.activeCmd_ResWordCode != MyParser::cmdcod_else);

        //init current condition test result (assume test in preceding clause ('if' or 'elseif') passed, so this clause needs to be skipped)
        bool fail = !precedingTestFailOrNone;
        if (testClauseCondition) {                                                                                // result of test in preceding 'if' or 'elseif' clause FAILED ? Check this clause
            Val operand;                                                                                            // operand and result
            bool operandIsVar = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);
            char valueType = operandIsVar ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
            if ((valueType != value_isLong) && (valueType != value_isFloat)) { execResult = result_testexpr_numberExpected; return execResult; }
            operand.floatConst = operandIsVar ? *_pEvalStackTop->varOrConst.value.pFloatConst : _pEvalStackTop->varOrConst.value.floatConst;        // valid for long values as well (same memory locations are copied)

            fail = (valueType == value_isFloat) ? (operand.floatConst == 0.) : (operand.longConst == 0);                                                                        // current test (elseif clause)
            ((blockTestData*)_pFlowCtrlStackTop)->loopControl = fail ? ((blockTestData*)_pFlowCtrlStackTop)->loopControl | testFail : ((blockTestData*)_pFlowCtrlStackTop)->loopControl & ~testFail;                                          // remember test result (true -> 0x1)
        }

        bool setNextToken = fail || (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_for);
        if (setNextToken) {                                                                                  // skip this clause ? (either a preceding test passed, or it failed but the curreent test failed as well)
            TokenIsResWord* pToToken;
            int toTokenStep{ 0 };
            pToToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;
            memcpy(&toTokenStep, pToToken->toTokenStep, sizeof(char[2]));
            _activeFunctionData.pNextStep = _programStorage + toTokenStep;              // prepare jump to 'else', 'elseif' or 'end' command
        }

        clearEvalStackLevels(cmdParamCount);      // clear evaluation stack

        _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // command execution ended
        _activeFunctionData.activeCmd_tokenAddress = nullptr;
    }
    break;


    // -----------------
    //
    // -----------------

    case MyParser::cmdcod_break:
    case MyParser::cmdcod_continue:
    {
        char blockType = MyParser::block_none;
        bool isLoop{};
        do {
            blockType = *(char*)_pFlowCtrlStackTop;
            // inner block(s) could be IF...END blocks (before reaching loop block)
            isLoop = ((blockType == MyParser::block_while) || (blockType == MyParser::block_for));
            if (isLoop) {
                Interpreter::TokenIsResWord* pToken;
                int toTokenStep{ 0 };
                pToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;                // pointer to loop start command token
                memcpy(&toTokenStep, pToken->toTokenStep, sizeof(char[2]));
                pToken = (TokenIsResWord*)(_programStorage + toTokenStep);                         // pointer to loop end command token
                memcpy(&toTokenStep, pToken->toTokenStep, sizeof(char[2]));
                _activeFunctionData.pNextStep = _programStorage + toTokenStep;                        // prepare jump to 'END' command
            }

            else {          // inner IF...END block: remove from flow control stack 
                flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                _pFlowCtrlStackTop = _pFlowCtrlStackMinus1;
                _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
                _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);
            }
        } while (!isLoop);

        if (_activeFunctionData.activeCmd_ResWordCode == MyParser::cmdcod_break) { ((blockTestData*)_pFlowCtrlStackTop)->loopControl |= breakFromLoop; }

        _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // command execution ended
        _activeFunctionData.activeCmd_tokenAddress = nullptr;
    }
    break;


    // -----------------
    //
    // -----------------

    case MyParser::cmdcod_end:
    {
        char blockType = *(char*)_pFlowCtrlStackTop;       // determine currently open block

        if ((blockType == MyParser::block_if) || (blockType == MyParser::block_while) || (blockType == MyParser::block_for)) {

            bool exitLoop{ true };

            if ((blockType == MyParser::block_for) || (blockType == MyParser::block_while)) {
                exitLoop = ((blockTestData*)_pFlowCtrlStackTop)->loopControl & breakFromLoop;  // BREAK command encountered
            }

            if (!exitLoop) {      // no BREAK encountered: loop terminated anyway ?
                if (blockType == MyParser::block_for) { execResult = testForLoopCondition(exitLoop); if (execResult != result_execOK) { return execResult; } }
                else if (blockType == MyParser::block_while) { exitLoop = (((blockTestData*)_pFlowCtrlStackTop)->loopControl & testFail); } // false: test passed
            }

            if (!exitLoop) {        // flag still not set ?
                if (blockType == MyParser::block_for) {
                    _activeFunctionData.pNextStep = ((blockTestData*)_pFlowCtrlStackTop)->nextTokenAddress;
                }
                else {      // WHILE...END block
                    Interpreter::TokenIsResWord* pToToken;
                    int toTokenStep{ 0 };
                    pToToken = (Interpreter::TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;
                    memcpy(&toTokenStep, pToToken->toTokenStep, sizeof(char[2]));

                    _activeFunctionData.pNextStep = _programStorage + toTokenStep;         // prepare jump to start of new loop
                }
            }

            ((blockTestData*)_pFlowCtrlStackTop)->loopControl &= ~withinIteration;          // at the end of an iteration
            _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // command execution ended
            _activeFunctionData.activeCmd_tokenAddress = nullptr;

            if (exitLoop) {
                flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                _pFlowCtrlStackTop = _pFlowCtrlStackMinus1;
                _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
                _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);
            }
            break;      // break here: do not break if end function !
        }
    }

    // no break here: from here on, subsequent execution is the same for 'end' (function) and for 'return'


    // -----------------
    //
    // -----------------

    case MyParser::cmdcod_return:
    {
        isFunctionReturn = true;
        bool returnWithZero = (cmdParamCount == 0);                    // RETURN statement without expression, or END statement: return a zero
        execResult = terminateExternalFunction(returnWithZero);
        if (execResult != result_execOK) { return execResult; }
    }
    break;

    }

    return result_execOK;
}


// -------------------------------
// *   test for loop condition   *
// -------------------------------

Interpreter::execResult_type Interpreter::testForLoopCondition(bool& testFails) {

    char testTypeIsLong = (((blockTestData*)_pFlowCtrlStackTop)->testValueType == value_isLong);    // loop final value and step have the initial control variable value type
    bool ctrlVarIsLong = ((*(uint8_t*)((blockTestData*)_pFlowCtrlStackTop)->pControlValueType & value_typeMask) == value_isLong);
    bool ctrlVarIsFloat = ((*(uint8_t*)((blockTestData*)_pFlowCtrlStackTop)->pControlValueType & value_typeMask) == value_isFloat);
    if (!ctrlVarIsLong && !ctrlVarIsFloat) { return result_testexpr_numberExpected; }                       // value type changed to string within loop: error

    Val& pCtrlVar = ((blockTestData*)_pFlowCtrlStackTop)->pControlVar;                                       // pointer to control variable
    Val& finalValue = ((blockTestData*)_pFlowCtrlStackTop)->finalValue;
    Val& step = ((blockTestData*)_pFlowCtrlStackTop)->step;
    char& loopControl = ((blockTestData*)_pFlowCtrlStackTop)->loopControl;


    if (ctrlVarIsLong) {                                                                                    // current control variable value type is long
        if (testTypeIsLong) {                                                                               // loop final value and step are long
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pLongConst = *pCtrlVar.pLongConst + step.longConst; }
            if (step.longConst > 0) { testFails = (*pCtrlVar.pLongConst > finalValue.longConst); }
            else { testFails = (*pCtrlVar.pLongConst < finalValue.longConst); }
        }
        else {                                                                                              // loop final value and step are float: promote long values to float
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pLongConst = (long)((float)*pCtrlVar.pLongConst + step.floatConst); }  // store result back as LONG (do not change control variable value type)
            if (step.floatConst > 0.) { testFails = ((float)*pCtrlVar.pLongConst > finalValue.floatConst); }
            else { testFails = ((float)*pCtrlVar.pLongConst < finalValue.floatConst); }
        }
    }
    else {                                                                                                  // current control variable value type is float
        if (testTypeIsLong) {                                                                               // loop final value and step are long: promote long values to float
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pFloatConst = (*pCtrlVar.pFloatConst + (float)step.longConst); }
            if ((float)step.longConst > 0.) { testFails = (*pCtrlVar.pFloatConst > (float)finalValue.longConst); }
            else { testFails = (*pCtrlVar.pFloatConst < (float)finalValue.longConst); }
        }
        else {                                                                                              // loop final value and step are float
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pFloatConst = *pCtrlVar.pFloatConst + step.floatConst; }
            if (step.floatConst > 0.) { testFails = (*pCtrlVar.pFloatConst > finalValue.floatConst); }
            else { testFails = (*pCtrlVar.pFloatConst < finalValue.floatConst); }
        }
    }

    loopControl &= ~forLoopInit;             // reset 'FOR loop init' flag
    return result_execOK;
};


// -----------------------------------------------------------------------------------------
// *   jump n token steps, return token type and (for terminals and keywords) token code   *
// -----------------------------------------------------------------------------------------

// optional parameter not allowed with reference parameter: create separate entry
int Interpreter::jumpTokens(int n) {
    int tokenCode;
    char* pStep;
    return jumpTokens(n, pStep, tokenCode);
}

int Interpreter::jumpTokens(int n, char*& pStep) {
    int tokenCode;
    return jumpTokens(n, pStep, tokenCode);
}

int Interpreter::jumpTokens(int n, char*& pStep, int& tokenCode) {

    // pStep: pointer to token
    // n: number of tokens to jump
    // return 'tok_no_token' if not enough tokens are present 

    int tokenType = tok_no_token;

    for (int i = 1; i <= n; i++) {
        tokenType = *pStep & 0x0F;
        if (tokenType == tok_no_token) { return tok_no_token; }               // end of program reached
        // terminals and constants: token length is NOT stored in token type
        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
            (tokenType == Interpreter::tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;
        pStep = pStep + tokenLength;
    }

    tokenType = *pStep & 0x0F;
    int tokenIndex{ 0 };

    switch (tokenType) {
    case tok_isReservedWord:
        tokenIndex = (((TokenIsResWord*)pStep)->tokenIndex);
        tokenCode = MyParser::_resWords[tokenIndex].resWordCode;
        break;

    case tok_isTerminalGroup1:
    case tok_isTerminalGroup2:
    case tok_isTerminalGroup3:
        tokenIndex = ((((TokenIsTerminal*)pStep)->tokenTypeAndIndex >> 4) & 0x0F);
        tokenIndex += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
        tokenCode = MyParser::_terminals[tokenIndex].terminalCode;
        break;

    default:
        break;

    }

    return tokenType;
}

// ------------------------------------
// *   advance until specific token   *
// ------------------------------------

int Interpreter::findTokenStep(int tokenTypeToFind, char tokenCodeToFind, char*& pStep) {

    // pStep: pointer to first token to test versus token group and (if applicable) token code
    // tokenType: if 'tok_isTerminalGroup1', test for the three terminal groups !

    // exclude current token step
    int tokenType = *pStep & 0x0F;
    // terminals and constants: token length is NOT stored in token type
    int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
        (tokenType == Interpreter::tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;        // fetch next token 
    pStep = pStep + tokenLength;

    do {
        tokenType = *pStep & 0x0F;

        bool tokenTypeMatch = (tokenTypeToFind == tokenType);
        if (tokenTypeToFind == tok_isTerminalGroup1) { tokenTypeMatch = tokenTypeMatch || (tokenType == tok_isTerminalGroup2) || (tokenType == tok_isTerminalGroup3); }
        if (tokenTypeMatch) {
            bool tokenCodeMatch{ false };
            int tokenIndex{ 0 };

            switch (tokenTypeToFind) {
            case tok_isReservedWord:
                tokenIndex = (((TokenIsResWord*)pStep)->tokenIndex);
                tokenCodeMatch = MyParser::_resWords[tokenIndex].resWordCode == tokenCodeToFind;
                break;

            case tok_isTerminalGroup1:       // actual token can be part of any of the three terminal groups
                tokenIndex = ((((TokenIsTerminal*)pStep)->tokenTypeAndIndex >> 4) & 0x0F);
                tokenIndex += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
                tokenCodeMatch = MyParser::_terminals[tokenIndex].terminalCode == tokenCodeToFind;
                break;

            case tok_no_token:
                return tokenType;       // token not found
                break;

            default:
                break;
            }
            if (tokenCodeMatch) { return tokenType; }      // if terminal, then return exact group (entry: use terminalGroup1) 
        }

        int tokenLength = (tokenType >= Interpreter::tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
            (tokenType == Interpreter::tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;    // fetch next token 
        pStep = pStep + tokenLength;
    } while (true);
}


// ------------------------------------------------
// Save last value for future reuse by calculations 
// ------------------------------------------------

void Interpreter::saveLastValue(bool& overWritePrevious) {
    if (!(evalStack.getElementCount() > _activeFunctionData.callerEvalStackLevels)) { return; }           // safety: data available ?

    // if overwrite 'previous' last result, then replace first item (if there is one); otherwise replace last item if FiFo full (-1 if nothing to replace)
    int itemToRemove = overWritePrevious ? ((_lastResultCount >= 1) ? 0 : -1) :
        ((_lastResultCount == MAX_LAST_RESULT_DEPTH) ? MAX_LAST_RESULT_DEPTH - 1 : -1);

    // remove a previous item ?
    if (itemToRemove != -1) {
        // if item to remove is a string: delete heap object
        if (lastResultTypeFiFo[itemToRemove] == value_isStringPointer) {

            if (lastResultValueFiFo[itemToRemove].pStringConst != nullptr) {
#if printCreateDeleteHeapObjects
                Serial.print("----- (FiFo string) ");   Serial.println((uint32_t)lastResultValueFiFo[itemToRemove].pStringConst - RAMSTART);
#endif 
                // note: this is always an intermediate string
                delete[] lastResultValueFiFo[itemToRemove].pStringConst;
                lastValuesStringObjectCount--;
            }
        }
    }
    else {
        _lastResultCount++;     // only adding an item, without removing previous one
    }


    // move older last results one place up in FIFO, except when just overwriting 'previous' last result
    if (!overWritePrevious && (_lastResultCount > 1)) {       // if 'new' last result count is 1, no old results need to be moved  
        for (int i = _lastResultCount - 1; i > 0; i--) {
            lastResultValueFiFo[i] = lastResultValueFiFo[i - 1];
            lastResultTypeFiFo[i] = lastResultTypeFiFo[i - 1];
        }
    }



    // store new last value
    VarOrConstLvl lastvalue;
    bool lastValueIsVariable = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);
    bool lastValueNumeric = ((_pEvalStackTop->varOrConst.valueType == value_isLong) || (_pEvalStackTop->varOrConst.valueType == value_isFloat));
    bool lastValueIntermediate = ((_pEvalStackTop->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate);

    // line below works for long integers as well
    if (lastValueNumeric) { lastvalue.value.floatConst = (lastValueIsVariable ? (*_pEvalStackTop->varOrConst.value.pFloatConst) : _pEvalStackTop->varOrConst.value.floatConst); }
    else { lastvalue.value.pStringConst = (lastValueIsVariable ? (*_pEvalStackTop->varOrConst.value.ppStringConst) : _pEvalStackTop->varOrConst.value.pStringConst); }

    if ((lastValueNumeric) || (!lastValueNumeric && (lastvalue.value.pStringConst == nullptr))) {
        lastResultValueFiFo[0] = lastvalue.value;
    }
    // new last value is a non-empty string: make a copy of the string and store a reference to this new string
    else {
        int stringlen = min(strlen(lastvalue.value.pStringConst), MyParser::_maxAlphaCstLen);        // excluding terminating \0
        lastResultValueFiFo[0].pStringConst = new char[stringlen + 1];
        lastValuesStringObjectCount++;
        memcpy(lastResultValueFiFo[0].pStringConst, lastvalue.value.pStringConst, stringlen);        // copy the actual string (not the pointer); do not use strcpy
        lastResultValueFiFo[0].pStringConst[stringlen] = '\0';

#if printCreateDeleteHeapObjects
        Serial.print("+++++ (FiFo string) ");   Serial.println((uint32_t)lastResultValueFiFo[0].pStringConst - RAMSTART);
#endif            

        if (lastValueIntermediate) {
#if printCreateDeleteHeapObjects
            Serial.print("----- (intermd str) ");   Serial.println((uint32_t)lastvalue.value.pStringConst - RAMSTART);
#endif
            delete[] lastvalue.value.pStringConst;
            intermediateStringObjectCount--;
        }
    }

    // store new last value type
    lastResultTypeFiFo[0] = _pEvalStackTop->varOrConst.valueType;               // value type

    // delete the stack level containing the result
    evalStack.deleteListElement(_pEvalStackTop);
    _pEvalStackTop = (LE_evalStack*)evalStack.getLastListElement();
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

    overWritePrevious = true;

    return;
}

// ----------------------------------------------------------------
// Clear evaluation stack and associated intermediate string object 
// ----------------------------------------------------------------

void Interpreter::clearEvalStack() {

    clearEvalStackLevels(evalStack.getElementCount());
    _pEvalStackTop = nullptr;  _pEvalStackMinus1 = nullptr; _pEvalStackMinus2 = nullptr;            // should be already

    // error if not all intermediate string objects deleted (points to an internal Justina issue)
    if (intermediateStringObjectCount != 0) {
        Serial.print("*** Intermediate string cleanup error. Remaining: "); Serial.println(intermediateStringObjectCount);
    }
    return;
}


// --------------------------------------------------------------------------
// Clear n evaluation stack levels and associated intermediate string objects  
// --------------------------------------------------------------------------

void Interpreter::clearEvalStackLevels(int n) {

    if (n <= 0) { return; }             // nothing to do

    LE_evalStack* pstackLvl = _pEvalStackTop, * pPrecedingStackLvl{};

    for (int i = 1; i <= n; i++) {
        // if intermediate constant string, then delete char string object (test op non-empty intermediate string object in called routine)  
        if (pstackLvl->genericToken.tokenType == tok_isConstant) { deleteIntermStringObject(pstackLvl); }    // exclude non-constant tokens (terminals, keywords, functions, ...)

        // delete evaluation stack level
        pPrecedingStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pstackLvl);
        evalStack.deleteListElement(pstackLvl);
        pstackLvl = pPrecedingStackLvl;
    }

    _pEvalStackTop = pstackLvl;
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);
    return;
}

// ------------------------
// Clear flow control stack  
// ------------------------

void Interpreter::clearFlowCtrlStack() {                // and remaining local storage + local variable string and array values for open functions

    void* pMainLvl = flowCtrlStack.getFirstListElement();        // main level or null pointer

    if (flowCtrlStack.getElementCount() > 1) {                // exclude main level
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;

        while (pFlowCtrlStackLvl != pMainLvl) {
            char blockType = *(char*)_pFlowCtrlStackTop;

            if (blockType == MyParser::block_extFunction) {               // open function
                int localVarCount = extFunctionData[_activeFunctionData.functionIndex].localVarCountInFunction;

                if (localVarCount > 0) {
                    _pmyParser->deleteArrayElementStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true);
                    _pmyParser->deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true);

                    // release local variable storage for function that has been called
                    delete[] _activeFunctionData.pLocalVarValues;
                    delete[] _activeFunctionData.pVariableAttributes;
                    delete[] _activeFunctionData.ppSourceVarTypes;
                }

                pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                if (pFlowCtrlStackLvl == nullptr) { break; }         // all done

                // load local storage pointers again for deepest CALLER function and restore pending step & active function information for caller function
                _activeFunctionData = *(FunctionData*)_pFlowCtrlStackTop;
            }
            else { pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl); }
        }
    }

    flowCtrlStack.deleteList();
    _pFlowCtrlStackTop = nullptr;   _pFlowCtrlStackMinus2 = nullptr; _pFlowCtrlStackMinus1 = nullptr;
}


// --------------------------------------------------------------------------------------------------------------------------
// *   execute internal or external function, calculate array element address or remove parenthesis around single argument  *
// --------------------------------------------------------------------------------------------------------------------------

Interpreter::execResult_type Interpreter::execParenthesesPair(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& firstArgStackLvl, int argCount) {
    // perform internal or external function, calculate array element address or simply make an expression result within parentheses an intermediate constant

    // no lower stack levels before left parenthesis (removed in the meantime) ? Is a simple parentheses pair
    if (pPrecedingStackLvl == nullptr) {
        makeIntermediateConstant(_pEvalStackTop);                     // left parenthesis already removed from evaluation stack
        return result_execOK;
    }

    // stack level preceding left parenthesis is internal function ? execute function
    else if (pPrecedingStackLvl->genericToken.tokenType == tok_isInternFunction) {
        execResult_type execResult = execInternalFunction(pPrecedingStackLvl, firstArgStackLvl, argCount);

        return execResult;
    }

    // stack level preceding left parenthesis is external function ? execute function
    else if (pPrecedingStackLvl->genericToken.tokenType == tok_isExternFunction) {
        execResult_type execResult = launchExternalFunction(pPrecedingStackLvl, firstArgStackLvl, argCount);
        return execResult;
    }

    // stack level preceding left parenthesis is an array variable name AND it requires an array element ?
    // (if it is a variable name, it can still be an array name used as previous argument in a function call)
    else if (pPrecedingStackLvl->genericToken.tokenType == tok_isVariable) {
        if ((pPrecedingStackLvl->varOrConst.valueAttributes & var_isArray_pendingSubscripts) == var_isArray_pendingSubscripts) {
            execResult_type execResult = arrayAndSubscriptsToarrayElement(pPrecedingStackLvl, firstArgStackLvl, argCount);
            return execResult;
        }
    }

    // none of the te above: simple parenthesis pair ? If variable inside, make it an intermediate constant on the stack 
    makeIntermediateConstant(_pEvalStackTop);                     // left parenthesis already removed from evaluation stack
    return result_execOK;
}

// ------------------------------------------------------------------------------------------------------------------
// *   replace array variable base address and subscripts with the array element address on the evaluation stack   *
// ------------------------------------------------------------------------------------------------------------------

Interpreter::execResult_type Interpreter::arrayAndSubscriptsToarrayElement(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pStackLvl, int argCount) {
    void* pArray = *pPrecedingStackLvl->varOrConst.value.ppArray;
    _activeFunctionData.errorProgramCounter = pPrecedingStackLvl->varOrConst.tokenAddress;                // token adress of array name (in the event of an error)

    int elemSpec[3] = { 0,0,0 };
    int dimNo = 0;
    do {
        bool opIsLong = (pStackLvl->varOrConst.valueType == value_isLong);
        bool opIsFloat = (pStackLvl->varOrConst.valueType == value_isFloat);
        if (!(opIsLong || opIsFloat)) { return result_array_subscriptNonNumeric; }

        if (opIsLong) {
            int l = (pStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pStackLvl->varOrConst.value.pLongConst) : pStackLvl->varOrConst.value.longConst;
            elemSpec[dimNo] = l;
        }
        else {
            float f = (pStackLvl->varOrConst.tokenType == tok_isVariable) ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst;
            elemSpec[dimNo] = f;
            if (f != elemSpec[dimNo]) { return result_array_subscriptNonInteger; }
        }

        pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
    } while (++dimNo < argCount);


    // calculate array element address and replace array base address with it in stack
    // -------------------------------------------------------------------------------

    // dim count test only needed for function parameters receiving arrays: dimension count not yet known during parsing (should always equal caller's array dim count) 

    int arrayDimCount = ((char*)pArray)[3];
    if (arrayDimCount != argCount) { return result_array_dimCountInvalid; }

    void* pArrayElem = arrayElemAddress(pArray, elemSpec);
    if (pArrayElem == nullptr) { return result_array_subscriptOutsideBounds; }

    pPrecedingStackLvl->varOrConst.value.pBaseValue = pArrayElem;
    pPrecedingStackLvl->varOrConst.valueAttributes &= ~var_isArray_pendingSubscripts;           // remove 'pending subscripts' flag 
    // note: other data does not change (array attributes, value type, token type, intermediate constant, variable type address)


    // Remove array subscripts from evaluation stack
    // ----------------------------------------------

    clearEvalStackLevels(argCount);

    return result_execOK;
}

// -----------------------------------------------------
// *   turn stack operand into intermediate constant   *
// -----------------------------------------------------

void Interpreter::makeIntermediateConstant(LE_evalStack* pEvalStackLvl) {
    // if a (scalar) variable or a parsed constant: replace by an intermediate constant

    if ((pEvalStackLvl->varOrConst.valueAttributes & constIsIntermediate) == 0) {                    // not an intermediate constant (variable or parsed constant)
        Val operand, result;                                                               // operands and result
        bool operandIsVar = (pEvalStackLvl->varOrConst.tokenType == tok_isVariable);
        char valueType = operandIsVar ? (*pEvalStackLvl->varOrConst.varTypeAddress & value_typeMask) : pEvalStackLvl->varOrConst.valueType;

        bool opIsLong = (valueType == value_isLong);
        bool opIsFloat = (valueType == value_isFloat);
        // next line is valid for long integers as well
        if (opIsLong || opIsFloat) { operand.floatConst = operandIsVar ? (*pEvalStackLvl->varOrConst.value.pFloatConst) : pEvalStackLvl->varOrConst.value.floatConst; }
        else { operand.pStringConst = operandIsVar ? (*pEvalStackLvl->varOrConst.value.ppStringConst) : pEvalStackLvl->varOrConst.value.pStringConst; }

        // if the value (parsed constant or variable value) is a non-empty string value, make a copy of the character string and store a pointer to this copy as result
        // as the operand is not an intermediate constant, NO intermediate string object (if it's a string) needs to be deleted
        if (opIsLong || opIsFloat || ((!opIsLong && !opIsFloat) && ((operand.pStringConst == nullptr)))) {
            result = operand;
        }
        else {
            int stringlen = strlen(operand.pStringConst);
            result.pStringConst = new char[stringlen + 1];
            intermediateStringObjectCount++;
            strcpy(result.pStringConst, operand.pStringConst);        // copy the actual strings 
#if printCreateDeleteHeapObjects
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)result.pStringConst - RAMSTART);

#endif
        }
        pEvalStackLvl->varOrConst.value = result;                        // float or pointer to string (type: no change)
        pEvalStackLvl->varOrConst.valueType = valueType;
        pEvalStackLvl->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        pEvalStackLvl->varOrConst.valueAttributes = constIsIntermediate;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        pEvalStackLvl->varOrConst.variableAttributes = 0x00;                  // not an array, not an array element (it's a constant) 
    }
}


// ----------------------------------------
// *   execute all processed operations   *
// ----------------------------------------

Interpreter::execResult_type  Interpreter::execAllProcessedOperators() {            // prefix and infix

    // _pEvalStackTop should point to an operand on entry (parsed constant, variable, expression result)

    int pendingTokenIndex{ 0 };
    int pendingTokenType{ tok_no_token }, pendingTokenPriorityLvl{};
    bool currentOpHasPriority{ false };

#if debugPrint
    Serial.print("** exec processed infix operators -stack levels: "); Serial.println(evalStack.getElementCount());
#endif
    // check if (an) operation(s) can be executed 
    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)

    while (evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 2) {                                                      // a preceding token exists on the stack

        // the entry preceding the current parsed constant, variable or expression result is ALWAYS a terminal (but never a right parenthesis, which is never pushed to the evaluation stack)
        // the current entry could also be preceded by a generic name on the evaluation stack: check
        ////  kan generic name zijn !!! adapt comment 

        int terminalIndex{};
        bool minus1IsOperator{ false };       // init
        bool minus1IsTerminal = ((_pEvalStackMinus1->genericToken.tokenType == tok_isTerminalGroup1) || (_pEvalStackMinus1->genericToken.tokenType == tok_isTerminalGroup2) || (_pEvalStackMinus1->genericToken.tokenType == tok_isTerminalGroup3));
        if (minus1IsTerminal) {
            terminalIndex = _pEvalStackMinus1->terminal.index & 0x7F;
            minus1IsOperator = (MyParser::_terminals[terminalIndex].terminalCode <= MyParser::termcod_opRangeEnd);  // preceding entry is operator ?
        }
        if (minus1IsOperator) {
            // check pending (not yet processed) token (always present and always a terminal token after a variable or constant token)
            // pending token can be any terminal token: infix operator, left or right parenthesis, comma or semicolon 
            // it can not be a prefix operator because it follows an operand (on top of stack)
            pendingTokenType = *_activeFunctionData.pNextStep & 0x0F;                                    // there's always minimum one token pending (even if it is a semicolon)
            pendingTokenIndex = (*_activeFunctionData.pNextStep >> 4) & 0x0F;                            // terminal token only: index stored in high 4 bits of token type 
            pendingTokenIndex += ((pendingTokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (pendingTokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);

            // infix operation ?
            bool isPrefixOperator = true;             // init as prefix operation
            if (evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 3) {         // TWO preceding tokens exist on the stack               
                isPrefixOperator = (!(_pEvalStackMinus2->genericToken.tokenType == tok_isConstant) && !(_pEvalStackMinus2->genericToken.tokenType == tok_isVariable));
                // comma separators are not pushed to the evaluation stack, but if it is followed by a (prefix) operator, a flag is set in order not to mistake a token sequence as two operands and an infix operation
                if (_pEvalStackMinus1->terminal.index & 0x80) { isPrefixOperator = true; }            // e.g. print 5, -6 : prefix operation on second expression ('-6') and not '5-6' as infix operation
            }

            // check priority and associativity (prefix or infix)
            int priority{ 0 };
            if (isPrefixOperator) { priority = _pmyParser->_terminals[terminalIndex].prefix_priority & 0x1F; }       // bits v43210 = priority
            else { priority = _pmyParser->_terminals[terminalIndex].infix_priority & 0x1F; }

            bool RtoLassociativity = isPrefixOperator ? true : _pmyParser->_terminals[terminalIndex].infix_priority & MyParser::op_RtoL;

            // is pending token a postfix operator ? (it can not be a prefix operator)
            bool isPostfixOperator = (_pmyParser->_terminals[pendingTokenIndex].postfix_priority != 0);

            // if a pending operator has higher priority, or, it has equal priority and operator is right-to-left associative, do not execute operator yet 
            // note that a PENDING LEFT PARENTHESIS also has priority over the preceding operator
            pendingTokenPriorityLvl = (isPostfixOperator ? (_pmyParser->_terminals[pendingTokenIndex].postfix_priority & 0x1F) :        // bits v43210 = priority
                (_pmyParser->_terminals[pendingTokenIndex].infix_priority) & 0x1F);  // pending terminal is either an infix or a postfix operator
            currentOpHasPriority = (priority >= pendingTokenPriorityLvl);
            if (RtoLassociativity && (priority == pendingTokenPriorityLvl)) { currentOpHasPriority = false; }
            if (!currentOpHasPriority) { break; }   // exit while() loop

            // execute operator
            execResult_type execResult = (isPrefixOperator) ? execUnaryOperation(true) : execInfixOperation();
            if (execResult != result_execOK) { return execResult; }
        }

        // token preceding the operand is a left parenthesis ? exit while loop (nothing to do for now)
        else { break; }
    }

    return result_execOK;
}


// -------------------------------
// *   execute unary operation   *
// -------------------------------

Interpreter::execResult_type  Interpreter::execUnaryOperation(bool isPrefix) {

    Val operand, opResult;                                                               // operand and result

    // what are the stack levels for operator and operand ?
    LE_evalStack* pOperandStackLvl = isPrefix ? _pEvalStackTop : _pEvalStackMinus1;
    LE_evalStack* pUnaryOpStackLvl = isPrefix ? _pEvalStackMinus1 : _pEvalStackTop;
    _activeFunctionData.errorProgramCounter = pUnaryOpStackLvl->terminal.tokenAddress;                // in the event of an error


    // (1) Fetch operator info, whether operand is variables, and operand value type 
    // -----------------------------------------------------------------------------

    // operator
    int terminalIndex = pUnaryOpStackLvl->terminal.index & 0x7F;
    char terminalCode = _pmyParser->_terminals[terminalIndex].terminalCode;
    bool requiresLongOp = (_pmyParser->_terminals[terminalIndex].prefix_priority & MyParser::op_long);
    bool resultCastLong = (_pmyParser->_terminals[terminalIndex].prefix_priority & MyParser::res_long);

    // operand, result
    bool operandIsVar = (pOperandStackLvl->varOrConst.tokenType == tok_isVariable);
    char opValueType = operandIsVar ? (*pOperandStackLvl->varOrConst.varTypeAddress & value_typeMask) : pOperandStackLvl->varOrConst.valueType;
    bool opIsFloat = (opValueType == value_isFloat);
    bool opIsLong = (opValueType == value_isLong);

    // (2) apply RULES: check for value type errors. ERROR if operand is either not numeric, or it is a float while a long is required
    // -------------------------------------------------------------------------------------------------------------------------------

    if (!opIsLong && !opIsFloat) { return result_numberExpected; }                   // value is numeric ?
    if (!opIsLong && requiresLongOp) { return result_integerExpected; }              // only integer value type allowed


    // (3) fetch operand - note that line is valid for long integers as well
    // ---------------------------------------------------------------------
    operand.floatConst = operandIsVar ? *pOperandStackLvl->varOrConst.value.pFloatConst : pOperandStackLvl->varOrConst.value.floatConst;


    // (4) execute (prefix or postfix) operator
    // ----------------------------------------

    if (terminalCode == _pmyParser->termcod_minus) { opIsFloat ? opResult.floatConst = -operand.floatConst : opResult.longConst = -operand.longConst; } // prefix minus 
    else if (terminalCode == _pmyParser->termcod_plus) { opResult = operand; } // prefix plus
    else if (terminalCode == _pmyParser->termcod_not) { opResult.longConst = opIsFloat ? (operand.floatConst == 0.) : (operand.longConst == 0); } // prefix: not
    else if (terminalCode == _pmyParser->termcod_incr) { opIsFloat ? opResult.floatConst = operand.floatConst + 1. : opResult.longConst = operand.longConst + 1; } // prefix & postfix: increment
    else if (terminalCode == _pmyParser->termcod_decr) { opIsFloat ? opResult.floatConst = operand.floatConst - 1. : opResult.longConst = operand.longConst - 1; } // prefix & postfix: decrement
    else if (terminalCode == _pmyParser->termcod_bitCompl) { opResult.longConst = ~operand.longConst; } // prefix: bit complement


    // float values: extra value tests

    int resultValueType = resultCastLong ? value_isLong : opValueType;

    if (resultValueType == value_isFloat) {      // floats only
        if (isnan(opResult.floatConst)) { return result_undefined; }
        else if (!isfinite(opResult.floatConst)) { return result_overflow; }
    }


    // (5) post process
    // ----------------

    // decrement or increment operation: store value in variable (variable type does not change) 

    bool isIncrDecr = ((terminalCode == _pmyParser->termcod_incr) || (terminalCode == _pmyParser->termcod_decr));
    if (isIncrDecr) { *pOperandStackLvl->varOrConst.value.pFloatConst = opResult.floatConst; }   // line is valid for long integers as well (same size)


    // if a prefix increment / decrement, then keep variable reference on the stack
    // if a postfix increment / decrement, replace variable reference in stack by UNMODIFIED value as intermediate constant
    //  if not a decrement / increment, replace value in stack by a new value (intermediate constant)

    if (!(isIncrDecr && isPrefix)) {                                              // prefix increment / decrement: keep variable reference (skip)
        pOperandStackLvl->varOrConst.value = isIncrDecr ? operand : opResult;       // replace stack entry with unmodified or modified value as intermediate constant
        pOperandStackLvl->varOrConst.valueType = resultValueType;
        pOperandStackLvl->varOrConst.tokenType = tok_isConstant;                    // use generic constant type
        pOperandStackLvl->varOrConst.valueAttributes = constIsIntermediate;
        pOperandStackLvl->varOrConst.variableAttributes = 0x00;                     // not an array, not an array element (it's a constant) 
    }


    //  clean up stack (drop operator)

    _pEvalStackTop = pOperandStackLvl;
    evalStack.deleteListElement(pUnaryOpStackLvl);
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

    return result_execOK;
}


// -------------------------------
// *   execute infix operation   *
// -------------------------------

Interpreter::execResult_type  Interpreter::execInfixOperation() {

    Val operand1, operand2, opResult;                                                               // operands and result

    _activeFunctionData.errorProgramCounter = _pEvalStackMinus1->terminal.tokenAddress;                // in the event of an error


    // (1) Fetch operator info, whether operands are variables, and operand value types 
    // --------------------------------------------------------------------------------

    // operator
    int operatorCode = _pmyParser->_terminals[_pEvalStackMinus1->terminal.index & 0x7F].terminalCode;
    bool operationIncludesAssignment = ((_pmyParser->_terminals[_pEvalStackMinus1->terminal.index & 0x7F].infix_priority & 0x1F) == 0x01);
    bool requiresLongOp = (_pmyParser->_terminals[_pEvalStackMinus1->terminal.index & 0x7F].infix_priority & MyParser::op_long);
    bool resultCastLong = (_pmyParser->_terminals[_pEvalStackMinus1->terminal.index & 0x7F].infix_priority & MyParser::res_long);

    // operands
    bool operand1IsVar = (_pEvalStackMinus2->varOrConst.tokenType == tok_isVariable);
    char operand1valueType = operand1IsVar ? (*_pEvalStackMinus2->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackMinus2->varOrConst.valueType;
    bool op1isLong = ((uint8_t)operand1valueType == value_isLong);
    bool op1isFloat = ((uint8_t)operand1valueType == value_isFloat);
    bool op1isString = ((uint8_t)operand1valueType == value_isStringPointer);

    bool operand2IsVar = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);
    char operand2valueType = operand2IsVar ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
    bool op2isLong = ((uint8_t)operand2valueType == value_isLong);
    bool op2isFloat = ((uint8_t)operand2valueType == value_isFloat);
    bool op2isString = ((uint8_t)operand2valueType == value_isStringPointer);

    // (2) apply RULES: check for value type errors. ERROR if:
    // -------------------------------------------------------

    // - '=' (pure assignment) : if assignment to an array, the value to be assigned OR the (fixed) value type of the array is string, the other value type is numeric (long, float) 
    // - infix '+' (math plus or string concat operator): not both operands are either strings or numeric (long, float)
    // - %, %=, and &, |, ^, &=, |=, ^=, and <<, >>, <<=, >>= (bitwise operators): not both operands are long 
    // - other operators: not both operands are numeric (long, float)

    // main if...else level conditions: only include operatorCode tests
    if (operatorCode == _pmyParser->termcod_assign) { if ((op1isString != op2isString) && (_pEvalStackMinus2->varOrConst.variableAttributes & var_isArray)) { return result_array_valueTypeIsFixed; } }
    else if (((operatorCode == _pmyParser->termcod_plus) || (operatorCode == _pmyParser->termcod_plusAssign))) { if (op1isString != op2isString) { return result_operandsNumOrStringExpected; } }
    else if (requiresLongOp) { if (!op1isLong || !op2isLong) { return result_integerExpected; } }
    else { if (op1isString || op2isString) { return result_numberExpected; } }


    // (3) fetch operands: numeric constants or pointers to character strings - line is valid for long integers as well
    // ----------------------------------------------------------------------------------------------------------------

    if (op1isLong || op1isFloat) { operand1.floatConst = (operand1IsVar ? (*_pEvalStackMinus2->varOrConst.value.pFloatConst) : _pEvalStackMinus2->varOrConst.value.floatConst); }
    else { operand1.pStringConst = (operand1IsVar ? (*_pEvalStackMinus2->varOrConst.value.ppStringConst) : _pEvalStackMinus2->varOrConst.value.pStringConst); }
    if (op2isLong || op2isFloat) { operand2.floatConst = (operand2IsVar ? (*_pEvalStackTop->varOrConst.value.pFloatConst) : _pEvalStackTop->varOrConst.value.floatConst); }
    else { operand2.pStringConst = (operand2IsVar ? (*_pEvalStackTop->varOrConst.value.ppStringConst) : _pEvalStackTop->varOrConst.value.pStringConst); }


    // (4) if required, promote an OPERAND to float (after rules as per (1) have been applied)
    // ---------------------------------------------------------------------------------------

    // - '=' (pure assignment) : no action (operand 2 will overwrite 1)
    // - '**' (power): promote any long operand to float
    // - other operators: promote a long operand to float if the other operand is float

    // main if...else level conditions: only include operatorCode tests
    bool promoteOperandsToFloat{ false };
    if (operatorCode == _pmyParser->termcod_assign) {}           // pure assignment: no action 
    else if (operatorCode == _pmyParser->termcod_pow) { promoteOperandsToFloat = op1isLong || op2isLong; }
    else { promoteOperandsToFloat = op1isFloat ^ op2isFloat; }

    if (promoteOperandsToFloat) {
        if (op1isLong) { operand1.floatConst = operand1.longConst; op1isLong = false; op1isFloat = true; }
        if (op2isLong) { operand2.floatConst = operand2.longConst; op2isLong = false; op2isFloat = true; }
    }


    // (5) execute infix operator
    // --------------------------

    bool opResultLong = op2isLong || requiresLongOp || resultCastLong;              // before checking array value type, if assigning to array, ...
    bool opResultFloat = op2isFloat && !(requiresLongOp || resultCastLong);         // ...operand value types: after promotion, if promoted
    bool opResultString = op2isString && !requiresLongOp || resultCastLong;

    switch (operatorCode) {                                                  // operation to execute

    case MyParser::termcod_assign:
        opResult = operand2;
        break;

        // note: no overflow checks for arithmatic operators (+ - * /)

    case MyParser::termcod_plus:            // also for concatenation
    case MyParser::termcod_plusAssign:
        if (opResultString) {      // then operands are strings as well
            bool op1emptyString = (operand1.pStringConst == nullptr);
            bool op2emptyString = (operand2.pStringConst == nullptr);

            // concatenate two operand strings objects and store pointer to it in result
            int stringlen = 0;                                  // is both operands are empty strings
            if (!op1emptyString) { stringlen = strlen(operand1.pStringConst); }
            if (!op2emptyString) { stringlen += strlen(operand2.pStringConst); }
            if (stringlen == 0) { opResult.pStringConst = nullptr; }                                // empty strings are represented by a nullptr (conserve heap space)
            else {
                opResult.pStringConst = new char[stringlen + 1];
                intermediateStringObjectCount++;
                opResult.pStringConst[0] = '\0';                                // in case first operand is nullptr
                if (!op1emptyString) { strcpy(opResult.pStringConst, operand1.pStringConst); }
                if (!op2emptyString) { strcat(opResult.pStringConst, operand2.pStringConst); }

#if printCreateDeleteHeapObjects
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)opResult.pStringConst - RAMSTART);
#endif
            }
        }

        else {
            opResultLong ? opResult.longConst = operand1.longConst + operand2.longConst : opResult.floatConst = operand1.floatConst + operand2.floatConst;
        }
        break;

    case MyParser::termcod_minus:
    case MyParser::termcod_minusAssign:
        opResultLong ? opResult.longConst = operand1.longConst - operand2.longConst : opResult.floatConst = operand1.floatConst - operand2.floatConst;
        break;

    case MyParser::termcod_mult:
    case MyParser::termcod_multAssign:
        opResultLong ? opResult.longConst = operand1.longConst * operand2.longConst : opResult.floatConst = operand1.floatConst * operand2.floatConst;
        if (opResultFloat) { if ((operand1.floatConst != 0) && (operand2.floatConst != 0) && (!isnormal(opResult.floatConst))) { return result_underflow; } }
        break;

    case MyParser::termcod_div:
    case MyParser::termcod_divAssign:
        if (opResultFloat) { if ((operand1.floatConst != 0) && (operand2.floatConst == 0)) { return result_divByZero; } }
        else { if (operand2.longConst == 0) { return (operand1.longConst == 0) ? result_undefined : result_divByZero; } }
        opResultLong ? opResult.longConst = operand1.longConst / operand2.longConst : opResult.floatConst = operand1.floatConst / operand2.floatConst;
        if (opResultFloat) { if ((operand1.floatConst != 0) && (!isnormal(opResult.floatConst))) { return result_underflow; } }
        break;

    case MyParser::termcod_mod:
    case MyParser::termcod_modAssign:
        if (operand2.longConst == 0) { return (operand1.longConst == 0) ? result_undefined : result_divByZero; }
        opResult.longConst = operand1.longConst % operand2.longConst;
        break;

    case MyParser::termcod_bitAnd:
    case MyParser::termcod_bitAndAssign:
        opResult.longConst = operand1.longConst & operand2.longConst;
        break;

    case MyParser::termcod_bitOr:
    case MyParser::termcod_bitOrAssign:
        opResult.longConst = operand1.longConst | operand2.longConst;
        break;

    case MyParser::termcod_bitXor:
    case MyParser::termcod_bitXorAssign:
        opResult.longConst = operand1.longConst ^ operand2.longConst;
        break;

    case MyParser::termcod_bitShLeft:
    case MyParser::termcod_bitShLeftAssign:
        if ((operand2.longConst < 0) || (operand2.longConst >= 8 * sizeof(long))) { return result_outsideRange; }
        opResult.longConst = operand1.longConst << operand2.longConst;
        break;

    case MyParser::termcod_bitShRight:
    case MyParser::termcod_bitShRightAssign:
        if ((operand2.longConst < 0) || (operand2.longConst >= 8 * sizeof(long))) { return result_outsideRange; }
        opResult.longConst = operand1.longConst >> operand2.longConst;
        break;

    case MyParser::termcod_pow:     // operands always (converted to) floats
        if ((operand1.floatConst == 0) && (operand2.floatConst == 0)) { return result_undefined; } // C++ pow() provides 1 as result
        opResult.floatConst = pow(operand1.floatConst, operand2.floatConst);
        break;

    case MyParser::termcod_and:
        opResult.longConst = opResultLong ? (operand1.longConst && operand2.longConst) : (operand1.floatConst && operand2.floatConst);
        break;

    case MyParser::termcod_or:
        opResult.longConst = opResultLong ? (operand1.longConst || operand2.longConst) : (operand1.floatConst || operand2.floatConst);
        break;

    case MyParser::termcod_lt:
        opResult.longConst = opResultLong ? (operand1.longConst < operand2.longConst) : (operand1.floatConst < operand2.floatConst);
        break;

    case MyParser::termcod_gt:
        opResult.longConst = opResultLong ? (operand1.longConst > operand2.longConst) : (operand1.floatConst > operand2.floatConst);
        break;

    case MyParser::termcod_eq:
        opResult.longConst = opResultLong ? (operand1.longConst == operand2.longConst) : (operand1.floatConst == operand2.floatConst);
        break;

    case MyParser::termcod_ltoe:
        opResult.longConst = opResultLong ? (operand1.longConst <= operand2.longConst) : (operand1.floatConst <= operand2.floatConst);
        break;

    case MyParser::termcod_gtoe:
        opResult.longConst = opResultLong ? (operand1.longConst >= operand2.longConst) : (operand1.floatConst >= operand2.floatConst);
        break;

    case MyParser::termcod_ne:
        opResult.longConst = opResultLong ? (operand1.longConst != operand2.longConst) : (operand1.floatConst != operand2.floatConst);
        break;
    }       // switch


    // float values: extra value tests

    if ((opResultFloat) && (operatorCode != _pmyParser->termcod_assign)) {     // check error (float values only, not for pure assignment)
        if (isnan(opResult.floatConst)) { return result_undefined; }
        else if (!isfinite(opResult.floatConst)) { return result_overflow; }
    }


    // (6) store result in variable, if operation is a (pure or compound) assignment
    // -----------------------------------------------------------------------------

    if (operationIncludesAssignment) {

        // if variable currently holds a non-empty string (indicated by a nullptr), delete char string object
        execResult_type execResult = deleteVarStringObject(_pEvalStackMinus2); if (execResult != result_execOK) { return execResult; }

        // if the value to be assigned is numeric OR an empty string: simply assign the value (not a heap object)

        if (opResultLong || opResultFloat) {
            bool assignToArray = (_pEvalStackMinus2->varOrConst.variableAttributes & var_isArray);
            bool castToArrayValueType = (assignToArray && (((uint8_t)operand1valueType == value_isLong) ^ opResultLong));
            if (castToArrayValueType) {
                opResultLong = ((uint8_t)operand1valueType == value_isLong); opResultFloat = !opResultLong;
                opResultLong ? opResult.longConst = opResult.floatConst : opResult.floatConst = opResult.longConst;
            }
        }
        // the value (parsed constant, variable value or intermediate result) to be assigned to the receiving variable is a non-empty string value
        else if (opResultString && (opResult.pStringConst == nullptr)) {
            // nothing to do
        }
        else {  // non-empty string
            // note that for reference variables, the variable type fetched is the SOURCE variable type
            int varScope = _pEvalStackMinus2->varOrConst.variableAttributes & var_scopeMask;

            // make a copy of the character string and store a pointer to this copy as result (even if operand string is already an intermediate constant)
            // because the value will be stored in a variable, limit to the maximum allowed string length
            char* pUnclippedResultString = opResult.pStringConst;
            int stringlen = min(strlen(pUnclippedResultString), MyParser::_maxAlphaCstLen);
            opResult.pStringConst = new char[stringlen + 1];
            (varScope == var_isUser) ? userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? globalStaticVarStringObjectCount++ : localVarStringObjectCount++;
            memcpy(opResult.pStringConst, pUnclippedResultString, stringlen);        // copy the actual string (not the pointer); do not use strcpy
            opResult.pStringConst[stringlen] = '\0';                                         // add terminating \0
#if printCreateDeleteHeapObjects
            Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
            Serial.println((uint32_t)opResult.pStringConst - RAMSTART);
#endif
            if (operatorCode != _pmyParser->termcod_assign) {           // compound statement
#if printCreateDeleteHeapObjects
                Serial.print("----- (Intermd str) "); Serial.println("????"); //// CORRIGEER FOUT: Serial.println((uint32_t)toPrint.pStringConst - RAMSTART);
#endif
                if (pUnclippedResultString != nullptr) {     // pure assignment: is in fact pointing to operand 2 
                    delete[] pUnclippedResultString;
                    intermediateStringObjectCount--;
                }
            }
        }

        // store value in variable and adapt variable value type - next line is valid for long integers as well
        if (opResultLong || opResultFloat) { *_pEvalStackMinus2->varOrConst.value.pFloatConst = opResult.floatConst; }
        else { *_pEvalStackMinus2->varOrConst.value.ppStringConst = opResult.pStringConst; }
        *_pEvalStackMinus2->varOrConst.varTypeAddress = (*_pEvalStackMinus2->varOrConst.varTypeAddress & ~value_typeMask) |
            (opResultLong ? value_isLong : opResultFloat ? value_isFloat : value_isStringPointer);

        // if variable reference, then value type on the stack indicates 'variable reference', so don't overwrite it
        bool operand1IsVarRef = (_pEvalStackMinus2->varOrConst.valueType == value_isVarRef);
        if (!operand1IsVarRef) { // if reference, then value type on the stack indicates 'variable reference', so don't overwrite it
            _pEvalStackMinus2->varOrConst.valueType = (_pEvalStackMinus2->varOrConst.valueType & ~value_typeMask) |
                (opResultLong ? value_isLong : opResultFloat ? value_isFloat : value_isStringPointer);
        }
    }


    // (7) post process
    // ----------------

    // Delete any intermediate result string objects used as operands 

    // if operands are intermediate constant strings, then delete char string object
    deleteIntermStringObject(_pEvalStackTop);
    deleteIntermStringObject(_pEvalStackMinus2);


    //  clean up stack

    // drop highest 2 stack levels( operator and operand 2 ) 
    evalStack.deleteListElement(_pEvalStackTop);                          // operand 2 
    evalStack.deleteListElement(_pEvalStackMinus1);                       // operator
    _pEvalStackTop = _pEvalStackMinus2;
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);


    //  if operation did not include an assignment, store result in stack as an intermediate constant

    // if assignment, then result is already stored in variable and the stack top still contains the reference to the variable
    if (!operationIncludesAssignment) {
        _pEvalStackTop->varOrConst.value = opResult;                        // float or pointer to string
        _pEvalStackTop->varOrConst.valueType = opResultLong ? value_isLong : opResultFloat ? value_isFloat : value_isStringPointer;     // value type of second operand  
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
        _pEvalStackTop->varOrConst.variableAttributes = 0x00;                  // not an array, not an array element (it's a constant) 
    }
    return result_execOK;
}


// ---------------------------------
// *   execute internal function   *
// ---------------------------------

Interpreter::execResult_type Interpreter::execInternalFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount) {

    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;  // before pushing to stack   
    int functionIndex = pFunctionStackLvl->function.index;
    char functionCode = MyParser::_functions[functionIndex].functionCode;
    int arrayPattern = MyParser::_functions[functionIndex].arrayPattern;
    int minArgs = MyParser::_functions[functionIndex].minArgs;
    int maxArgs = MyParser::_functions[functionIndex].maxArgs;
    bool fcnResultIsLong = false, fcnResultIsFloat = false;   // init
    Val fcnResult;
    bool argIsVar[8], argIsLong[8], argIsFloat[8];
    char argValueType[8];
    Val args[8];


    // preprocess: retrieve argument(s) info: variable or constant, value type
    // -----------------------------------------------------------------------


    if (suppliedArgCount > 0) {
        LE_evalStack* pStackLvl = pFirstArgStackLvl;         // pointing to first argument on stack

        for (int i = 0; i < suppliedArgCount; i++) {
            // value type of args
            argIsVar[i] = (pStackLvl->varOrConst.tokenType == tok_isVariable);
            argValueType[i] = argIsVar[i] ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
            argIsLong[i] = ((uint8_t)argValueType[i] == value_isLong);
            argIsFloat[i] = ((uint8_t)argValueType[i] == value_isFloat);

            // fetch args: real constants or pointers to character strings (pointers to arrays: not used) - next line is valid for long values as well
            if (argIsLong || argIsFloat) { args[i].floatConst = (argIsVar[i] ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst); }
            else { args[i].pStringConst = (argIsVar[i] ? (*pStackLvl->varOrConst.value.ppStringConst) : pStackLvl->varOrConst.value.pStringConst); }

            pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);       // value fetched: go to next argument
        }
    }

    // execute a specific function
    // ---------------------------

    switch (functionCode) {

        // square root
        // -----------

    case MyParser::fnccod_sqrt:
    {
        if (!argIsLong[0] && !argIsFloat[0]) { return result_numberExpected; }
        if (argIsLong[0] ? args[0].longConst < 0 : args[0].floatConst < 0.) { return result_arg_outsideRange; }

        fcnResultIsFloat = true;
        fcnResult.floatConst = argIsLong[0] ? sqrt(args[0].longConst) : sqrt(args[0].floatConst);
    }
    break;


    // dimension count of an array
    // ---------------------------

    case MyParser::fnccod_dims:
    {
        void* pArray = *pFirstArgStackLvl->varOrConst.value.ppArray;

        fcnResultIsLong = true;
        fcnResult.longConst = ((char*)pArray)[3];
    }
    break;


    // array upper bound
    // -----------------
    case MyParser::fnccod_ubound:
    {
        if (!argIsLong[1] && !argIsFloat[1]) { return result_arg_dimNumberIntegerExpected; }
        void* pArray = *pFirstArgStackLvl->varOrConst.value.ppArray;
        int arrayDimCount = ((char*)pArray)[3];
        int dimNo = argIsLong[1] ? args[1].longConst : int(args[1].floatConst);
        if (argIsFloat[1]) { if (args[1].floatConst != dimNo) { return result_arg_dimNumberIntegerExpected; } }
        if ((dimNo < 1) || (dimNo > arrayDimCount)) { return result_arg_dimNumberInvalid; }

        fcnResultIsLong = true;
        fcnResult.longConst = ((char*)pArray)[--dimNo];
    }
    break;


    // variable value type
    // -------------------

    case MyParser::fnccod_valueType:
    {
        // note: to obtain the value type of an array, check the value type of one of its elements
        fcnResultIsLong = true;
        fcnResult.longConst = argValueType[0];
    }
    break;


    // retrieve one of the last calculation results
    // --------------------------------------------

    case MyParser::fnccod_last:
    {
        int FiFoElement = 1;    // init: newest FiFo element
        if (suppliedArgCount == 1) {              // FiFo element specified
            if (!argIsLong[0] && !argIsFloat[0]) { return result_arg_integerExpected; }
            FiFoElement = argIsLong[0] ? args[0].longConst : int(args[0].floatConst);
            if (argIsFloat[0]) { if (args[0].floatConst != FiFoElement) { return result_arg_integerExpected; } }
            if ((FiFoElement < 1) || (FiFoElement > MAX_LAST_RESULT_DEPTH)) { return result_arg_outsideRange; }
        }
        if (FiFoElement > _lastResultCount) { return result_arg_invalid; }

        --FiFoElement;
        fcnResultIsLong = (lastResultTypeFiFo[FiFoElement] == value_isLong);
        fcnResultIsFloat = (lastResultTypeFiFo[FiFoElement] == value_isFloat);
        if (fcnResultIsLong || fcnResultIsFloat || (!fcnResultIsLong && !fcnResultIsFloat && (lastResultValueFiFo[FiFoElement].pStringConst == nullptr))) {
            fcnResult = lastResultValueFiFo[FiFoElement];
        }
        else {                              // string
            fcnResult.pStringConst = new char[strlen(lastResultValueFiFo[FiFoElement].pStringConst + 1)];
            intermediateStringObjectCount++;
            strcpy(fcnResult.pStringConst, lastResultValueFiFo[FiFoElement].pStringConst);
#if printCreateDeleteHeapObjects
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif            
        }

    }
    break;


    // time since boot, in milliseconds
    // --------------------------------

    case MyParser::fnccod_millis:
    {
        fcnResultIsLong = true;
        fcnResult.longConst = millis();
    }
    break;


    // ASCII code of a single character in a string
    // -------------------------------------------

    case MyParser::fnccod_asc:
    {
        if (argIsLong[0] || argIsFloat[0]) { return result_arg_stringExpected; }
        if (args[0].pStringConst == nullptr) { return result_arg_invalid; }     // empty string
        int charPos = 1;            // first character
        if (suppliedArgCount == 2) {              // character position in string specified
            if (!argIsLong[1] && !argIsFloat[1]) { return result_arg_integerExpected; }
            charPos = argIsLong[1] ? args[1].longConst : int(args[1].floatConst);
            if (argIsFloat[1]) { if (args[1].floatConst != charPos) { return result_arg_integerExpected; } }
            if (charPos < 1) { return result_arg_outsideRange; }
        }
        if (charPos > strlen(args[0].pStringConst)) { return result_arg_invalid; }

        fcnResultIsLong = true;
        fcnResult.longConst = args[0].pStringConst[--charPos];     // character code
    }
    break;


    // return character with a given ASCII code
    // ----------------------------------------

    case MyParser::fnccod_char:     // convert ASCII code to 1-character string
    {
        if (!argIsLong[0] && !argIsFloat[0]) { return result_arg_integerExpected; }
        int asciiCode = argIsLong[0] ? args[0].longConst : int(args[0].floatConst);
        if (argIsFloat[0]) { if (args[0].floatConst != asciiCode) { return result_arg_integerExpected; } }
        if ((asciiCode < 1) || (asciiCode > 0xFF)) { return result_arg_outsideRange; }        // do not allow \0

        // result is string
        fcnResult.pStringConst = new char[2];
        intermediateStringObjectCount++;
        fcnResult.pStringConst[0] = asciiCode;
        fcnResult.pStringConst[1] = '\0';                                // terminating \0
#if printCreateDeleteHeapObjects
        Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif            
    }
    break;


    // return CR and LF character string
    // ---------------------------------

    case MyParser::fnccod_nl:             // new line character
    {
        // result is string
        fcnResult.pStringConst = new char[3];
        intermediateStringObjectCount++;
        fcnResult.pStringConst[0] = '\r';
        fcnResult.pStringConst[1] = '\n';
        fcnResult.pStringConst[2] = '\0';                                // terminating \0
    }
#if printCreateDeleteHeapObjects
    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif            
    break;


    // format a number or a string into a destination string
    // -----------------------------------------------------

    case MyParser::fnccod_format:
    {
        // mandatory argument 1: value to be formatted
        // optional arguments 2-5: width, precision, [specifier (F:fixed, E:scientific, G:general, D: long integer, X:hex)], flags, characters printed (return value)
        // behaviour corresponds to c++ printf, sprintf, ..., result is a formatted string
        // note that specifier argument can be left out, flags argument taking its place
        // width, precision, specifier and flags are used as defaults for next calls to this function, if they are not provided again
        // if the value to be formatted is a string, the precision argument is interpreted as 'maximum characters to print', otherwise it indicates numeric precision (both values retained seperately)
        // specifier is only relevant for formatting numbers (ignored for formatting strings), but can be set while formatting a string

        const int leftJustify = 0b1, forceSign = 0b10, blankIfNoSign = 0b100, addDecPoint = 0b1000, padWithZeros = 0b10000;     // flags
        bool isIntFmt{ false };
        int charsPrinted{ 0 };

        // INIT print width, precision, specifier, flags
        int& width = _printWidth, & precision = ((argIsLong[0] || argIsFloat[0]) ? _printNumPrecision : _printCharsToPrint), & flags = _printFmtFlags;

        // test arguments and ADAPT print width, precision, specifier, flags
        // -----------------------------------------------------------------

        execResult_type execResult = checkFmtSpecifiers(false, (!argIsLong[0] && !argIsFloat[0]), suppliedArgCount, argValueType, args, _printNumSpecifier[0], width, precision, flags);
        if (execResult != result_execOK) { return execResult; }

        // optional argument returning #chars that were printed is present ?  Variable expected
        bool hasSpecifierArg = false; // init
        if (suppliedArgCount >= 3) { hasSpecifierArg = (!argIsLong[3] && !argIsFloat[3]); }       // third argument is either a specifier (string) or set of flags (number)
        if (suppliedArgCount == (hasSpecifierArg ? 6 : 5)) {
            if (!argIsVar[suppliedArgCount - 1]) { return result_arg_varExpected; }          // it should be a variable
        }

        // prepare format specifier string and format
        // ------------------------------------------

        char  fmtString[20];        // long enough to contain all format specifier parts
        char* specifier = "s";
        if (argIsLong[0] || argIsFloat[0]) {
            specifier = _printNumSpecifier;
            isIntFmt = (specifier[0] == 'X') || (specifier[0] == 'x') || (specifier[0] == 'd') || (specifier[0] == 'D');
        }
        makeFormatString(flags, isIntFmt, specifier, fmtString);
        printToString(width, precision, (!argIsLong[0] && !argIsFloat[0]), isIntFmt, argValueType, args, fmtString, fcnResult, charsPrinted);

        // return number of characters printed into (variable) argument if it was supplied
        // -------------------------------------------------------------------------------

        // note: NO errors should occur beyond this point, OR the intermediate string containing the function result should be deleted
        if (suppliedArgCount == (hasSpecifierArg ? 6 : 5)) {      // optional argument returning #chars that were printed is present
            // if  variable currently holds a non-empty string (indicated by a nullptr), delete char string object
            execResult_type execResult = deleteVarStringObject(_pEvalStackTop); if (execResult != result_execOK) { return execResult; }

            // save value in variable and set variable value type to real ) {
            // note: if variable reference, then value type on the stack indicates 'variable reference' which should not be changed (but stack level will be deleted now anyway)
            *_pEvalStackTop->varOrConst.value.pFloatConst = charsPrinted;
            *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isFloat;
        }
    }
    break;


    // retrieve a system variable
    // --------------------------

    case MyParser::fnccod_sysVar:
    {
        if (!argIsLong[0] && !argIsFloat[0]) { return result_arg_integerExpected; }
        int sysVar = argIsLong[0] ? args[0].longConst : int(args[0].floatConst);
        if (argIsFloat[0]) { if (args[0].floatConst != sysVar) { return result_arg_integerExpected; } }

        fcnResultIsLong = true;  //init

        switch (sysVar) {

        case 0: fcnResult.longConst = _dispWidth; break;
        case 1: fcnResult.longConst = _dispNumPrecision; break;
        case 2: fcnResult.longConst = _dispCharsToPrint; break;
        case 3: fcnResult.longConst = _dispFmtFlags; break;

        case 5: fcnResult.longConst = _printWidth; break;
        case 6: fcnResult.longConst = _printNumPrecision; break;
        case 7: fcnResult.longConst = _printCharsToPrint; break;
        case 8: fcnResult.longConst = _printFmtFlags; break;

        case 4:
        case 9:
        {
            fcnResultIsLong = false;   // is string
            fcnResult.pStringConst = new char[2];
            intermediateStringObjectCount++;
            strcpy(fcnResult.pStringConst, (sysVar == 4) ? _dispNumSpecifier : _printNumSpecifier);
#if printCreateDeleteHeapObjects
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif
        }
        break;

        case 10: fcnResult.longConst = _promptAndEcho; break;
        case 11: fcnResult.longConst = _printLastResult; break;
        case 12: fcnResult.longConst = _userCBprocStartSet_count; break;
        case 13: fcnResult.longConst = _userCBprocAliasSet_count; break;

        case 14:
        {
            fcnResultIsLong = false;   // is string
            fcnResult.pStringConst = new char[_maxIdentifierNameLen + 1];
            intermediateStringObjectCount++;
            strcpy(fcnResult.pStringConst, _programName);
#if printCreateDeleteHeapObjects
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif
        }
        break;

        case 15:
        case 16:
        case 17:
        case 18:
            fcnResultIsLong = false;   // is string
            fcnResult.pStringConst = new char[((sysVar == 15) ? strlen(ProductName) : (sysVar == 16) ? strlen(LegalCopyright) : (sysVar == 17) ? strlen(ProductVersion) : strlen(BuildDate)) + 1];
            intermediateStringObjectCount++;
            strcpy(fcnResult.pStringConst, (sysVar == 15) ? ProductName : (sysVar == 16) ? LegalCopyright : (sysVar == 17) ? ProductVersion : BuildDate);
#if printCreateDeleteHeapObjects
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif

            break;

        default:return result_arg_invalid; break;
        }       // switch (sysVar)
        break;
    }


    }       // end switch


    // postprocess: delete function name token and arguments from evaluation stack, create stack entry for function result 
    // -------------------------------------------------------------------------------------------------------------------

    clearEvalStackLevels(suppliedArgCount + 1);

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

    // push result to stack
    // --------------------

    _pEvalStackTop->varOrConst.value = fcnResult;                        // float or pointer to string
    _pEvalStackTop->varOrConst.valueType = fcnResultIsLong ? value_isLong : fcnResultIsFloat ? value_isFloat : value_isStringPointer;     // value type of second operand  
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;              // use generic constant type
    _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    _pEvalStackTop->varOrConst.variableAttributes = 0x00;                  // not an array, not an array element (it's a constant) 

    return result_execOK;
}


// -----------------------
// check format specifiers
// -----------------------

Interpreter::execResult_type Interpreter::checkFmtSpecifiers(bool isDispFmt, bool valueIsString, int suppliedArgCount, char* valueType, Val* operands, char& numSpecifier,
    int& width, int& precision, int& flags) {

    // format a value: third argument is either a specifier (string) or set of flags (number)
    bool hasSpecifierArg = false; // init
    if (suppliedArgCount >= (isDispFmt ? 3 : 4)) { hasSpecifierArg = ((valueType[isDispFmt ? 2 : 3] != value_isLong) && (valueType[isDispFmt ? 2 : 3] != value_isFloat)); }

    for (int argNo = (isDispFmt ? 1 : 2); argNo <= suppliedArgCount; argNo++) {

        // Specifier argument ? Single character specifier (FfEeGgXxDd) expected
        if (hasSpecifierArg && (argNo == (isDispFmt ? 3 : 4))) {     // position of specifier in arg list varies
            if (valueType[argNo - 1] != value_isStringPointer) { return result_arg_stringExpected; }
            if (operands[argNo - 1].pStringConst == nullptr) { return result_arg_invalid; }
            if (strlen(operands[argNo - 1].pStringConst) != 1) { return result_arg_invalid; }
            numSpecifier = operands[argNo - 1].pStringConst[0];
            char* pChar(strchr("FfGgEeXxDd", numSpecifier));
            if (pChar == nullptr) { Serial.println("*** error"); ; return result_arg_invalid; }
        }

        // Width, precision flags ? Numeric arguments expected
        else if (argNo != (hasSpecifierArg ? 6 : 5)) {      // (exclude optional argument returning #chars printed from tests)
            if ((valueType[argNo - 1] != value_isLong) && (valueType[argNo - 1] != value_isFloat)) { return result_arg_numValueExpected; }                                               // numeric ?
            if ((valueType[argNo - 1] == value_isLong) ? operands[argNo - 1].longConst < 0 : operands[argNo - 1].floatConst < 0.) { return result_arg_outsideRange; }                                           // positive ?
            int argValue = (valueType[argNo - 1] == value_isLong) ? operands[argNo - 1].longConst : (long)operands[argNo - 1].floatConst;
            ((argNo == (isDispFmt ? 1 : 2)) ? width : (argNo == (isDispFmt ? 2 : 3)) ? precision : flags) = argValue;                             // set with, precision, flags to respective argument
            if (argValue != ((argNo == (isDispFmt ? 1 : 2)) ? width : (argNo == (isDispFmt ? 2 : 3)) ? precision : flags)) { return result_arg_invalid; }    // integer ?
        }
    }
    // format STRING: precision argument NOT specified: init precision to width. Note that for strings, precision specifies MAXIMUM no of characters that will be printed

    if (valueIsString && (suppliedArgCount == 2)) { precision = width; }        // fstr() with explicit change of width and without explicit change of precision: init precision to width

    width = min(width, _maxPrintFieldWidth);            // limit width to _maxPrintFieldWidth
    precision = min(precision, valueIsString ? _maxCharsToPrint : _maxNumPrecision);
    flags &= 0b11111;       // apply mask
    return result_execOK;
}


// ----------------------
// create a format string
// ----------------------


void  Interpreter::makeFormatString(int flags, bool isIntFmt, char* numFmt, char* fmtString) {

    // prepare format string
    // ---------------------

    fmtString[0] = '%';
    int strPos = 1;
    for (int i = 1; i <= 5; i++, flags >>= 1) {
        if (flags & 0b1) { fmtString[strPos] = ((i == 1) ? '-' : (i == 2) ? '+' : (i == 3) ? ' ' : (i == 4) ? '#' : '0'); ++strPos; }
    }
    fmtString[strPos] = '*'; ++strPos; fmtString[strPos] = '.'; ++strPos; fmtString[strPos] = '*'; ++strPos;             // width and precision specified with additional arguments
    if (isIntFmt) { fmtString[strPos] = 'l'; ++strPos; fmtString[strPos] = numFmt[0]; ++strPos; }
    else { fmtString[strPos] = numFmt[0]; ++strPos; }
    fmtString[strPos] = '%'; ++strPos; fmtString[strPos] = 'n'; ++strPos; fmtString[strPos] = '\0'; ++strPos;            // %n specifier (return characters printed)

    return;
}


// -----------------------------------------------------------------------
// format number or string according to format string (result is a string)
// -----------------------------------------------------------------------

void  Interpreter::printToString(int width, int precision, bool inputIsString, bool isIntFmt, char* valueType, Val* operands, char* fmtString,
    Val& fcnResult, int& charsPrinted) {

    int opStrLen{ 0 }, resultStrLen{ 0 };
    if (inputIsString) {
        if (operands[0].pStringConst != nullptr) {
            opStrLen = strlen(operands[0].pStringConst);
            if (opStrLen > _maxPrintFieldWidth) { operands[0].pStringConst[_maxPrintFieldWidth] = '\0'; opStrLen = _maxPrintFieldWidth; }   // clip input string without warning (won't need it any more)
        }
        resultStrLen = max(width + 10, opStrLen + 10);  // allow for a few extra formatting characters, if any
    }
    else {
        resultStrLen = max(width + 10, 30);         // 30: ensure length is sufficient to print a formatted nummber
    }

    fcnResult.pStringConst = new char[resultStrLen];
    intermediateStringObjectCount++;

#if printCreateDeleteHeapObjects
    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif

    if (inputIsString) { sprintf(fcnResult.pStringConst, fmtString, width, precision, (operands[0].pStringConst == nullptr) ? "" : operands[0].pStringConst, &charsPrinted); }
    else if (isIntFmt) { sprintf(fcnResult.pStringConst, fmtString, width, precision, (valueType[0] == value_isLong) ? operands[0].longConst : (long)operands[0].floatConst, &charsPrinted); }     // hex output for floating point numbers not provided (Arduino)
    else { sprintf(fcnResult.pStringConst, fmtString, width, precision, (valueType[0] == value_isLong) ? (float)operands[0].longConst : operands[0].floatConst, &charsPrinted); }

    return;
}


// -------------------------------
// delete a variable string object
// -------------------------------

// if not a string, then do nothing. If not a variable, then exit WITH error

Interpreter::execResult_type Interpreter::deleteVarStringObject(LE_evalStack* pStackLvl) {

    if (pStackLvl->varOrConst.tokenType != tok_isVariable) { return result_arg_varExpected; };                            // not a variable
    if ((*pStackLvl->varOrConst.varTypeAddress & value_typeMask) != value_isStringPointer) { return result_execOK; }      // not a string object
    if (*pStackLvl->varOrConst.value.ppStringConst == nullptr) { return result_execOK; }

    char varScope = (pStackLvl->varOrConst.variableAttributes & var_scopeMask);

    // delete variable string object
#if printCreateDeleteHeapObjects
    Serial.print((varScope == var_isUser) ? "----- (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
    Serial.println((uint32_t)*_pEvalStackMinus2->varOrConst.value.ppStringConst - RAMSTART);
#endif
    delete[] * pStackLvl->varOrConst.value.ppStringConst;
    (varScope == var_isUser) ? userVarStringObjectCount-- : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? globalStaticVarStringObjectCount-- : localVarStringObjectCount--;
    return result_execOK;
}


// ------------------------------------
// delete an intermediate string object
// ------------------------------------

// if not a string, then do nothing. If not an intermediate string object, then exit WITHOUT error

Interpreter::execResult_type Interpreter::deleteIntermStringObject(LE_evalStack* pStackLvl) {

    if ((pStackLvl->varOrConst.valueAttributes & constIsIntermediate) != constIsIntermediate) { return result_execOK; }       // not an intermediate constant
    if (pStackLvl->varOrConst.valueType != value_isStringPointer) { return result_execOK; }                                   // not a string object
    if (pStackLvl->varOrConst.value.pStringConst == nullptr) { return result_execOK; }
#if printCreateDeleteHeapObjects
    Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)_pEvalStackTop->varOrConst.value.pStringConst - RAMSTART);
#endif
    delete[] pStackLvl->varOrConst.value.pStringConst;
    intermediateStringObjectCount--;

    return result_execOK;
}

// ---------------------------------------------------------------------------
// copy command arguments or internal function arguments from evaluation stack
// ---------------------------------------------------------------------------

Interpreter::execResult_type Interpreter::copyValueArgsFromStack(LE_evalStack*& pStackLvl, int argCount, bool* argIsVar, bool* argIsArray, char* valueType, Val* args, bool prepareForCallback) {
    execResult_type execResult;

    for (int i = 0; i < argCount; i++) {
        argIsVar[i] = (pStackLvl->varOrConst.tokenType == tok_isVariable);
        argIsArray[i] = argIsVar[i] ? (pStackLvl->varOrConst.variableAttributes & var_isArray) : false;
        valueType[i] = argIsVar[i] ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;

        if (prepareForCallback && ((valueType[i] & value_typeMask) == value_noValue)) { continue; }

        // argument is long or float: if preparing for callback, return pointer to value. Otherwise, return value itself
        if ((valueType[i] & value_typeMask) == value_isLong) {
            if (prepareForCallback) { args[i].pLongConst = (argIsVar[i] ? (pStackLvl->varOrConst.value.pLongConst) : &pStackLvl->varOrConst.value.longConst); }
            else { args[i].longConst = (argIsVar[i] ? (*pStackLvl->varOrConst.value.pLongConst) : pStackLvl->varOrConst.value.longConst); }
        }
        else if ((valueType[i] & value_typeMask) == value_isFloat) {
            if (prepareForCallback) { args[i].pFloatConst = (argIsVar[i] ? (pStackLvl->varOrConst.value.pFloatConst) : &pStackLvl->varOrConst.value.floatConst); }
            else { args[i].floatConst = (argIsVar[i] ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst); }
        }

        // argument is string: always return a pointer to string, but if preparing for callback, this pointer MAY point to a newly created empty string or copy of a non-empty string (see below)
        else {
            args[i].pStringConst = (argIsVar[i] ? (*pStackLvl->varOrConst.value.ppStringConst) : pStackLvl->varOrConst.value.pStringConst); // init: fetch pointer to string  
            if (prepareForCallback) {       // for callback calls only      
                int strLength{ 0 };
                // empty variable and empty constant strings: create a real empty string (no null pointer); non-empty constant strings: create a string copy
                if ((args[i].pStringConst == nullptr) || !argIsVar[i]) {       // note: non-empty variable strings (only): pointer keeps pointing to variable string (no copy)           
                    valueType[i] |= passCopyToCallback;           // string copy, or new empty string, passed
                    strLength = (args[i].pStringConst == nullptr) ? 0 : strlen(args[i].pStringConst);
                    args[i].pStringConst = new char[strLength + 1];                                         // change pointer to copy of string
                    intermediateStringObjectCount++;
                    if (strLength == 0) { args[i].pStringConst[0] = '\0'; }                                 // empty strings ("" -> no null pointer)
                    else { strcpy(args[i].pStringConst, pStackLvl->varOrConst.value.pStringConst); }        // non-empty constant string
#if printCreateDeleteHeapObjects
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)args[i].pStringConst - RAMSTART);
#endif
                }
            }
        }

        pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
    }

    return result_execOK;
}

// --------------------------------
// *   launch external function   *
// --------------------------------

Interpreter::execResult_type  Interpreter::launchExternalFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount) {

    // note current token (function token) position, in case an error happens IN THE CALLER immediately upon return from function to be called
    // ---------------------------------------------------------------------------------------------------------------------------------------

    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;  // CALLER function token position, before pushing caler function data to stack   


    // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
    // ----------------------------------------------------------------------------------------------

    _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
    _pFlowCtrlStackTop = (FunctionData*)flowCtrlStack.appendListElement(sizeof(FunctionData));
    *((FunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                // push caller function data to stack


    // function to be called: create storage and init local variables with supplied arguments (populate _activeFunctionData)
    // --------------------------------------------------------------------------------------------------------------------

    _activeFunctionData.functionIndex = pFunctionStackLvl->function.index;     // index of external function to call
    _activeFunctionData.blockType = MyParser::block_extFunction;
    _activeFunctionData.activeCmd_ResWordCode = MyParser::cmdcod_none;        // no command is being executed
    _activeFunctionData.activeCmd_tokenAddress = nullptr;

    // create local variable storage for external function to be called
    int localVarCount = extFunctionData[_activeFunctionData.functionIndex].localVarCountInFunction;
    int paramCount = extFunctionData[_activeFunctionData.functionIndex].paramOnlyCountInFunction;

    if (localVarCount > 0) {
        _activeFunctionData.pLocalVarValues = new Val[localVarCount];              // local variable value: real, pointer to string or array, or (if reference): pointer to 'source' (referenced) variable
        _activeFunctionData.ppSourceVarTypes = new char* [localVarCount];           // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        _activeFunctionData.pVariableAttributes = new char[localVarCount];         // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) 

        // save function caller's arguments to function's local storage and remove them from evaluation stack
        if (suppliedArgCount > 0) {
            LE_evalStack* pStackLvl = pFirstArgStackLvl;         // pointing to first argument on stack
            for (int i = 0; i < suppliedArgCount; i++) {
                int valueType = pStackLvl->varOrConst.valueType;
                bool operandIsLong = (valueType == value_isLong);
                bool operandIsFloat = (valueType == value_isFloat);
                bool operandIsVariable = (pStackLvl->varOrConst.tokenType == tok_isVariable);

                // variable (could be an array) passed ?
                if (operandIsVariable) {                                      // argument is a variable => local value is a reference to 'source' variable
                    _activeFunctionData.pLocalVarValues[i].pBaseValue = pStackLvl->varOrConst.value.pBaseValue;  // pointer to 'source' variable
                    _activeFunctionData.ppSourceVarTypes[i] = pStackLvl->varOrConst.varTypeAddress;            // pointer to 'source' variable value type
                    _activeFunctionData.pVariableAttributes[i] = value_isVarRef |                              // local variable value type (reference) ...
                        (pStackLvl->varOrConst.variableAttributes & var_scopeMask);                             // ... and SOURCE variable scope (user, global, static; local, param)
                }
                else {      // parsed, or intermediate, constant passed as value
                    if (operandIsLong || operandIsFloat) {                                                      // operand is float constant
                        _activeFunctionData.pLocalVarValues[i] = pStackLvl->varOrConst.value;   // store a local copy
                        _activeFunctionData.pVariableAttributes[i] = operandIsLong ? value_isLong : value_isFloat;
                    }
                    else {                      // operand is string constant: create a local copy
                        _activeFunctionData.pLocalVarValues[i].pStringConst = nullptr;             // init (if empty string)
                        _activeFunctionData.pVariableAttributes[i] = value_isStringPointer;
                        if (pStackLvl->varOrConst.value.pStringConst != nullptr) {
                            int stringlen = strlen(pStackLvl->varOrConst.value.pStringConst);
                            _activeFunctionData.pLocalVarValues[i].pStringConst = new char[stringlen + 1];
                            localVarStringObjectCount++;
                            strcpy(_activeFunctionData.pLocalVarValues[i].pStringConst, pStackLvl->varOrConst.value.pStringConst);
#if printCreateDeleteHeapObjects
                            Serial.print("+++++ (loc var str) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues[i].pStringConst - RAMSTART);
#endif
                        }
                    };
                }

                deleteIntermStringObject(pStackLvl);                                              // if intermediate constant string, then delete char string object (tested within called routine)
                pStackLvl = (LE_evalStack*)evalStack.deleteListElement(pStackLvl);        // argument saved: remove argument from stack and point to next argument
            }
        }
    }

    // also delete function name token from evaluation stack
    _pEvalStackTop = (LE_evalStack*)evalStack.getPrevListElement(pFunctionStackLvl);
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);
    (LE_evalStack*)evalStack.deleteListElement(pFunctionStackLvl);

    _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                          // store evaluation stack levels in use by callers (call stack)



    // init local variables for non_supplied arguments (scalar parameters with default values) and local (non-parameter) variables
    // ---------------------------------------------------------------------------------------------------------------------------

    char* calledFunctionTokenStep = extFunctionData[_activeFunctionData.functionIndex].pExtFunctionStartToken;
    initFunctionDefaultParamVariables(calledFunctionTokenStep, suppliedArgCount, paramCount);      // return with first token after function definition...
    initFunctionLocalNonParamVariables(calledFunctionTokenStep, paramCount, localVarCount);         // ...and create storage for local array variables


    // set next step to start of called function
    // -----------------------------------------

    _activeFunctionData.pNextStep = calledFunctionTokenStep;                     // first step in first statement in called function
    _activeFunctionData.errorStatementStartStep = calledFunctionTokenStep;
    _activeFunctionData.errorProgramCounter = calledFunctionTokenStep;

    return  result_execOK;
}


// -----------------------------------------------------------------------------------------------
// *   init local variables for non_supplied arguments (scalar parameters with default values)   *
// -----------------------------------------------------------------------------------------------

void Interpreter::initFunctionDefaultParamVariables(char*& pStep, int suppliedArgCount, int paramCount) {
    int tokenType = *pStep & 0x0F;                                                          // function name token of called function

    if (suppliedArgCount < paramCount) {      // missing arguments: use parameter default values to init local variables
        int count = 0, terminalCode = 0;
        tokenType = jumpTokens(1, pStep);
        // now positioned at opening parenthesis in called function (after FUNCTION token)
        // find n-th argument separator (comma), with n is number of supplied arguments (stay at left parenthesis if none provided)
        while (count < suppliedArgCount) { tokenType = findTokenStep(tok_isTerminalGroup1, MyParser::termcod_comma, pStep); count++; }

        // now positioned before first parameter for non-supplied scalar argument. It always has an initializer
        // we only need the constant value, because we know the variable value index already (count): skip variable and assignment 
        while (count < paramCount) {
            tokenType = jumpTokens(((count == suppliedArgCount) ? 3 : 4), pStep);

            // now positioned at constant initializer
            char valueType = ((*(char*)pStep) >> 4) & value_typeMask;
            bool operandIsLong = (valueType == value_isLong);
            bool operandIsFloat = (valueType == value_isFloat);

            _activeFunctionData.pVariableAttributes[count] = valueType;                // long, float or string

            if (operandIsLong) {                                                      // operand is float constant
                memcpy(&_activeFunctionData.pLocalVarValues[count].longConst, ((TokenIsConstant*)pStep)->cstValue.longConst, sizeof(long));
            }
            else if (operandIsFloat) {                                                      // operand is float constant
                memcpy(&_activeFunctionData.pLocalVarValues[count].floatConst, ((TokenIsConstant*)pStep)->cstValue.floatConst, sizeof(float));
            }
            else {                      // operand is parsed string constant: create a local copy and store in variable
                char* s{ nullptr };
                memcpy(&s, ((TokenIsConstant*)pStep)->cstValue.pStringConst, sizeof(char*));  // copy the pointer, NOT the string  

                _activeFunctionData.pLocalVarValues[count].pStringConst = nullptr;   // init (if empty string)
                if (s != nullptr) {
                    int stringlen = strlen(s);
                    _activeFunctionData.pLocalVarValues[count].pStringConst = new char[stringlen + 1];
                    localVarStringObjectCount++;
                    strcpy(_activeFunctionData.pLocalVarValues[count].pStringConst, s);
#if printCreateDeleteHeapObjects
                    Serial.print("+++++ (loc var str) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues[count].pStringConst - RAMSTART);
#endif
                }
            }
            count++;
        }
    }

    // skip (remainder of) function definition
    findTokenStep(tok_isTerminalGroup1, MyParser::termcod_semicolon, pStep);
};



// --------------------------------------------
// *   init local variables (non-parameter)   *
// --------------------------------------------

void Interpreter::initFunctionLocalNonParamVariables(char* pStep, int paramCount, int localVarCount) {
    // upon entry, positioned at first token after FUNCTION statement

    int tokenType{}, terminalCode{};

    int count = paramCount;         // sum of mandatory and optional parameters

    while (count != localVarCount) {
        findTokenStep(tok_isReservedWord, MyParser::cmdcod_local, pStep);     // find 'LOCAL' keyword (always there)

        do {
            // in case variable is not an array and it does not have an initializer: init now as zero (float). Arrays without initializer will be initialized later
            _activeFunctionData.pLocalVarValues[count].floatConst = 0;
            _activeFunctionData.pVariableAttributes[count] = value_isFloat;        // for now, assume scalar

            tokenType = jumpTokens(2, pStep, terminalCode);            // either left parenthesis, assignment, comma or semicolon separator (always a terminal)

            // handle array definition dimensions 
            // ----------------------------------

            int dimCount = 0, arrayElements = 1;
            int arrayDims[MAX_ARRAY_DIMS]{ 0 };

            if (terminalCode == MyParser::termcod_leftPar) {        // array opening parenthesis
                do {
                    tokenType = jumpTokens(1, pStep);         // dimension

                    // increase dimension count and calculate elements (checks done during parsing)
                    char valueType = ((*(char*)pStep) >> 4) & value_typeMask;
                    bool isLong = (valueType == value_isLong);        // or float (checked during parsing)
                    Val dimSubscript{};
                    if (isLong) { memcpy(&dimSubscript, ((TokenIsConstant*)pStep)->cstValue.longConst, sizeof(long)); }
                    else { memcpy(&dimSubscript, ((TokenIsConstant*)pStep)->cstValue.floatConst, sizeof(float)); dimSubscript.longConst = (long)dimSubscript.floatConst; }
                    arrayElements *= dimSubscript.longConst;
                    arrayDims[dimCount] = dimSubscript.longConst;
                    dimCount++;

                    tokenType = jumpTokens(1, pStep, terminalCode);         // comma (dimension separator) or right parenthesis
                } while (terminalCode != MyParser::termcod_rightPar);

                // create array (init later)
                float* pArray = new float[arrayElements + 1];
                localArrayObjectCount++;
#if printCreateDeleteHeapObjects
                Serial.print("+++++ (loc ar stor) "); Serial.println((uint32_t)pArray - RAMSTART);
#endif
                _activeFunctionData.pLocalVarValues[count].pArray = pArray;
                _activeFunctionData.pVariableAttributes[count] |= var_isArray;             // set array bit

                // store dimensions in element 0: char 0 to 2 is dimensions; char 3 = dimension count 
                for (int i = 0; i < MAX_ARRAY_DIMS; i++) {
                    ((char*)pArray)[i] = arrayDims[i];
                }
                ((char*)pArray)[3] = dimCount;        // (note: for param arrays, set to max dimension count during parsing)

                tokenType = jumpTokens(1, pStep, terminalCode);       // assignment, comma or semicolon
            }


            // handle initialisation (if initializer provided)
            // -----------------------------------------------

            if (terminalCode == MyParser::termcod_assign) {
                tokenType = jumpTokens(1, pStep);       // constant

                // fetch constant
                tokenType = *pStep & 0x0F;

                Val initializer{ };        // last token is a number constant: dimension spec
                char* pString{ nullptr };

                char valueType = ((*(char*)pStep) >> 4) & value_typeMask;
                bool isLong = (valueType == value_isLong);
                bool isFloat = (valueType == value_isFloat);

                if (isLong) { memcpy(&initializer, ((TokenIsConstant*)pStep)->cstValue.longConst, sizeof(long)); }
                if (isFloat) { memcpy(&initializer, ((TokenIsConstant*)pStep)->cstValue.floatConst, sizeof(float)); }
                else { memcpy(&pString, ((TokenIsConstant*)pStep)->cstValue.pStringConst, sizeof(pString)); }     // copy pointer to string (not the string itself)
                int length = (isLong || isFloat) ? 0 : (pString == nullptr) ? 0 : strlen(pString);       // only relevant for strings
                _activeFunctionData.pVariableAttributes[count] =
                    (_activeFunctionData.pVariableAttributes[count] & ~value_typeMask) | valueType;

                // array: initialize (note: test for non-empty string - which are not allowed as initializer - done during parsing)
                if ((_activeFunctionData.pVariableAttributes[count] & var_isArray) == var_isArray) {
                    void* pArray = ((void**)_activeFunctionData.pLocalVarValues)[count];        // void pointer to an array 
                    // fill up with numeric constants or (empty strings:) null pointers
                    if (isLong) { for (int elem = 1; elem <= arrayElements; elem++) { ((long*)pArray)[elem] = initializer.longConst; } }
                    else if (isFloat) { for (int elem = 1; elem <= arrayElements; elem++) { ((float*)pArray)[elem] = initializer.floatConst; } }
                    else { for (int elem = 1; elem <= arrayElements; elem++) { ((char**)pArray)[elem] = nullptr; } }
                }
                // scalar: initialize
                else {
                    if (isLong) { _activeFunctionData.pLocalVarValues[count].longConst = initializer.longConst; }      // store numeric constant
                    else if (isFloat) { _activeFunctionData.pLocalVarValues[count].floatConst = initializer.floatConst; }      // store numeric constant
                    else {
                        if (length == 0) { _activeFunctionData.pLocalVarValues[count].pStringConst = nullptr; }       // an empty string does not create a heap object
                        else { // create string object and store string
                            char* pVarString = new char[length + 1];          // create char array on the heap to store alphanumeric constant, including terminating '\0'
                            // store alphanumeric constant in newly created character array
                            strcpy(pVarString, pString);              // including terminating \0
                            _activeFunctionData.pLocalVarValues[count].pStringConst = pVarString;       // store pointer to string
                            localVarStringObjectCount++;
#if printCreateDeleteHeapObjects
                            Serial.print("+++++ (loc var str) "); Serial.println((uint32_t)pVarString - RAMSTART);
#endif
                        }
                    }
                }

                tokenType = jumpTokens(1, pStep, terminalCode);       // comma or semicolon
            }

            else {  // no initializer: if array, initialize it now (scalar has been initialized already)
                if ((_activeFunctionData.pVariableAttributes[count] & var_isArray) == var_isArray) {
                    void* pArray = ((void**)_activeFunctionData.pLocalVarValues)[count];        // void pointer to an array 
                    for (int elem = 1; elem <= arrayElements; elem++) { ((float*)pArray)[elem] = 0.; } // float (by default)
                }
            }
            count++;

        } while (terminalCode == MyParser::termcod_comma);

    }
};


// -----------------------------------
// *   terminate external function   *
// -----------------------------------

Interpreter::execResult_type Interpreter::terminateExternalFunction(bool addZeroReturnValue) {

    if (addZeroReturnValue) {
        _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;
        _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;          // use generic constant type
        _pEvalStackTop->varOrConst.value.longConst = 0;                // default return value (long)
        _pEvalStackTop->varOrConst.valueType = value_isLong;
        _pEvalStackTop->varOrConst.variableAttributes = 0x00;
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    }
    else { makeIntermediateConstant(_pEvalStackTop); }             // if not already an intermediate constant

    // delete local variable arrays and strings (only if local variable is not a reference)

    int localVarCount = extFunctionData[_activeFunctionData.functionIndex].localVarCountInFunction;      // of function to be terminated

    if (localVarCount > 0) {
        _pmyParser->deleteArrayElementStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true);
        _pmyParser->deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, false, false, true);

        // release local variable storage for function that has been called
        delete[] _activeFunctionData.pLocalVarValues;
        delete[] _activeFunctionData.pVariableAttributes;
        delete[] _activeFunctionData.ppSourceVarTypes;
    }

    char blockType = MyParser::block_none;

    do {
        blockType = *(char*)_pFlowCtrlStackTop;            // always at least one open function (because returning to caller from it)

        // load local storage pointers again for caller function and restore pending step & active function information for caller function
        if (blockType == MyParser::block_extFunction) { _activeFunctionData = *(FunctionData*)_pFlowCtrlStackTop; }

        // delete FLOW CONTROL stack level that contained caller function storage pointers and return address (all just retrieved to _activeFunctionData)
        flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackTop = _pFlowCtrlStackMinus1;
        _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);

    } while (blockType != MyParser::block_extFunction);


    if (_activeFunctionData.pNextStep >= _programStart) {   // not within a function        
        if (localVarStringObjectCount != 0) {
            Serial.print("*** Local variable string objects cleanup error. Remaining: "); Serial.println(localVarStringObjectCount);
            localVarStringObjectCount = 0;
        }

        if (localArrayObjectCount != 0) {
            Serial.print("*** Local array objects cleanup error. Remaining: "); Serial.println(localArrayObjectCount);
            localArrayObjectCount = 0;
        }
    }

    execResult_type execResult = execAllProcessedOperators();     // continue in caller !!!


    return execResult;
}


// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Interpreter::fetchVarBaseAddress(TokenIsVariable* pVarToken, char*& sourceVarTypeAddress, char& localValueType, char& variableAttributes, char& valueAttributes) {

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

    int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : programVarValueIndex[varNameIndex];   // value index in allocated Justina data memory for this variable

    if (isUserVar) {
        localValueType = userVarType[valueIndex] & value_typeMask;                                     // value type (indicating float or string)
        sourceVarTypeAddress = userVarType + valueIndex;                                                // pointer to value type and the 'is array' flag          
        variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

        return &userVarValues[valueIndex];                                                             // pointer to value (float, char* or (array variables only) pointer to array start in memory)
    }
    else if (isGlobalVar) {
        localValueType = globalVarType[valueIndex] & value_typeMask;                                   // value type (indicating float or string)
        sourceVarTypeAddress = globalVarType + valueIndex;                                              // pointer to value type and the 'is array' flag
        variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

        return &globalVarValues[valueIndex];                                                           // pointer to value (float, char* or (array variables only) pointer to array start in memory)
    }
    else if (isStaticVar) {
        localValueType = staticVarType[valueIndex] & value_typeMask;                                   // value type (indicating float or string)
        sourceVarTypeAddress = staticVarType + valueIndex;                                              // pointer to value type and the 'is array' flag
        variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

        return &staticVarValues[valueIndex];                                                           // pointer to value (float, char* or (array variables only) pointer to array start in memory)
    }

    // local variables (including parameters)    
    else {
        // note (function parameter variables only): when a function is called with a variable argument (always passed by reference), 
        // the parameter value type has been set to 'reference' when the function was called
        localValueType = _activeFunctionData.pVariableAttributes[valueIndex] & value_typeMask;         // local variable value type (indicating float or string or REFERENCE)

        if (localValueType == value_isVarRef) {                                                       // local value is a reference to 'source' variable                                                         
            sourceVarTypeAddress = _activeFunctionData.ppSourceVarTypes[valueIndex];                   // pointer to 'source' variable value type
            // local variable value type (reference); SOURCE variable scope (user, global, static; local, param), 'is array' flag
            variableAttributes = _activeFunctionData.pVariableAttributes[valueIndex] | (pVarToken->identInfo & var_isArray);

            return   ((Val**)_activeFunctionData.pLocalVarValues)[valueIndex];                       // pointer to 'source' variable value 
        }

        // local variable OR parameter variable that received the result of an expression (or constant) as argument (passed by value) OR optional parameter variable that received no value (default initialization) 
        else {
            sourceVarTypeAddress = _activeFunctionData.pVariableAttributes + valueIndex;               // pointer to local variable value type and 'is array' flag
            // local variable value type (reference); local variable scope (user, global, static; local, param), 'is array' flag
            variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);

            return (Val*)&_activeFunctionData.pLocalVarValues[valueIndex];                           // pointer to local variable value 
        }
    }
}


// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------

void* Interpreter::arrayElemAddress(void* varBaseAddress, int* elemSpec) {

    // varBaseAddress argument must be base address of an array variable (containing itself a pointer to the array)
    // elemSpec array must specify an array element (max. 3 dimensions)
    // return pointer will point to a float or a string pointer (both can be array elements) - nullptr if outside boundaries

    void* pArray = varBaseAddress;                                                      // will point to float or string pointer (both can be array elements)
    int arrayDimCount = ((char*)pArray)[3];

    int arrayElement{ 0 };
    for (int i = 0; i < arrayDimCount; i++) {
        int arrayDim = ((char*)pArray)[i];
        if ((elemSpec[i] < 1) || (elemSpec[i] > arrayDim)) { return nullptr; }      // is outside array boundaries

        int arrayNextDim = (i < arrayDimCount - 1) ? ((char*)pArray)[i + 1] : 1;
        arrayElement = (arrayElement + (elemSpec[i] - 1)) * arrayNextDim;
    }
    arrayElement++;                                                                     // add one (first array element contains dimensions and dimension count)
    return (Val*)pArray + arrayElement;                                              // pointer to a 4-byte array element (long, float or pointer to string)
}


// -----------------------------------------------
// *   push terminal token to evaluation stack   *
// -----------------------------------------------

void Interpreter::PushTerminalToken(int& tokenType) {                                 // terminal token is assumed

    // push internal or external function index to stack

    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(TerminalTokenLvl));
    _pEvalStackTop->terminal.tokenType = tokenType;
    _pEvalStackTop->terminal.tokenAddress = _programCounter;                                                    // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;                                            // terminal token only: calculate from partial index stored in high 4 bits of token type 
    _pEvalStackTop->terminal.index += ((tokenType == Interpreter::tok_isTerminalGroup2) ? 0x10 : (tokenType == Interpreter::tok_isTerminalGroup3) ? 0x20 : 0);
};


// ------------------------------------------------------------------------
// *   push internal or external function name token to evaluation stack   *
// ------------------------------------------------------------------------

void Interpreter::pushFunctionName(int& tokenType) {                                  // function name is assumed (internal or external)

    // push internal or external function index to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(FunctionLvl));
    _pEvalStackTop->function.tokenType = tokenType;
    _pEvalStackTop->function.tokenAddress = _programCounter;                                    // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->function.index = ((TokenIsIntFunction*)_programCounter)->tokenIndex;
};


// -------------------------------------------------------------
// *   push real or string constant token to evaluation stack   *
// -------------------------------------------------------------

void Interpreter::pushConstant(int& tokenType) {                                              // float or string constant token is assumed

    // push real or string parsed constant, value type and array flag (false) to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;          // use generic constant type
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;                                  // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->varOrConst.valueType = ((*(char*)_programCounter) >> 4) & value_typeMask;     // for constants, upper 4 bits contain the value type
    _pEvalStackTop->varOrConst.variableAttributes = 0x00;
    _pEvalStackTop->varOrConst.valueAttributes = 0x00;

    Val constant{};
    if ((_pEvalStackTop->varOrConst.valueType & value_typeMask) == value_isLong) {
        memcpy(&_pEvalStackTop->varOrConst.value.longConst, ((TokenIsConstant*)_programCounter)->cstValue.longConst, sizeof(long));          // float  not necessarily aligned with word size: copy memory instead
    }
    else if ((_pEvalStackTop->varOrConst.valueType & value_typeMask) == value_isFloat) {
        memcpy(&_pEvalStackTop->varOrConst.value.longConst, ((TokenIsConstant*)_programCounter)->cstValue.floatConst, sizeof(float));          // float  not necessarily aligned with word size: copy memory instead
    }
    else {
        memcpy(&_pEvalStackTop->varOrConst.value.pStringConst, ((TokenIsConstant*)_programCounter)->cstValue.pStringConst, sizeof(void*)); // char pointer not necessarily aligned with word size: copy pointer instead
    }
};


// ---------------------------------------------------
// *   push generic name token to evaluation stack   *
// ---------------------------------------------------

void Interpreter::pushGenericName(int& tokenType) {                                              // float or string constant token is assumed

    // push real or string parsed constant, value type and array flag (false) to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    // just push the string pointer to the generic name (no indexes, ...)
    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(GenNameLvl));
    _pEvalStackTop->varOrConst.tokenType = tok_isGenericName;          // use generic constant type
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;                                  // only for finding source error position during unparsing (for printing)

    char* pAnum{ nullptr };
    memcpy(&pAnum, ((TokenIsConstant*)_programCounter)->cstValue.pStringConst, sizeof(pAnum)); // char pointer not necessarily aligned with word size: copy pointer instead
    _pEvalStackTop->genericName.pStringConst = pAnum;                                  // store char* in stack 
};


// ----------------------------------------------
// *   push variable token to evaluation stack   *
// ----------------------------------------------

void Interpreter::pushVariable(int& tokenType) {                                              // variable name token is assumed

    // push variable base address, variable value type (real, string) and array flag to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackTop->varOrConst.tokenType = tokenType;
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;

    // note: _pEvalStackTop->varOrConst.valueType is a value ONLY containing the value type of the variable pushed on the stack (float, string, reference)
    //       _pEvalStackTop->varOrConst.varTypeAddress is a pointer to the SOURCE variable's variable info (either a referenced variable or the variable itself), with ...
    //       the source variable info containing the value type of the variable AND the 'is array' flag 

    void* varAddress = fetchVarBaseAddress((TokenIsVariable*)_programCounter, _pEvalStackTop->varOrConst.varTypeAddress, _pEvalStackTop->varOrConst.valueType,
        _pEvalStackTop->varOrConst.variableAttributes, _pEvalStackTop->varOrConst.valueAttributes);
    _pEvalStackTop->varOrConst.value.pBaseValue = varAddress;                                    // base address of variable
}
