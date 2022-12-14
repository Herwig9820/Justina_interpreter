/***************************************************************************************
    Justina interpreter library for Arduino Nano 33 IoT and Arduino RP2040.

    Version:    v1.00 - xx/xx/2022
    Author:     Herwig Taveirne

    Justina is an interpreter which does NOT require you to use an IDE to write
    and compile programs. Programs are written on the PC using any text processor
    and transferred to the Arduino using any serial terminal capable of sending files.
    Justina can store and retrieve programs and other data on an SD card as well.

    See GitHub for more information and documentation: //// <links>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************************/


#include "Justina.h"

#define printCreateDeleteListHeapObjects 0
#define printProcessedTokens 0
#define debugPrint 0


// *****************************************************************
// ***        class Justina_interpreter - implementation         ***
// *****************************************************************

// ---------------------------------
// *   execute parsed statements   *
// ---------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::exec(char* startHere) {
#if printProcessedTokens
    Serial.print("\r\n*** enter exec: eval stack depth: "); Serial.println(evalStack.getElementCount());
#endif
    // init

    _appFlags &= ~appFlag_errorConditionBit;              // clear error condition flag
    _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_executing;     // set bits b54 to 10: executing

    int tokenType = *startHere & 0x0F;
    int tokenIndex{ 0 };
    bool isFunctionReturn = false;
    bool precedingIsComma = false;                                      // used to detect prefix operators following a comma separator
    bool isEndOfStatementSeparator = false;                     // false, because this is already the start of a new instruction
    bool lastWasEndOfStatementSeparator = false;                     // false, because this is already the start of a new instruction

    bool doStopForDebugNow = false;
    bool userRequestsStop = false, userRequestsAbort = false;
    bool isBackslashStop{ false };
    bool doSingleStep = false;
    bool lastTokenIsSemicolon = false;                   // do not stop a program after an 'empty' statement (occurs when returning to caller)
    bool doSkip = false;

    execResult_type execResult = result_execOK;
    char* holdProgramCnt_StatementStart{ nullptr }, * programCnt_previousStatementStart{ nullptr };
    char* holdErrorProgramCnt_StatementStart{ nullptr }, * errorProgramCnt_previousStatement{ nullptr };

    _stepCmdExecuted = db_continue;      // switch single step mode OFF before starting to execute command line (even in debug mode). Step and Debug commands will switch it on again (to execute one step).
    _debugCmdExecuted = false;      // function to debug must be on same comand line as Debug command

    _programCounter = startHere;
    holdProgramCnt_StatementStart = _programCounter; programCnt_previousStatementStart = _programCounter;

    holdErrorProgramCnt_StatementStart = _programCounter, errorProgramCnt_previousStatement = _programCounter;
    bool execError{ false };    // init

    _activeFunctionData.functionIndex = 0;                  // main program level: not relevant
    _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // no command is being executed
    _activeFunctionData.activeCmd_tokenAddress = nullptr;
    _activeFunctionData.errorStatementStartStep = _programCounter;
    _activeFunctionData.errorProgramCounter = _programCounter;
    _activeFunctionData.blockType = block_extFunction;  // consider main as an 'external' function      

    _lastValueIsStored = false;


    // -----------------
    // 1. process tokens
    // -----------------

    while (tokenType != tok_no_token) {                                                                    // for all tokens in token list
        // if terminal token, determine which terminal type
    #if printProcessedTokens
        Serial.print(">>>> exec: token step = "); Serial.println(_programCounter - _programStorage);
    #endif

        bool isTerminal = ((tokenType == tok_isTerminalGroup1) || (tokenType == tok_isTerminalGroup2) || (tokenType == tok_isTerminalGroup3));
        if (isTerminal) {
            tokenIndex = ((((TokenIsTerminal*)_programCounter)->tokenTypeAndIndex >> 4) & 0x0F);
            tokenIndex += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
        }

        bool isOperator = (isTerminal ? (_terminals[tokenIndex].terminalCode <= termcod_opRangeEnd) : false);
        bool isSemicolon = (isTerminal ? (_terminals[tokenIndex].terminalCode == termcod_semicolon) : false);
        bool isComma = (isTerminal ? (_terminals[tokenIndex].terminalCode == termcod_comma) : false);
        bool isLeftPar = (isTerminal ? (_terminals[tokenIndex].terminalCode == termcod_leftPar) : false);
        bool isRightPar = (isTerminal ? (_terminals[tokenIndex].terminalCode == termcod_rightPar) : false);

        // fetch next token (for some token types, the size is stored in the upper 4 bits of the token type byte)
        int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) : (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*_programCounter >> 4) & 0x0F;
        _activeFunctionData.pNextStep = _programCounter + tokenLength;                                  // look ahead

        lastWasEndOfStatementSeparator = isEndOfStatementSeparator;
        isEndOfStatementSeparator = false;


        // 1.1 process by token type
        // -------------------------

        switch (tokenType) {                // process according to token type

            // Case: process keyword token
            // ---------------------------

            case tok_isReservedWord:
            {

                // compile time statements (program, function, var, local, static, ...): skip for execution

                tokenIndex = ((TokenIsResWord*)_programCounter)->tokenIndex;

            #if debugPrint
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - keyword: "); Serial.print(_resWords[tokenIndex]._resWordName);
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif
            #if printProcessedTokens
                Serial.print("=== process keyword: address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                Serial.print(_resWords[tokenIndex]._resWordName); Serial.println("]");
            #endif
                bool skipStatement = ((_resWords[tokenIndex].restrictions & cmd_skipDuringExec) != 0);
                if (skipStatement) {
                    findTokenStep(tok_isTerminalGroup1, termcod_semicolon, _programCounter);  // find semicolon (always match)
                    _activeFunctionData.pNextStep = _programCounter;
                    break;
                }

                // commands are executed when processing final semicolon statement (note: activeCmd_ResWordCode identifies individual commands; not command blocks)
                _activeFunctionData.activeCmd_ResWordCode = _resWords[tokenIndex].resWordCode;       // store command for now
                _activeFunctionData.activeCmd_tokenAddress = _programCounter;

                break;
            }


            // Case: process external function token
            // -------------------------------------

            case tok_isInternFunction:
            {

                pushFunctionName(tokenType);

            #if printProcessedTokens
                Serial.print("=== process "); Serial.print(tokenType == tok_isInternFunction ? "int fcn" : "ext fcn"); Serial.print(": address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                int funcNameIndex = ((TokenIsIntFunction*)_programCounter)->tokenIndex;
                Serial.print(_functions[funcNameIndex].funcName); Serial.println("]");
            #endif
                break;
            }


            // Case: process external function token
            // -------------------------------------

            case tok_isExternFunction:
            {

                pushFunctionName(tokenType);

            #if printProcessedTokens
                Serial.print("=== process "); Serial.print(tokenType == tok_isInternFunction ? "int fcn" : "ext fcn"); Serial.print(": address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                int funcNameIndex = ((TokenIsExtFunction*)_programCounter)->identNameIndex;
                Serial.print(extFunctionNames[funcNameIndex]); Serial.println("]");
            #endif

            #if debugPrint
                int index = ((TokenIsExtFunction*)_programCounter)->identNameIndex;
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - function name: "); Serial.print(extFunctionNames[index]);
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif

                break;
            }


            // Case: generic identifier token token
            // -------------------------------------------------

            case tok_isGenericName:
            {
                pushGenericName(tokenType);

            #if printProcessedTokens
                Serial.print("=== process identif: address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                Serial.print(_pEvalStackTop->varOrConst.value.pStringConst); Serial.print("]");
            #endif
            }
            break;


            // Case: parsed or intermediate constant value (long, float or string)
            // -------------------------------------------------------------------

            case tok_isConstant:
            {

                _activeFunctionData.errorProgramCounter = _programCounter;      // in case an error occurs while processing token

                pushConstant(tokenType);

            #if debugPrint
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - constant");
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif

            #if printProcessedTokens
                Serial.print("=== process const  : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                char valueType = _pEvalStackTop->varOrConst.valueType;
                if (valueType == value_isLong) { Serial.print(_pEvalStackTop->varOrConst.value.longConst); Serial.println("]"); }
                else if (valueType == value_isFloat) { Serial.print(_pEvalStackTop->varOrConst.value.floatConst); Serial.println("]"); }
                else { Serial.print("'"); Serial.print(_pEvalStackTop->varOrConst.value.pStringConst); Serial.println("']"); }
            #endif

                // check if (an) operation(s) can be executed. 
                // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
                execResult = execAllProcessedOperators();
                if (execResult != result_execOK) { break; }
                break;
            }


            // Case: process real or string constant token, variable token
            // -----------------------------------------------------------

            case tok_isVariable:
            {
                _activeFunctionData.errorProgramCounter = _programCounter;      // in case an error occurs while processing token

                pushVariable(tokenType);

            #if debugPrint
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - variable");
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif

            #if printProcessedTokens
                Serial.print("=== process var name: address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                int varNameIndex = ((TokenIsVariable*)_programCounter)->identNameIndex;
                char scopeMask = ((TokenIsVariable*)_programCounter)->identInfo & var_scopeMask;
                char* varName = (scopeMask == var_isUser) ? userVarNames[varNameIndex] : programVarNames[varNameIndex];
                Serial.print(varName); Serial.println("]");
            #endif

                // next token
                int nextTokenType = *_activeFunctionData.pNextStep & 0x0F;
                int nextTokenIndex{ 0 };
                bool nextIsTerminal = ((nextTokenType == tok_isTerminalGroup1) || (nextTokenType == tok_isTerminalGroup2) || (nextTokenType == tok_isTerminalGroup3));
                if (nextIsTerminal) {
                    nextTokenIndex = ((((TokenIsTerminal*)_activeFunctionData.pNextStep)->tokenTypeAndIndex >> 4) & 0x0F);
                    nextTokenIndex += ((nextTokenType == tok_isTerminalGroup2) ? 0x10 : (nextTokenType == tok_isTerminalGroup3) ? 0x20 : 0);
                }

                bool nextIsLeftPar = (nextIsTerminal ? (_terminals[nextTokenIndex].terminalCode == termcod_leftPar) : false);
                if (nextIsLeftPar) {                                                           // array variable name (this token) is followed by subscripts (to be processed)
                    _pEvalStackTop->varOrConst.valueAttributes |= var_isArray_pendingSubscripts;    // flag that array element still needs to be processed
                }

                // check if (an) operation(s) can be executed. 
                // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)

                execResult = execAllProcessedOperators();
                if (execResult != result_execOK) { break; }

            #if debugPrint
                Serial.println("=== processed var name");
            #endif
                break;
            }


            // Case: process terminal token 
            // ----------------------------

            case tok_isTerminalGroup1:
            case tok_isTerminalGroup2:
            case tok_isTerminalGroup3:


                // operator or left parenthesis ?
                // ------------------------------

                if (isOperator || isLeftPar) {

                    bool doCaseBreak{ false };

                    // terminal tokens: only operators and left parentheses are pushed on the stack
                    pushTerminalToken(tokenType);

                #if debugPrint
                    Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - terminal: "); Serial.print(_terminals[_pEvalStackTop->terminal.index & 0x7F].terminalName);
                    Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
                #endif
                    if (precedingIsComma) { _pEvalStackTop->terminal.index |= 0x80;   doCaseBreak = true; }      // flag that preceding token is comma separator 

                    if (!doCaseBreak) {
                        if (evalStack.getElementCount() < _activeFunctionData.callerEvalStackLevels + 2) { doCaseBreak = true; }         // no preceding token exist on the stack      
                    }
                    if (!doCaseBreak) {
                        if (!(_pEvalStackMinus1->genericToken.tokenType == tok_isConstant) && !(_pEvalStackMinus1->genericToken.tokenType == tok_isVariable)) { doCaseBreak = true;; };
                    }
                    if (!doCaseBreak) {
                        // previous token is constant or variable: check if current token is an infix or a postfix operator (it cannot be a prefix operator)
                        // if postfix operation, execute it first (it always has highest priority)
                        bool isPostfixOperator = (_terminals[_pEvalStackTop->terminal.index & 0x7F].postfix_priority != 0);
                        if (isPostfixOperator) {
                            execUnaryOperation(false);        // flag postfix operation
                            execResult = execAllProcessedOperators();
                            if (execResult != result_execOK) { doCaseBreak = true;; }
                        }
                    }

                #if printProcessedTokens        // after evaluation stack has been updated and before breaking 
                    Serial.print("=== processed termin : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif

                    if (doCaseBreak) { break; }
                }


                // comma separator ?
                // -----------------

                else if (isComma) {

                    // no action needed

                #if printProcessedTokens
                    Serial.print("=== process termin : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif
                }


                // right parenthesis ?
                // -------------------

                else if (isRightPar) {

                #if debugPrint
                    Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - right par");
                    Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
                #endif

                    bool doCaseBreak{ false };
                    int argCount = 0;                                                // init number of supplied arguments (or array subscripts) to 0
                    LE_evalStack* pStackLvl = _pEvalStackTop;     // stack level of last argument / array subscript before right parenthesis, or left parenthesis (if function call and no arguments supplied)

                    // set pointer to stack level for left parenthesis and pointer to stack level for preceding token (if any)
                    while (true) {
                        bool isTerminalLvl = ((pStackLvl->genericToken.tokenType == tok_isTerminalGroup1) || (pStackLvl->genericToken.tokenType == tok_isTerminalGroup2) || (pStackLvl->genericToken.tokenType == tok_isTerminalGroup3));
                        bool isLeftParLvl = isTerminalLvl ? (_terminals[pStackLvl->terminal.index & 0x7F].terminalCode == termcod_leftPar) : false;
                        if (isLeftParLvl) { break; }   // break if left parenthesis found 
                        pStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);
                        argCount++;
                    }

                    LE_evalStack* pPrecedingStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);     // stack level PRECEDING left parenthesis (or null pointer)

                    // remove left parenthesis stack level
                    pStackLvl = (LE_evalStack*)evalStack.deleteListElement(pStackLvl);                            // pStackLvl now pointing to first function argument or array subscript (or nullptr if none)
                #if debugPrint
                    Serial.println("REMOVE left parenthesis from stack");
                    Serial.print("                 "); Serial.print(evalStack.getElementCount());  Serial.println(" - first argument");
                #endif

                    // correct pointers (now wrong, if from 0 to 2 arguments)
                    _pEvalStackTop = (LE_evalStack*)evalStack.getLastListElement();        // this line needed if no arguments
                    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
                    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

                    // execute internal or external function, calculate array element address or remove parenthesis around single argument (if no function or array)
                    execResult = execParenthesesPair(pPrecedingStackLvl, pStackLvl, argCount);
                #if debugPrint
                    Serial.print("    right par.: exec result "); Serial.println(execResult);
                #endif
                    if (execResult != result_execOK) { doCaseBreak = true; }

                    // the left parenthesis and the argument(s) are now removed and replaced by a single scalar (function result, array element, single argument)
                    // check if additional operators preceding the left parenthesis can now be executed.
                    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
                    if (!doCaseBreak) {
                        execResult = execAllProcessedOperators(); if (execResult != result_execOK) { doCaseBreak = true; }
                    }



                #if printProcessedTokens        // after evaluation stack has been updated and before breaking because of error
                    Serial.print("=== processed termin : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif
                    if (doCaseBreak) { break; }
                }


                // statement separator ?
                // ---------------------

                else if (isSemicolon) {
                #if debugPrint
                    Serial.print("=== process semicolon : eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - semicolon");
                    Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
                #endif
                    bool doCaseBreak{ false };

                    lastTokenIsSemicolon = true;
                    isEndOfStatementSeparator = true;

                    if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_none) {       // currently not executing a command, but a simple expression
                        if (evalStack.getElementCount() > (_activeFunctionData.callerEvalStackLevels + 1)) {
                            //// _pConsole ???
                            // if tracing, message would not be correct. Eval stack levels will be deleted right after printing a traced value (or trace execution error)
                            if (!_parsingExecutingTraceString) { Serial.print("*** Evaluation stack error. Remaining stack levels for current program level: "); Serial.println(evalStack.getElementCount() - (_activeFunctionData.callerEvalStackLevels + 1)); }
                        }

                        // did the last expression produce a result ?  
                        else if (evalStack.getElementCount() == _activeFunctionData.callerEvalStackLevels + 1) {

                            if (_activeFunctionData.blockType == block_eval) {  // executing parsed eval() string
                                // never store a last value; delete all expression results except the last one
                                int tokenType, tokenCode;
                                char* pStep = _programCounter;
                                do {
                                    tokenType = jumpTokens(1, pStep, tokenCode);        // check next parsed token
                                    bool isTerminal = ((tokenType == tok_isTerminalGroup1) || (tokenType == tok_isTerminalGroup2) || (tokenType == tok_isTerminalGroup3));
                                    bool nextExpressionFound = isTerminal ? (tokenCode != termcod_semicolon) : (tokenType != tok_isEvalEnd);
                                    if (nextExpressionFound) { break; }      // not last expression
                                } while ((*pStep & 0x0F) != tok_isEvalEnd);             // always match
                                if (tokenType != tok_isEvalEnd) { clearEvalStackLevels(1); }     // a next expression is found: delete the current expression's result
                            }

                            else if (_parsingExecutingTraceString) {
                                // keep result for now (do nothing)
                            }

                            else {  // not an eval() block, not tracing

                                // in main program level ? store as last value (for now, we don't know if it will be followed by other 'last' values)
                                if (_programCounter >= (_programStorage + PROG_MEM_SIZE)) {
                                    saveLastValue(_lastValueIsStored);
                                }                // save last result in FIFO and delete stack level
                                else {
                                    clearEvalStackLevels(1);
                                } // NOT main program level: we don't need to keep the statement result

                            }
                        }
                    }
                    // command with optional expression(s) processed ? Execute command
                    else {
                        execResult = execProcessedCommand(isFunctionReturn, userRequestsStop, userRequestsAbort);      // userRequestsStop: '\s' sent while a command (e.g. Input) was waiting for user input
                        if (execResult != result_execOK) { doCaseBreak = true; }                     // other error: break (case label) immediately
                    }

                #if debugPrint
                    Serial.print("                 "); Serial.print(evalStack.getElementCount());  Serial.println(" - semicolon processed");
                #endif

                #if printProcessedTokens        // after evaluation stack has been updated and before breaking 
                    Serial.print("=== processed semicolon : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif

                    if (doCaseBreak) { break; }
                }

                break;  // (case label)


                // ------------------------------
                // parsed eval() statements end ?
                // ------------------------------

            case tok_isEvalEnd:
            {
                execResult = terminateEval();
                if (execResult != result_execOK) { break; }                     // other error: break (case label) immediately

            #if printProcessedTokens        // after evaluation stack has been updated and before breaking because of error
                Serial.print("=== processed 'eval end' token : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                Serial.print("(eval end token)");   Serial.println(" ]");
            #endif
            }
            break;

        }   // end 'switch (tokenType)'


        // 1.2. a token has been processed (with or without error): advance to next token
        // ------------------------------------------------------------------------------

        _programCounter = _activeFunctionData.pNextStep;         // note: will be altered when calling an external function and upon return of a called function
        tokenType = *_activeFunctionData.pNextStep & 0x0F;                                                               // next token type (could be token within caller, if returning now)
        precedingIsComma = isComma;                             // remember if this was a comma

    #if debugPrint
        Serial.print("** token has been processed: next step = "); Serial.println(_activeFunctionData.pNextStep - _programStorage);
    #endif


        // 1.3 last token processed was a statement separator ? 
        // ----------------------------------------------------

        // this code executes after a statement was executed (simple expression or command)

    #if debugPrint
        Serial.print("*** token processed: eval stack depth: "); Serial.print(evalStack.getElementCount()); Serial.print(", list element address: "); Serial.println((uint32_t)_pEvalStackTop - sizeof(LinkedList::ListElemHead) - RAMSTART); Serial.println();
    #endif
        if (isEndOfStatementSeparator) {        // after expression AND after command

        #if printProcessedTokens        
            Serial.println("\r\n");
            Serial.println("**** is 'end of statement separator'\r\n");
        #endif

            programCnt_previousStatementStart = holdProgramCnt_StatementStart;
            holdProgramCnt_StatementStart = _programCounter;

            if (execResult == result_execOK) {          // no error ? 
                if (!isFunctionReturn) {            // adapt error program step pointers
                    // note: if returning from user function, error statement pointers retrieved from flow control stack 
                    _activeFunctionData.errorStatementStartStep = _programCounter;
                    _activeFunctionData.errorProgramCounter = _programCounter;
                }
            }


            // empty console character buffer and check for '\a' (abort) and '\s' (stop) character sequence (in case program keeps running, e.g. in an endless loop)
            // -----------------------------------------------------------------------------------------------------------------------------------------------------

            // note: while executing trace string or eval() string, only purpose is character buffer flush

            bool charsFound = false, backslashFound = false, doAbort = false, doStop = false;

            char c{ 0xFF };                                                           // init: no character available
            // read a character, if available in buffer
            do {
                if (getKey(c)) { execResult = result_eval_kill; return execResult; }      // return value true: kill Justina interpreter (buffer is now flushed until next line character)
                if (c != 0xFF) {                                                                           // terminal character available for reading ?
                    // Check for Justina ESCAPE sequence (sent by terminal as individual characters) and cancel input, or use default value, if indicated
                    // Note: if Justina ESCAPE sequence is not recognized, then backslash character is simply discarded
                    if (c == '\\') { backslashFound = !backslashFound; }                                                                                     // backslash character found
                    else if ((tolower(c) == 'a') || (tolower(c) == 's')) {                                                                    // part of a Justina ESCAPE sequence ? Abort evaluation phase 
                        if (backslashFound) { backslashFound = false;   ((tolower(c) == 'a') ? doAbort : doStop) = true; }
                    }
                }
            } while (c != 0xFF);      // until buffer flushed

            if (doStop) { isBackslashStop = true; }                                           // for error message only

            userRequestsAbort = userRequestsAbort || doAbort;
            userRequestsStop = userRequestsStop || doStop || _debugCmdExecuted;               // 'backslash S' received from command line, either here ('doStop') or while a command is waiting for user input (e.g. Input)


            // process debugging commands (entered from the command line, or '\a' and '\s' escape sequences entered while a program is running  
            // --------------------------------------------------------------------------------------------------------------------------------

            // note: skip while executing trace expressions, parsed eval() expresions or quitting Justina

            bool executingEvalString = (_activeFunctionData.blockType == block_eval);
            if (!_parsingExecutingTraceString && !executingEvalString && (execResult != result_eval_kill) && (execResult != result_eval_quit)) {
                bool nextIsSameLvlEnd{ false };
                if ((_stepCmdExecuted == db_stepToBlockEnd) && (flowCtrlStack.getElementCount() == _stepFlowCtrlStackLevels)
                    && ((*_activeFunctionData.pNextStep & 0x0F) == tok_isReservedWord)) {
                    int index = ((TokenIsResWord*)(_activeFunctionData.pNextStep))->tokenIndex;
                    nextIsSameLvlEnd = (_resWords[index].resWordCode == cmdcod_end);
                }

                // a program may only stop after a program statement (not an immediate mode statement, not an eval() parsed expression) was executed, and it's not the last program statement
                bool executedStepIsprogram = programCnt_previousStatementStart < (_programStorage + PROG_MEM_SIZE); // always a program function step
                bool nextStepIsprogram = (_programCounter < (_programStorage + PROG_MEM_SIZE));

                // stop for debug now ? (either '\s' sequence, or one of the 'step...' commands issed
                doStopForDebugNow = (userRequestsStop || (_stepCmdExecuted == db_singleStep) ||            // userRequestsStop: '\s' while code is executing
                    ((_stepCmdExecuted == db_stepOut) && (_callStackDepth < _stepCallStackLevel)) ||
                    ((_stepCmdExecuted == db_stepOver) && (_callStackDepth <= _stepCallStackLevel)) ||
                    ((_stepCmdExecuted == db_stepOutOfBlock) && (flowCtrlStack.getElementCount() < _stepFlowCtrlStackLevels)) ||
                    ((_stepCmdExecuted == db_stepToBlockEnd) && ((flowCtrlStack.getElementCount() < _stepFlowCtrlStackLevels) || nextIsSameLvlEnd)))

                    && executedStepIsprogram && nextStepIsprogram && !isFunctionReturn;


                // skipping a statement and stopping again for debug ?
                doSkip = (_stepCmdExecuted == db_skip) && nextStepIsprogram && !isFunctionReturn;
                if (doSkip) {
                    // check if next step is start of a command (reserved word) and that it is the end of a block command
                    int tokenType = (_activeFunctionData.pNextStep[0] & 0x0F);             // always first character (any token)
                    if (tokenType == tok_isReservedWord) {                       // ok
                        int tokenindex = ((TokenIsResWord*)_activeFunctionData.pNextStep)->tokenIndex;
                        // if the command to be skipped is an 'end' block command, remove the open block from the flow control stack
                        if (_resWords[tokenindex].resWordCode == cmdcod_end) {       // it's an open block end (already tested before)
                            flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);        // delete open block stack level
                            _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
                        }
                    }
                    // skip a statement in program memory: adapt program step pointers
                    findTokenStep(tok_isTerminalGroup1, termcod_semicolon, _programCounter);  // find next semicolon (always match)
                    _activeFunctionData.pNextStep = _programCounter += sizeof(TokenIsTerminal);
                    tokenType = *_activeFunctionData.pNextStep & 0x0F;                                                               // next token type (could be token within caller, if returning now)
                    precedingIsComma = false;
                    _activeFunctionData.errorStatementStartStep = _activeFunctionData.pNextStep;
                    _activeFunctionData.errorProgramCounter = _activeFunctionData.pNextStep;
                }

                if (doStopForDebugNow) { userRequestsStop = false; _debugCmdExecuted = false; }           // reset request to stop program

                if (userRequestsAbort) { execResult = result_eval_abort; }
                else if (doStopForDebugNow || doSkip) { execResult = result_eval_stopForDebug; }

                isFunctionReturn = false;
            }
        }


        // 1.4 did an execution error occur within token ? signal error
        // ------------------------------------------------------------

        // do not print error message if currently executing trace expressions

        if (!_parsingExecutingTraceString && (execResult != result_execOK)) {          // execution error (printed as expression result if within trace -> not here)
            if (!_atLineStart) { _pConsole->println(); _atLineStart = true; }
            execError = true;

            bool isEvent = (execResult >= result_eval_startOfEvents);       // not an error but an event ?
            char execInfo[150] = "";

            // plain error ? 
            if (!isEvent) {
                int sourceErrorPos{ 0 };
                int functionNameLength{ 0 };
                long programCounterOffset{ 0 };

                // if error during executing (nested) eval() function(s): find first flow control stack level that called (nested) eval() function(s)

                char* errorStatementStartStep = _activeFunctionData.errorStatementStartStep;
                char* errorProgramCounter = _activeFunctionData.errorProgramCounter;
                int functionIndex = _activeFunctionData.functionIndex;          // init

                if (_activeFunctionData.blockType == block_eval) {
                    // if error while executing an eval() function: find first flow control stack level immediately below (optionally nested) eval() levels 
                    // this will always be a flow control stack level for the external function block type that 'called' the eval() function, because an eval() function can not have any open blocks 

                    void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;       // one level below _activeFunctionData
                    char* pImmediateCmdStackLvl = _pImmediateCmdStackTop;

                    while (((OpenFunctionData*)pFlowCtrlStackLvl)->blockType == block_eval) {
                        pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                        pImmediateCmdStackLvl = immModeCommandStack.getPrevListElement(pImmediateCmdStackLvl);
                    }

                    // retrieve error statement pointers and function index (in case the 'function' block type is referring to immediate mode statements)
                    errorStatementStartStep = ((OpenFunctionData*)pFlowCtrlStackLvl)->errorStatementStartStep;
                    errorProgramCounter = ((OpenFunctionData*)pFlowCtrlStackLvl)->errorProgramCounter;
                    functionIndex = ((OpenFunctionData*)pFlowCtrlStackLvl)->functionIndex;

                    // imm. mode program memory currently contains the (potentially inner, if nested) parsed eval() string where the error occured
                    // any parsed outer eval() strings (if nested) have been pushed on the imm.mode parsed command stack  
                    // as well as the imm. mode parsed statements at the time of the call to the outer eval() function
                    // (even if the outer eval() was called from a program, because that program had been called before from imm. mode)

                    // if the error statement pointers refer to immediate mode code (not to a program), pretty print directly from the imm.mode parsed command stack: add an offset to the pointers 
                    bool isImmMode = (errorStatementStartStep >= (_programStorage + PROG_MEM_SIZE));
                    if (isImmMode) { programCounterOffset = pImmediateCmdStackLvl - (_programStorage + PROG_MEM_SIZE); }
                }

                _pConsole->print("\r\n  ");
                prettyPrintInstructions(1, errorStatementStartStep + programCounterOffset, errorProgramCounter + programCounterOffset, &sourceErrorPos);
                for (int i = 1; i <= sourceErrorPos; ++i) { _pConsole->print(" "); }

                sprintf(execInfo, "  ^\r\n  Exec error %d", execResult);     // in main program level 
                _pConsole->print(execInfo);

                // errorProgramCounter is never pointing to a token directly contained in a parsed() eval() string 
                if (errorProgramCounter >= (_programStorage + PROG_MEM_SIZE)) { sprintf(execInfo, ""); }
                else { sprintf(execInfo, " - user function %s", extFunctionNames[functionIndex]); }
                _pConsole->print(execInfo);

                if (execResult != result_eval_parsingError) { sprintf(execInfo, "\r\n"); }
                else { sprintf(execInfo, " (eval() parsing error %ld)\r\n", _evalParseErrorCode); }
                _pConsole->print(execInfo);
            }

            else if (execResult == result_eval_quit) {
                strcpy(execInfo, "\r\nExecuting 'quit' command, ");
                _pConsole->print(strcat(execInfo, _keepInMemory ? "data retained\r\n" : "memory released\r\n"));
            }
            else if (execResult == result_eval_kill) {}      // do nothing
            else if (execResult == result_eval_abort) { _pConsole->print("\r\n+++ Abort: code execution terminated +++\r\n"); }
            else if (execResult == result_eval_stopForDebug) { if (isBackslashStop) { _pConsole->print("\r\n+++ Program stopped +++\r\n"); } }

            _lastValueIsStored = false;              // prevent printing last result (if any)
            break;
        }
    }   // end 'while ( tokenType != tok_no_token )'                                                                                       


    // -----------
    // 2. finalize
    // -----------

    // 2.1 did the execution produce a result ? print it
    // -------------------------------------------------

    if (!_parsingExecutingTraceString) {
        if (!_atLineStart) { _pConsole->println(); _atLineStart = true; }
        if (_lastValueIsStored && _printLastResult) {

            // print last result
            bool isLong = (lastResultTypeFiFo[0] == value_isLong);
            bool isFloat = (lastResultTypeFiFo[0] == value_isFloat);
            int charsPrinted{  };        // needed but not used
            Val toPrint;
            char* fmtString = (isLong || isFloat) ? _dispNumberFmtString : _dispStringFmtString;

            printToString(_dispWidth, (isLong || isFloat) ? _dispNumPrecision : MAX_STRCHAR_TO_PRINT,
                (!isLong && !isFloat), _dispIsIntFmt, lastResultTypeFiFo, lastResultValueFiFo, fmtString, toPrint, charsPrinted);
            _pConsole->println(toPrint.pStringConst);


            if (toPrint.pStringConst != nullptr) {
            #if printCreateDeleteListHeapObjects
                Serial.print("----- (Intermd str) "); Serial.println((uint32_t)toPrint.pStringConst - RAMSTART);
            #endif
                delete[] toPrint.pStringConst;
                _intermediateStringObjectCount--;
            }
        }
    }

    // 2.2 adapt imm. mode parsed statement stack, flow control stack and evaluation stack
    // -----------------------------------------------------------------------------------

    if (execResult == result_eval_stopForDebug) {              // stopping for debug now ('STOP' command or single step)
        // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
        _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;

        _pFlowCtrlStackTop = (OpenFunctionData*)flowCtrlStack.appendListElement(sizeof(OpenFunctionData));
        *((OpenFunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                // push caller function data to stack
        ++_callStackDepth;      // user function level added to flow control stack

        _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                          // store evaluation stack levels in use by callers (call stack)

        // push current command line storage to command line stack, to make room for debug commands
        Serial.print("  >> PUSH  (stop for debug): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + PROG_MEM_SIZE));
        long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + PROG_MEM_SIZE) + 1;
        _pImmediateCmdStackTop = (char*)immModeCommandStack.appendListElement(sizeof(char*) + parsedUserCmdLen);
        *(char**)_pImmediateCmdStackTop = _lastUserCmdStep;
        _lastUserCmdStep = nullptr;
        memcpy(_pImmediateCmdStackTop + sizeof(char*), (_programStorage + PROG_MEM_SIZE), parsedUserCmdLen);

        ++_openDebugLevels;
    }

    // no programs in debug: always; otherwise: only if error is in fact quit or kill event 
    else if ((_openDebugLevels == 0) || (execResult == result_eval_quit) || (execResult == result_eval_kill)) {             // do not clear flow control stack while in debug mode
        int dummy{};
        _openDebugLevels = 0;       // (if not yet zero)
        clearImmediateCmdStack(immModeCommandStack.getElementCount());
        clearFlowCtrlStack(dummy);           // and remaining local storage + local variable string and array values
        clearEvalStack();
    }

    // tracing (this means at least one program is stopped; if no exec error then one evaluation stack level (with the result) needs to be deleted
    else if (_parsingExecutingTraceString) {

        int charsPrinted{  };        // needed but not used
        Val toPrint;
        if (execResult == result_execOK) {
            Val value;
            bool isVar = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);
            char valueType = isVar ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
            bool isLong = (valueType == value_isLong);
            bool isFloat = (valueType == value_isFloat);
            char* fmtString = (isLong || isFloat) ? _dispNumberFmtString : _dispStringFmtString;
            // printToString() expects long, float or char*: remove extra level of indirection (variables only)
            value.floatConst = isVar ? *_pEvalStackTop->varOrConst.value.pFloatConst : _pEvalStackTop->varOrConst.value.floatConst;  // works for long and string as well
            printToString(0, (isLong || isFloat) ? _dispNumPrecision : MAX_STRCHAR_TO_PRINT,
                (!isLong && !isFloat), _dispIsIntFmt, &valueType, &value, fmtString, toPrint, charsPrinted);
        }
        else {
            char valTyp = value_isStringPointer;
            char  errStr[12];       // includes place for terminating '\0'
            sprintf(errStr, "<ErrE%d>", (int)execResult);
            Val temp;
            temp.pStringConst = errStr;
            printToString(0, MAX_STRCHAR_TO_PRINT, true, false, &valTyp, &temp, _dispStringFmtString, toPrint, charsPrinted);
        }

        if (toPrint.pStringConst == nullptr) { _pConsole->println(); }
        else {
            _pConsole->print(toPrint.pStringConst);
        #if printCreateDeleteListHeapObjects
            Serial.print("----- (Intermd str) "); Serial.println((uint32_t)toPrint.pStringConst - RAMSTART);
        #endif
            delete[] toPrint.pStringConst;
            _intermediateStringObjectCount--;
        }


        // note: flow control stack and immediate command stack (program code) are not affected: only need to clear evaluation stack
        clearEvalStackLevels(evalStack.getElementCount() - (int)_activeFunctionData.callerEvalStackLevels);
    }

    // program or command line exec error while at least one other program is currently stopped in debug mode ?
    else if (execResult != result_execOK) {
        int deleteImmModeCmdStackLevels{ 0 };
        clearFlowCtrlStack(deleteImmModeCmdStackLevels, execResult, true);           // returns imm. mode command stack levels to delete
        clearImmediateCmdStack(deleteImmModeCmdStackLevels);                        // do not delete all stack levels but only supplied level count
        clearEvalStackLevels(evalStack.getElementCount() - (int)_activeFunctionData.callerEvalStackLevels);
    }

    // adapt application flags
    ((execResult == result_execOK) || (execResult >= result_eval_startOfEvents)) ? _appFlags &= ~appFlag_errorConditionBit : _appFlags |= appFlag_errorConditionBit;              // clear or set error condition flag 
    _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_idle;     // clear bits b54: evaluation ended

#if debugPrint
    Serial.print("*** exit exec: eval stack depth: "); Serial.println(evalStack.getElementCount());
    Serial.print("** EXEC: return error code: "); Serial.println(execResult);
    Serial.println("**** returning to main");
#endif
    _activeFunctionData.pNextStep = _programStorage + PROG_MEM_SIZE;                // only to signal 'immediate mode command level'

    return execResult;   // return result, in case it's needed by caller
};


// --------------------------------------------------
// *   read character from keyboard, if available   *
// --------------------------------------------------

bool Justina_interpreter::getKey(char& c) {

    bool quitNow{ false };

    // do a housekeeping callback at regular intervals (if callback function defined)
    if (_housekeepingCallback != nullptr) {
        _currenttime = millis();
        _previousTime = _currenttime;
        // note: also handles millis() overflow after about 47 days
        if ((_lastCallBackTime + callbackPeriod < _currenttime) || (_currenttime < _previousTime)) {            // while executing, limit calls to housekeeping callback routine 
            _lastCallBackTime = _currenttime;
            _housekeepingCallback(quitNow, _appFlags);                                                           // execute housekeeping callback
            if (quitNow) { while (_pConsole->available() > 0) { _pConsole->read(); } return true; }             // flush buffer and flag 'quit (request from Justina caller)'
        }
    }

    // read a character, if available in buffer
    c = 0xFF;                                                                                                 // init: no character read
    if (_pConsole->available() > 0) { c = _pConsole->read(); }                                          // if terminal character available for reading
    return false;                                                                                           // do not quit
}

// -----------------------------------------------------
// *   read text from keyboard and store in variable   *
// -----------------------------------------------------

// read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
// return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
// return value 'true' indicates kill request from Justina caller

bool Justina_interpreter::readText(bool& doAbort, bool& doStop, bool& doCancel, bool& doDefault, char* input, int& length) {
    bool backslashFound{ false }, quitNow{ false };

    length = 0;  // init
    do {                                                                                                            // until new line character encountered
        // read a character, if available in buffer
        char c{ 0xFF };                                                           // init: no character available
        if (getKey(c)) { return true; }      // return value true: kill Justina interpreter (buffer is now flushed until next line character)

        if (c != 0xFF) {                                                                           // terminal character available for reading ?
            if (c == '\n') { break; }                                                                               // read until new line character
            else if (c < ' ') { continue; }                                                                         // skip control-chars except new line (ESC is skipped here as well - flag already set)

            // Check for Justina ESCAPE sequence (sent by terminal as individual characters) and cancel input, or use default value, if indicated
            // Note: if Justina ESCAPE sequence is not recognized, then backslash character is simply discarded
            if (c == '\\') {                                                                                        // backslash character found
                backslashFound = !backslashFound;
                if (backslashFound) { continue; }                                                                   // first backslash in a sequence: note and do nothing
            }

            else if (tolower(c) == 'a') {                                                                    // part of a Justina ESCAPE sequence ? Abort evaluation phase 
                if (backslashFound) { backslashFound = false;  doAbort = true;  continue; }
            }
            else if (tolower(c) == 's') {                                                                    // part of a Justina ESCAPE sequence ? Stop and enter debug mode 
                if (backslashFound) { backslashFound = false;  doStop = true;  continue; }
            }
            else if (tolower(c) == 'c') {                                                                    // part of a Justina ESCAPE sequence ? Cancel if allowed 
                if (backslashFound) { backslashFound = false;  doCancel = true;  continue; }
            }
            else if (tolower(c) == 'd') {                                                                    // part of a Justina ESCAPE sequence ? Use default value if provided
                if (backslashFound) { backslashFound = false; doDefault = true;  continue; }
            }

            if (length >= MAX_USER_INPUT_LEN) { continue; }                                                           // max. input length exceeded: drop character
            input[length] = c; input[++length] = '\0';
        }
    } while (true);

    return false;
}

// ----------------------------------------------------------------------
// *   execute a processed command  (statement starting with a keyword) *
// ----------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::execProcessedCommand(bool& isFunctionReturn, bool& userRequestsStop, bool& userRequestsAbort) {

    // this function is called when the END of the command (semicolon) is encountered during execution, and all arguments are on the stack already

    isFunctionReturn = false;  // init
    execResult_type execResult = result_execOK;
    int cmdParamCount = evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels;

    // note supplied argument count and go to first argument (if any)
    LE_evalStack* pStackLvl = _pEvalStackTop;
    for (int i = 1; i < cmdParamCount; i++) {                                                                               // skipped if no arguments, or if one argument
        pStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);                                                 // iterate to first argument
    }

    _activeFunctionData.errorProgramCounter = _activeFunctionData.activeCmd_tokenAddress;

#if debugPrint
    Serial.print("                 process command code: "); Serial.println((int)_activeFunctionData.activeCmd_ResWordCode);
#endif

    switch (_activeFunctionData.activeCmd_ResWordCode) {                                                                    // command code 

        case cmdcod_stop:
        {
            // -------------------------------------------------
            // Stop code execution (program only, for debugging)
            // -------------------------------------------------

            // 'stop' behaves as if an error occured, in order to follow the same processing logic  

            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
            return result_eval_stopForDebug;
            break;
        }


        // ------------------------
        // Quit Justina interpreter
        // ------------------------

        case cmdcod_quit:
        {

            // optional argument 1 clear all
            // - value is 0: keep interpreter in memory on quitting, value is 1: clear all and exit Justina 
            // 'quit' behaves as if an error occured, in order to follow the same processing logic  

            if (cmdParamCount != 0) {                                                                                           // 'Quit' command only                                                                                      
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];

                copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);            // copy arguments from stack
                if (((uint8_t)(valueType[0]) != value_isLong) && ((uint8_t)(valueType[0]) != value_isFloat)) { return result_arg_numValueExpected; }
                if ((uint8_t)(valueType[0]) == value_isFloat) { args[1].longConst = (int)args[1].floatConst; }
                _keepInMemory = (args[0].longConst == 0);       // silent mode (even not possible to cancel)
                return result_eval_quit;
            }

            else {      // keep in memory when quitting, cancel: ask user
                _appFlags |= appFlag_waitingForUser;    // bit b6 set: waiting for user interaction
                do {
                    _pConsole->println("===== Quit Justina: keep in memory ? (please answer Y, N or \\c to cancel) =====");

                    // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                    // return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
                    bool doAbort{ false }, doStop{ false }, doCancel{ false }, doDefault{ false }, backslashFound{ false }, answerIsNo{ false }, quitNow{ false };
                    int length{ 0 };
                    char input[MAX_USER_INPUT_LEN + 1] = "";                                                                          // init: empty string
                    if (readText(doAbort, doStop, doCancel, doDefault, input, length)) { return result_eval_kill; }  // kill request from caller ? 
                    if (doAbort) { userRequestsAbort = true; break; }    // '\a': abort running code (program or immediate mode statements - highest priority)
                    else if (doStop) { userRequestsStop = true; }       // '\s': stop a running program AFTER next PROGRAM statement (will have no effect anyway, because quitting)
                    else if (doCancel) { break; }                       // '\c': cancel operation (lowest priority)

                    bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                    if (validAnswer) {
                        _keepInMemory = (tolower(input[0]) == 'y');
                        return result_eval_quit;                        // Justina Quit command executed 
                    }
                } while (true);
            }

            _appFlags &= ~appFlag_waitingForUser;    // bit b6 reset: NOT waiting for user interaction
            clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
            break;
        }


        // -------------------------------------
        // Retart or abort stopped program again
        // -------------------------------------

        // these commands behave as if an error occured, in order to follow the same processing logic  
        // the commands are issued from the command line and restart a program stopped for debug

        // step: executes one program step. If a 'parsing only' statement is encountered, it will simply skip it
        // step over: if the statement is a function call, executes the function without stopping until control returns to the caller. For other statements, behaves like 'step'
        // step out: continues execution without stopping, until control is passed to the caller
        // step out of block: if in an open block (while, for, ...), continues execution until control passes to a statement outside the open block. Otherwise, behaves like 'step'
        // step to block end: if in an open block (while, for, ...), continues execution until the next statement to execute is the 'block end' statement...
        // ... this allows you to execute a 'for' loop one loop at the time, for instance. If outside an open block, behaves like 'step' 
        // go: continues execution until control returns to the user
        // skip: skip a statement (see notes)
        // abort a program while it is stopped

        // notes: when the next statement to execute is a block start command (if, while, ...), control is still OUTSIDE the loop
        //        you can not skip a block start command (if, while, ...). However, you can skip all statements inside it, including the block 'end' statement 
        //        you can not skip a function 'end' command

        case cmdcod_step:
        case cmdcod_stepOver:
        case cmdcod_stepOut:
        case cmdcod_stepOutOfBlock:
        case cmdcod_stepToBlockEnd:
        case cmdcod_go:
        case cmdcod_skip:
        case cmdcod_abort:
        {

            bool OpenBlock{ true };
            char nextStepBlockAction{ block_na };          // init

            if (_openDebugLevels == 0) { return result_noProgramStopped; }

            // debugging command requiring an open block ? (-> step out of block, step to block end commands)
            // debugging command not applicable to block start and block end commands ? (-> skip command)
            if (((_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOutOfBlock) ||
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepToBlockEnd) ||
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_skip))) {

                // determine whether an open block exists within the active function:
                // to do that, locate flow control control stack level below the open function data (function level and one level below are always present)
                void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;
                char blockType{};
                do {
                    // skip all debug level blocks and open function block (always there). Then, check the next control flow stack level (also always there)
                    blockType = *(char*)pFlowCtrlStackLvl;

                    // skip command ? 
                    if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_skip) {
                        // If open function block found, check that skipping next step is allowed
                        if (blockType == block_extFunction) {  // open function block (not an open loop block)
                            // check if next step is start of a command (reserved word) and that it is the start or end of a block command
                            char* pNextStep = ((OpenFunctionData*)pFlowCtrlStackLvl)->pNextStep;
                            int tokenType = *pNextStep & 0x0F;             // always first character (any token)
                            if (tokenType != tok_isReservedWord) { break; }                        // ok
                            int tokenindex = ((TokenIsResWord*)pNextStep)->tokenIndex;
                            nextStepBlockAction = _resWords[tokenindex].cmdBlockDef.blockPosOrAction;
                        }
                    }

                    pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                } while ((blockType != block_extFunction) && (blockType != block_eval));

                // access the flow control stack level below the stack level for the active function, and check the blocktype: is it an ope block within the function ?
                // (if not, then it's the stack level for the caller already)
                blockType = *(char*)pFlowCtrlStackLvl;
                if ((blockType != block_for) && (blockType != block_while) && (blockType != block_if)) { OpenBlock = false; }       // is it an open block ?

                // skip command (only): is skip allowed ? If not, produce error (this will not abort the program)
                if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_skip) {
                    if (!OpenBlock && (nextStepBlockAction == block_endPos)) { return result_skipNotAllowedHere; }        // end function: skip not allowed
                    if (nextStepBlockAction == block_startPos) { return result_skipNotAllowedHere; }
                }
            }


            // overwrite the parsed command line (containing the 'step', 'go' or 'abort' command) with the command line stack top and pop the command line stack top
            // before removing, delete any parsed string constants for that command line

            _lastUserCmdStep = *(char**)_pImmediateCmdStackTop;                                     // pop program step of last user cmd token ('tok_no_token')
            long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + PROG_MEM_SIZE) + 1;
            deleteConstStringObjects(_programStorage + PROG_MEM_SIZE);
            memcpy((_programStorage + PROG_MEM_SIZE), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);        // size berekenen
            immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
            _pImmediateCmdStackTop = immModeCommandStack.getLastListElement();
            Serial.print("  >> POP (Go): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + PROG_MEM_SIZE));
            --_openDebugLevels;

            // abort: all done
            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_abort) { return result_eval_abort; }


            _stepCmdExecuted = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_step) ? db_singleStep :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOut) ? db_stepOut :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOver) ? db_stepOver :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOutOfBlock) ? (OpenBlock ? db_stepOutOfBlock : db_singleStep) :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepToBlockEnd) ? (OpenBlock ? db_stepToBlockEnd : db_singleStep) :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_skip) ? db_skip :
                db_continue;

            // currently, at least one program is stopped (we are in debug mode)
            // find the flow control stack entry for the stopped function and make it the active function again (remove the flow control stack level for the debugging command line)
            char blockType = block_none;            // init
            do {
                blockType = *(char*)_pFlowCtrlStackTop;            // always at least one open function (because returning to caller from it)

                // load local storage pointers again for interrupted function and restore pending step & active function information for interrupted function
                if (blockType == block_extFunction) {
                    _activeFunctionData = *(OpenFunctionData*)_pFlowCtrlStackTop;
                }

                // delete FLOW CONTROL stack level that contained caller function storage pointers and return address (all just retrieved to _activeFunctionData)
                flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
            } while (blockType != block_extFunction);
            --_callStackDepth;          // deepest open function removed from flow control stack (as well as optional debug command line open blocks) 

            // info needed to check when commands like step out, ... have finished executing, returning control to user
            _stepCallStackLevel = _callStackDepth;                          // call stack levels at time of first program step to execute after step,... command
            _stepFlowCtrlStackLevels = flowCtrlStack.getElementCount();     // all flow control stack levels at time of first program step to execute after step,... command (includes open blocks)

            _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
            _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);

            // do NOT reset _activeFunctionData.activeCmd_ResWordCode:  _activeFunctionData just received its values from the flow control stack 
            break;
        }


        // -------------------------------------------------------------
        // Define Trace expressions, define and execute Eval expressions
        // -------------------------------------------------------------

        case cmdcod_trace:
        {
            bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
            char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
            Val value;
            value.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst); // line is valid for all value types  

            bool opIsString = ((uint8_t)valueType == value_isStringPointer);
            if (!opIsString) { return result_arg_stringExpected; }

            char* pString = _pTraceString;      // current trace string (will be replaced now)
            if (pString != nullptr) {
            #if printCreateDeleteListHeapObjects
                Serial.print("----- (system var str) "); Serial.println((uint32_t)pString - RAMSTART);
            #endif
                delete[] pString;
                _pTraceString = nullptr;      // old trace or eval string
                _systemVarStringObjectCount--;
            }

            if (value.pStringConst != nullptr) {                           // new trace string
                _systemVarStringObjectCount++;
                pString = new char[strlen(value.pStringConst) + 2]; // room for additional semicolon (in case string is not ending with it) and terminating '\0'
                strcpy(pString, value.pStringConst);              // copy the actual string
                pString[strlen(value.pStringConst)] = term_semicolon[0];
                pString[strlen(value.pStringConst) + 1] = '\0';
                _pTraceString = pString;
            #if printCreateDeleteListHeapObjects
                Serial.print("+++++ (system var str) "); Serial.println((uint32_t)pString - RAMSTART);
            #endif

            }

            clearEvalStackLevels(cmdParamCount);                                                                                // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
            break;
            }


        // ---------------------------------------------------------------------------------------------------------
        // Switch on single step mode (use to debug a program without Stop command programmed, right from the start)
        // ---------------------------------------------------------------------------------------------------------

        case cmdcod_debug:
            _debugCmdExecuted = true;

            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
            break;


            // ----------------
            // Print call stack
            // ----------------

        case cmdcod_printCallSt:
        {
            if (_callStackDepth > 0) {      // including eval() stack levels but excluding open block (for, if, ...) stack levels
                int indent = 0;
                void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                    int blockType = block_none;
                for (int i = 0; i < flowCtrlStack.getElementCount(); ++i) {
                    char s[MAX_IDENT_NAME_LEN + 1] = "";
                    blockType = *(char*)pFlowCtrlStackLvl;
                    if (blockType == block_eval) {
                        for (int space = 0; space < indent - 4; ++space) { _pConsole->print(" "); }
                        if (indent > 0) { _pConsole->print("|__ "); }
                        _pConsole->println("eval() string");
                        indent += 4;
                    }
                    else if (blockType == block_extFunction) {
                        if (((OpenFunctionData*)pFlowCtrlStackLvl)->pNextStep < (_programStorage + PROG_MEM_SIZE)) {
                            for (int space = 0; space < indent - 4; ++space) { _pConsole->print(" "); }
                            if (indent > 0) { _pConsole->print("|__ "); }
                            int index = ((OpenFunctionData*)pFlowCtrlStackLvl)->functionIndex;              // print function name
                            sprintf(s, "%s()", extFunctionNames[index]);
                            _pConsole->println(s);
                            indent += 4;
                        }
                        else {
                            for (int space = 0; space < indent - 4; ++space) { _pConsole->print(" "); }
                            if (indent > 0) { _pConsole->print("|__ "); }
                            _pConsole->println((i < flowCtrlStack.getElementCount() - 1) ? "debugging command line" : "command line");       // command line
                            indent = 0;
                        }
                    }
                    pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                }
            }
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        case cmdcod_info:

            // --------------------------------------------------------------------
            // Print information or question, requiring user confirmation or answer
            // --------------------------------------------------------------------

            // mandatory argument 1: prompt (string expression)
            // optional argument 2: numeric variable
            // - on entry: value is 0 or argument not supplied: confirmation required by pressing ENTER (any preceding characters are skipped)
            //             value is 1: idem, but if '\c' encountered in input stream the operation is canceled by user 
            //             value is 2: only positive or negative answer allowed, by pressing 'y' or 'n' followed by ENTER   
            //             value is 3: idem, but if '\c' encountered in input stream the operation is canceled by user 
            // - on exit:  value is 0: operation was canceled by user, 1 if operation confirmed by user

            // no break here: continue with Input command code


        case cmdcod_input:
        {

            // -------------------------------
            // Requests user to input a string
            // -------------------------------

            // if '\c' is encountered in the input stream, the operation is canceled by the user

            // mandatory argument 1: prompt (character string expression)
            // mandatory argument 2: variable
            // - on entry: if the argument contains a default value (see further) OR it's an array element, then it must contain a string value
            // - on exit:  string value entered by the user
            // mandatory argument 3: numeric variable
            // - on entry: value is 0: '\d' sequences in the input stream are ignored
            //             value is 1: if '\d' is encountered in the input stream, argument 2 is not changed (default value provided on entry)
            // - on exit:  value is 0: operation was canceled by user, value is 1: a value was entered by the user

            // notes: if both '\c' and '\d' are encountered in the input stream, '\c' (cancel operation) takes precedence over '\d' (use default)
            //        if a '\' character is followed by a character other then 'c' or 'd', the backslash character is discarded


            bool argIsVar[3];
            bool argIsArray[3];
            char valueType[3];
            Val args[3];

            copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

            if (valueType[0] != value_isStringPointer) { return result_arg_stringExpected; }                                    // prompt 

            bool isInput = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_input);                               // init
            bool isInfoWithYesNo = false;

            bool checkForDefault = false;       // init
            bool checkForCancel = false;

            bool answerValid{ false };
            _appFlags |= appFlag_waitingForUser;    // bit b6 set: waiting for user interaction

            do {                                                                                                                // until valid answer typed
                if (isInput) {                                                                                                  // input command
                    if (cmdParamCount == 3) {                                                                                   // 'allow default' if not zero 
                        if (((uint8_t)(valueType[2]) != value_isLong) && ((uint8_t)(valueType[2]) != value_isFloat)) { return result_arg_numValueExpected; }    // flag: with default 
                        checkForDefault = (((uint8_t)(valueType[2]) == value_isLong) ? args[2].longConst != 0 : args[2].floatConst != 0.);
                    }
                    checkForCancel = true;

                    if ((argIsArray[1]) && (valueType[1] != value_isStringPointer)) { return result_array_valueTypeIsFixed; }   // an array cannot change type: it needs to be string te receive result
                    if (checkForDefault && (valueType[1] != value_isStringPointer)) { return result_arg_stringExpected; }       // default supplied: it needs to be string

                    char s[80] = "===== Input (\\c to cancel";                                                                  // title static text
                    char title[80 + MAX_ALPHA_CONST_LEN] = "";                                                                   // title static text + string contents of variable
                    strcat(s, checkForDefault ? ", \\d for default = '%s') =====" : "): =====");
                    sprintf(title, s, args[1].pStringConst);
                    _pConsole->println(title);
                }

                else {                                                                                                          // info command
                    if (cmdParamCount == 2) {
                        if (((uint8_t)(valueType[1]) != value_isLong) && ((uint8_t)(valueType[1]) != value_isFloat)) { return result_arg_numValueExpected; }
                        if ((uint8_t)(valueType[1]) == value_isFloat) { args[1].longConst = (int)args[1].floatConst; }
                        if ((args[1].longConst < 0) || (args[1].longConst > 3)) { execResult = result_arg_invalid; return execResult; };

                        isInfoWithYesNo = args[1].longConst & 0x02;
                        checkForCancel = args[1].longConst & 0x01;
                    }
                    checkForDefault = false;

                    char s[120] = "===== Information ";
                    strcat(s, isInfoWithYesNo ? "(please answer Y or N" : "(please confirm by pressing ENTER");
                    _pConsole->println(strcat(s, checkForCancel ? ", \\c to cancel): =====" : "): ====="));
                }

                _pConsole->println(args[0].pStringConst);       // user prompt 

                // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                // return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
                bool doAbort{ false }, doStop{ false }, doCancel{ false }, doDefault{ false }, backslashFound{ false }, answerIsNo{ false }, quitNow{ false };
                int length{ 0 };
                char input[MAX_USER_INPUT_LEN + 1] = "";                                                                          // init: empty string
                if (readText(doAbort, doStop, doCancel, doDefault, input, length)) { execResult = result_eval_kill; return execResult; }
                doDefault = checkForDefault && doDefault;        // gate doDefault
                doCancel = checkForCancel && doCancel;           // gate doCancel

                if (doAbort) { userRequestsAbort = true; }                                         // '\a': abort running code (program or immediate mode statements)
                else if (doStop) { userRequestsStop = true; }                                           // '\s': stop a running program (do not produce stop event yet, wait until program statement executed)

                // if request to stop ('\s') received, first handle input data
                answerValid = true;                                                                                             // init
                if (!doAbort && !doCancel && !doDefault) {                                                                      // doStop: continue execution for now (stop when current statement is executed)
                    if (isInfoWithYesNo) {                                                                                      // check validity of answer ('y' or 'n')
                        if (length != 1) { answerValid = false; }
                        if (answerValid) {
                            if ((input[0] != 'n') && (input[0] != 'N') && (input[0] != 'y') && (input[0] != 'Y')) { answerValid = false; }
                            answerIsNo = (input[0] == 'n') || (input[0] == 'N');
                        }
                        if (!answerValid) { _pConsole->println("\r\nERROR: answer is not valid. Please try again"); }
                    }
                    else  if (isInput) {

                        LE_evalStack* pStackLvl = (cmdParamCount == 3) ? _pEvalStackMinus1 : _pEvalStackTop;
                        // if  variable currently holds a non-empty string (indicated by a nullptr), delete char string object
                        execResult_type execResult = deleteVarStringObject(pStackLvl); if (execResult != result_execOK) { return execResult; }

                        if (strlen(input) == 0) { args[1].pStringConst = nullptr; }
                        else {
                            // note that for reference variables, the variable type fetched is the SOURCE variable type
                            int varScope = pStackLvl->varOrConst.variableAttributes & var_scopeMask;
                            int stringlen = min(strlen(input), MAX_ALPHA_CONST_LEN);
                            (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;

                            args[1].pStringConst = new char[stringlen + 1];
                            memcpy(args[1].pStringConst, input, stringlen);                                                     // copy the actual string (not the pointer); do not use strcpy
                            args[1].pStringConst[stringlen] = '\0';

                        #if printCreateDeleteListHeapObjects
                            Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
                            Serial.println((uint32_t)args[1].pStringConst - RAMSTART);
                        #endif
                        }
                        *pStackLvl->varOrConst.value.ppStringConst = args[1].pStringConst;
                        *pStackLvl->varOrConst.varTypeAddress = (*pStackLvl->varOrConst.varTypeAddress & ~value_typeMask) | value_isStringPointer;

                        // if NOT a variable REFERENCE, then value type on the stack indicates the real value type and NOT 'variable reference' ...
                        // but it does not need to be changed, because in the next step, the respective stack level will be deleted 
                        }
                    }


                if (cmdParamCount == (isInput ? 3 : 2)) {       // last argument (optional second if Info, third if Input statement) serves a dual purpose: allow cancel (on entry) and signal 'canceled' (on exit)
                    // store result in variable and adapt variable value type
                    // 0 if canceled, 1 if 'OK' or 'Yes',  -1 if 'No' (variable is already numeric: no variable string to delete)
                    *_pEvalStackTop->varOrConst.value.pLongConst = doCancel ? 0 : answerIsNo ? -1 : 1;                          // 1: 'OK' or 'Yes' (yes / no question) answer                       
                    *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isLong;

                    // if NOT a variable REFERENCE, then value type on the stack indicates the real value type and NOT 'variable reference' ...
                    // but it does not need to be changed, because in the next step, the respective stack level will be deleted 
                }
                } while (!answerValid);
                _appFlags &= ~appFlag_waitingForUser;    // bit b6 reset: NOT waiting for user interaction

                clearEvalStackLevels(cmdParamCount);                                                                                // clear evaluation stack and intermediate strings
                _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
            }
        break;


        // -----------------------------------------------------------------------------------------------
        // stop or pause a running program and wait for the user to continue (without entering debug mode)
        //------------------------------------------------------------------------------------------------

        case cmdcod_pause:
        case cmdcod_halt:
        {

            long pauseTime = 1000;      // default: 1 second
            if (cmdParamCount == 1) {       // copy pause delay, in seconds, from stack, if provided
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];
                copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

                if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numValueExpected; }
                pauseTime = (valueType[0] == value_isLong) ? args[0].longConst : (int)args[0].floatConst;    // in seconds
                if (pauseTime < 1) { pauseTime = 1; }
                else if (pauseTime > 10) { pauseTime = 10; };
                pauseTime *= 1000; // to milliseconds
            }
            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_halt) {
                char s[100 + MAX_IDENT_NAME_LEN];
                sprintf(s, "===== Program stopped in user function %s: press ENTER to continue =====", extFunctionNames[_activeFunctionData.functionIndex]);
                _pConsole->println(s);
            }

            bool doAbort{ false }, doStop{ false }, backslashFound{ false };

            long startPauseAt = millis();                                                                                   // if pause, not stop;

            _appFlags |= appFlag_waitingForUser;    // bit b6 set: waiting for user interaction (or program paused)
            do {                                                                                                            // until new line character encountered
                char c{ 0xFF };                                                           // init: no character available
                // read a character, if available in buffer
                if (getKey(c)) { execResult = result_eval_kill; return execResult; }      // return value true: kill Justina interpreter (buffer is now flushed until next line character)

                if (c != 0xFF) {                                                                           // terminal character available for reading ?
                    if (c == '\n') { if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_halt) { break; } }

                    // Check for Justina ESCAPE sequence (sent by terminal as individual characters) and cancel input, or use default value, if indicated
                    // Note: if Justina ESCAPE sequence is not recognized, then backslash character is simply discarded
                    else if (c == '\\') { backslashFound = !backslashFound; }                                                                                     // backslash character found
                    else if ((tolower(c) == 'a') || (tolower(c) == 's')) {                                                                    // part of a Justina ESCAPE sequence ? Abort evaluation phase 
                        if (backslashFound) {
                            backslashFound = false;   ((tolower(c) == 'a') ? doAbort : doStop) = true;
                            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_pause) { break; }
                        }
                    }
                }

                if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_pause) {
                    if (startPauseAt + pauseTime < millis()) { break; }                                                           // if still characters in buffer, buffer will be flushed when processng of statement finalised
                }
            } while (true);

            _appFlags &= ~appFlag_waitingForUser;    // bit b6 reset: NOT waiting for user interaction (and program not paused)

            if (doAbort) { execResult = result_eval_abort; return execResult; }                                     // stop a running Justina program (buffer is now flushed until nex line character) 
            else if (doStop) { userRequestsStop = true; }                                                           // '\s': stop a running program (do not produce stop event yet, wait until program statement executed)

            // finalise
            clearEvalStackLevels(cmdParamCount);                                                                                // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -------------------------------------------------------------------------------------------------------------------------------------------------------------
        // print all arguments (longs, floats and strings) in succession. Floats are printed in compact format with maximum 3 digits / decimals and an optional exponent
        // -------------------------------------------------------------------------------------------------------------------------------------------------------------

        // note: the print command does not take into account the display format set to print the last calculation result
        // to format output produced with the print command, use the formatting function provided (function code: fnccod_format) 

        case cmdcod_print:
        {
            for (int i = 1; i <= cmdParamCount; i++) {
                bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
                char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
                bool opIsLong = ((uint8_t)valueType == value_isLong);
                bool opIsFloat = ((uint8_t)valueType == value_isFloat);
                char* printString = nullptr;

                Val operand;
                if (opIsLong || opIsFloat) {
                    char s[20];  // largely long enough to print long values, or float values with "G" specifier, without leading characters
                    printString = s;    // pointer
                    // next line is valid for long values as well (same memory locations are copied)
                    operand.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst);
                    if (opIsLong) { sprintf(s, "%ld", operand.longConst); }
                    else { sprintf(s, "%.3G", operand.floatConst); }
                }
                else {
                    operand.pStringConst = operandIsVar ? (*pStackLvl->varOrConst.value.ppStringConst) : pStackLvl->varOrConst.value.pStringConst;
                    // no need to copy string - just print the original, directly from stack (it's still there)
                    printString = operand.pStringConst;     // attention: null pointers not transformed into zero-length strings here
                }
                // NOTE that there is no limit on the number of characters printed here (MAX_PRINT_WIDTH not checked)
                if (printString != nullptr) {
                    _pConsole->print(printString);         // test needed because zero length strings stored as nullptr
                    if (strlen(printString) > 0) { _atLineStart = (printString[strlen(printString) - 1] == '\n'); }       // no change if empty string
                }
                pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
            }

            clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -------------------------------------------------------
        // Set display format for printing last calculation result
        // -------------------------------------------------------

        case cmdcod_dispfmt:
        {
            // mandatory argument 1: width (used for both numbers and strings) 
            // optional arguments 2-4 (relevant for printing numbers only): [precision, [specifier (F:fixed, E:scientific, G:general, D: decimal, X:hex), ] flags]
            // note that specifier argument can be left out, flags argument taking its place

            bool argIsVar[4];
            bool argIsArray[4];
            char valueType[4];
            Val args[4];

            if (cmdParamCount > 4) { execResult = result_arg_tooManyArgs; return execResult; }
            copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

            // set format for numbers and strings

            execResult_type execResult = checkFmtSpecifiers(true, false, cmdParamCount, valueType, args, _dispNumSpecifier[0],
                _dispWidth, _dispNumPrecision, _dispFmtFlags);
            if (execResult != result_execOK) { return execResult; }

            _dispIsIntFmt = (_dispNumSpecifier[0] == 'X') || (_dispNumSpecifier[0] == 'x') || (_dispNumSpecifier[0] == 'd') || (_dispNumSpecifier[0] == 'D');
            makeFormatString(_dispFmtFlags, _dispIsIntFmt, _dispNumSpecifier, _dispNumberFmtString);       // for numbers

            _dispCharsToPrint = _dispWidth;
            strcpy(_dispStringFmtString, "%*.*s%n");                                                           // strings: set characters to print to display width

            clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings

            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // ------------------------
        // set console display mode
        // ------------------------

        case cmdcod_dispmod:      // takes two arguments: width & flags
        {
            // mandatory argument 1: 0 = do not print prompt and do not echo user input; 1 = print prompt but no not echo user input; 2 = print prompt and echo user input 
            // mandatory argument 2: 0 = do not print last result; 1 = print last result

            bool argIsVar[2];
            bool argIsArray[2];
            char valueType[2];               // 2 arguments
            Val args[2];

            copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

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

            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -------------------------- 
        // Call a user routine in C++
        // --------------------------

        case cmdcod_callback:
        {
            // preprocess
            // ----------

            // determine which callback routine to call, based upon alias (argument 1) 
            LE_evalStack* aliasStackLvl = pStackLvl;
            char* alias = aliasStackLvl->genericName.pStringConst;
            bool isDeclared = false;
            int index{};
            for (index = 0; index < _userCBprocAliasSet_count; index++) {                               // find alias in table (break if found)
                if (strcmp(_callbackUserProcAlias[index], alias) == 0) { isDeclared = true; break; }
            }
            if (!isDeclared) { execResult = result_aliasNotDeclared; return execResult; }

            LE_evalStack* pStackLvlFirstValueArg = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
            pStackLvl = pStackLvlFirstValueArg;

            // variable references to store (arguments 2[..4]) 
            const char isVariable = 0x80;                                                               // mask: is variable (not a constant) 

            // if more than 8 arguments are supplied, excess arguments are discarded
            Val args[8]{  };                                                                            // values to be passed to user routine
            char valueType[8]{ value_noValue,value_noValue,value_noValue,value_noValue,value_noValue,value_noValue,value_noValue,value_noValue };                             // value types (long, float, char string)
            char varScope[8]{};                                                                         // if variable: variable scope (user, program global, static, local)
            bool argIsVar[8]{};                                                                         // flag: is variable (scalar or aray)
            bool argIsArray[8]{};                                                                       // flag: is array element

            const void* values[8]{};                                                                    // to keep it simple for the c++ user writing the user routine, we simply pass const void pointers

            // any data to pass ? (optional arguments 2 to 9)
            if (cmdParamCount >= 2) {                                                                   // first argument (callback procedure) processed (but still on the stack)
                copyValueArgsFromStack(pStackLvl, cmdParamCount - 1, argIsVar, argIsArray, valueType, args, true);  // creates a NEW temporary string object if empty string OR or constant (non-variable) string 
                pStackLvl = pStackLvlFirstValueArg;     // set stack level again to first value argument
                for (int i = 0; i < cmdParamCount - 1; i++) {
                    if (argIsVar[i]) {                                                                  // is this a variable ? (not a constant)
                        valueType[i] |= isVariable;                                                     // flag as variable (scalar or array element)
                        varScope[i] = (pStackLvl->varOrConst.variableAttributes & var_scopeMask);       // remember variable scope (user, program global, local, static) 
                    }
                    values[i] = args[i].pBaseValue;                                                     // set void pointer to: integer, float, char* 
                    pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
                }
            }


            // call user routine
            // -----------------

            _callbackUserProcStart[index](values, valueType);                                           // call back user procedure


            // postprocess: check any strings RETURNED by callback procedure
            // -------------------------------------------------------------

            pStackLvl = pStackLvlFirstValueArg;                                                         // set stack level again to first value argument
            for (int i = 0; i < 8; i++) {
                if ((valueType[i] & value_typeMask) == value_isStringPointer) {

                    // string COPY (or newly created empty variable string) passed to user routine ? (only if string passed is empty string OR or constant (non-variable) string)
                    if (valueType[i] & passCopyToCallback) {
                    #if printCreateDeleteListHeapObjects
                        Serial.print("----- (Intermd str) "); Serial.println((uint32_t)args[i].pStringConst - RAMSTART);
                    #endif
                        delete[] args[i].pStringConst;                                                  // delete temporary string
                        _intermediateStringObjectCount--;
                    }

                    // callback routine changed non-empty VARIABLE string into empty variable string ("\0") ?
                    else if (strlen(args[i].pStringConst) == 0) {

                    #if printCreateDeleteListHeapObjects 
                        Serial.print((varScope[i] == var_isUser) ? "----- (usr var str) " : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
                        Serial.println((uint32_t)args[i].pStringConst - RAMSTART);
                    #endif
                        delete[]args[i].pStringConst;                                                   // delete variable string
                        (varScope[i] == var_isUser) ? _userVarStringObjectCount-- : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount-- : _localVarStringObjectCount--;

                        // set variable string pointer to null pointer
                        *pStackLvl->varOrConst.value.ppStringConst = nullptr;                           // change pointer to string (in variable) to null pointer
                    }
                }
                pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
            }


            // finalize
            // --------

            clearEvalStackLevels(cmdParamCount);                                                        // clear evaluation stack and intermediate strings

            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -----------------
        //
        // -----------------

        case cmdcod_for:
        case cmdcod_if:                                                                                                   // 'if' command
        case cmdcod_while:                                                                                                // 'while' command
        {
            // start a new loop, or execute an existing loop ? 
            bool initNew{ true };        // IF...END: only one iteration (always new), FOR...END loop: always first itaration of a new loop, because only pass (command skipped for next iterations)
            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_while) {        // while block: start of an iteration
                if (flowCtrlStack.getElementCount() != 0) {                 // at least one open block exists in current function (or main) ?
                    char blockType = *(char*)_pFlowCtrlStackTop;
                    if ((blockType == block_for) || (blockType == block_if)) { initNew = true; }
                    else if (blockType == block_while) {
                        // currently executing an iteration of an outer 'if', 'while' or 'for' loop ? Then this is the start of the first iteration of a new (inner) 'if' or 'while' loop
                        initNew = ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & withinIteration;      // 'within iteration' flag set ?
                    }
                }
            }

            if (initNew) {
                _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
                _pFlowCtrlStackTop = (OpenBlockTestData*)flowCtrlStack.appendListElement(sizeof(OpenBlockTestData));
                ((OpenBlockTestData*)_pFlowCtrlStackTop)->blockType =
                    (_activeFunctionData.activeCmd_ResWordCode == cmdcod_if) ? block_if :
                    (_activeFunctionData.activeCmd_ResWordCode == cmdcod_while) ? block_while :
                    block_for;       // start of 'if...end' or 'while...end' block

                // FOR...END loops only: initialize ref to control variable, final value and step
                if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_for) {

                    // store variable reference, upper limit, optional increment / decrement (only once), address of token directly following 'FOR...; statement
                    ((OpenBlockTestData*)_pFlowCtrlStackTop)->nextTokenAddress = _activeFunctionData.pNextStep;

                    bool controlVarIsLong{ false }, finalValueIsLong{ false }, stepIsLong{ false };
                    for (int i = 1; i <= cmdParamCount; i++) {        // skipped if no arguments
                        Val operand;                                                                                            // operand and result
                        bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
                        char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
                        if ((valueType != value_isLong) && (valueType != value_isFloat)) { execResult = result_testexpr_numberExpected; return execResult; }
                        operand.floatConst = (operandIsVar ? *pStackLvl->varOrConst.value.pFloatConst : pStackLvl->varOrConst.value.floatConst);        // valid for long values as well

                        // store references to control variable and its value type
                        if (i == 1) {
                            controlVarIsLong = (valueType == value_isLong);         // remember
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlVar = pStackLvl->varOrConst.value;      // pointer to variable (containing a long or float constant)
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlValueType = pStackLvl->varOrConst.varTypeAddress;        // pointer to variable value type
                        }

                        // store final loop value
                        else if (i == 2) {
                            finalValueIsLong = (valueType == value_isLong);         // remember
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue = operand;
                        }

                        // store loop step
                        else {      // third parameter
                            stepIsLong = (valueType == value_isLong);         // remember
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->step = operand;
                        }                         // store loop increment / decrement 

                        pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
                    }

                    if (cmdParamCount < 3) {        // step not specified: init with default (1.)  
                        stepIsLong = false;
                        ((OpenBlockTestData*)_pFlowCtrlStackTop)->step.floatConst = 1.;     // init as float
                    }

                    // determine value type to use for loop tests, promote final value and step to float if value type to use for loop tests is float
                    // the initial value type of the control variable and the value type of (constant) final value and step define the loop test value type
                    ((OpenBlockTestData*)_pFlowCtrlStackTop)->testValueType = (controlVarIsLong && finalValueIsLong && stepIsLong ? value_isLong : value_isFloat);
                    if (((OpenBlockTestData*)_pFlowCtrlStackTop)->testValueType == value_isFloat) {
                        if (finalValueIsLong) { ((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue.floatConst = (float)((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue.longConst; }
                        if (stepIsLong) { ((OpenBlockTestData*)_pFlowCtrlStackTop)->step.floatConst = (float)((OpenBlockTestData*)_pFlowCtrlStackTop)->step.longConst; }
                    }

                    ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl |= forLoopInit;           // init at the start of initial FOR loop iteration
                }

                ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl &= ~breakFromLoop;            // init at the start of initial iteration for any loop
            }

            ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl |= withinIteration;               // init at the start of an iteration for any loop
        }

        // no break here: from here on, subsequent execution is common for 'if', 'elseif', 'else' and 'while'


        // -----------------
        //
        // -----------------

        case cmdcod_else:
        case cmdcod_elseif:

        {
            bool precedingTestFailOrNone{ true };  // init: preceding test failed ('elseif', 'else' command), or no preceding test ('if', 'for' command)
            // init: set flag to test condition of current 'if', 'while', 'elseif' command
            bool testClauseCondition = (_activeFunctionData.activeCmd_ResWordCode != cmdcod_for);
            // 'else, 'elseif': if result of previous test (in preceding 'if' or 'elseif' clause) FAILED (fail = false), then CLEAR flag to test condition of current command (not relevant for 'else') 
            if ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_else) ||
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_elseif)) {
                precedingTestFailOrNone = (bool)(((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & testFail);
            }
            testClauseCondition = precedingTestFailOrNone && (_activeFunctionData.activeCmd_ResWordCode != cmdcod_for) && (_activeFunctionData.activeCmd_ResWordCode != cmdcod_else);

            //init current condition test result (assume test in preceding clause ('if' or 'elseif') passed, so this clause needs to be skipped)
            bool fail = !precedingTestFailOrNone;
            if (testClauseCondition) {                                                                                // result of test in preceding 'if' or 'elseif' clause FAILED ? Check this clause
                Val operand;                                                                                            // operand and result
                bool operandIsVar = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);
                char valueType = operandIsVar ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
                if ((valueType != value_isLong) && (valueType != value_isFloat)) { execResult = result_testexpr_numberExpected; return execResult; }
                operand.floatConst = operandIsVar ? *_pEvalStackTop->varOrConst.value.pFloatConst : _pEvalStackTop->varOrConst.value.floatConst;        // valid for long values as well (same memory locations are copied)

                fail = (valueType == value_isFloat) ? (operand.floatConst == 0.) : (operand.longConst == 0);                                                                        // current test (elseif clause)
                ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl = fail ? ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl | testFail : ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & ~testFail;                                          // remember test result (true -> 0x1)
            }

            bool setNextToken = fail || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_for);
            if (setNextToken) {                                                                                  // skip this clause ? (either a preceding test passed, or it failed but the curreent test failed as well)
                TokenIsResWord* pToToken;
                int toTokenStep{ 0 };
                pToToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;
                memcpy(&toTokenStep, pToToken->toTokenStep, sizeof(char[2]));
                _activeFunctionData.pNextStep = _programStorage + toTokenStep;              // prepare jump to 'else', 'elseif' or 'end' command
            }

            clearEvalStackLevels(cmdParamCount);      // clear evaluation stack

            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -----------------
        //
        // -----------------

        case cmdcod_break:
        case cmdcod_continue:
        {
            char blockType = block_none;
            bool isLoop{};
            do {
                blockType = *(char*)_pFlowCtrlStackTop;
                // inner block(s) could be IF...END blocks (before reaching loop block)
                isLoop = ((blockType == block_while) || (blockType == block_for));
                if (isLoop) {
                    TokenIsResWord* pToken;
                    int toTokenStep{ 0 };
                    pToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;                // pointer to loop start command token
                    memcpy(&toTokenStep, pToken->toTokenStep, sizeof(char[2]));
                    pToken = (TokenIsResWord*)(_programStorage + toTokenStep);                         // pointer to loop end command token
                    memcpy(&toTokenStep, pToken->toTokenStep, sizeof(char[2]));
                    _activeFunctionData.pNextStep = _programStorage + toTokenStep;                        // prepare jump to 'END' command
                }

                else {          // inner IF...END block: remove from flow control stack 
                    flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                    _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
                    _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
                    _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);
                }
            } while (!isLoop);

            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_break) { ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl |= breakFromLoop; }

            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // ----------------------------------------------
        // end block command (While, For, If) or Function
        // ----------------------------------------------

        case cmdcod_end:
        {
            char blockType = *(char*)_pFlowCtrlStackTop;       // determine currently open block

            if ((blockType == block_if) || (blockType == block_while) || (blockType == block_for)) {

                bool exitLoop{ true };

                if ((blockType == block_for) || (blockType == block_while)) {
                    exitLoop = ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & breakFromLoop;  // BREAK command encountered
                }

                if (!exitLoop) {      // no BREAK encountered: loop terminated anyway ?
                    if (blockType == block_for) { execResult = testForLoopCondition(exitLoop); if (execResult != result_execOK) { return execResult; } }
                    else if (blockType == block_while) { exitLoop = (((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & testFail); } // false: test passed
                }

                if (!exitLoop) {        // flag still not set ?
                    if (blockType == block_for) {
                        _activeFunctionData.pNextStep = ((OpenBlockTestData*)_pFlowCtrlStackTop)->nextTokenAddress;
                    }
                    else {      // WHILE...END block
                        TokenIsResWord* pToToken;
                        int toTokenStep{ 0 };
                        pToToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;
                        memcpy(&toTokenStep, pToToken->toTokenStep, sizeof(char[2]));

                        _activeFunctionData.pNextStep = _programStorage + toTokenStep;         // prepare jump to start of new loop
                    }
                }

                ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl &= ~withinIteration;          // at the end of an iteration

                // do NOT reset in case of End Function: _activeFunctionData will receive its values in routine terminateExternalFunction()
                _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended

                if (exitLoop) {
                    flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                    _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
                    _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
                    _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);
                }
                break;      // break here: do not break if end function !
            }

        }

        // no break here: from here on, subsequent execution is the same for 'end' (function) and for 'return'


        // --------------------
        // return from function
        // --------------------

        case cmdcod_return:
        {
            isFunctionReturn = true;
            bool returnWithZero = (cmdParamCount == 0);                    // RETURN statement without expression, or END statement: return a zero
            execResult = terminateExternalFunction(returnWithZero);
            if (execResult != result_execOK) { return execResult; }

            // do NOT reset _activeFunctionData.activeCmd_ResWordCode: _activeFunctionData will receive its values in routine terminateExternalFunction()

        }
        break;

        }       // end switch

    return result_execOK;
        }


// -------------------------------
// *   test for loop condition   *
// -------------------------------

Justina_interpreter::execResult_type Justina_interpreter::testForLoopCondition(bool& testFails) {

    char testTypeIsLong = (((OpenBlockTestData*)_pFlowCtrlStackTop)->testValueType == value_isLong);    // loop final value and step have the initial control variable value type
    bool ctrlVarIsLong = ((*(uint8_t*)((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlValueType & value_typeMask) == value_isLong);
    bool ctrlVarIsFloat = ((*(uint8_t*)((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlValueType & value_typeMask) == value_isFloat);
    if (!ctrlVarIsLong && !ctrlVarIsFloat) { return result_testexpr_numberExpected; }                       // value type changed to string within loop: error

    Val& pCtrlVar = ((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlVar;                                       // pointer to control variable
    Val& finalValue = ((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue;
    Val& step = ((OpenBlockTestData*)_pFlowCtrlStackTop)->step;
    char& loopControl = ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl;


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
int Justina_interpreter::jumpTokens(int n) {
    int tokenCode;
    char* pStep;
    return jumpTokens(n, pStep, tokenCode);
}

int Justina_interpreter::jumpTokens(int n, char*& pStep) {
    int tokenCode;
    return jumpTokens(n, pStep, tokenCode);
}

int Justina_interpreter::jumpTokens(int n, char*& pStep, int& tokenCode) {

    // pStep: pointer to token
    // n: number of tokens to jump
    // return 'tok_no_token' if not enough tokens are present 

    int tokenType = tok_no_token;

    for (int i = 1; i <= n; i++) {
        tokenType = *pStep & 0x0F;
        if (tokenType == tok_no_token) { return tok_no_token; }               // end of program reached
        // terminals and constants: token length is NOT stored in token type
        int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
            (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;
        pStep = pStep + tokenLength;
    }

    tokenType = *pStep & 0x0F;
    int tokenIndex{ 0 };

    switch (tokenType) {
        case tok_isReservedWord:
            tokenIndex = (((TokenIsResWord*)pStep)->tokenIndex);
            tokenCode = _resWords[tokenIndex].resWordCode;
            break;

        case tok_isTerminalGroup1:
        case tok_isTerminalGroup2:
        case tok_isTerminalGroup3:
            tokenIndex = ((((TokenIsTerminal*)pStep)->tokenTypeAndIndex >> 4) & 0x0F);
            tokenIndex += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
            tokenCode = _terminals[tokenIndex].terminalCode;
            break;

        default:
            break;

    }

    return tokenType;
}

// ------------------------------------
// *   advance until specific token   *
// ------------------------------------

int Justina_interpreter::findTokenStep(int tokenTypeToFind, char tokenCodeToFind, char*& pStep) {

    // pStep: pointer to first token to test versus token group and (if applicable) token code
    // tokenType: if 'tok_isTerminalGroup1', test for the three terminal groups !

    // exclude current token step
    int tokenType = *pStep & 0x0F;
    // terminals and constants: token length is NOT stored in token type
    int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
        (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;        // fetch next token 
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
                    tokenCodeMatch = _resWords[tokenIndex].resWordCode == tokenCodeToFind;
                    break;

                case tok_isTerminalGroup1:       // actual token can be part of any of the three terminal groups
                    tokenIndex = ((((TokenIsTerminal*)pStep)->tokenTypeAndIndex >> 4) & 0x0F);
                    tokenIndex += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
                    tokenCodeMatch = _terminals[tokenIndex].terminalCode == tokenCodeToFind;
                    break;

                default:
                    return tokenType;
                    break;
            }
            if (tokenCodeMatch) { return tokenType; }      // if terminal, then return exact group (entry: use terminalGroup1) 
        }

        int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
            (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;    // fetch next token 
        pStep = pStep + tokenLength;
    } while (true);
}


// ------------------------------------------------
// Save last value for future reuse by calculations 
// ------------------------------------------------

void Justina_interpreter::saveLastValue(bool& overWritePrevious) {
    if (!(evalStack.getElementCount() > _activeFunctionData.callerEvalStackLevels)) { return; }           // safety: data available ?
    // if overwrite 'previous' last result, then replace first item (if there is one); otherwise replace last item if FiFo full (-1 if nothing to replace)
    int itemToRemove = overWritePrevious ? ((_lastResultCount >= 1) ? 0 : -1) :
        ((_lastResultCount == MAX_LAST_RESULT_DEPTH) ? MAX_LAST_RESULT_DEPTH - 1 : -1);

    // remove a previous item ?
    if (itemToRemove != -1) {
        // if item to remove is a string: delete heap object
        if (lastResultTypeFiFo[itemToRemove] == value_isStringPointer) {

            if (lastResultValueFiFo[itemToRemove].pStringConst != nullptr) {
            #if printCreateDeleteListHeapObjects
                Serial.print("----- (FiFo string) ");   Serial.println((uint32_t)lastResultValueFiFo[itemToRemove].pStringConst - RAMSTART);
            #endif 
                // note: this is always an intermediate string
                delete[] lastResultValueFiFo[itemToRemove].pStringConst;
                _lastValuesStringObjectCount--;
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
    char sourceValueType = lastValueIsVariable ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
    bool lastValueNumeric = ((sourceValueType == value_isLong) || (sourceValueType == value_isFloat));
    bool lastValueIntermediate = ((_pEvalStackTop->varOrConst.valueAttributes & constIsIntermediate) == constIsIntermediate);

    // line below works for long integers as well
    if (lastValueNumeric) { lastvalue.value.floatConst = (lastValueIsVariable ? (*_pEvalStackTop->varOrConst.value.pFloatConst) : _pEvalStackTop->varOrConst.value.floatConst); }
    else { lastvalue.value.pStringConst = (lastValueIsVariable ? (*_pEvalStackTop->varOrConst.value.ppStringConst) : _pEvalStackTop->varOrConst.value.pStringConst); }

    if ((lastValueNumeric) || (!lastValueNumeric && (lastvalue.value.pStringConst == nullptr))) {
        lastResultValueFiFo[0] = lastvalue.value;
    }
    // new last value is a non-empty string: make a copy of the string and store a reference to this new string
    else {
        int stringlen = min(strlen(lastvalue.value.pStringConst), MAX_ALPHA_CONST_LEN);        // excluding terminating \0
        lastResultValueFiFo[0].pStringConst = new char[stringlen + 1];
        _lastValuesStringObjectCount++;
        memcpy(lastResultValueFiFo[0].pStringConst, lastvalue.value.pStringConst, stringlen);        // copy the actual string (not the pointer); do not use strcpy
        lastResultValueFiFo[0].pStringConst[stringlen] = '\0';

    #if printCreateDeleteListHeapObjects
        Serial.print("+++++ (FiFo string) ");   Serial.println((uint32_t)lastResultValueFiFo[0].pStringConst - RAMSTART);
    #endif            

        if (lastValueIntermediate) {
        #if printCreateDeleteListHeapObjects
            Serial.print("----- (intermd str) ");   Serial.println((uint32_t)lastvalue.value.pStringConst - RAMSTART);
        #endif
            delete[] lastvalue.value.pStringConst;
            _intermediateStringObjectCount--;
    }
}

    // store new last value type
    lastResultTypeFiFo[0] = sourceValueType;               // value type

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

void Justina_interpreter::clearEvalStack() {

    clearEvalStackLevels(evalStack.getElementCount());
    _pEvalStackTop = nullptr;  _pEvalStackMinus1 = nullptr; _pEvalStackMinus2 = nullptr;            // should be already

    // error if not all intermediate string objects deleted (points to an internal Justina issue)
    if (_intermediateStringObjectCount != 0) {
        //// _pConsole ???
        Serial.print("*** Intermediate string cleanup error. Remaining: "); Serial.println(_intermediateStringObjectCount);
        _intermediateStringObjectErrors += abs(_intermediateStringObjectCount);
        _intermediateStringObjectCount = 0;
    }
    return;
}


// --------------------------------------------------------------------------
// Clear n evaluation stack levels and associated intermediate string objects  
// --------------------------------------------------------------------------

void Justina_interpreter::clearEvalStackLevels(int n) {

    if (n <= 0) { return; }             // nothing to do

    LE_evalStack* pStackLvl = _pEvalStackTop, * pPrecedingStackLvl{};

    for (int i = 1; i <= n; i++) {
        // if intermediate constant string, then delete char string object (test op non-empty intermediate string object in called routine)  
        if (pStackLvl->genericToken.tokenType == tok_isConstant) { deleteIntermStringObject(pStackLvl); }    // exclude non-constant tokens (terminals, keywords, functions, ...)

        // delete evaluation stack level
        pPrecedingStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);
        evalStack.deleteListElement(pStackLvl);
        pStackLvl = pPrecedingStackLvl;
    }

    _pEvalStackTop = pStackLvl;
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);
    return;
}


// ------------------------
// Clear flow control stack  
// ------------------------

void Justina_interpreter::clearFlowCtrlStack(int& deleteImmModeCmdStackLevels, execResult_type execResult, bool debugModeError) {

    // if NOT in debug mode while an error occurs (no programs are currently stopped): delete all flow control stack entries
    //
    // if IN debug mode (at least one program is currently stopped) while an error occurs
    // - if immediate mode error (error did not occur in a running program or during execution of a parsed eval() string): 
    //   _activeFunctionData already refers to (debug level) imm. mode data: flow control stack is OK
    // - if error occured within a running program or during execution of a parsed eval() string: delete all flow control stack levels UNTIL stopped program function is encountered   
    //   (-> do NOT delete any stack levels for the stopped program)
    //   when function type block for debug command level is encountered in between: move to _activeFunctionData but continue deleting command level open blocks, if any 
    //   (-> function type block for debug command level can still be followed by open block (if, for, ...) stack levels for that function)
    // 
    // always clear remaining local storage + local variable string and array values for open functions referenced in functions to abort

    deleteImmModeCmdStackLevels = 0;      // init
    bool isDebugCmdLevel = (_activeFunctionData.blockType == block_extFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + PROG_MEM_SIZE)) : false;  // debug command line

    // imm. mode level error while in debug mode: do not end currently stopped program, except when 'error' indicates 'abort currently STOPPED program'
    if (debugModeError && isDebugCmdLevel && (execResult != result_eval_abort)) { return; }

    if (flowCtrlStack.getElementCount() > 0) {                // skip if only main level (no program running)
        bool isInitialLoop{ true };
        bool noMoreProgramsToTerminate = (debugModeError && isDebugCmdLevel && (execResult != result_eval_abort));                // true if stopped program was terminated
        bool deleteDebugLevelOpenBlocksOnly = (debugModeError && isDebugCmdLevel && (execResult != result_eval_abort));
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                   // deepest caller, parsed eval() expression, loop ..., if any

        // delete all flow control stack levels above the current command line stack level (this could be a debugging command line)

        do {
            // first loop: retrieve block type of currently active function (could be 'main' level = immediate mode instruction as well)
            char blockType = isInitialLoop ? _activeFunctionData.blockType : *(char*)pFlowCtrlStackLvl;    // first character of structure is block type

            if (blockType == block_extFunction) {               // block type: function (is current function) - NOT a (for while, ...) loop or other block type

                if (!isInitialLoop) { _activeFunctionData = *((OpenFunctionData*)pFlowCtrlStackLvl); }      // after first loop, load from stack

                bool isProgramFunction = (_activeFunctionData.pNextStep < (_programStorage + PROG_MEM_SIZE));
                bool isImmModeStatements = !isProgramFunction;             // for clear doc.
                if (isProgramFunction && noMoreProgramsToTerminate) { break; }            // within program but no programs to abort any more ? exit loop

                // error while in debug mode: error occured in imm. mode statement ? 
                // abort command: initial loop deals with debug command level where command was issued and needs to be discarded (function(s) data still need to be deleted)
                if (debugModeError && isImmModeStatements && !isInitialLoop) { noMoreProgramsToTerminate = true; }

                if (isProgramFunction) {                    // program stack level
                    int functionIndex = _activeFunctionData.functionIndex;
                    int localVarCount = extFunctionData[functionIndex].localVarCountInFunction;
                    int paramOnlyCount = extFunctionData[functionIndex].paramOnlyCountInFunction;

                    if (localVarCount > 0) {
                    #if printCreateDeleteListHeapObjects
                        Serial.print("----- (LOCAL STORAGE) ");   Serial.println((uint32_t)(_activeFunctionData.pLocalVarValues) - RAMSTART);
                    #endif
                        deleteArrayElementStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);
                        deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);

                        // release local variable storage for function that has been called
                        delete[] _activeFunctionData.pLocalVarValues;
                        delete[] _activeFunctionData.pVariableAttributes;
                        delete[] _activeFunctionData.ppSourceVarTypes;
                        _localVarValueAreaCount--;
                    }
                }
                if (!isInitialLoop) { --_callStackDepth; }
            }

            else if (blockType == block_eval) {
                // no need to copy flow control stack level to _activeFunctionData
                if (!isInitialLoop) { --_callStackDepth; }

                if (debugModeError) { ++deleteImmModeCmdStackLevels; }          // remember how many imm. mode command line stack levels need to be deleted (later)
            }

            // all block types: go to previous list element and delete last list element
            if (!isInitialLoop) {
                pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                flowCtrlStack.deleteListElement(nullptr);                           // delete last element
            }


            if (pFlowCtrlStackLvl == nullptr) { break; }       // all done
            isInitialLoop = false;
        } while (true);
    }

    _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
    _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
    _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);
}


// -----------------------------
// Clear immediate command stack  
// -----------------------------

// deletes parsed strings referenced in program storage for the current command line statements, and subsequently pops a parsed command line from the stack to program storage
// the parameter n specifies the number of stack levels to pop

void Justina_interpreter::clearImmediateCmdStack(int n) {

    // note: ensure that only entries are cleared for eval() levels for currently running program (if there is one) and current debug level (if at least one program is stopped)
    _pImmediateCmdStackTop = immModeCommandStack.getLastListElement();

    while (n-- > 0) {
        // copy command line stack top to command line program storage and pop command line stack top
        _lastUserCmdStep = *(char**)_pImmediateCmdStackTop;                                     // pop parsed user cmd length
        long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + PROG_MEM_SIZE) + 1;
        deleteConstStringObjects(_programStorage + PROG_MEM_SIZE);      // current parsed user command statements in immediate mode program memory
        memcpy((_programStorage + PROG_MEM_SIZE), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);        // size berekenen
        immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
        _pImmediateCmdStackTop = immModeCommandStack.getLastListElement();
        Serial.print("  >> POP (clr imm cmd stack): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + PROG_MEM_SIZE));
    }

    // do NOT delete parsed string constants for original command line (last copied to program storage) - handled later 
}


// --------------------------------------------------------------------------------------------------------------------------
// *   execute internal or external function, calculate array element address or remove parenthesis around single argument  *
// --------------------------------------------------------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::execParenthesesPair(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& firstArgStackLvl, int argCount) {
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

Justina_interpreter::execResult_type Justina_interpreter::arrayAndSubscriptsToarrayElement(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pStackLvl, int argCount) {
    void* pArray = *pPrecedingStackLvl->varOrConst.value.ppArray;
    _activeFunctionData.errorProgramCounter = pPrecedingStackLvl->varOrConst.tokenAddress;                // token adress of array name (in the event of an error)

    int elemSpec[3] = { 0,0,0 };
    int dimNo = 0;
    do {
        bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
        char sourceValueType = operandIsVar ? *pStackLvl->varOrConst.varTypeAddress & value_typeMask : pStackLvl->varOrConst.valueType;
        bool opIsLong = sourceValueType == value_isLong;
        bool opIsFloat = sourceValueType == value_isFloat;

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

void Justina_interpreter::makeIntermediateConstant(LE_evalStack* pEvalStackLvl) {
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
            _intermediateStringObjectCount++;
            strcpy(result.pStringConst, operand.pStringConst);        // copy the actual strings 
        #if printCreateDeleteListHeapObjects
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

Justina_interpreter::execResult_type  Justina_interpreter::execAllProcessedOperators() {            // prefix and infix

    // _pEvalStackTop should point to an operand on entry (parsed constant, variable, expression result)

    int pendingTokenIndex{ 0 };
    int pendingTokenType{ tok_no_token }, pendingTokenPriority{};
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
            minus1IsOperator = (_terminals[terminalIndex].terminalCode <= termcod_opRangeEnd);  // preceding entry is operator ?
        }
        if (minus1IsOperator) {     // operator before current token
            bool isPrefixOperator = true;             // prefix or infix operator ?
            if (evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 3) {         // TWO preceding tokens exist on the stack               
                isPrefixOperator = (!(_pEvalStackMinus2->genericToken.tokenType == tok_isConstant) && !(_pEvalStackMinus2->genericToken.tokenType == tok_isVariable));
                // comma separators are not pushed to the evaluation stack, but if it is followed by a (prefix) operator, a flag is set in order not to mistake a token sequence as two operands and an infix operation
                if (_pEvalStackMinus1->terminal.index & 0x80) { isPrefixOperator = true; }            // e.g. print 5, -6 : prefix operation on second expression ('-6') and not '5-6' as infix operation
            }

            // check operator priority
            int priority{ 0 };
            if (isPrefixOperator) { priority = _terminals[terminalIndex].prefix_priority & 0x1F; }       // bits v43210 = priority
            else { priority = _terminals[terminalIndex].infix_priority & 0x1F; }
            bool RtoLassociativity = isPrefixOperator ? true : _terminals[terminalIndex].infix_priority & op_RtoL;


            // pending (not yet processed) token (always present and always a terminal token after a variable or constant token)
            // pending token can be any terminal token: infix operator, left or right parenthesis, comma or semicolon 
            // it can not be a prefix operator because it follows an operand (on top of stack)
            pendingTokenType = *_activeFunctionData.pNextStep & 0x0F;                                    // there's always minimum one token pending (even if it is a semicolon)
            pendingTokenIndex = (*_activeFunctionData.pNextStep >> 4) & 0x0F;                            // terminal token only: index stored in high 4 bits of token type 
            pendingTokenIndex += ((pendingTokenType == tok_isTerminalGroup2) ? 0x10 : (pendingTokenType == tok_isTerminalGroup3) ? 0x20 : 0);
            bool pendingIsPostfixOperator = (_terminals[pendingTokenIndex].postfix_priority != 0);        // postfix or infix operator ?

            // check pending operator priority
            pendingTokenPriority = (pendingIsPostfixOperator ? (_terminals[pendingTokenIndex].postfix_priority & 0x1F) :        // bits v43210 = priority
                (_terminals[pendingTokenIndex].infix_priority) & 0x1F);  // pending terminal is either an infix or a postfix operator


            // determine final priority
            currentOpHasPriority = (priority >= pendingTokenPriority);
            if ((priority == pendingTokenPriority) && (RtoLassociativity)) { currentOpHasPriority = false; }

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

Justina_interpreter::execResult_type  Justina_interpreter::execUnaryOperation(bool isPrefix) {

    Val operand, opResult;                                                               // operand and result

    // what are the stack levels for operator and operand ?
    LE_evalStack* pOperandStackLvl = isPrefix ? _pEvalStackTop : _pEvalStackMinus1;
    LE_evalStack* pUnaryOpStackLvl = isPrefix ? _pEvalStackMinus1 : _pEvalStackTop;
    _activeFunctionData.errorProgramCounter = pUnaryOpStackLvl->terminal.tokenAddress;                // in the event of an error


    // (1) Fetch operator info, whether operand is variables, and operand value type 
    // -----------------------------------------------------------------------------

    // operator
    int terminalIndex = pUnaryOpStackLvl->terminal.index & 0x7F;
    char terminalCode = _terminals[terminalIndex].terminalCode;
    bool requiresLongOp = (_terminals[terminalIndex].prefix_priority & op_long);
    bool resultCastLong = (_terminals[terminalIndex].prefix_priority & res_long);

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

    if (terminalCode == termcod_minus) { opIsFloat ? opResult.floatConst = -operand.floatConst : opResult.longConst = -operand.longConst; } // prefix minus 
    else if (terminalCode == termcod_plus) { opResult = operand; } // prefix plus
    else if (terminalCode == termcod_not) { opResult.longConst = opIsFloat ? (operand.floatConst == 0.) : (operand.longConst == 0); } // prefix: not
    else if (terminalCode == termcod_incr) { opIsFloat ? opResult.floatConst = operand.floatConst + 1. : opResult.longConst = operand.longConst + 1; } // prefix & postfix: increment
    else if (terminalCode == termcod_decr) { opIsFloat ? opResult.floatConst = operand.floatConst - 1. : opResult.longConst = operand.longConst - 1; } // prefix & postfix: decrement
    else if (terminalCode == termcod_bitCompl) { opResult.longConst = ~operand.longConst; } // prefix: bit complement


    // float values: extra value tests

    int resultValueType = resultCastLong ? value_isLong : opValueType;

    if (resultValueType == value_isFloat) {      // floats only
        if (isnan(opResult.floatConst)) { return result_undefined; }
        else if (!isfinite(opResult.floatConst)) { return result_overflow; }
    }


    // (5) post process
    // ----------------

    // decrement or increment operation: store value in variable (variable type does not change) 

    bool isIncrDecr = ((terminalCode == termcod_incr) || (terminalCode == termcod_decr));
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

Justina_interpreter::execResult_type  Justina_interpreter::execInfixOperation() {

    Val operand1, operand2, opResult;                                                               // operands and result

    _activeFunctionData.errorProgramCounter = _pEvalStackMinus1->terminal.tokenAddress;                // in the event of an error


    // (1) Fetch operator info, whether operands are variables, and operand value types 
    // --------------------------------------------------------------------------------

    // operator
    int operatorCode = _terminals[_pEvalStackMinus1->terminal.index & 0x7F].terminalCode;
    bool operationIncludesAssignment = ((_terminals[_pEvalStackMinus1->terminal.index & 0x7F].infix_priority & 0x1F) == 0x01);
    bool requiresLongOp = (_terminals[_pEvalStackMinus1->terminal.index & 0x7F].infix_priority & op_long);
    bool resultCastLong = (_terminals[_pEvalStackMinus1->terminal.index & 0x7F].infix_priority & res_long);

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
    if (operatorCode == termcod_assign) { if ((op1isString != op2isString) && (_pEvalStackMinus2->varOrConst.variableAttributes & var_isArray)) { return result_array_valueTypeIsFixed; } }
    else if (((operatorCode == termcod_plus) || (operatorCode == termcod_plusAssign))) { if (op1isString != op2isString) { return result_operandsNumOrStringExpected; } }
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
    if (operatorCode == termcod_assign) {}           // pure assignment: no action 
    else if (operatorCode == termcod_pow) { promoteOperandsToFloat = op1isLong || op2isLong; }
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

        case termcod_assign:
            opResult = operand2;
            break;

            // note: no overflow checks for arithmatic operators (+ - * /)

        case termcod_plus:            // also for concatenation
        case termcod_plusAssign:
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
                    _intermediateStringObjectCount++;
                    opResult.pStringConst[0] = '\0';                                // in case first operand is nullptr
                    if (!op1emptyString) { strcpy(opResult.pStringConst, operand1.pStringConst); }
                    if (!op2emptyString) { strcat(opResult.pStringConst, operand2.pStringConst); }

                #if printCreateDeleteListHeapObjects
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)opResult.pStringConst - RAMSTART);
                #endif
                }
            }

            else {
                opResultLong ? opResult.longConst = operand1.longConst + operand2.longConst : opResult.floatConst = operand1.floatConst + operand2.floatConst;
            }
            break;

        case termcod_minus:
        case termcod_minusAssign:
            opResultLong ? opResult.longConst = operand1.longConst - operand2.longConst : opResult.floatConst = operand1.floatConst - operand2.floatConst;
            break;

        case termcod_mult:
        case termcod_multAssign:
            opResultLong ? opResult.longConst = operand1.longConst * operand2.longConst : opResult.floatConst = operand1.floatConst * operand2.floatConst;
            if (opResultFloat) { if ((operand1.floatConst != 0) && (operand2.floatConst != 0) && (!isnormal(opResult.floatConst))) { return result_underflow; } }
            break;

        case termcod_div:
        case termcod_divAssign:
            if (opResultFloat) { if ((operand1.floatConst != 0) && (operand2.floatConst == 0)) { return result_divByZero; } }
            else { if (operand2.longConst == 0) { return (operand1.longConst == 0) ? result_undefined : result_divByZero; } }
            opResultLong ? opResult.longConst = operand1.longConst / operand2.longConst : opResult.floatConst = operand1.floatConst / operand2.floatConst;
            if (opResultFloat) { if ((operand1.floatConst != 0) && (!isnormal(opResult.floatConst))) { return result_underflow; } }
            break;

        case termcod_mod:
        case termcod_modAssign:
            if (operand2.longConst == 0) { return (operand1.longConst == 0) ? result_undefined : result_divByZero; }
            opResult.longConst = operand1.longConst % operand2.longConst;
            break;

        case termcod_bitAnd:
        case termcod_bitAndAssign:
            opResult.longConst = operand1.longConst & operand2.longConst;
            break;

        case termcod_bitOr:
        case termcod_bitOrAssign:
            opResult.longConst = operand1.longConst | operand2.longConst;
            break;

        case termcod_bitXor:
        case termcod_bitXorAssign:
            opResult.longConst = operand1.longConst ^ operand2.longConst;
            break;

        case termcod_bitShLeft:
        case termcod_bitShLeftAssign:
            if ((operand2.longConst < 0) || (operand2.longConst >= 8 * sizeof(long))) { return result_outsideRange; }
            opResult.longConst = operand1.longConst << operand2.longConst;
            break;

        case termcod_bitShRight:
        case termcod_bitShRightAssign:
            if ((operand2.longConst < 0) || (operand2.longConst >= 8 * sizeof(long))) { return result_outsideRange; }
            opResult.longConst = operand1.longConst >> operand2.longConst;
            break;

        case termcod_pow:     // operands always (converted to) floats
            if ((operand1.floatConst == 0) && (operand2.floatConst == 0)) { return result_undefined; } // C++ pow() provides 1 as result
            opResult.floatConst = pow(operand1.floatConst, operand2.floatConst);
            break;

        case termcod_and:
            opResult.longConst = opResultLong ? (operand1.longConst && operand2.longConst) : (operand1.floatConst && operand2.floatConst);
            break;

        case termcod_or:
            opResult.longConst = opResultLong ? (operand1.longConst || operand2.longConst) : (operand1.floatConst || operand2.floatConst);
            break;

        case termcod_lt:
            opResult.longConst = opResultLong ? (operand1.longConst < operand2.longConst) : (operand1.floatConst < operand2.floatConst);
            break;

        case termcod_gt:
            opResult.longConst = opResultLong ? (operand1.longConst > operand2.longConst) : (operand1.floatConst > operand2.floatConst);
            break;

        case termcod_eq:
            opResult.longConst = opResultLong ? (operand1.longConst == operand2.longConst) : (operand1.floatConst == operand2.floatConst);
            break;

        case termcod_ltoe:
            opResult.longConst = opResultLong ? (operand1.longConst <= operand2.longConst) : (operand1.floatConst <= operand2.floatConst);
            break;

        case termcod_gtoe:
            opResult.longConst = opResultLong ? (operand1.longConst >= operand2.longConst) : (operand1.floatConst >= operand2.floatConst);
            break;

        case termcod_ne:
            opResult.longConst = opResultLong ? (operand1.longConst != operand2.longConst) : (operand1.floatConst != operand2.floatConst);
            break;
                }       // switch


                // float values: extra value tests

    if ((opResultFloat) && (operatorCode != termcod_assign)) {     // check error (float values only, not for pure assignment)
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
            int stringlen = min(strlen(pUnclippedResultString), MAX_ALPHA_CONST_LEN);
            opResult.pStringConst = new char[stringlen + 1];
            (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;
            memcpy(opResult.pStringConst, pUnclippedResultString, stringlen);        // copy the actual string (not the pointer); do not use strcpy
            opResult.pStringConst[stringlen] = '\0';                                         // add terminating \0
        #if printCreateDeleteListHeapObjects
            Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
            Serial.println((uint32_t)opResult.pStringConst - RAMSTART);
        #endif
            if (operatorCode != termcod_assign) {           // compound statement
            #if printCreateDeleteListHeapObjects
                Serial.print("----- (Intermd str) "); Serial.println("????"); //// CORRIGEER FOUT: Serial.println((uint32_t)toPrint.pStringConst - RAMSTART);
            #endif
                if (pUnclippedResultString != nullptr) {     // pure assignment: is in fact pointing to operand 2 
                    delete[] pUnclippedResultString;
                    _intermediateStringObjectCount--;
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

#if debugPrint
    Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.println(" - infix operation done");
    Serial.print("                 result = "); Serial.println(_pEvalStackTop->varOrConst.value.longConst);

    Serial.print("    eval stack depth: "); Serial.print(evalStack.getElementCount()); Serial.print(", list element address: "); Serial.println((uint32_t)_pEvalStackTop - sizeof(LinkedList::ListElemHead) - RAMSTART);
    if (opResultLong) { Serial.print("    'op': stack top long value "); Serial.println(_pEvalStackTop->varOrConst.value.longConst); }
    else if (opResultFloat) { Serial.print("    'op': stack top float value "); Serial.println(_pEvalStackTop->varOrConst.value.floatConst); }
    else { Serial.print("    'op': stack top string value "); Serial.println(_pEvalStackTop->varOrConst.value.pStringConst); }
#endif

    return result_execOK;
}


// ---------------------------------
// *   execute internal function   *
// ---------------------------------

Justina_interpreter::execResult_type Justina_interpreter::execInternalFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount) {

    // remember token address of internal function token (this where the internal function is called), in case an error occurs (while passing arguments etc.)   
    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;
    int functionIndex = pFunctionStackLvl->function.index;
    char functionCode = _functions[functionIndex].functionCode;
    int arrayPattern = _functions[functionIndex].arrayPattern;
    int minArgs = _functions[functionIndex].minArgs;
    int maxArgs = _functions[functionIndex].maxArgs;
    char fcnResultValueType = value_noValue;  // init
    Val fcnResult;
    bool argIsVar[16], argIsLong[16], argIsFloat[16];
    char argValueType[16];
    Val args[16];


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

        // evaluatie expression contained within quotes
        // --------------------------------------------

        case fnccod_eval:
            //// \s en \a tijdens eval ?
        {
            // only one argument possible (eval() string)
            if (argIsLong[0] || argIsFloat[0]) { return result_stringExpected; }
            char resultValueType;
            execResult_type execResult = launchEval(pFunctionStackLvl, args[0].pStringConst);
            if (execResult != result_execOK) { return execResult; }
            // an 'EXTERNAL function' (executing the parsed eval() expressions) has just been 'launched' (and will start after current (right parenthesis) token is processed)
            // because eval function name token and single argument will be removed from stack now (see below, at end of this function), adapt CALLER evaluation stack levels
            _activeFunctionData.callerEvalStackLevels -= 2;
        }
        break;


        // switch function
        // ---------------

        case fnccod_switch:
        {
            //// test 
            fcnResultValueType = value_isFloat;
            fcnResult.floatConst = 1.23;
        }
        break;


        // square root
        // -----------

        case fnccod_sqrt:
        {
            if (!argIsLong[0] && !argIsFloat[0]) { return result_numberExpected; }
            if (argIsLong[0] ? args[0].longConst < 0 : args[0].floatConst < 0.) { return result_arg_outsideRange; }

            fcnResultValueType = value_isFloat;
            fcnResult.floatConst = argIsLong[0] ? sqrt(args[0].longConst) : sqrt(args[0].floatConst);
        }
        break;


        // dimension count of an array
        // ---------------------------

        case fnccod_dims:
        {
            void* pArray = *pFirstArgStackLvl->varOrConst.value.ppArray;

            fcnResultValueType = value_isLong;
            fcnResult.longConst = ((char*)pArray)[3];
        }
        break;


        // array upper bound
        // -----------------
        case fnccod_ubound:
        {
            if (!argIsLong[1] && !argIsFloat[1]) { return result_arg_dimNumberIntegerExpected; }
            void* pArray = *pFirstArgStackLvl->varOrConst.value.ppArray;
            int arrayDimCount = ((char*)pArray)[3];
            int dimNo = argIsLong[1] ? args[1].longConst : int(args[1].floatConst);
            if (argIsFloat[1]) { if (args[1].floatConst != dimNo) { return result_arg_dimNumberIntegerExpected; } }
            if ((dimNo < 1) || (dimNo > arrayDimCount)) { return result_arg_dimNumberInvalid; }

            fcnResultValueType = value_isLong;
            fcnResult.longConst = ((char*)pArray)[--dimNo];
        }
        break;


        // variable value type
        // -------------------

        case fnccod_valueType:
        {
            // note: to obtain the value type of an array, check the value type of one of its elements
            fcnResultValueType = value_isLong;
            fcnResult.longConst = argValueType[0];
        }
        break;


        // retrieve one of the last calculation results
        // --------------------------------------------

        case fnccod_last:
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

            fcnResultValueType = lastResultTypeFiFo[FiFoElement];
            bool fcnResultIsLong = (lastResultTypeFiFo[FiFoElement] == value_isLong);
            bool fcnResultIsFloat = (lastResultTypeFiFo[FiFoElement] == value_isFloat);
            if (fcnResultIsLong || fcnResultIsFloat || (!fcnResultIsLong && !fcnResultIsFloat && (lastResultValueFiFo[FiFoElement].pStringConst == nullptr))) {
                fcnResult = lastResultValueFiFo[FiFoElement];
            }
            else {                              // string
                fcnResult.pStringConst = new char[strlen(lastResultValueFiFo[FiFoElement].pStringConst + 1)];
                _intermediateStringObjectCount++;
                strcpy(fcnResult.pStringConst, lastResultValueFiFo[FiFoElement].pStringConst);
            #if printCreateDeleteListHeapObjects
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
            #endif            
            }

        }
        break;


        // time since boot, in milliseconds
        // --------------------------------

        case fnccod_millis:
        {
            fcnResultValueType = value_isLong;
            fcnResult.longConst = millis();
        }
        break;


        // ASCII code of a single character in a string
        // -------------------------------------------

        case fnccod_asc:
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

            fcnResultValueType = value_isLong;
            fcnResult.longConst = args[0].pStringConst[--charPos];     // character code
        }
        break;


        // return character with a given ASCII code
        // ----------------------------------------

        case fnccod_char:     // convert ASCII code to 1-character string
        {
            if (!argIsLong[0] && !argIsFloat[0]) { return result_arg_integerExpected; }
            int asciiCode = argIsLong[0] ? args[0].longConst : int(args[0].floatConst);
            if (argIsFloat[0]) { if (args[0].floatConst != asciiCode) { return result_arg_integerExpected; } }
            if ((asciiCode < 0) || (asciiCode > 0x7F)) { return result_arg_outsideRange; }

            // result is string
            fcnResultValueType = value_isStringPointer;
            fcnResult.pStringConst = new char[2];
            _intermediateStringObjectCount++;
            fcnResult.pStringConst[0] = asciiCode;
            fcnResult.pStringConst[1] = '\0';                                // terminating \0
        #if printCreateDeleteListHeapObjects
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
        #endif            
        }
        break;


        case fnccod_len:     // return length of a string
        {
            if (argIsLong[0] || argIsFloat[0]) { return result_arg_stringExpected; }
            fcnResult.longConst = 0;      // init
            if (args[0].pStringConst != nullptr) { fcnResult.longConst = strlen(args[0].pStringConst); }
            fcnResultValueType = value_isLong;
        }
        break;


        // return CR and LF character string
        // ---------------------------------

        case fnccod_nl:             // new line character
        {
            // result is string
            fcnResultValueType = value_isStringPointer;
            fcnResult.pStringConst = new char[3];
            _intermediateStringObjectCount++;
            fcnResult.pStringConst[0] = '\r';
            fcnResult.pStringConst[1] = '\n';
            fcnResult.pStringConst[2] = '\0';                                // terminating \0
        }
    #if printCreateDeleteListHeapObjects
        Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
    #endif            
        break;


        // format a number or a string into a destination string
        // -----------------------------------------------------

        case fnccod_format:
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
            fcnResultValueType = value_isStringPointer;

            // return number of characters printed into (variable) argument if it was supplied
            // -------------------------------------------------------------------------------

            // note: NO errors should occur beyond this point, OR the intermediate string containing the function result should be deleted
            if (suppliedArgCount == (hasSpecifierArg ? 6 : 5)) {      // optional argument returning #chars that were printed is present
                // if  variable currently holds a non-empty string (indicated by a nullptr), delete char string object
                execResult_type execResult = deleteVarStringObject(_pEvalStackTop); if (execResult != result_execOK) { return execResult; }

                // save value in variable and set variable value type to real 
                // note: if variable reference, then value type on the stack indicates 'variable reference' which should not be changed (but stack level will be deleted now anyway)
                *_pEvalStackTop->varOrConst.value.pFloatConst = charsPrinted;
                *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isFloat;
            }
        }
        break;


        // retrieve a system variable
        // --------------------------

        case fnccod_sysVal:
        {
            if (!argIsLong[0] && !argIsFloat[0]) { return result_arg_integerExpected; }
            int sysVal = argIsLong[0] ? args[0].longConst : int(args[0].floatConst);
            if (argIsFloat[0]) { if (args[0].floatConst != sysVal) { return result_arg_integerExpected; } }

            fcnResultValueType = value_isLong;      // default for most system values

            switch (sysVal) {

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
                    fcnResultValueType = value_isStringPointer;
                    fcnResult.pStringConst = new char[2];
                    _intermediateStringObjectCount++;
                    strcpy(fcnResult.pStringConst, (sysVal == 4) ? _dispNumSpecifier : _printNumSpecifier);
                #if printCreateDeleteListHeapObjects
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
                    fcnResultValueType = value_isStringPointer;
                    fcnResult.pStringConst = new char[MAX_IDENT_NAME_LEN + 1];
                    _intermediateStringObjectCount++;
                    strcpy(fcnResult.pStringConst, _programName);
                #if printCreateDeleteListHeapObjects
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
                #endif
                }
                break;

                case 15:    // product name
                case 16:    // legal copy right
                case 17:    // product version 
                case 18:    // build date
                {
                    fcnResultValueType = value_isStringPointer;
                    fcnResult.pStringConst = new char[((sysVal == 15) ? strlen(ProductName) : (sysVal == 16) ? strlen(LegalCopyright) : (sysVal == 17) ? strlen(ProductVersion) : strlen(BuildDate)) + 1];
                    _intermediateStringObjectCount++;
                    strcpy(fcnResult.pStringConst, (sysVal == 15) ? ProductName : (sysVal == 16) ? LegalCopyright : (sysVal == 17) ? ProductVersion : BuildDate);
                #if printCreateDeleteListHeapObjects
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
                #endif
                }
                break;

                // note:parsing stack element count is always zero during evaluation: no entry provided here
                case 19:fcnResult.longConst = _callStackDepth; break;                       // call stack depth; this excludes stack levels used by blocks (while, if, ...)
                case 20:fcnResult.longConst = _openDebugLevels; break;
                case 21:fcnResult.longConst = evalStack.getElementCount(); break;           // evaluation stack element count
                case 22:fcnResult.longConst = flowCtrlStack.getElementCount(); break;       // flow control stack element count (call stack depth + stack levels used by open blocks)
                case 23:fcnResult.longConst = immModeCommandStack.getElementCount(); break; // immediate mode parsed programs stack element count

                case 24:                                                                    // current object count
                case 1001:                                                                  // current accumulated object count errors since cold start
                {
                    fcnResultValueType = value_isStringPointer;
                    fcnResult.pStringConst = new char[13 * 5];  // includes place for 13 times 5 characters (3 digits max. for each number, max. 2 extra in between) and terminating \0
                    _intermediateStringObjectCount++;
                #if printCreateDeleteListHeapObjects
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
                #endif
                    if (sysVal == 24) {     // print heap object counts
                        // (1)program variable and function NAMES-(2)user variable NAMES-(3)parsed string constants-(4)last value strings-
                        // (5)global and static variable strings-(6)global and static array storage areas-(7)user variable strings-(8)user array storage areas-
                        // (9)local variable strings-(10)local array storage areas-(11)local variable base value areas-(12)intermediate string constants
                        sprintf(fcnResult.pStringConst, "%0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d",
                            min(999, _identifierNameStringObjectCount), min(999, _userVarNameStringObjectCount), min(999, _parsedStringConstObjectCount), min(999, _lastValuesStringObjectCount),
                            min(999, _globalStaticVarStringObjectCount), min(999, _globalStaticArrayObjectCount), min(999, _userVarStringObjectCount), min(999, _userArrayObjectCount),
                            min(999, _localVarStringObjectCount), min(999, _localArrayObjectCount), min(999, _localVarValueAreaCount), min(999, _intermediateStringObjectCount),
                            min(999, _systemVarStringObjectCount));
                }
                    else {     // print heap object create/delete errors
                        sprintf(fcnResult.pStringConst, "%0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d",
                            min(999, _identifierNameStringObjectErrors), min(999, _userVarNameStringObjectErrors), min(999, _parsedStringConstObjectErrors), min(999, _lastValuesStringObjectErrors),
                            min(999, _globalStaticVarStringObjectErrors), min(999, _globalStaticArrayObjectErrors), min(999, _userVarStringObjectErrors), min(999, _userArrayObjectErrors),
                            min(999, _localVarStringObjectErrors), min(999, _localArrayObjectErrors), min(999, _localVarValueAreaErrors), min(999, _intermediateStringObjectErrors),
                            min(999, _systemVarStringObjectErrors));
                    }
                }
                break;

                case 25:                                                                    // trace string
                {
                    fcnResultValueType = value_isStringPointer;
                    fcnResult.pStringConst = nullptr;                                       // init (empty string)
                    if (_pTraceString != nullptr) {
                        fcnResult.pStringConst = new char[strlen(_pTraceString) + 1];
                        _intermediateStringObjectCount++;
                        strcpy(fcnResult.pStringConst, _pTraceString);
                    #if printCreateDeleteListHeapObjects
                        Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
                    #endif
                    }
                }
                break;

                default: return result_arg_invalid; break;
                    }       // switch (sysVal)
                }
        break;

                }       // end switch


                // postprocess: delete function name token and arguments from evaluation stack, create stack entry for function result 
                // -------------------------------------------------------------------------------------------------------------------

    clearEvalStackLevels(suppliedArgCount + 1);

    if (functionCode != fnccod_eval) {    // Note: function eval() (only) does not yet have a function result: the eval() string has been parsed but execution is yet to start 

        // push result to stack
        // --------------------

        _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
        _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
        _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

        _pEvalStackTop->varOrConst.value = fcnResult;                           // long, float or pointer to string
        _pEvalStackTop->varOrConst.valueType = fcnResultValueType;              // value type of second operand  
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;                  // use generic constant type
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
        _pEvalStackTop->varOrConst.variableAttributes = 0x00;                   // not an array, not an array element (it's a constant) 
    }

    return result_execOK;
                }


// -----------------------
// check format specifiers
// -----------------------

Justina_interpreter::execResult_type Justina_interpreter::checkFmtSpecifiers(bool isDispFmt, bool valueIsString, int suppliedArgCount, char* valueType, Val* operands, char& numSpecifier,
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
            if (pChar == nullptr) { return result_arg_invalid; }
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

    width = min(width, MAX_PRINT_WIDTH);            // limit width to MAX_PRINT_WIDTH
    precision = min(precision, valueIsString ? MAX_STRCHAR_TO_PRINT : MAX_NUM_PRECISION);
    flags &= 0b11111;       // apply mask
    return result_execOK;
}


// ----------------------
// create a format string
// ----------------------


void  Justina_interpreter::makeFormatString(int flags, bool isIntFmt, char* numFmt, char* fmtString) {

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

void  Justina_interpreter::printToString(int width, int precision, bool inputIsString, bool isIntFmt, char* valueType, Val* value, char* fmtString,
    Val& fcnResult, int& charsPrinted) {

    int opStrLen{ 0 }, resultStrLen{ 0 };
    if (inputIsString) {
        if ((*value).pStringConst != nullptr) {
            opStrLen = strlen((*value).pStringConst);
            if (opStrLen > MAX_PRINT_WIDTH) { (*value).pStringConst[MAX_PRINT_WIDTH] = '\0'; opStrLen = MAX_PRINT_WIDTH; }   // clip input string without warning (won't need it any more)
        }
        resultStrLen = max(width + 10, opStrLen + 10);  // allow for a few extra formatting characters, if any
    }
    else {
        resultStrLen = max(width + 10, 30);         // 30: ensure length is sufficient to print a formatted nummber
    }

    fcnResult.pStringConst = new char[resultStrLen];
    _intermediateStringObjectCount++;

#if printCreateDeleteListHeapObjects
    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst - RAMSTART);
#endif

    if (inputIsString) { sprintf(fcnResult.pStringConst, fmtString, width, precision, ((*value).pStringConst == nullptr) ? "" : (*value).pStringConst, &charsPrinted); }
    else if (isIntFmt) { sprintf(fcnResult.pStringConst, fmtString, width, precision, (*valueType == value_isLong) ? (*value).longConst : (long)(*value).floatConst, &charsPrinted); }     // hex output for floating point numbers not provided (Arduino)
    else { sprintf(fcnResult.pStringConst, fmtString, width, precision, (*valueType == value_isLong) ? (float)(*value).longConst : (*value).floatConst, &charsPrinted); }

    return;
}


// -------------------------------
// delete a variable string object
// -------------------------------

// if not a string, then do nothing. If not a variable, then exit WITH error

Justina_interpreter::execResult_type Justina_interpreter::deleteVarStringObject(LE_evalStack* pStackLvl) {

    if (pStackLvl->varOrConst.tokenType != tok_isVariable) { return result_arg_varExpected; };                            // not a variable
    if ((*pStackLvl->varOrConst.varTypeAddress & value_typeMask) != value_isStringPointer) { return result_execOK; }      // not a string object
    if (*pStackLvl->varOrConst.value.ppStringConst == nullptr) { return result_execOK; }

    char varScope = (pStackLvl->varOrConst.variableAttributes & var_scopeMask);

    // delete variable string object
#if printCreateDeleteListHeapObjects
    Serial.print((varScope == var_isUser) ? "----- (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
    Serial.println((uint32_t)*_pEvalStackMinus2->varOrConst.value.ppStringConst - RAMSTART);
#endif
    delete[] * pStackLvl->varOrConst.value.ppStringConst;
    (varScope == var_isUser) ? _userVarStringObjectCount-- : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount-- : _localVarStringObjectCount--;
    return result_execOK;
}


// ------------------------------------
// delete an intermediate string object
// ------------------------------------

// if not a string, then do nothing. If not an intermediate string object, then exit WITHOUT error

Justina_interpreter::execResult_type Justina_interpreter::deleteIntermStringObject(LE_evalStack* pStackLvl) {

    if ((pStackLvl->varOrConst.valueAttributes & constIsIntermediate) != constIsIntermediate) { return result_execOK; }       // not an intermediate constant
    if (pStackLvl->varOrConst.valueType != value_isStringPointer) { return result_execOK; }                                   // not a string object
    if (pStackLvl->varOrConst.value.pStringConst == nullptr) { return result_execOK; }
#if printCreateDeleteListHeapObjects
    Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)_pEvalStackTop->varOrConst.value.pStringConst - RAMSTART);
#endif
    delete[] pStackLvl->varOrConst.value.pStringConst;
    _intermediateStringObjectCount--;

    return result_execOK;
}

// ---------------------------------------------------------------------------
// copy command arguments or internal function arguments from evaluation stack
// ---------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::copyValueArgsFromStack(LE_evalStack*& pStackLvl, int argCount, bool* argIsVar, bool* argIsArray, char* valueType, Val* args, bool prepareForCallback) {
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
                    _intermediateStringObjectCount++;
                    if (strLength == 0) { args[i].pStringConst[0] = '\0'; }                                 // empty strings ("" -> no null pointer)
                    else { strcpy(args[i].pStringConst, pStackLvl->varOrConst.value.pStringConst); }        // non-empty constant string
                #if printCreateDeleteListHeapObjects
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

Justina_interpreter::execResult_type  Justina_interpreter::launchExternalFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount) {

    // remember token address of internal function token (this is where the internal function is called), in case an error occurs (while passing arguments etc.)   
    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;


    // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
    // ----------------------------------------------------------------------------------------------

    _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
    _pFlowCtrlStackTop = (OpenFunctionData*)flowCtrlStack.appendListElement(sizeof(OpenFunctionData));
    *((OpenFunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                // push caller function data to stack
    ++_callStackDepth;                                                              // caller can be main, another external function or an eval() string

    _activeFunctionData.functionIndex = pFunctionStackLvl->function.index;     // index of external function to call
    _activeFunctionData.blockType = block_extFunction;
    _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended      // nodig ??


    // create local variable storage for external function to be called
    // ----------------------------------------------------------------

    int localVarCount = extFunctionData[_activeFunctionData.functionIndex].localVarCountInFunction;
    int paramCount = extFunctionData[_activeFunctionData.functionIndex].paramOnlyCountInFunction;

    if (localVarCount > 0) {
        _activeFunctionData.pLocalVarValues = new Val[localVarCount];              // local variable value: real, pointer to string or array, or (if reference): pointer to 'source' (referenced) variable
        _activeFunctionData.ppSourceVarTypes = new char* [localVarCount];           // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        _activeFunctionData.pVariableAttributes = new char[localVarCount];         // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) 
        _localVarValueAreaCount++;

    #if printCreateDeleteListHeapObjects
        Serial.print("+++++ (LOCAL STORAGE) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues - RAMSTART);
    #endif
    }


    // init local variables: parameters with supplied arguments (scalar and array var refs) and with default values (scalars only), local variables (scalar and array)
    // ---------------------------------------------------------------------------------------------------------------------------------------------------------------

    initFunctionParamVarWithSuppliedArg(suppliedArgCount, pFirstArgStackLvl);
    char* calledFunctionTokenStep = extFunctionData[_activeFunctionData.functionIndex].pExtFunctionStartToken;
    initFunctionDefaultParamVariables(calledFunctionTokenStep, suppliedArgCount, paramCount);      // return with first token after function definition...
    initFunctionLocalNonParamVariables(calledFunctionTokenStep, paramCount, localVarCount);         // ...and create storage for local array variables


    // delete function name token from evaluation stack
    // ------------------------------------------------
    _pEvalStackTop = (LE_evalStack*)evalStack.getPrevListElement(pFunctionStackLvl);
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);
    evalStack.deleteListElement(pFunctionStackLvl);

    _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                          // store evaluation stack levels in use by callers (call stack)


    // set next step to start of called function
    // -----------------------------------------

    _activeFunctionData.pNextStep = calledFunctionTokenStep;                     // first step in first statement in called function
    _activeFunctionData.errorStatementStartStep = calledFunctionTokenStep;
    _activeFunctionData.errorProgramCounter = calledFunctionTokenStep;

    return  result_execOK;
    }


// ------------------------------------
// launch execution of an eval() string
// ------------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::launchEval(LE_evalStack*& pFunctionStackLvl, char* parsingInput) {

    execResult_type execResult{ result_execOK };

    if (parsingInput == nullptr) { return result_eval_nothingToEvaluate; }


    // push current command line storage to command line stack, to make room for the evaluation string (to parse) 
    // ----------------------------------------------------------------------------------------------------------

    // the parsed command line pushed, contains the parsed statements 'calling' (parsing and executing) the eval() string 
    // this is either an outer level parsed eval() string, or the parsed command line where execution started  

    Serial.print("  >> PUSH (launch eval): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + PROG_MEM_SIZE));
    long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + PROG_MEM_SIZE) + 1;
    _pImmediateCmdStackTop = (char*)immModeCommandStack.appendListElement(sizeof(char*) + parsedUserCmdLen);
    *(char**)_pImmediateCmdStackTop = _lastUserCmdStep;
    _lastUserCmdStep = nullptr;
    memcpy(_pImmediateCmdStackTop + sizeof(char*), (_programStorage + PROG_MEM_SIZE), parsedUserCmdLen);


    // parse eval() string
    // -------------------
    char* pDummy{};
    char* holdProgramCounter = _programCounter;
    _programCounter = _programStorage + PROG_MEM_SIZE;                                  // parsed statements go to immediate mode program memory
    _parsingEvalString = true;

    // create a temporary string to hold expressions to parse, with an extra semicolon added at the end (in case it's missing)
    char* pEvalParsingInput = new char[strlen(parsingInput) + 2]; // room for additional semicolon (in case string is not ending with it) and terminating '\0'
    _intermediateStringObjectCount++;
    strcpy(pEvalParsingInput, parsingInput);              // copy the actual string

    pEvalParsingInput[strlen(parsingInput)] = term_semicolon[0];
    pEvalParsingInput[strlen(parsingInput) + 1] = '\0';
#if printCreateDeleteListHeapObjects
    Serial.print("+++++ (system var str) "); Serial.println((uint32_t)pEvalParsingInput - RAMSTART);
#endif
    char* pParsingInput_temp = pEvalParsingInput;        // temp, because value will be changed upon return (preserve original pointer value)
    parseTokenResult_type result = parseStatements(pParsingInput_temp, pDummy);           // parse all eval() expressions in ONE go (which is not the case for standard parsing and trace string parsing)
    delete[] pEvalParsingInput;
    _intermediateStringObjectCount--;
#if printCreateDeleteListHeapObjects
    Serial.print("----- (system var str) "); Serial.println((uint32_t)pEvalParsingInput - RAMSTART);
#endif

    // last step of just parsed eval() string. Note: adding sizeof(tok_no_token) because not yet added
    _lastUserCmdStep = ((result == result_tokenFound) ? _programCounter + sizeof(tok_no_token) : nullptr);    // if parsing error, store nullptr as last token position

    _parsingEvalString = false;
    if (result != result_tokenFound) {
        // remove imm.mode parsed statement stack top level again because corresponding entry in flow ctrl stack will not be created
        // before removing, delete any parsed string constants for that command line. Note the exception below:

        Serial.print("<eval parse error> block type = "); Serial.println((int)((OpenFunctionData*)_pFlowCtrlStackTop)->blockType);
        if (((OpenFunctionData*)_pFlowCtrlStackTop)->blockType == block_eval) {deleteConstStringObjects(_programStorage + PROG_MEM_SIZE);}

        // reverse push to stack that just happened
        deleteConstStringObjects(_programStorage + PROG_MEM_SIZE);      // string constants that were created just now 
        memcpy((_programStorage + PROG_MEM_SIZE), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);
        immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
        _pImmediateCmdStackTop = (char*)immModeCommandStack.getLastListElement();
        Serial.print("  >> POP (launch eval parse error): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + PROG_MEM_SIZE));


        _evalParseErrorCode = result;       // remember
        return result_eval_parsingError;
    }

    *(_programCounter) = tok_isEvalEnd | 0x10;       // replace '\0' after parsed statements with 'end eval ()' token (length 1 in upper 4 bits)
    *(_programCounter + 1) = tok_no_token;
    _programCounter = holdProgramCounter;           // original program counter (points to closing par. of eval() function)



    // note current token (internal function'eval' token) position, in case an error happens IN THE CALLER immediately upon return from function to be called
    // ---------------------------------------------------------------------------------------------------------------------------------------

    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;  // CALLER function 'eval' token position, before pushing caller function data to stack   


    // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
    // ----------------------------------------------------------------------------------------------

    _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
    _pFlowCtrlStackTop = (OpenFunctionData*)flowCtrlStack.appendListElement(sizeof(OpenFunctionData));
    *((OpenFunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                // push caller function data to stack
    ++_callStackDepth;                                                                  // caller can be main, another external function or an eval() string

    _activeFunctionData.functionIndex = pFunctionStackLvl->function.index;     // index of (internal) eval() function - but will not be used
    _activeFunctionData.blockType = block_eval;                                 // now executing parsed 'eval' string
    _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                    // command execution ended 

    _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                          // store evaluation stack levels in use by callers (call stack)

    // set next step to start of called function
    // -----------------------------------------

    _activeFunctionData.pNextStep = _programStorage + PROG_MEM_SIZE;                     // first step in first statement in parsed eval() string
    _activeFunctionData.errorStatementStartStep = _programStorage + PROG_MEM_SIZE;
    _activeFunctionData.errorProgramCounter = _programStorage + PROG_MEM_SIZE;

    return  result_execOK;
}


// ------------------------------------------------------------------------------------------------
// *   init parameter variables with supplied arguments (scalar parameters with default values)   *
// ------------------------------------------------------------------------------------------------

void Justina_interpreter::initFunctionParamVarWithSuppliedArg(int suppliedArgCount, LE_evalStack*& pFirstArgStackLvl) {
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
                    (pStackLvl->varOrConst.variableAttributes & (var_scopeMask | var_isArray));             // ... and SOURCE variable scope (user, global, static; local, param), array flag
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
                        _localVarStringObjectCount++;
                        strcpy(_activeFunctionData.pLocalVarValues[i].pStringConst, pStackLvl->varOrConst.value.pStringConst);
                    #if printCreateDeleteListHeapObjects
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


// ------------------------------------------------------------------------------------------------------------
// *   init function parameter variables for non_supplied arguments (scalar parameters with default values)   *
// ------------------------------------------------------------------------------------------------------------

void Justina_interpreter::initFunctionDefaultParamVariables(char*& pStep, int suppliedArgCount, int paramCount) {
    int tokenType = *pStep & 0x0F;                                                          // function name token of called function

    if (suppliedArgCount < paramCount) {      // missing arguments: use parameter default values to init local variables
        int count = 0, terminalCode = 0;
        tokenType = jumpTokens(1, pStep);
        // now positioned at opening parenthesis in called function (after FUNCTION token)
        // find n-th argument separator (comma), with n is number of supplied arguments (stay at left parenthesis if none provided)
        while (count < suppliedArgCount) { tokenType = findTokenStep(tok_isTerminalGroup1, termcod_comma, pStep); count++; }

        // now positioned before first parameter for non-supplied scalar argument. It always has an initializer
        // we only need the constant value, because we know the variable value index already (count): skip variable and assignment 
        while (count < paramCount) {
            tokenType = jumpTokens(((count == suppliedArgCount) ? 3 : 4), pStep);

            // now positioned at constant initializer
            char valueType = ((*(char*)pStep) >> 4) & value_typeMask;
            bool operandIsLong = (valueType == value_isLong);
            bool operandIsFloat = (valueType == value_isFloat);

            _activeFunctionData.pVariableAttributes[count] = valueType;                // long, float or string (array flag is reset here)

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
                    _localVarStringObjectCount++;
                    strcpy(_activeFunctionData.pLocalVarValues[count].pStringConst, s);
                #if printCreateDeleteListHeapObjects
                    Serial.print("+++++ (loc var str) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues[count].pStringConst - RAMSTART);
                #endif
                }
            }
            count++;
        }
            }

    // skip (remainder of) function definition
    findTokenStep(tok_isTerminalGroup1, termcod_semicolon, pStep);
        };



// --------------------------------------------
// *   init local variables (non-parameter)   *
// --------------------------------------------

void Justina_interpreter::initFunctionLocalNonParamVariables(char* pStep, int paramCount, int localVarCount) {
    // upon entry, positioned at first token after FUNCTION statement

    int tokenType{}, terminalCode{};

    int count = paramCount;         // sum of mandatory and optional parameters

    while (count != localVarCount) {
        findTokenStep(tok_isReservedWord, cmdcod_local, pStep);     // find 'LOCAL' keyword (always there)

        do {
            // in case variable is not an array and it does not have an initializer: init now as zero (float). Arrays without initializer will be initialized later
            _activeFunctionData.pLocalVarValues[count].floatConst = 0;
            _activeFunctionData.pVariableAttributes[count] = value_isFloat;        // for now, assume scalar

            tokenType = jumpTokens(2, pStep, terminalCode);            // either left parenthesis, assignment, comma or semicolon separator (always a terminal)

            // handle array definition dimensions 
            // ----------------------------------

            int dimCount = 0, arrayElements = 1;
            int arrayDims[MAX_ARRAY_DIMS]{ 0 };

            if (terminalCode == termcod_leftPar) {        // array opening parenthesis
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
                } while (terminalCode != termcod_rightPar);

                // create array (init later)
                float* pArray = new float[arrayElements + 1];
                _localArrayObjectCount++;
            #if printCreateDeleteListHeapObjects
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

            if (terminalCode == termcod_assign) {
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
                            _localVarStringObjectCount++;
                        #if printCreateDeleteListHeapObjects
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

                } while (terminalCode == termcod_comma);

            }
        };


// -----------------------------------
// *   terminate external function   *
// -----------------------------------

Justina_interpreter::execResult_type Justina_interpreter::terminateExternalFunction(bool addZeroReturnValue) {
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
    int paramOnlyCount = extFunctionData[_activeFunctionData.functionIndex].paramOnlyCountInFunction;      // of function to be terminated

    if (localVarCount > 0) {
        deleteArrayElementStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);
        deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);

    #if printCreateDeleteListHeapObjects
        Serial.print("----- (LOCAL STORAGE) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues - RAMSTART);
    #endif
        // release local variable storage for function that has been called
        delete[] _activeFunctionData.pLocalVarValues;
        delete[] _activeFunctionData.pVariableAttributes;
        delete[] _activeFunctionData.ppSourceVarTypes;
        _localVarValueAreaCount--;
    }

    char blockType = block_none;            // init
    do {
        blockType = *(char*)_pFlowCtrlStackTop;            // always at least one level present for caller (because returning to it)

        // load local storage pointers again for caller function and restore pending step & active function information for caller function
        if ((blockType == block_extFunction) || (blockType == block_eval)) { _activeFunctionData = *(OpenFunctionData*)_pFlowCtrlStackTop; }        // caller level

        // delete FLOW CONTROL stack level (any optional CALLED function open block stack level) 
        flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
        _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);

    } while ((blockType != block_extFunction) && (blockType != block_eval));        // caller level can be caller eval() or caller external function
    --_callStackDepth;                  // caller reached: call stack depth decreased by 1


    if ((_activeFunctionData.pNextStep >= (_programStorage + PROG_MEM_SIZE)) && (_callStackDepth == 0)) {   // not within a function, not within eval() execution, and not in debug mode       
        if (_localVarValueAreaCount != 0) {
            //// _pConsole ???
            Serial.print("*** Local variable storage area objects cleanup error. Remaining: "); Serial.println(_localVarValueAreaCount);
            _localVarValueAreaErrors += abs(_localVarValueAreaCount);
            _localVarValueAreaCount = 0;
        }

        if (_localVarStringObjectCount != 0) {
            //// _pConsole ???
            Serial.print("*** Local variable string objects cleanup error. Remaining: "); Serial.println(_localVarStringObjectCount);
            _localVarStringObjectErrors += abs(_localVarStringObjectCount);
            _localVarStringObjectCount = 0;
        }

        if (_localArrayObjectCount != 0) {
            //// _pConsole ???
            Serial.print("*** Local array objects cleanup error. Remaining: "); Serial.println(_localArrayObjectCount);
            _localArrayObjectErrors += abs(_localArrayObjectCount);
            _localArrayObjectCount = 0;
        }
    }

    execResult_type execResult = execAllProcessedOperators();     // continue in caller !!!

    return execResult;
}


// ---------------------------------------
// terminate execution of an eval() string
// ---------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::terminateEval() {
    execResult_type execResult{ result_execOK };        // init

    if (evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels >= 1) {
        makeIntermediateConstant(_pEvalStackTop);
    }
    else { return result_eval_nothingToEvaluate; }


    char blockType = block_none;        // init
    do {
        blockType = *(char*)_pFlowCtrlStackTop;            // always at least one level present for caller (because returning to it)

        // load local storage pointers again for caller function and restore pending step & active function information for caller function
        if ((blockType == block_extFunction) || (blockType == block_eval)) { _activeFunctionData = *(OpenFunctionData*)_pFlowCtrlStackTop; }

        // delete FLOW CONTROL stack level (any optional CALLED function open block stack level) 
        flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
        _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);

    } while ((blockType != block_extFunction) && (blockType != block_eval));        // caller level can be caller eval() or caller external function
    --_callStackDepth;                  // caller reached: call stack depth decreased by 1

    // overwrite the parsed 'EVAL' string expressions
    // before removing, delete any parsed strng constants for that command line

    _lastUserCmdStep = *(char**)_pImmediateCmdStackTop;                                     // pop parsed user cmd length
    long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + PROG_MEM_SIZE) + 1;
    deleteConstStringObjects(_programStorage + PROG_MEM_SIZE);
    memcpy((_programStorage + PROG_MEM_SIZE), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);        // size berekenen
    immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
    _pImmediateCmdStackTop = (char*)immModeCommandStack.getLastListElement();
    Serial.print("  >> POP (terminate eval): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + PROG_MEM_SIZE));

    if (evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels >= 1) {
        execResult = execAllProcessedOperators();
        if (execResult != result_execOK) { return execResult; }
    }
    return execResult;
}


// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Justina_interpreter::fetchVarBaseAddress(TokenIsVariable* pVarToken, char*& sourceVarTypeAddress, char& localValueType, char& variableAttributes, char& valueAttributes) {

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
    bool isStaticVar = (varScope == var_isStaticInFunc);                                // could also be a static variable in a stopped function (determined during parsing)

    // init source variable scope (if the current variable is a reference variable, this will be changed to the source variable scope later)
    valueAttributes = 0;                                                                                // not an intermediate constant                                         

    int valueIndex = pVarToken->identValueIndex;

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
        // first locate the debug command level (either in active function data or down in the flow control stack)
        // from there onwards, find the first flow control stack level containing a 'function' block type  
        // The open function data (function where the program was stopped) needed to retrieve function variable data will referenced in that flow control stack level
        //
        // note: levels in between debug command level and open function level may exist, containing open block data for the debug command level
        // these levels can NOT refer to an eval() string execution level, because a program can not be stopped during the execution of an eval() string
        // (although it can during an external function called from an eval() string)

        int blockType = _activeFunctionData.blockType;      // init
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;       // one level below _activeFunctionData

        // variable is a local (including parameter) value: if the current flow control stack level does not refer to a function, but to a command line or eval() block type,
        // then the variable is a local variable of a stopped program's open function 
        bool isStoppedFunctionVar = (blockType == block_extFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + PROG_MEM_SIZE)) : true;     // command line or eval() block type
        ////Serial.print("** FETCHING variable: block type "); Serial.println(blockType);

        if (isStoppedFunctionVar) {
            bool isDebugCmdLevel = (blockType == block_extFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + PROG_MEM_SIZE)) : false;
            ////Serial.print("   is debug command level: "); Serial.println(isDebugCmdLevel);

            if (!isDebugCmdLevel) {       // find debug level in flow control stack instead
                do {
                    blockType = *(char*)pFlowCtrlStackLvl;
                    isDebugCmdLevel = (blockType == block_extFunction) ? (((OpenFunctionData*)pFlowCtrlStackLvl)->pNextStep >= (_programStorage + PROG_MEM_SIZE)) : false;
                    ////Serial.print("   ** new flow ctrl stack lvl: block type "); Serial.println(blockType);
                    ////Serial.print("      is debug command level: "); Serial.println(isDebugCmdLevel);
                    pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                } while (!isDebugCmdLevel);          // stack level for open function found immediate below debug line found (always match)
            }
            ////Serial.print("   ** block type of stack level beneath debug command level "); Serial.println((int)((OpenFunctionData*)pFlowCtrlStackLvl)->blockType);

            blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
            while (blockType != block_extFunction) {
                pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
            }
            ////Serial.print("   ** block type of final flow ctrl stack level "); Serial.println((int)((OpenFunctionData*)pFlowCtrlStackLvl)->blockType);
        }
        else {       // the variable is a local variable of the function referenced in _activeFunctionData
            pFlowCtrlStackLvl = &_activeFunctionData;
        }


        ////Serial.println("       (end loop)");
        // note (function parameter variables only): when a function is called with a variable argument (always passed by reference), 
        // the parameter value type has been set to 'reference' when the function was called
        localValueType = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes[valueIndex] & value_typeMask;         // local variable value type (indicating float or string or REFERENCE)

        if (localValueType == value_isVarRef) {                                                       // local value is a reference to 'source' variable                                                         
            sourceVarTypeAddress = ((OpenFunctionData*)pFlowCtrlStackLvl)->ppSourceVarTypes[valueIndex];                   // pointer to 'source' variable value type

            // local variable value type (reference); SOURCE variable scope (user, global, static; local, param), 'is array' flag
            variableAttributes = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes[valueIndex] | (pVarToken->identInfo & var_isArray);      // add array flag

            ////Serial.print("   is VAR REF - local value index is "); Serial.println(valueIndex);

            return   ((Val**)((OpenFunctionData*)pFlowCtrlStackLvl)->pLocalVarValues)[valueIndex];                       // pointer to 'source' variable value 
        }

        // local variable OR parameter variable that received the result of an expression (or constant) as argument (passed by value) OR optional parameter variable that received no value (default initialization) 
        else {
            ////Serial.print("   is LOCAL VAR - local value index is "); Serial.println(valueIndex);

            sourceVarTypeAddress = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes + valueIndex;               // pointer to local variable value type and 'is array' flag
            // local variable value type (reference); local variable scope (user, global, static; local, param), 'is array' flag
            variableAttributes = pVarToken->identInfo & (var_scopeMask | var_isArray);
            ////Serial.print("       push var: is local (non var ref), var attrib = "); Serial.println(variableAttributes, HEX);
            return (Val*)&(((OpenFunctionData*)pFlowCtrlStackLvl)->pLocalVarValues[valueIndex]);                           // pointer to local variable value 
        }
    }
}


// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------

void* Justina_interpreter::arrayElemAddress(void* varBaseAddress, int* subscripts) {

    // varBaseAddress argument must be base address of an array variable (containing itself a pointer to the array)
    // subscripts array must specify an array element (max. 3 dimensions)
    // return pointer will point to a long, float or a string pointer (both can be array elements) - nullptr if outside boundaries

    void* pArray = varBaseAddress;                                                      // will point to float or string pointer (both can be array elements)
    int arrayDimCount = ((char*)pArray)[3];

    int arrayElement{ 0 };
    for (int i = 0; i < arrayDimCount; i++) {
        int arrayDim = ((char*)pArray)[i];
        if ((subscripts[i] < 1) || (subscripts[i] > arrayDim)) { return nullptr; }      // is outside array boundaries

        int arrayNextDim = (i < arrayDimCount - 1) ? ((char*)pArray)[i + 1] : 1;
        arrayElement = (arrayElement + (subscripts[i] - 1)) * arrayNextDim;
    }
    arrayElement++;                                                                     // add one (first array element contains dimensions and dimension count)
    return (Val*)pArray + arrayElement;                                              // pointer to a 4-byte array element (long, float or pointer to string)
}


// -----------------------------------------------
// *   push terminal token to evaluation stack   *
// -----------------------------------------------

void Justina_interpreter::pushTerminalToken(int& tokenType) {                                 // terminal token is assumed

    // push internal or external function index to stack

    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(TerminalTokenLvl));
    _pEvalStackTop->terminal.tokenType = tokenType;
    _pEvalStackTop->terminal.tokenAddress = _programCounter;                                                    // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;                                            // terminal token only: calculate from partial index stored in high 4 bits of token type 
    _pEvalStackTop->terminal.index += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
};


// ------------------------------------------------------------------------
// *   push internal or external function name token to evaluation stack   *
// ------------------------------------------------------------------------

void Justina_interpreter::pushFunctionName(int& tokenType) {                                  // function name is assumed (internal or external)

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

void Justina_interpreter::pushConstant(int& tokenType) {                                              // float or string constant token is assumed

    // push real or string parsed constant, value type and array flag (false) to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;          // use generic constant type
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;                                  // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->varOrConst.valueType = ((*(char*)_programCounter) >> 4) & value_typeMask;     // for constants, upper 4 bits contain the value type
    _pEvalStackTop->varOrConst.variableAttributes = 0x00;
    _pEvalStackTop->varOrConst.valueAttributes = 0x00;

    if ((_pEvalStackTop->varOrConst.valueType & value_typeMask) == value_isLong) {
        memcpy(&_pEvalStackTop->varOrConst.value.longConst, ((TokenIsConstant*)_programCounter)->cstValue.longConst, sizeof(long));          // float  not necessarily aligned with word size: copy memory instead
    }
    else if ((_pEvalStackTop->varOrConst.valueType & value_typeMask) == value_isFloat) {
        memcpy(&_pEvalStackTop->varOrConst.value.floatConst, ((TokenIsConstant*)_programCounter)->cstValue.floatConst, sizeof(float));          // float  not necessarily aligned with word size: copy memory instead
    }
    else {
        memcpy(&_pEvalStackTop->varOrConst.value.pStringConst, ((TokenIsConstant*)_programCounter)->cstValue.pStringConst, sizeof(void*)); // char pointer not necessarily aligned with word size: copy pointer instead
    }
};


// ---------------------------------------------------
// *   push generic name token to evaluation stack   *
// ---------------------------------------------------

void Justina_interpreter::pushGenericName(int& tokenType) {                                              // float or string constant token is assumed

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

void Justina_interpreter::pushVariable(int& tokenType) {                                              // variable name token is assumed

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
    /*
    Serial.println("** PUSHING variable to eval stack top");
    Serial.print("     source vartype address: "); Serial.println((uint32_t)_pEvalStackTop->varOrConst.varTypeAddress - RAMSTART);
    Serial.print("     local value type: "); Serial.println(_pEvalStackTop->varOrConst.valueType, HEX);
    Serial.print("     source variable attributes: "); Serial.println(_pEvalStackTop->varOrConst.variableAttributes, HEX);
    Serial.print("     value attributes: "); Serial.println(_pEvalStackTop->varOrConst.valueAttributes, HEX);
    Serial.print("     var base value address: "); Serial.println((uint32_t)_pEvalStackTop->varOrConst.value.pBaseValue - RAMSTART);
    */
}
