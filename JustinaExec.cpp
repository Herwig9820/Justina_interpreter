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

// for debugging purposes, prints to Serial
#define PRINT_HEAP_OBJ_CREA_DEL 0
#define PRINT_PROCESSED_TOKEN 0
#define PRINT_DEBUG_INFO 0
#define PRINT_PARSED_STAT_STACK 0
#define PRINT_OBJECT_COUNT_ERRORS 0

// *****************************************************************
// ***        class Justina_interpreter - implementation         ***
// *****************************************************************

// ---------------------------------
// *   execute parsed statements   *
// ---------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::exec(char* startHere) {
#if PRINT_PROCESSED_TOKEN
    Serial.print("\r\n*** enter exec: eval stack depth: "); Serial.println(evalStack.getElementCount());
#endif
    // init

    _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_executing;     // status 'executing'

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
    #if PRINT_PROCESSED_TOKEN
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

            #if PRINT_DEBUG_INFO
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - keyword: "); Serial.print(_resWords[tokenIndex]._resWordName);
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif
            #if PRINT_PROCESSED_TOKEN
                Serial.print("=== process keyword: address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                Serial.print(_resWords[tokenIndex]._resWordName); Serial.println("]");
            #endif
                bool skipStatement = ((_resWords[tokenIndex].restrictions & cmd_skipDuringExec) != 0);
                if (skipStatement) {
                    findTokenStep(_programCounter, tok_isTerminalGroup1, termcod_semicolon);  // find semicolon (always match)
                    _activeFunctionData.pNextStep = _programCounter;
                    break;
                }

                // commands are executed when processing final semicolon statement (note: activeCmd_ResWordCode identifies individual commands; not command blocks)
                _activeFunctionData.activeCmd_ResWordCode = _resWords[tokenIndex].resWordCode;       // store command for now
                _activeFunctionData.activeCmd_tokenAddress = _programCounter;
            }
            break;


            // Case: process external function token
            // -------------------------------------

            case tok_isInternFunction:
            {

                pushFunctionName(tokenType);

            #if PRINT_PROCESSED_TOKEN
                Serial.print("=== process "); Serial.print(tokenType == tok_isInternFunction ? "int fcn" : "ext fcn"); Serial.print(": address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                int funcNameIndex = ((TokenIsIntFunction*)_programCounter)->tokenIndex;
                Serial.print(_functions[funcNameIndex].funcName); Serial.println("]");
            #endif
            }
            break;


            // Case: process external function token
            // -------------------------------------

            case tok_isExternFunction:
            {

                pushFunctionName(tokenType);

            #if PRINT_PROCESSED_TOKEN
                Serial.print("=== process "); Serial.print(tokenType == tok_isInternFunction ? "int fcn" : "ext fcn"); Serial.print(": address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [");
                int funcNameIndex = ((TokenIsExtFunction*)_programCounter)->identNameIndex;
                Serial.print(extFunctionNames[funcNameIndex]); Serial.println("]");
            #endif

            #if PRINT_DEBUG_INFO
                int index = ((TokenIsExtFunction*)_programCounter)->identNameIndex;
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - function name: "); Serial.print(extFunctionNames[index]);
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif
            }
            break;


            // Case: generic identifier token token
            // -------------------------------------------------

            case tok_isGenericName:
            {
                pushGenericName(tokenType);

            #if PRINT_PROCESSED_TOKEN
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

            #if PRINT_DEBUG_INFO
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - constant");
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif

            #if PRINT_PROCESSED_TOKEN
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
            }
            break;


            // Case: process real or string constant token, variable token
            // -----------------------------------------------------------

            case tok_isVariable:
            {
                _activeFunctionData.errorProgramCounter = _programCounter;      // in case an error occurs while processing token

                pushVariable(tokenType);

            #if PRINT_DEBUG_INFO
                Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - variable");
                Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
            #endif

            #if PRINT_PROCESSED_TOKEN
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

            #if PRINT_DEBUG_INFO
                Serial.println("=== processed var name");
            #endif
            }
            break;


            // Case: process terminal token 
            // ----------------------------

            case tok_isTerminalGroup1:
            case tok_isTerminalGroup2:
            case tok_isTerminalGroup3:
            {

                // operator or left parenthesis ?
                // ------------------------------

                if (isOperator || isLeftPar) {

                    bool doCaseBreak{ false };

                    // terminal tokens: only operators and left parentheses are pushed on the stack
                    pushTerminalToken(tokenType);

                #if PRINT_DEBUG_INFO
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
                            execResult = execUnaryOperation(false);        // flag postfix operation
                            if (execResult == result_execOK) { execResult = execAllProcessedOperators(); }
                            if (execResult != result_execOK) { doCaseBreak = true;; }
                        }
                    }

                #if PRINT_PROCESSED_TOKEN        // after evaluation stack has been updated and before breaking 
                    Serial.print("=== processed termin : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif

                    if (doCaseBreak) { break; }
                }


                // comma separator ?
                // -----------------

                else if (isComma) {

                    // no action needed

                #if PRINT_PROCESSED_TOKEN
                    Serial.print("=== process termin : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif
                }


                // right parenthesis ?
                // -------------------

                else if (isRightPar) {

                #if PRINT_DEBUG_INFO
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
                #if PRINT_DEBUG_INFO
                    Serial.println("REMOVE left parenthesis from stack");
                    Serial.print("                 "); Serial.print(evalStack.getElementCount());  Serial.println(" - first argument");
                #endif

                    // correct pointers (now wrong, if from 0 to 2 arguments)
                    _pEvalStackTop = (LE_evalStack*)evalStack.getLastListElement();        // this line needed if no arguments
                    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
                    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

                    // execute internal or external function, calculate array element address or remove parenthesis around single argument (if no function or array)
                    execResult = execParenthesesPair(pPrecedingStackLvl, pStackLvl, argCount);
                #if PRINT_DEBUG_INFO
                    Serial.print("    right par.: exec result "); Serial.println(execResult);
                #endif
                    if (execResult != result_execOK) { doCaseBreak = true; }

                    // the left parenthesis and the argument(s) are now removed and replaced by a single scalar (function result, array element, single argument)
                    // check if additional operators preceding the left parenthesis can now be executed.
                    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
                    if (!doCaseBreak) {
                        execResult = execAllProcessedOperators(); if (execResult != result_execOK) { doCaseBreak = true; }
                    }



                #if PRINT_PROCESSED_TOKEN        // after evaluation stack has been updated and before breaking because of error
                    Serial.print("=== processed termin : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif
                    if (doCaseBreak) { break; }
                }


                // statement separator ?
                // ---------------------

                else if (isSemicolon) {
                #if PRINT_DEBUG_INFO
                    Serial.print("=== process semicolon : eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.print(" - semicolon");
                    Serial.print("   ["); Serial.print(_programCounter - _programStorage); Serial.println("]");
                #endif
                    bool doCaseBreak{ false };

                    lastTokenIsSemicolon = true;
                    isEndOfStatementSeparator = true;

                    if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_none) {       // currently not executing a command, but a simple expression
                        if (evalStack.getElementCount() > (_activeFunctionData.callerEvalStackLevels + 1)) {
                            // if tracing, message would not be correct. Eval stack levels will be deleted right after printing a traced value (or trace execution error)
                            if (!_parsingExecutingTraceString) {
                            #if PRINT_OBJECT_COUNT_ERRORS
                                Serial.print("*** Evaluation stack error. Remaining stack levels for current program level: "); Serial.println(evalStack.getElementCount() - (_activeFunctionData.callerEvalStackLevels + 1));
                            #endif
                            }
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
                                if (_programCounter >= (_programStorage + _progMemorySize)) {
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

                #if PRINT_DEBUG_INFO
                    Serial.print("                 "); Serial.print(evalStack.getElementCount());  Serial.println(" - semicolon processed");
                #endif

                #if PRINT_PROCESSED_TOKEN        // after evaluation stack has been updated and before breaking 
                    Serial.print("=== processed semicolon : address "); Serial.print(_programCounter - _programStorage);  Serial.print(", eval stack depth "); Serial.print(evalStack.getElementCount()); Serial.print(" [ ");
                    Serial.print(_terminals[tokenIndex].terminalName);   Serial.println(" ]");
                #endif

                    if (doCaseBreak) { break; }
                }
            }
            break;  // (case label)


            // ------------------------------
            // parsed eval() statements end ?
            // ------------------------------

            case tok_isEvalEnd:
            {
                execResult = terminateEval();
                if (execResult != result_execOK) { break; }                     // other error: break (case label) immediately

            #if PRINT_PROCESSED_TOKEN        // after evaluation stack has been updated and before breaking because of error
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

    #if PRINT_DEBUG_INFO
        Serial.print("** token has been processed: next step = "); Serial.println(_activeFunctionData.pNextStep - _programStorage);
    #endif


        // 1.3 last token processed was a statement separator ? 
        // ----------------------------------------------------

        // this code executes after a statement was executed (simple expression or command)

    #if PRINT_DEBUG_INFO
        Serial.print("*** token processed: eval stack depth: "); Serial.print(evalStack.getElementCount()); Serial.print(", list element address: "); Serial.println((uint32_t)_pEvalStackTop - sizeof(LinkedList::ListElemHead), HEX); Serial.println();
    #endif
        if (isEndOfStatementSeparator) {        // after expression AND after command

        #if PRINT_PROCESSED_TOKEN        
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

            char c{};                                                           // init: no character available
            bool kill{ false };
            do {
                c = getCharacter(static_cast<Stream*>(_pConsole), kill);                                               // get a key (character from console) if available, and perform a regular housekeeping callback as well
                if (kill) { execResult = result_kill; return execResult; }      // kill Justina interpreter ? (buffer is now flushed until next line character)

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
            if (!_parsingExecutingTraceString && !executingEvalString && (execResult != result_kill) && (execResult != result_quit)) {
                bool nextIsSameLvlEnd{ false };
                if ((_stepCmdExecuted == db_stepToBlockEnd) && (flowCtrlStack.getElementCount() == _stepFlowCtrlStackLevels)
                    && ((*_activeFunctionData.pNextStep & 0x0F) == tok_isReservedWord)) {
                    int index = ((TokenIsResWord*)(_activeFunctionData.pNextStep))->tokenIndex;
                    nextIsSameLvlEnd = (_resWords[index].resWordCode == cmdcod_end);
                }

                // a program may only stop after a program statement (not an immediate mode statement, not an eval() parsed expression) was executed, and it's not the last program statement
                bool executedStepIsprogram = programCnt_previousStatementStart < (_programStorage + _progMemorySize); // always a program function step
                bool nextStepIsprogram = (_programCounter < (_programStorage + _progMemorySize));

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
                    findTokenStep(_programCounter, tok_isTerminalGroup1, termcod_semicolon);  // find next semicolon (always match)
                    _activeFunctionData.pNextStep = _programCounter += sizeof(TokenIsTerminal);
                    tokenType = *_activeFunctionData.pNextStep & 0x0F;                                                               // next token type (could be token within caller, if returning now)
                    precedingIsComma = false;
                    _activeFunctionData.errorStatementStartStep = _activeFunctionData.pNextStep;
                    _activeFunctionData.errorProgramCounter = _activeFunctionData.pNextStep;
                }

                if (doStopForDebugNow) { userRequestsStop = false; _debugCmdExecuted = false; }           // reset request to stop program

                if (userRequestsAbort) { execResult = result_abort; }
                else if (doStopForDebugNow || doSkip) { execResult = result_stopForDebug; }

                isFunctionReturn = false;
            }
        }


        // 1.4 did an execution error occur within token ? signal error
        // ------------------------------------------------------------

        // do not print error message if currently executing trace expressions

        if (!_parsingExecutingTraceString && (execResult != result_execOK)) {          // execution error (printed as expression result if within trace -> not here)
            if (!_consoleAtLineStart) { _pConsole->println(); _consoleAtLineStart = true; }
            execError = true;

            bool isEvent = (execResult >= result_startOfEvents);       // not an error but an event ?
            char execInfo[150] = "";

            // plain error ? 
            if (!isEvent) {
                int sourceErrorPos{ 0 };
                int functionNameLength{ 0 };
                long programCounterOffset{ 0 };

                // if an execution error occurs, normally the info needed to correctly identify and print the statement and the function (if not an imm. mode statement) where the error occured...
                // will be found in structure _activeFunctionData.
                // But if the cause of the STATEMENT execution error is actually a PARSING or EXECUTION error in a (nested or not) eval() string, the info MAY be found in the flow ctrl stack 

                // [1] If a PARSING error occurs while parsing an UNNESTED eval() string, as in statement  a = 3 + eval("2+5*")   (the asterisk will produce a parsing error),
                // then the info pointing to the correct statement ('caller' of the eval() function) is still available in the active function data structure (block type 'block_extFunction'),  
                // because the data has not yet been pushed to the flow ctrl stack

                // [2] If a PARSING error occurs while parsing a NESTED eval() string, or an EXECUTION error occurs while executing ANY parsed eval() string (nested or not),
                // the info pointing to the correct statement has been pushed to the flow ctrl stack already 

                char* errorStatementStartStep = _activeFunctionData.errorStatementStartStep;
                char* errorProgramCounter = _activeFunctionData.errorProgramCounter;
                int functionIndex = _activeFunctionData.functionIndex;          // init

                // info to identify and print the statement where the error occured is on the flow ctrl stack ? find it there
                if (_activeFunctionData.blockType == block_eval) {
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

                    // if the error statement pointers refer to immediate mode code (not to a program), pretty print directly from the imm.mode parsed command stack: add an offset to the pointers 
                    bool isImmMode = (errorStatementStartStep >= (_programStorage + _progMemorySize));
                    if (isImmMode) { programCounterOffset = pImmediateCmdStackLvl + sizeof(char*) - (_programStorage + _progMemorySize); }
                }

                _pConsole->print("\r\n  ");
                prettyPrintStatements(1, errorStatementStartStep + programCounterOffset, errorProgramCounter + programCounterOffset, &sourceErrorPos);
                for (int i = 1; i <= sourceErrorPos; ++i) { _pConsole->print(" "); }

                sprintf(execInfo, "  ^\r\n  Exec error %d", execResult);     // in main program level 
                _pConsole->print(execInfo);

                // errorProgramCounter is never pointing to a token directly contained in a parsed() eval() string 
                if (errorProgramCounter >= (_programStorage + _progMemorySize)) { sprintf(execInfo, ""); }
                else { sprintf(execInfo, " - user function %s", extFunctionNames[functionIndex]); }
                _pConsole->print(execInfo);

                if (execResult == result_eval_parsingError) { sprintf(execInfo, " (eval() parsing error %ld)\r\n", _evalParseErrorCode); }
                else if (execResult == result_list_parsingError) { sprintf(execInfo, " (list input parsing error %ld)\r\n", _evalParseErrorCode); }
                else { sprintf(execInfo, "\r\n"); }
                _pConsole->print(execInfo);
            }

            else if (execResult == result_quit) {
                strcpy(execInfo, "\r\nExecuting 'quit' command, ");
                _pConsole->print(strcat(execInfo, _keepInMemory ? "data retained\r\n" : "memory released\r\n"));
            }
            else if (execResult == result_kill) {}      // do nothing
            else if (execResult == result_abort) { _pConsole->print("\r\n+++ Abort: code execution terminated +++\r\n"); }
            else if (execResult == result_stopForDebug) { if (isBackslashStop) { _pConsole->print("\r\n+++ Program stopped +++\r\n"); } }
            else if (execResult == result_initiateProgramLoad) {}        // (nothing to do here for this event)

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
        if (!_consoleAtLineStart) { _pConsole->println(); _consoleAtLineStart = true; }
        if (_lastValueIsStored && (_printLastResult > 0)) {

            // print last result
            bool isLong = (lastResultTypeFiFo[0] == value_isLong);
            bool isFloat = (lastResultTypeFiFo[0] == value_isFloat);
            int charsPrinted{  };        // needed but not used
            Val toPrint;
            char* fmtString = (isLong || isFloat) ? _dispNumberFmtString : _dispStringFmtString;

            printToString(_dispWidth, (isLong || isFloat) ? _dispNumPrecision : MAX_STRCHAR_TO_PRINT,
                (!isLong && !isFloat), _dispIsIntFmt, lastResultTypeFiFo, lastResultValueFiFo, fmtString, toPrint, charsPrinted, (_printLastResult == 2));
            _pConsole->println(toPrint.pStringConst);

            if (toPrint.pStringConst != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (Intermd str) "); Serial.println((uint32_t)toPrint.pStringConst, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] toPrint.pStringConst;
            }
        }
    }

    // 2.2 adapt imm. mode parsed statement stack, flow control stack and evaluation stack
    // -----------------------------------------------------------------------------------

    if (execResult == result_stopForDebug) {              // stopping for debug now ('STOP' command or single step)
        // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
        _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;

        _pFlowCtrlStackTop = (OpenFunctionData*)flowCtrlStack.appendListElement(sizeof(OpenFunctionData));
        *((OpenFunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                // push caller function data to stack
        ++_callStackDepth;      // user function level added to flow control stack

        _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                          // store evaluation stack levels in use by callers (call stack)

        // push current command line storage to command line stack, to make room for debug commands
    #if PRINT_PARSED_STAT_STACK
        Serial.print("  >> PUSH parsed statements (stop for debug): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + _progMemorySize));
    #endif
        long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
        _pImmediateCmdStackTop = (char*)immModeCommandStack.appendListElement(sizeof(char*) + parsedUserCmdLen);
        *(char**)_pImmediateCmdStackTop = _lastUserCmdStep;
        memcpy(_pImmediateCmdStackTop + sizeof(char*), (_programStorage + _progMemorySize), parsedUserCmdLen);

        ++_openDebugLevels;
    }

    // no programs in debug: always; otherwise: only if error is in fact quit or kill event 
    else if ((_openDebugLevels == 0) || (execResult == result_quit) || (execResult == result_kill)) {             // do not clear flow control stack while in debug mode
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
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("----- (Intermd str) "); Serial.println((uint32_t)toPrint.pStringConst, HEX);
        #endif
            _intermediateStringObjectCount--;
            delete[] toPrint.pStringConst;
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
    _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_idle;     // status 'idle'

#if PRINT_DEBUG_INFO
    Serial.print("*** exit exec: eval stack depth: "); Serial.println(evalStack.getElementCount());
    Serial.print("** EXEC: return error code: "); Serial.println(execResult);
    Serial.println("**** returning to main");
#endif
    _activeFunctionData.pNextStep = _programStorage + _progMemorySize;                // only to signal 'immediate mode command level'

    return execResult;   // return result, in case it's needed by caller
};


// ----------------------------------------------------------------------
// *   execute a processed command  (statement starting with a keyword) *
// ----------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::execProcessedCommand(bool& isFunctionReturn, bool& userRequestsStop, bool& userRequestsAbort) {

    // this c++ function is called when the END of a command statement (semicolon) is encountered during execution, and all arguments are on the stack already

    // IMPORTANT: when adding code for new Justina functions, it must be written so that when a Justina  error occurs, a c++ RETURN <error code> statement is executed.
    // BUT prior to this, all 'intermediate character strings' which are NOT referenced within the evaluation stack MUST BE  DELETED (if referenced, they will be deleted automatically by error handling)

    isFunctionReturn = false;  // init
    execResult_type execResult = result_execOK;
    int cmdParamCount = evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels;

    // note supplied argument count and go to first argument (if any)
    LE_evalStack* pStackLvl = _pEvalStackTop;
    for (int i = 1; i < cmdParamCount; i++) {                                                                               // skipped if no arguments, or if one argument
        pStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);                                                 // iterate to first argument
    }

    _activeFunctionData.errorProgramCounter = _activeFunctionData.activeCmd_tokenAddress;

#if PRINT_DEBUG_INFO
    Serial.print("                 process command code: "); Serial.println((int)_activeFunctionData.activeCmd_ResWordCode);
#endif

    switch (_activeFunctionData.activeCmd_ResWordCode) {                                                                    // command code 

        case cmdcod_stop:
        {
            // -------------------------------------------------
            // Stop code execution (program only, for debugging)
            // -------------------------------------------------

            // 'stop' behaves as if an error occured, in order to follow the same processing logic  

            // RETURN with 'event' error
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
            return result_stopForDebug;
        }
        break;


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
                if (((uint8_t)(valueType[0]) != value_isLong) && ((uint8_t)(valueType[0]) != value_isFloat)) { return result_arg_numberExpected; }
                if ((uint8_t)(valueType[0]) == value_isFloat) { args[0].longConst = (int)args[0].floatConst; }
                _keepInMemory = (args[0].longConst == 0);       // silent mode (even not possible to cancel)
                return result_quit;
            }

            else {      // keep in memory when quitting, cancel: ask user
                _appFlags |= appFlag_waitingForUser;    // bit b6 set: waiting for user interaction

                do {
                    _pConsole->println("===== Quit Justina: keep in memory ? (please answer Y, N or \\c to cancel) =====");

                    // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                    // return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
                    bool doAbort{ false }, doStop{ false }, doCancel{ false }, doDefault{ false };
                    int length{ 0 };
                    char input[MAX_USER_INPUT_LEN + 1] = "";                                                                          // init: empty string
                    if (readText(doAbort, doStop, doCancel, doDefault, input, length)) { return result_kill; }  // kill request from caller ? 
                    if (doAbort) { userRequestsAbort = true; break; }    // '\a': abort running code (program or immediate mode statements - highest priority)
                    else if (doStop) { userRequestsStop = true; }       // '\s': stop a running program AFTER next PROGRAM statement (will have no effect anyway, because quitting)
                    else if (doCancel) { break; }                       // '\c': cancel operation (lowest priority)

                    bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                    if (validAnswer) {
                        _keepInMemory = (tolower(input[0]) == 'y');
                        return result_quit;                        // Justina Quit command executed 
                    }
                } while (true);

            }

            _appFlags &= ~appFlag_waitingForUser;    // bit b6 reset: NOT waiting for user interaction

            // clean up
            clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


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

                // access the flow control stack level below the stack level for the active function, and check the blocktype: is it an open block within the function ?
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
            long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
            deleteConstStringObjects(_programStorage + _progMemorySize);
            memcpy((_programStorage + _progMemorySize), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);        // size berekenen
            immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
            _pImmediateCmdStackTop = immModeCommandStack.getLastListElement();
        #if PRINT_PARSED_STAT_STACK
            Serial.print("  >> POP parsed statements (Go): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + _progMemorySize));
        #endif
            --_openDebugLevels;

            // abort: all done
            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_abort) { return result_abort; }


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

            // !!! DO NOT clean up: evaluation stack has been set correctly, and _activeFunctionData.activeCmd_ResWordCode:  _activeFunctionData just received its values from the flow control stack 
        }
        break;


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
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (system var str) "); Serial.println((uint32_t)pString, HEX);
            #endif
                _systemVarStringObjectCount--;
                delete[] pString;
                _pTraceString = nullptr;      // old trace or eval string
            }

            if (value.pStringConst != nullptr) {                           // new trace string
                _systemVarStringObjectCount++;
                pString = new char[strlen(value.pStringConst) + 2]; // room for additional semicolon (in case string is not ending with it) and terminating '\0'
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (system var str) "); Serial.println((uint32_t)pString, HEX);
            #endif

                strcpy(pString, value.pStringConst);              // copy the actual string
                pString[strlen(value.pStringConst)] = term_semicolon[0];
                pString[strlen(value.pStringConst) + 1] = '\0';
                _pTraceString = pString;

            }

            // clean up
            clearEvalStackLevels(cmdParamCount);                                                                                // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // ---------------------------------------------------------------------------------------------------------
        // Switch on single step mode (use to debug a program without Stop command programmed, right from the start)
        // ---------------------------------------------------------------------------------------------------------

        case cmdcod_debug:
        {
            _debugCmdExecuted = true;

            // clean up
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -----------------------------------
        // read and parse program from console
        // -----------------------------------

        case cmdcod_loadProg:
        {
            _loadProgFromFileNo = 0;        // init: load from console 
            if (cmdParamCount == 1) {       // source specified (console, alternate input or file name)
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];
                copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

                // SD source file name specified ?
                if (valueType[0] == value_isStringPointer) {            // load program from SD file
                    // open file and retrieve file number
                    execResult_type execResult = SD_open(_loadProgFromFileNo, args[0].pStringConst, O_READ);    // this performs a few card & file checks as well
                    if (execResult == result_SD_couldNotOpenFile) {
                        if (!SD.exists(args[0].pStringConst)) { execResult = result_SD_fileNotFound; }        // replace error code for clarity
                    }
                    if (execResult != result_execOK) { return execResult; }
                }

                // external source specified ?
                else if ((valueType[0] == value_isLong) || (valueType[0] == value_isFloat)) {      // external source specified: console or alternate input
                    _loadProgFromFileNo = ((valueType[0] == value_isLong) ? args[0].longConst : args[0].floatConst);
                    if (_loadProgFromFileNo > 0) { return result_IO_invalidStreamNumber; }
                    else if ((-_loadProgFromFileNo) > _altIOstreamCount) { return result_IO_invalidStreamNumber; }
                }
            }

            return result_initiateProgramLoad;              // not an error but an 'event'

            // no clean up to do (return statement executed already)
        }
        break;


        // ------------------------------------------------
        // send a file from SD card to console
        // ------------------------------------------------

        case cmdcod_sendFile:
        {
            bool argIsVar[2];
            bool argIsArray[2];
            char valueType[2];
            Val args[2];
            copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

            // destination specified ?
            Stream* pOut = static_cast<Stream*> (_pConsole);     // init
            if (cmdParamCount == 2) {       // destination specified as first argument
                if ((valueType[0] == value_isLong) || (valueType[0] == value_isFloat)) {      // external source specified: console or alternate input
                    int destination = ((valueType[0] == value_isLong) ? args[0].longConst : (long)args[0].floatConst);      // zero or negative
                    if (destination > 0) { return result_IO_invalidStreamNumber; }
                    else if ((-destination) > _altIOstreamCount) { return result_IO_invalidStreamNumber; }
                    else if (destination == 0) { pOut = static_cast<Stream*> (_pConsole); }
                    else pOut = static_cast<Stream*>(_pAltIOstreams[(-destination) - 1]);     // stream number -1 => array index 0, etc.
                }
                else { return result_arg_numberExpected; }
            }

            // source file
            if (valueType[cmdParamCount - 1] != value_isStringPointer) { return result_arg_stringExpected; }      // mandatory file name

            // open source file for reading
            int fileNumber{ 0 };
            execResult_type execResult = SD_open(fileNumber, args[cmdParamCount - 1].pStringConst, O_READ);     // this performs a few card & file checks as well
            if (execResult == result_SD_couldNotOpenFile) {
                if (!SD.exists(args[cmdParamCount - 1].pStringConst)) { execResult = result_SD_fileNotFound; }        // replace error code for clarity
            }
            if (execResult != result_execOK) { return execResult; }
            File& file = openFiles[fileNumber - 1].file;

            // send file now
            char c{};
            bool kill{ false };
            _pConsole->println("\r\nSending file... please wait\r\n");
            while (file.available() > 0) {
                char  a = file.read(); pOut->write(a);     // read and write, byte per byte
                c = getCharacter(static_cast<Stream*>(_pConsole), kill);          // keep console input buffer empty and perform a regular housekeeping callback as well
                if (kill) { return result_kill; }  // kill request from caller ?
            }

            SD_closeFile(fileNumber);
            _pConsole->println("\r\nFile sent");

            _pConsole->println();       // blank line

            // clean up
            clearEvalStackLevels(cmdParamCount);                                                                                // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // ------------------------------------------------
        // receive a file from console and store on SD card
        // ------------------------------------------------

        case cmdcod_receiveFile:
        {
            bool argIsVar[2];
            bool argIsArray[2];
            char valueType[2];
            Val args[2];
            copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

            // source specified ?
            Stream* pOut = static_cast<Stream*> (_pConsole);     // init
            if (cmdParamCount == 2) {       // destination specified as first argument
                if ((valueType[0] == value_isLong) || (valueType[0] == value_isFloat)) {      // external source specified: console or alternate input
                    int destination = ((valueType[0] == value_isLong) ? args[0].longConst : (long)args[0].floatConst);
                    if (destination > 0) { return result_IO_invalidStreamNumber; }
                    else if ((-destination) > _altIOstreamCount) { return result_IO_invalidStreamNumber; }
                    else if (destination == 0) { pOut = static_cast<Stream*> (_pConsole); }
                    else pOut = static_cast<Stream*>(_pAltIOstreams[(-destination) - 1]);     // stream number -1 => array index 0, etc.
                }
                else { return result_arg_numberExpected; }
            }

            // receiving file name
            if (valueType[cmdParamCount - 1] != value_isStringPointer) { return result_arg_stringExpected; }      // mandatory file name

            // perform preliminary checks
            if (!_SDinitOK) { return result_SD_noCardOrCardError; }
            if (!pathValid(args[cmdParamCount - 1].pStringConst)) { return result_SD_pathIsNotValid; }


            // if file exists, ask if overwriting it is OK
            if (SD.exists(args[cmdParamCount - 1].pStringConst)) {
                bool doReceive{ false };
                do {
                    char s[70] = "===== File exists already. Overwrite ? (please answer Y or N) =====";
                    _pConsole->println(s);
                    // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                    // return flags doAbort, doStop, doCancel, doDefault if user included corresponding escape sequences in input string.
                    bool doAbort{ false }, doStop{ false }, doCancel{ false }, doDefault{ false };      // not used but mandatory
                    int length{ 0 };
                    char input[MAX_USER_INPUT_LEN + 1] = "";                                                                          // init: empty string
                    if (readText(doAbort, doStop, doCancel, doDefault, input, length)) { return result_kill; }  // kill request from caller ?

                    bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                    if (validAnswer) { doReceive = (tolower(input[0]) == 'y'); break; }
                } while (true);
                if (!doReceive) { break; }
            }

            // file does not yet exist ? check if directory exists. If not, create without asking
            else {
                char* dirPath = new char[strlen(args[cmdParamCount - 1].pStringConst) + 1];
                strcpy(dirPath, args[cmdParamCount - 1].pStringConst);
                int pos{ 0 };
                bool dirCreated{ true };      // init
                for (pos = strlen(args[cmdParamCount - 1].pStringConst) - 1; pos >= 0; pos--) { if (dirPath[pos] == '/') { dirPath[pos] = '\0'; break; } }      // isolate path

                if (pos > 0) {    // pos > 0: is NOT a root folder file (pos = 0: root '/' character found; pos=-1: no root '/' character found)
                    if (!SD.exists(dirPath)) {   // if (sub-)directory path does not exist, create it now
                        dirCreated = SD.mkdir(dirPath);
                    }
                }
                delete[]dirPath;
                if (!dirCreated) { return result_SD_couldNotCreateFileDir; }      // no success ? error
            }

            // open destination file for writing. Create it if it doesn't exist yet, truncate it if it does 
            int fileNumber{ 0 };
            execResult_type execResult = SD_open(fileNumber, args[cmdParamCount - 1].pStringConst, O_WRITE + O_CREAT + O_TRUNC);
            if (execResult != result_execOK) { return execResult; }
            File& file = openFiles[fileNumber - 1].file;

            // receive file now
            _pConsole->println("\r\nWaiting for file... press ENTER to cancel");
            char c{};
            bool kill{ false };

            bool waitForFirstChar = true;
            do {
                c = getCharacter(pOut, kill, true);          // get a key (character from console) if available and perform a regular housekeeping callback as well
                if (kill) { return result_kill; }  // kill request from caller ?
                if (c == 0xff) {
                    if (waitForFirstChar) { continue; }
                    else { break; }
                }

                if (waitForFirstChar) {
                    _pConsole->println("\r\nReceiving file... please wait");
                    waitForFirstChar = false;
                }
                file.write(c);          // write character to file
            } while (true);

            SD_closeFile(fileNumber);
            _pConsole->println("\r\nFile stored on SD card");

            // clean up
            clearEvalStackLevels(cmdParamCount);                                                                                // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // --------------
        // SD card: start
        // --------------

        case cmdcod_startSD:
        {
            execResult_type execResult = startSD();
            if (execResult != result_execOK) { return execResult; };

            // clean up
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -------------
        // SD card: stop
        // -------------

        case cmdcod_stopSD:
        {
            SD_closeAllFiles();
            _SDinitOK = false;
            SD.end();


            // clean up
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

            // NO BREAK here: continue with Input command code


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


            // the 'input' and 'info' statements do not accept constants for specific arguments. IN contrast to functions, which can only test this at runtime,...
            // ... statements can test this during parsing. This is why there are no tests related to constants here. 

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
                    if (((uint8_t)(valueType[2]) != value_isLong) && ((uint8_t)(valueType[2]) != value_isFloat)) { return result_arg_numberExpected; }    // flag: with default 
                    checkForDefault = (((uint8_t)(valueType[2]) == value_isLong) ? args[2].longConst != 0 : args[2].floatConst != 0.);
                    checkForCancel = true;

                    if ((argIsArray[1]) && (valueType[1] != value_isStringPointer)) { return result_array_valueTypeIsFixed; }   // an array cannot change type: it needs to be string to receive result
                    if (checkForDefault && (valueType[1] != value_isStringPointer)) { return result_arg_stringExpected; }       // default supplied: it needs to be string

                    char s[80] = "===== Input (\\c to cancel";                                                                  // title static text
                    char title[80 + MAX_ALPHA_CONST_LEN] = "";                                                                   // title static text + string contents of variable
                    strcat(s, checkForDefault ? ", \\d for default = '%s') =====" : "): =====");
                    sprintf(title, s, args[1].pStringConst);
                    _pConsole->println(title);
                }

                else {                                                                                                          // info command
                    if (cmdParamCount == 2) {
                        if (((uint8_t)(valueType[1]) != value_isLong) && ((uint8_t)(valueType[1]) != value_isFloat)) { return result_arg_numberExpected; }
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
                bool doAbort{ false }, doStop{ false }, doCancel{ false }, doDefault{ false };
                int length{ 0 };
                char input[MAX_USER_INPUT_LEN + 1] = "";                                                                          // init: empty string
                if (readText(doAbort, doStop, doCancel, doDefault, input, length)) { execResult = result_kill; return execResult; }
                doDefault = checkForDefault && doDefault;        // gate doDefault
                doCancel = checkForCancel && doCancel;           // gate doCancel

                if (doAbort) { userRequestsAbort = true; }                                         // '\a': abort running code (program or immediate mode statements)
                else if (doStop) { userRequestsStop = true; }                                           // '\s': stop a running program (do not produce stop event yet, wait until program statement executed)

                // if request to stop ('\s') received, first handle input data
                bool  answerIsNo{ false };
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
                            int varScope = pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask;
                            int stringlen = min(strlen(input), MAX_ALPHA_CONST_LEN);

                            (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;
                            args[1].pStringConst = new char[stringlen + 1];
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
                            Serial.println((uint32_t)args[1].pStringConst, HEX);
                        #endif

                            memcpy(args[1].pStringConst, input, stringlen);                                                     // copy the actual string (not the pointer); do not use strcpy
                            args[1].pStringConst[stringlen] = '\0';

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

            // clean up
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

                if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }
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
                char c{};
                bool kill{ false };
                c = getCharacter(static_cast<Stream*>(_pConsole), kill);                                               // get a key (character from console) if available and perform a regular housekeeping callback as well
                if (kill) { execResult = result_kill; return execResult; }      // kill Justina interpreter ? (buffer is now flushed until next line character)

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

            if (doAbort) { execResult = result_abort; return execResult; }                                     // stop a running Justina program (buffer is now flushed until nex line character) 
            else if (doStop) { userRequestsStop = true; }                                                           // '\s': stop a running program (do not produce stop event yet, wait until program statement executed)

            // clean up
            clearEvalStackLevels(cmdParamCount);                                                                                // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // -------------------------------------------------------------------------------------------------------------------------------------------------------------
        // print all arguments (longs, floats and strings) in succession. Floats are printed in compact format with maximum 3 digits / decimals and an optional exponent
        // -------------------------------------------------------------------------------------------------------------------------------------------------------------

        // note: the print command does not take into account the display format set to print the last calculation result
        // to format output produced with the print command, use the formatting function provided (function code: fnccod_format) 

        case cmdcod_printCons:
        case cmdcod_printLineCons:
        case cmdcod_printListCons:
        case cmdcod_print:
        case cmdcod_printLine:
        case cmdcod_printList:
        case cmdcod_printToVar:
        case cmdcod_printLineToVar:
        case cmdcod_printListToVar:
        {
            // print to console, file or string ?
            bool isStreamPrint = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_print) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLine)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printList));
            bool isPrintToVar = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_printToVar) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLineToVar)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListToVar));
            bool isConsolePrint = !(isStreamPrint || isPrintToVar);
            int firstValueIndex = isConsolePrint ? 1 : 2;        // print to file or string: first argument is file or string

            // normal or list print ?
            bool doPrintList = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListCons) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printList)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListToVar));

            // print new line sequence ?
            bool doPrintLineEnd = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLineCons) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLine)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLineToVar)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListCons) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printList)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListToVar));


            char argSep[3] = "  "; argSep[0] = term_comma[0];
            Stream* pOut = static_cast<Stream*> (_pConsole);                   // init

            char* assembledString{ nullptr };       // if printing to strings
            int assembledLen{ 0 };
            LE_evalStack* pFirstArgStackLvl = pStackLvl;


            for (int i = 1; i <= cmdParamCount; i++) {
                bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
                char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
                bool opIsLong = ((uint8_t)valueType == value_isLong);
                bool opIsFloat = ((uint8_t)valueType == value_isFloat);
                bool opIsString = ((uint8_t)valueType == value_isStringPointer);
                char* printString = nullptr;
                Val operand;

                // next line is valid for values of all types (same memory locations are copied)
                operand.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst);

                // print to file or print to stream: first argument is file number or receiving variable
                if (i < firstValueIndex) {
                    if (isStreamPrint) {     // print to file number
                        // check file number (also perform related file and SD card object checks)
                        if ((!opIsLong) && (!opIsFloat)) { return result_arg_numberExpected; }                      // file number
                        int streamNumber = opIsLong ? operand.longConst : operand.floatConst;

                        if (streamNumber == 0) { pOut = static_cast<Stream*> (_pConsole); isConsolePrint = true; }
                        else if ((-streamNumber) > _altIOstreamCount) { return result_IO_invalidStreamNumber; }
                        else if (streamNumber < 0) {                                                                // external IO
                            pOut = static_cast<Stream*>(_pAltIOstreams[(-streamNumber) - 1]);     // stream number -1 => array index 0, etc.
                            if (pOut == _pConsole) { isConsolePrint = true; }
                        }
                        else {
                            File* pFile{};
                            execResult_type execResult = SD_fileChecks(pFile, streamNumber);    // operand: file number
                            if (execResult != result_execOK) { return execResult; }
                            pOut = static_cast<Stream*> (pFile);
                        }
                    }

                    else {      // print to variable
                        if (!operandIsVar) { return result_arg_varExpected; }
                        bool isArray = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isArray);
                        if (isArray && !opIsString) { return result_array_valueTypeIsFixed; }
                    }
                }

                // value to print
                else {
                    // prepare one value for printing
                    if (opIsLong || opIsFloat) {
                        char s[20];  // largely long enough to print long values, or float values with "G" specifier, without leading characters
                        printString = s;    // pointer
                        // next line is valid for long values as well (same memory locations are copied)
                        operand.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst);
                        if (opIsLong) { sprintf(s, "%ld", operand.longConst); }
                        else { sprintf(s, "%3.7G", operand.floatConst); }       // specifier 'G': print minimum 3 characters, print 7 significant digits maximum  
                    }
                    else {
                        operand.pStringConst = operandIsVar ? (*pStackLvl->varOrConst.value.ppStringConst) : pStackLvl->varOrConst.value.pStringConst;
                        // no need to copy string - just print the original, directly from stack (it's still there)
                        printString = operand.pStringConst;     // attention: null pointers not transformed into zero-length strings here
                        if (doPrintList) { quoteAndExpandEscSeq(printString); }
                    }

                    // print one value

                    // NOTE that there is no limit on the number of characters printed here (MAX_PRINT_WIDTH not checked)

                    if (isPrintToVar) {        // print to string ?
                        // remember 'old' string length and pointer to 'old' string
                        char* oldAssembString = assembledString;

                        // calculate length of new string: provide room for argument AND
                        // - if print list: for all value arguments except the last one: sufficient room for argument separator 
                        // - if print new line: if last argument, provide room for new line sequence
                        if (printString != nullptr) { assembledLen += strlen(printString); }            // provide room for new string
                        if (doPrintList && (i < cmdParamCount)) { assembledLen += strlen(argSep); }   // provide room for argument separator

                        // create new string object with sufficient room for argument AND extras (arg. separator and new line sequence, if applicable)
                        if (assembledLen > 0) {
                            _intermediateStringObjectCount++;
                            assembledString = new char[assembledLen + 1]; assembledString[0] = '\0';
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)assembledString, HEX);
                        #endif
                        }

                        // copy string with all previous arguments (if not empty)
                        if (oldAssembString != nullptr) strcpy(assembledString, oldAssembString);
                        if (printString != nullptr) { strcat(assembledString, printString); }
                        // if applicable, copy argument separator or new line sequence
                        if (doPrintList && (i < cmdParamCount)) { strcat(assembledString, argSep); }

                        // delete previous assembled string
                        if (oldAssembString != nullptr) {
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            Serial.print("----- (Intermd str) "); Serial.println((uint32_t)oldAssembString, HEX);
                        #endif
                            _intermediateStringObjectCount--;
                            delete[] oldAssembString;
                        }
                    }

                    else {      // print to file or console ?
                        if (printString != nullptr) { pOut->print(printString); }
                        if ((i < cmdParamCount) && doPrintList) { pOut->print(argSep); }
                    }


                    // console print only: is print position at line start ? 
                    if (isConsolePrint) {
                        if (printString != nullptr) { _consoleAtLineStart = (printString[strlen(printString) - 1] == '\n'); }
                    }

                    if (opIsString && doPrintList) {                            // created above in quoteAndExpandEscSeq(): never a nullptr
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        Serial.print("----- (Intermd str) "); Serial.println((uint32_t)printString, HEX);
                    #endif
                        _intermediateStringObjectCount--;
                        delete[] printString;
                    }
                }

                pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
            }

            // finalise
            if (isPrintToVar) {        // print to string ? save in variable
                // receiving argument is a variable, and if it's an array element, it has string type 

                // if currently the variable contains a string object, delete it
                // NOTE: error can not occur, because 
                execResult = deleteVarStringObject(pFirstArgStackLvl);     // if not empty; checks done above (is variable, is not a numeric array)      n
                if (execResult != result_execOK) {
                    if (assembledString != nullptr) {
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        Serial.print("----- (Intermd str) "); Serial.println((uint32_t)assembledString, HEX);
                    #endif
                        _intermediateStringObjectCount--;
                        delete[] assembledString;
                    }
                    return execResult;
                }

                // print line end without supplied arguments for printing: a string object does not exist yet, so create it now
                if (doPrintLineEnd && (cmdParamCount == 1)) {       // only receiving variable supplied     
                    _intermediateStringObjectCount++;
                    assembledString = new char[3]; assembledString[0] = '\r'; assembledString[1] = '\n'; assembledString[2] = '\0';
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)assembledString, HEX);
                #endif
                }

                // save new string in variable 
                *pFirstArgStackLvl->varOrConst.value.ppStringConst = assembledString;       // init: copy pointer (OK if string length not above limit)
                *pFirstArgStackLvl->varOrConst.varTypeAddress = (*pFirstArgStackLvl->varOrConst.varTypeAddress & ~value_typeMask) | value_isStringPointer;

                // string stored in variable: clip to maximum length
                if (strlen(assembledString) > MAX_ALPHA_CONST_LEN) {
                    char* clippedString = new char[MAX_ALPHA_CONST_LEN];
                    memcpy(clippedString, assembledString, MAX_ALPHA_CONST_LEN);           // copy the string, not the pointer
                    clippedString[MAX_ALPHA_CONST_LEN] = '\0';
                    *pFirstArgStackLvl->varOrConst.value.ppStringConst = clippedString;
                }

                if (assembledString != nullptr) {
                    // non-empty string, adapt object counters (change from intermediate to variable string)
                    _intermediateStringObjectCount--;        // but do not delete the object: it became a variable string
                    char varScope = (pFirstArgStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);
                    (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;

                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)assembledString, HEX);
                    Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
                    Serial.println((uint32_t)*pFirstArgStackLvl->varOrConst.value.ppStringConst, HEX);
                #endif              
                }

                if (strlen(assembledString) > MAX_ALPHA_CONST_LEN) { delete[] assembledString; }        // not referenced in eval. stack (clippedString is), so will not be deleted as part of cleanup
            }

            else {      // print to file or console
                if (doPrintLineEnd) {
                    pOut->println();
                    if (isConsolePrint) { _consoleAtLineStart = true; }
                }
            }

            // clean up
            clearEvalStackLevels(cmdParamCount);      // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // ---------------------------------------------------
        // print all variables (global and user) or call stack
        // ---------------------------------------------------

        case cmdcod_printVars:
        case cmdcod_printCallSt:
        case cmdcod_listFiles:
        {
            Stream* pOut{ static_cast<Stream*> (_pConsole) };          // init
            int streamNumber = 0;                 // console

            if (cmdParamCount == 1) {       // file name specified
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];
                copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);
                if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }
                streamNumber = (valueType[0] == value_isLong) ? args[0].longConst : args[0].floatConst;

                if (streamNumber == 0) { pOut = static_cast<Stream*> (_pConsole); }
                else if ((-streamNumber) > _altIOstreamCount) { return result_IO_invalidStreamNumber; }
                else if (streamNumber < 0) {
                    pOut = static_cast<Stream*>(_pAltIOstreams[(-streamNumber) - 1]);    // external IO (stream number -1 => array index 0, etc.)
                }
                else {
                    File* pFile{};
                    execResult_type execResult = SD_fileChecks(pFile, streamNumber);    // operand: file number
                    if (execResult != result_execOK) { return execResult; }
                    pOut = static_cast<Stream*> (pFile);
                }
            }

            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printVars) {
                pOut->println();
                printVariables(pOut, true);       // user variables
                printVariables(pOut, false);      // global program variables
            }
            else if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printCallSt) { printCallStack(pOut); }

            else {
                execResult_type execResult = SD_listFiles(pOut);
                if (execResult != result_execOK) { return execResult; };
            }

            // clean up
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
            // behaviour corresponds to c++ printf, sprintf, ..., result is a formatted string

            // precision, specifier and flags are used as defaults for next calls to this command, if they are not provided again
            // if the value to be formatted is a string, the precision argument is interpreted as 'maximum characters to print', otherwise it indicates numeric precision (both values retained seperately)
            // the specifier is only relevant for formatting numbers (ignored for formatting strings)

            // numeric precision: function depends on the specifier:
            // - with specifiers 'D'and 'X': specifies the minimum number of digits to be written. Shorter values are padded with leading zeros. Longer values are not truncated. 
            //   If the precision is zero, a zero value will not be printed.
            // - with 'E' and 'F' specifiers: number of decimals to be printed after the decimal point
            // - with 'G' specifier:maximum number of significant numbers to be printed

            // flags: value 1 = left justify, 2 = force sign, 4 = insert a space if no sign, 8: the use depends on the precision specifier:
            // - used with 'F', 'E', 'G' specifiers: always add a decimal point, even if no digits follow 
            // - used with 'X' (hex) specifier: preceed non-zero numbers with '0x'
            // - no function with 'D' (decimal) specifier
            // flag value 16 = pad with zeros 

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

            // clean up
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
            // mandatory argument 2: 0 = do not print last result; 1 = print last result; 2 = expand escape sequences and print last result

            bool argIsVar[2];
            bool argIsArray[2];
            char valueType[2];               // 2 arguments
            Val args[2];

            copyValueArgsFromStack(pStackLvl, cmdParamCount, argIsVar, argIsArray, valueType, args);

            for (int i = 0; i < cmdParamCount; i++) {           // always 2 parameters
                bool argIsLong = (valueType[i] == value_isLong);
                bool argIsFloat = (valueType[i] == value_isFloat);
                if (!(argIsLong || argIsFloat)) { execResult = result_arg_numberExpected; return execResult; }

                if (argIsFloat) { args[i].longConst = (int)args[i].floatConst; }
                if ((args[i].longConst < 0) || (args[i].longConst > 2)) { execResult = result_arg_invalid; return execResult; };
            }
            if ((args[0].longConst == 0) && (args[1].longConst == 0)) { execResult = result_arg_invalid; return execResult; };   // no prompt AND no last result print: do not allow

            // if last result printing switched back on, then prevent printing pending last result (if any)
            _lastValueIsStored = false;               // prevent printing last result (if any)

            _promptAndEcho = args[0].longConst, _printLastResult = args[1].longConst;

            // clean up
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
                // both strings are NOT empty: no nullpointers 
                if (strcmp(_callbackUserProcAlias[index], alias) == 0) { isDeclared = true; break; }
            }
            if (!isDeclared) { execResult = result_userCB_aliasNotDeclared; return execResult; }

            LE_evalStack* pStackLvlFirstValueArg = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
            pStackLvl = pStackLvlFirstValueArg;

            // variable references to store (arguments 2[..4]) 
            const char isVariable = 0x80;                                                               // mask: is variable (not a constant) 

            // if more than 8 arguments are supplied, excess arguments are discarded
            // to keep it simple for the c++ user writing the user routine, we always pass const void pointers, to variables and constants
            // but for constants, the pointer will point to a copy of the data

            Val args[8]{}, dummyArgs[8]{};                                                                            // values to be passed to user routine
            char valueType[8]{ };                             // value types (long, float, char string)
            char varScope[8]{};                                                                         // if variable: variable scope (user, program global, static, local)
            bool argIsNonConstantVar[8]{};                                                                         // flag: is variable (scalar or aray)
            bool argIsArray[8]{};                                                                       // flag: is array element

            const void* pValues_copy[8]{};                                                              // copies for safety
            char valueTypes_copy[8];
            int cmdParamCount_copy{ cmdParamCount };

            // any data to pass ? (optional arguments 2 to 9: data)
            if (cmdParamCount >= 2) {                                                                   // first argument (callback procedure) processed (but still on the stack)
                copyValueArgsFromStack(pStackLvl, cmdParamCount - 1, argIsNonConstantVar, argIsArray, valueType, args, true, dummyArgs);
                pStackLvl = pStackLvlFirstValueArg;     // set stack level again to first value argument
                for (int i = 0; i < cmdParamCount - 1; i++) {
                    if (argIsNonConstantVar[i]) {                                                                  // is this a 'changeable' variable ? (not a constant & not a constant variable)
                        valueType[i] |= isVariable;                                                     // flag as 'changeable' variable (scalar or array element)
                        varScope[i] = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);       // remember variable scope (user, program global, local, static) 
                    }
                    pValues_copy[i] = args[i].pBaseValue;                                                   // copy pointers for safety (protect original pointers from changes by c++ routine) 
                    valueTypes_copy[i] = valueType[i];
                    pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
                }
            }


            // call user routine
            // -----------------

            _callbackUserProcStart[index](pValues_copy, valueTypes_copy, cmdParamCount_copy - 1);     // value pointers, value types copied for safety (if constants are passed, even te values have been copied: dummyArgs[])


            // postprocess: check any strings RETURNED by callback procedure
            // -------------------------------------------------------------

            pStackLvl = pStackLvlFirstValueArg;                                                         // set stack level again to first value argument
            for (int i = 0; i < 8; i++) {
                if ((valueType[i] & value_typeMask) == value_isStringPointer) {
                    // string argument was a constant (including a CONST variable) - OR it was empty (null pointer) ?  
                    // => a string copy or a new string solely consisting of a '\0' terminator (intermediate string) was passed to user routine and needs to be deleted 
                    if (valueType[i] & passCopyToCallback) {
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        Serial.print("----- (Intermd str) "); Serial.println((uint32_t)args[i].pStringConst, HEX);
                    #endif
                        _intermediateStringObjectCount--;
                        delete[] args[i].pStringConst;                                                  // delete temporary string
                    }

                    // string argument was a (NON-CONSTANT) variable string: no copy was made, the string itself was passed to the user routine
                    // did the user routine change it to an empty, '\0' terminated string ?
                    // then this variable string object needs to be deleted and the pointer to it needs to be replaced by a null pointer (Justnia convention)
                    else if (strlen(args[i].pStringConst) == 0) {

                    #if PRINT_HEAP_OBJ_CREA_DEL 
                        Serial.print((varScope[i] == var_isUser) ? "----- (usr var str) " : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
                        Serial.println((uint32_t)args[i].pStringConst, HEX);
                    #endif
                        (varScope[i] == var_isUser) ? _userVarStringObjectCount-- : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount-- : _localVarStringObjectCount--;
                        delete[]args[i].pStringConst;                                                   // delete original variable string
                        *pStackLvl->varOrConst.value.ppStringConst = nullptr;                           // change pointer to string (in variable) to null pointer
                    }
                }
                pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
            }


            // clean up
            clearEvalStackLevels(cmdParamCount);                                                        // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // --------------------
        // block start commands
        // --------------------

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

        // NO BREAK here: from here on, subsequent execution is common for 'if', 'elseif', 'else' and 'while'


        // ------------------------
        // middle-of-block commands
        // ------------------------

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

            // clean up
            clearEvalStackLevels(cmdParamCount);      // clear evaluation stack
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;        // command execution ended
        }
        break;


        // ---------------------------------
        // block break and continue commands
        // ---------------------------------

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

            // clean up
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

        // NO BREAK here: from here on, subsequent execution is the same for 'end' (function) and for 'return'


        // --------------------
        // return from function
        // --------------------

        case cmdcod_return:
        {
            isFunctionReturn = true;
            bool returnWithZero = (cmdParamCount == 0);                    // RETURN statement without expression, or END statement: return a zero
            execResult = terminateExternalFunction(returnWithZero);
            if (execResult != result_execOK) { return execResult; }

            // DO NOT reset _activeFunctionData.activeCmd_ResWordCode: _activeFunctionData will receive its values in routine terminateExternalFunction()
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

int Justina_interpreter::findTokenStep(char*& pStep, int tokenType_spec, char criterium1, char criterium2) {

    // pStep: pointer to first token to test versus token group and (if applicable) token code
    // tokenType: if 'tok_isTerminalGroup1', test for the three terminal groups !

    // if looking for a specific reserved word or a specific terminal (optionally you can specify two)
    char& tokenCode1_spec = criterium1;    // keyword index or terminal index to look for
    char& tokenCode2_spec = criterium2;     // optional second index (-1 if only one index to look for)

    // if looking for a specific variable
    char& varScope_spec = criterium1;      //  variable scope to look for (user, global, ...)
    char& valueIndex_spec = criterium2;    // value index to look for

    // exclude current token step
    int tokenType = *pStep & 0x0F;
    // terminals and constants: token length is NOT stored in token type
    int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
        (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;        // fetch next token 
    pStep = pStep + tokenLength;

    do {
        tokenType = *pStep & 0x0F;
        if (tokenType == '\0') { return tokenType; }            // signal 'not found'

        bool tokenTypeMatch = (tokenType_spec == tokenType);

        if (tokenType_spec == tok_isTerminalGroup1) { tokenTypeMatch = tokenTypeMatch || (tokenType == tok_isTerminalGroup2) || (tokenType == tok_isTerminalGroup3); }
        if (tokenTypeMatch) {
            bool tokenCodeMatch{ false };
            int tokenIndex{ 0 };

            switch (tokenType_spec) {
                case tok_isReservedWord:
                {
                    int tokenIndex = (((TokenIsResWord*)pStep)->tokenIndex);
                    tokenCodeMatch = _resWords[tokenIndex].resWordCode == tokenCode1_spec;
                    if (!tokenCodeMatch && (tokenCode2_spec != -1)) { tokenCodeMatch = _resWords[tokenIndex].resWordCode == tokenCode2_spec; }
                }
                break;

                case tok_isTerminalGroup1:       // actual token can be part of any of the three terminal groups
                {
                    int tokenIndex = ((((TokenIsTerminal*)pStep)->tokenTypeAndIndex >> 4) & 0x0F);
                    tokenIndex += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
                    tokenCodeMatch = _terminals[tokenIndex].terminalCode == tokenCode1_spec;
                    if (!tokenCodeMatch && (tokenCode2_spec != -1)) { tokenCodeMatch = _resWords[tokenIndex].resWordCode == tokenCode2_spec; }
                }
                break;

                case tok_isVariable:
                {
                    int varScope = ((TokenIsVariable*)pStep)->identInfo & var_scopeMask;
                    int valueIndex = ((TokenIsVariable*)pStep)->identValueIndex;
                    tokenCodeMatch = (varScope == (varScope_spec & var_scopeMask)) && ((valueIndex_spec == -1) ? true : (valueIndex == tokenCode2_spec));
                }
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
    int itemToRemove = overWritePrevious ? ((_lastValuesCount >= 1) ? 0 : -1) :
        ((_lastValuesCount == MAX_LAST_RESULT_DEPTH) ? MAX_LAST_RESULT_DEPTH - 1 : -1);

    // remove a previous item ?
    if (itemToRemove != -1) {
        // if item to remove is a string: delete heap object
        if (lastResultTypeFiFo[itemToRemove] == value_isStringPointer) {

            if (lastResultValueFiFo[itemToRemove].pStringConst != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (FiFo string) ");   Serial.println((uint32_t)lastResultValueFiFo[itemToRemove].pStringConst, HEX);
            #endif 
                _lastValuesStringObjectCount--;
                delete[] lastResultValueFiFo[itemToRemove].pStringConst;                // note: this is always an intermediate string
            }
        }
    }
    else {
        _lastValuesCount++;     // only adding an item, without removing previous one
    }

    // move older last results one place up in FIFO, except when just overwriting 'previous' last result
    if (!overWritePrevious && (_lastValuesCount > 1)) {       // if 'new' last result count is 1, no old results need to be moved  
        for (int i = _lastValuesCount - 1; i > 0; i--) {
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
        _lastValuesStringObjectCount++;
        lastResultValueFiFo[0].pStringConst = new char[stringlen + 1];
    #if PRINT_HEAP_OBJ_CREA_DEL
        Serial.print("+++++ (FiFo string) ");   Serial.println((uint32_t)lastResultValueFiFo[0].pStringConst, HEX);
    #endif            

        memcpy(lastResultValueFiFo[0].pStringConst, lastvalue.value.pStringConst, stringlen);        // copy the actual string (not the pointer); do not use strcpy
        lastResultValueFiFo[0].pStringConst[stringlen] = '\0';

        if (lastValueIntermediate) {
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("----- (intermd str) ");   Serial.println((uint32_t)lastvalue.value.pStringConst, HEX);
        #endif
            _intermediateStringObjectCount--;
            delete[] lastvalue.value.pStringConst;
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
    #if PRINT_OBJECT_COUNT_ERRORS
        Serial.print("*** Intermediate string cleanup error. Remaining: "); Serial.println(_intermediateStringObjectCount);
    #endif
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
    bool isDebugCmdLevel = (_activeFunctionData.blockType == block_extFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + _progMemorySize)) : false;  // debug command line

    // imm. mode level error while in debug mode: do not end currently stopped program, except when 'error' indicates 'abort currently STOPPED program'
    if (debugModeError && isDebugCmdLevel && (execResult != result_abort)) { return; }

    if (flowCtrlStack.getElementCount() > 0) {                // skip if only main level (no program running)
        bool isInitialLoop{ true };
        bool noMoreProgramsToTerminate = (debugModeError && isDebugCmdLevel && (execResult != result_abort));                // true if stopped program was terminated
        bool deleteDebugLevelOpenBlocksOnly = (debugModeError && isDebugCmdLevel && (execResult != result_abort));
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                   // deepest caller, parsed eval() expression, loop ..., if any

        // delete all flow control stack levels above the current command line stack level (this could be a debugging command line)

        do {
            // first loop: retrieve block type of currently active function (could be 'main' level = immediate mode instruction as well)
            char blockType = isInitialLoop ? _activeFunctionData.blockType : *(char*)pFlowCtrlStackLvl;    // first character of structure is block type

            if (blockType == block_extFunction) {               // block type: function (is current function) - NOT a (for while, ...) loop or other block type

                if (!isInitialLoop) { _activeFunctionData = *((OpenFunctionData*)pFlowCtrlStackLvl); }      // after first loop, load from stack

                bool isProgramFunction = (_activeFunctionData.pNextStep < (_programStorage + _progMemorySize));
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
                        deleteStringArrayVarsStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);
                        deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);

                    #if PRINT_HEAP_OBJ_CREA_DEL
                        Serial.print("----- (LOCAL STORAGE) ");   Serial.println((uint32_t)(_activeFunctionData.pLocalVarValues), HEX);
                    #endif
                        _localVarValueAreaCount--;
                        // release local variable storage for function that has been called
                        delete[] _activeFunctionData.pLocalVarValues;
                        delete[] _activeFunctionData.pVariableAttributes;
                        delete[] _activeFunctionData.ppSourceVarTypes;
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
        long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
        deleteConstStringObjects(_programStorage + _progMemorySize);      // current parsed user command statements in immediate mode program memory
        memcpy((_programStorage + _progMemorySize), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);        // size berekenen
        immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
        _pImmediateCmdStackTop = immModeCommandStack.getLastListElement();
    #if PRINT_PARSED_STAT_STACK
        Serial.print("  >> POP parsed statements (clr imm cmd stack): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + _progMemorySize));
    #endif
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
            _intermediateStringObjectCount++;
            result.pStringConst = new char[stringlen + 1];
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)result.pStringConst, HEX);
        #endif

            strcpy(result.pStringConst, operand.pStringConst);        // copy the actual strings 
        }
        pEvalStackLvl->varOrConst.value = result;                        // float or pointer to string (type: no change)
        pEvalStackLvl->varOrConst.valueType = valueType;
        pEvalStackLvl->varOrConst.tokenType = tok_isConstant;              // use generic constant type
        pEvalStackLvl->varOrConst.valueAttributes = constIsIntermediate;             // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        pEvalStackLvl->varOrConst.sourceVarScopeAndFlags = 0x00;                  // not an array, not an array element (it's a constant) 
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

#if PRINT_DEBUG_INFO
    Serial.print("** exec processed infix operators -stack levels: "); Serial.println(evalStack.getElementCount());
#endif
    // check if (an) operation(s) can be executed 
    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)

    while (evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 2) {                                                      // at least one preceding token exists on the stack

        // the entry preceding the current value (parsed constant, variable or intermediate constant) is either a prefix or infix operator, another terminal (but never a right parenthesis, 
        // which is never pushed to the evaluation stack) or a generic name.
        // Check operator priority and associativity and execute if OK.  

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

        // token preceding the operand is not an operator ? (it can be a left parenthesis or a generic name) ? exit while loop (nothing to do for now)
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
    bool requiresLongOp = isPrefix ? (_terminals[terminalIndex].prefix_priority & op_long) : (_terminals[terminalIndex].postfix_priority & op_long);
    bool resultCastLong = isPrefix ? (_terminals[terminalIndex].prefix_priority & res_long) : (_terminals[terminalIndex].postfix_priority & res_long);

    // operand, result
    bool operandIsVar = (pOperandStackLvl->varOrConst.tokenType == tok_isVariable);
    char opValueType = operandIsVar ? (*pOperandStackLvl->varOrConst.varTypeAddress & value_typeMask) : pOperandStackLvl->varOrConst.valueType;
    bool opIsFloat = (opValueType == value_isFloat);
    bool opIsLong = (opValueType == value_isLong);

    // (2) apply RULES: check for value type errors. ERROR if operand is either not numeric, or it is a float while a long is required
    // -------------------------------------------------------------------------------------------------------------------------------


    execResult_type execResult = result_execOK;         // init  

    if (!opIsLong && !opIsFloat) { execResult = result_numberExpected; }                   // value is numeric ? (no prefix / postfix operators for strings)
    if (!opIsLong && requiresLongOp) { execResult = result_integerTypeExpected; }              // only integer value type allowed
    if (execResult != result_execOK) { return execResult; }

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
        pOperandStackLvl->varOrConst.sourceVarScopeAndFlags = 0x00;                     // not an array, not an array element (it's a constant) 
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
    if (operatorCode == termcod_assign) { if ((op1isString != op2isString) && (_pEvalStackMinus2->varOrConst.sourceVarScopeAndFlags & var_isArray)) { return result_array_valueTypeIsFixed; } }
    else if (((operatorCode == termcod_plus) || (operatorCode == termcod_plusAssign))) { if (op1isString != op2isString) { return result_operandsNumOrStringExpected; } }
    else if (requiresLongOp) { if (!op1isLong || !op2isLong) { return result_integerTypeExpected; } }
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
                else {                                                                                  // string to be assigned is not empty
                    _intermediateStringObjectCount++;
                    opResult.pStringConst = new char[stringlen + 1];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)opResult.pStringConst, HEX);
                #endif
                    opResult.pStringConst[0] = '\0';                                                    // init: in case first operand is nullptr
                    if (!op1emptyString) { strcpy(opResult.pStringConst, operand1.pStringConst); }
                    if (!op2emptyString) { strcat(opResult.pStringConst, operand2.pStringConst); }
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
            if (opResultFloat) {
                if (isnan(opResult.floatConst)) { return result_undefined; }
                else if (!isfinite(opResult.floatConst)) { return result_overflow; }
                if ((operand1.floatConst != 0) && (operand2.floatConst != 0) && (!isnormal(opResult.floatConst))) { return result_underflow; }
            }
            break;

        case termcod_div:
        case termcod_divAssign:
            if (opResultFloat) { if ((operand1.floatConst != 0) && (operand2.floatConst == 0)) { return result_divByZero; } }
            else { if (operand2.longConst == 0) { return (operand1.longConst == 0) ? result_undefined : result_divByZero; } }
            opResultLong ? opResult.longConst = operand1.longConst / operand2.longConst : opResult.floatConst = operand1.floatConst / operand2.floatConst;
            if (opResultFloat) {
                if (isnan(opResult.floatConst)) { return result_undefined; }
                else if (!isfinite(opResult.floatConst)) { return result_overflow; }
                if ((operand1.floatConst != 0) && (!isnormal(opResult.floatConst))) { return result_underflow; }
            }
            break;

        case termcod_pow:     // operands always (converted to) floats
            if ((operand1.floatConst == 0) && (operand2.floatConst == 0)) { return result_undefined; } // C++ pow() provides 1 as result
            opResult.floatConst = pow(operand1.floatConst, operand2.floatConst);
            if (isnan(opResult.floatConst)) { return result_undefined; }
            else if (!isfinite(opResult.floatConst)) { return result_overflow; }
            else if ((operand1.floatConst != 0) && (!isnormal(opResult.floatConst))) { return result_underflow; }
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
            if ((operand2.longConst < 0) || (operand2.longConst >= 8 * sizeof(long))) { return result_arg_outsideRange; }
            opResult.longConst = operand1.longConst << operand2.longConst;
            break;

        case termcod_bitShRight:
        case termcod_bitShRightAssign:
            if ((operand2.longConst < 0) || (operand2.longConst >= 8 * sizeof(long))) { return result_arg_outsideRange; }
            opResult.longConst = operand1.longConst >> operand2.longConst;
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



    // (6) store result in variable, if operation is a (pure or compound) assignment
    // -----------------------------------------------------------------------------

    if (operationIncludesAssignment) {    // assign the value (parsed constant, variable value or intermediate result) to the variable
        // if variable currently holds a non-empty string (indicated by a nullptr), delete char string object
        execResult_type execResult = deleteVarStringObject(_pEvalStackMinus2); if (execResult != result_execOK) { return execResult; }

        // the value to be assigned is numeric? upcast or downcast if needed (only for receiving array elements, because arrays cannot change value type)
        if (opResultLong || opResultFloat) {
            bool assignToArray = (_pEvalStackMinus2->varOrConst.sourceVarScopeAndFlags & var_isArray);
            bool castToArrayValueType = (assignToArray && (((uint8_t)operand1valueType == value_isLong) != opResultLong));
            if (castToArrayValueType) {
                opResultLong = ((uint8_t)operand1valueType == value_isLong); opResultFloat = !opResultLong;
                opResultLong ? opResult.longConst = opResult.floatConst : opResult.floatConst = opResult.longConst;
            }
        }

        // the value to be assigned is an empty string ? the value is OK already (nullptr)
        else if (opResultString && (opResult.pStringConst == nullptr)) {    // nothing to do

        }

        // the value to be assigned to the receiving variable is a non-empty string value:
        // clip it if needed (strings stored in variables have a maximum length)
        else {
            // note that for reference variables, the variable type fetched is the SOURCE variable type
            int varScope = _pEvalStackMinus2->varOrConst.sourceVarScopeAndFlags & var_scopeMask;

            // make a copy of the character string and store a pointer to this copy as result (even if operand string is already an intermediate constant)
            // because the value will be stored in a variable, limit to the maximum allowed string length
            char* pUnclippedResultString = opResult.pStringConst;
            int stringlen = min(strlen(pUnclippedResultString), MAX_ALPHA_CONST_LEN);
            (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;
            opResult.pStringConst = new char[stringlen + 1];
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
            Serial.println((uint32_t)opResult.pStringConst, HEX);
        #endif

            memcpy(opResult.pStringConst, pUnclippedResultString, stringlen);        // copy the actual string (not the pointer); do not use strcpy
            opResult.pStringConst[stringlen] = '\0';                                         // add terminating \0

            // compound statement ? then an intermediate string has been created (not pushed to the stack) and needs to be deleted now
            if (operatorCode != termcod_assign) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (Intermd str) "); Serial.println((uint32_t)pUnclippedResultString, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] pUnclippedResultString;     // compound assignment: pointing to the unclipped result WHICH IS NON-EMPTY: so it's a heap object and must be deleted now
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
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;                  // not an array, not an array element (it's a constant) 
    }

#if PRINT_DEBUG_INFO
    Serial.print("eval stack depth "); Serial.print(evalStack.getElementCount());  Serial.println(" - infix operation done");
    Serial.print("                 result = "); Serial.println(_pEvalStackTop->varOrConst.value.longConst);

    Serial.print("    eval stack depth: "); Serial.print(evalStack.getElementCount()); Serial.print(", list element address: "); Serial.println((uint32_t)_pEvalStackTop - sizeof(LinkedList::ListElemHead), HEX);
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

    // this procedure is called when the closing parenthesis of an internal Justina function is encountered.
    // all internal Justina functions use the same standard mechanism (with exception of the eval() function):
    // -> all variables are passed by reference; parsed constants, intermediate constants (intermediate calculation results) are passed by value (for a string, this refers to the string pointer).
    // right now, any function arguments (parsed constants, variable references, intermediate calculation results) have been pushed on the evaluation stack already.
    // first thing to do is to copy these arguments (longs, floats, pointers to strings) to a fixed 'arguments' array, as well as a few attributes.
    // - variable references are not copied, instead the actual value of the variable is stored (long, float, string pointer OR array pointer if the variable is an array)
    // - in case the function needs to change the variable value, the variable reference is still available on the stack.
    //   => if it is not certain that this particular stack element does not contain a variable reference, check this first.
    // next, control is passed to the specific Justina function (switch statement below).

    // when the Justina function terminates, arguments are removed from the evaluation stack and the function result is pushed on the stack (at the end of the current procedure)
    // as an intermediate constant (long, float, pointer to string).
    // if the result is a non-empty string, a new string is created on the heap (Justina convention: empty strings are represented by a null pointer to conserve memory).

    // IMPORTANT: at any time, when an error occurs, a RETURN <error code> statement can be called, BUT FIRST all 'intermediate character strings' which are NOT referenced 
    // within the evaluation stack MUST BE  DELETED (if referenced, they will be deleted automatically by error handling)


    // remember token address of internal function token (address from where the internal function is called), in case an error occurs (while passing arguments etc.)   
    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;

    int functionIndex = pFunctionStackLvl->function.index;
    char functionCode = _functions[functionIndex].functionCode;
    int arrayPattern = _functions[functionIndex].arrayPattern;      // available, just in case it's needed
    int minArgs = _functions[functionIndex].minArgs;
    int maxArgs = _functions[functionIndex].maxArgs;
    char fcnResultValueType{};  // init
    Val fcnResult;
    char argValueType[16];
    Val args[16];


    long argIsVarBits{ 0 }, argIsConstantVarBits{ 0 }, argIsLongBits{ 0 }, argIsFloatBits{ 0 }, argIsStringBits{ 0 };




    // preprocess: retrieve argument(s) info: variable or constant, value type
    // -----------------------------------------------------------------------

    if (suppliedArgCount > 0) {
        LE_evalStack* pStackLvl = pFirstArgStackLvl;                                // pointing to first argument on stack

        int bitNmask{ 0x01 };           // lsb
        for (int i = 0; i < suppliedArgCount; i++) {

            // value type of args
            if (pStackLvl->varOrConst.tokenType == tok_isVariable) { argIsVarBits |= bitNmask; }
            if (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isConstantVar) { argIsConstantVarBits |= bitNmask; }

            argValueType[i] = (argIsVarBits & (1 << i)) ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
            args[i].floatConst = (argIsVarBits & (0x1 << i)) ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst;// fetch args: line is valid for all value types

            if (((uint8_t)argValueType[i] == value_isLong)) { argIsLongBits |= bitNmask; }
            if (((uint8_t)argValueType[i] == value_isFloat)) { argIsFloatBits |= bitNmask; }
            if (((uint8_t)argValueType[i] == value_isStringPointer)) { argIsStringBits |= bitNmask; }

            bitNmask <<= 1;
            pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);     // value fetched: go to next argument
        }
    }

    // execute a specific function
    // ---------------------------

    switch (functionCode) {

        // SD card: open file
        // ------------------

        case fnccod_open:
        {
            int newFileNumber{};
            // file path must be string
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }          // file full name including path

            // access mode
            long mode = O_READ;      // init: open for reading
            if (suppliedArgCount == 2) {
                if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
                mode = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : args[1].floatConst;
            }

            // open file and retrieve file number
            execResult_type execResult = SD_open(newFileNumber, args[0].pStringConst, mode);
            if ((execResult != result_execOK) && (execResult != result_SD_couldNotOpenFile)) { return execResult; }

            // save file number as result
            fcnResultValueType = value_isLong;
            fcnResult.longConst = newFileNumber;        // 0: could not open file
        }
        break;


        // SD card: test if file exists, create or remove directory, remove file
        // ---------------------------------------------------------------------

        case fnccod_exists:         // does file of directory exist ?
        case fnccod_mkdir:          // create directory
        case fnccod_rmdir:          // remove directory
        case fnccod_remove:         // remove file
        case fnccod_fileNumber:     // return filenumber for given filename; return 0 if not open
        {
            // checks
            if (!_SDinitOK) { return result_SD_noCardOrCardError; }
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }          // file path
            char* filePath = args[0].pStringConst;
            if (!pathValid(filePath)) { return result_SD_pathIsNotValid; }        // is not a complete check, but it remedies a few plaws in SD library

            // first, check whether the file exists
            fcnResultValueType = value_isLong;
            bool fileExists = (long)SD.exists(filePath);

            // file exists check only: return 0 or 1;
            if (functionCode == fnccod_exists) {
                fcnResult.longConst = fileExists;
                break;
            }

            // make directory ? check that the file does not exist yet  
            if (functionCode == fnccod_mkdir) {
                fcnResult.longConst = fileExists ? 0 : (long)SD.mkdir(filePath);
                break;
            }

            // check whether the file is open 
            bool fileIsOpen{ false };
            int i{ 0 };
            if (_openFileCount > 0) {
                bool givenStartsWithSlash = (filePath[0] == '/');
                for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
                    if (openFiles[i].fileNumberInUse) {
                        // skip starting slash (always present) in stored file path if file path argument doesn't start with a slash
                        if (strcasecmp(openFiles[i].filePath + (givenStartsWithSlash ? 0 : 1), filePath) == 0) { fileIsOpen = true; break; }      // break inner loop only
                    }
                }
            }

            // check for open file ? return 1 if open, 0 if not
            if (functionCode == fnccod_fileNumber) {
                fcnResult.longConst = fileIsOpen ? (i + 1) : 0;
                break;
            }

            // remove directory or file ? return 1 if success, 0 if not
            // the SD library function itself will test for correct file type (lib or file)  
            if (functionCode == fnccod_rmdir) { fcnResult.longConst = fileIsOpen ? 0 : (long)SD.rmdir(filePath); }
            else if (functionCode == fnccod_remove) { fcnResult.longConst = fileIsOpen ? 0 : (long)SD.remove(filePath); }
        }
        break;


        // SD card: directory functions
        // ----------------------------

        case fnccod_isDirectory:
        case fnccod_rewindDirectory:
        case fnccod_openNextFile:
        {
            // check directory file number (also perform related file and SD card object checks)
            File* pFile{};
            int allowedFileTypes = (functionCode == fnccod_isDirectory) ? 0 : 2;        // 0: all file types allowed, 1: files only, 2: directories only
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile, allowedFileTypes);
            if (execResult != result_execOK) { return execResult; }

            // access mode (openNextFile only)
            long mode = O_READ;      // init: open for reading
            if (suppliedArgCount == 2) {
                if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
                mode = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : args[1].floatConst;
            }

            // execute function
            fcnResult.longConst = 0;                        // init
            fcnResultValueType = value_isLong;

            if (functionCode == fnccod_isDirectory) {
                fcnResult.longConst = (long)(pFile->isDirectory());
            }
            else if (functionCode == fnccod_rewindDirectory) {        // rewind directory
                pFile->rewindDirectory();
            }

            else {          // open next file in directory
                // open file and retrieve file number
                int dirFileNumber = (argIsLongBits & (0x1 << 0)) ? (args[0].longConst) : (args[0].floatConst);      // cast directory file number to integer
                int newFileNumber{ 0 };
                execResult_type execResult = SD_openNext(dirFileNumber, newFileNumber, pFile, mode);     // file could be open already: to be safe, open in read only mode here
                if (execResult != result_execOK) { return execResult; }
                fcnResult.longConst = newFileNumber;
            }
        }
        break;


        // SD card: close or flush file
        // ----------------------------

        case fnccod_close:
        case fnccod_flush:
        {
            // check file number (also perform related file and SD card object checks)
            File* pFile{};

            // check file number (also perform related file and SD card object checks)
            if ((!(argIsLongBits & (0x1 << 0))) && (!(argIsFloatBits & (0x1 << 0)))) { return result_arg_numberExpected; }                      // file number
            int fileNumber = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : args[0].floatConst;
            execResult_type execResult = SD_fileChecks(pFile, fileNumber, 0);            // all file types
            if (execResult != result_execOK) { return execResult; }

            if (functionCode == fnccod_flush) { pFile->flush(); }
            else { SD_closeFile(fileNumber); }  // close file

            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
        }
        break;


        // SD card: close or flush file
        // ----------------------------

        case fnccod_closeAll:
        {
            SD_closeAllFiles();
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
        }
        break;


        // SD: check if a file for a given file number is open
        // ---------------------------------------------------

        case fnccod_isOpenFile:
        {
            File* pFile{};
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile, 0);        // file number (all file types)
            // do not produce error if file is not open; all other errors result in error being reported
            if ((execResult != result_execOK) && (execResult != result_SD_fileIsNotOpen)) { return execResult; }            // error

            // save result
            fcnResultValueType = value_isLong;
            fcnResult.longConst = (execResult == result_execOK);   // 0 (not open) or 1 (open)
        }
        break;


        // SD: return file position, size or available characters for reading
        // ------------------------------------------------------------------

        case fnccod_position:
        case fnccod_size:
        case fnccod_available:
        {
            // perform checks and set pointer to IO stream or file
            Stream* pStream{  };
            int streamNumber{ 0 };

            // perform checks and set pointer to IO stream or file
            execResult_type execResult = checkStream(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber);            // return stream pointer AND stream number 
            if (execResult != result_execOK) { return execResult; }
            if ((streamNumber <= 0) && (functionCode != fnccod_available)) { return result_SD_invalidFileNumber; }                  // because a file number expected here

            // retrieve and return value
            int val{};
            if (functionCode == fnccod_position) { val = (static_cast<File*>(pStream))->position(); }         // SD file only
            else if (functionCode == fnccod_size) { val = (static_cast<File*>(pStream))->size(); }            // SD file only
            else { val = pStream->available(); }                                    // can be I/O stream as well

            fcnResultValueType = value_isLong;
            fcnResult.longConst = val;
        }
        break;



        // set time out
        // ------------

        case fnccod_setTimeout:
        {
            Stream* pStream{  };
            int streamNumber{ 0 };

            // perform checks and set pointer to IO stream or file
            execResult_type execResult = checkStream(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber);            // return stream pointer AND stream number 
            if (execResult != result_execOK) { return execResult; }

            // check second argument: timeout in milliseconds
            if ((!(argIsLongBits & (0x1 << 1))) && (!(argIsFloatBits & (0x1 << 1)))) { return result_arg_numberExpected; }      // number of bytes to read
            long arg2 = (argIsLongBits & (0x1 << 1)) ? (args[1].longConst) : (args[1].floatConst);

            pStream->setTimeout(arg2);

            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
        }
        break;


        // SD: set file position
        // ---------------------

        case fnccod_seek:
        {
            // check file number (also perform related file and SD card object checks)
            File* pFile{};
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile);               // do not allow file type 'directory'
            if (execResult != result_execOK) { return execResult; }

            // check second argument: position in file to seek 
            if ((!(argIsLongBits & (0x1 << 1))) && (!(argIsFloatBits & (0x1 << 1)))) { return result_arg_numberExpected; }      // number of bytes to read
            long arg2 = (argIsLongBits & (0x1 << 1)) ? (args[1].longConst) : (args[1].floatConst);

            if (!pFile->seek(arg2)) { return result_SD_fileSeekError; }

            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
        }
        break;


        // SD: return file name
        // --------------------

        case fnccod_name:
        case fnccod_fullName:
        {
            // check file number (also perform related file and SD card object checks)
            File* pFile{};
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile, 0);        // all file types
            if (execResult != result_execOK) { return execResult; }

            int fileNumber = (argIsLongBits & (0x1 << 0)) ? (args[0].longConst) : (args[0].floatConst);

            // retrieve file name or full name and save
            fcnResultValueType = value_isStringPointer;

            int len = strlen((functionCode == fnccod_name) ? pFile->name() : openFiles[fileNumber - 1].filePath);  // always longer than 0 characters
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len + 1];
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            
            strcpy(fcnResult.pStringConst, (functionCode == fnccod_name) ? pFile->name() : openFiles[fileNumber - 1].filePath);
        }
        break;


        // SD: find a target character sequence in characters read from an SD file. Two forms:
        // - //// doc
        // -----------------------------------------------------------------------------------

        case fnccod_find:
        case fnccod_findUntil:
        {
            // perform checks and set pointer to IO stream or file
            // check file number (also perform related file and SD card object checks)

            Stream* pStream{ _pConsole };
            int streamNumber{ 0 };
            execResult_type execResult = checkStream(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber);            // return stream pointer AND stream number
            if (execResult != result_execOK) { return execResult; }

            // check target string 
            if (!(argIsStringBits & (0x1 << 1))) { return result_arg_stringExpected; }
            if (args[1].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }

            // check terminator string
            if (functionCode == fnccod_findUntil) {
                if (!(argIsStringBits & (0x1 << 2))) { return result_arg_stringExpected; }
                if (args[2].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
            }

            // find target and save
            bool targetFound = (functionCode == fnccod_findUntil) ? pStream->findUntil(args[1].pStringConst, args[2].pStringConst) : pStream->find(args[1].pStringConst);
            fcnResult.longConst = (long)targetFound;
            fcnResultValueType = value_isLong;
        }
        break;


        // SD: read a character from file; peek character from a stream (file or IO)
        // -------------------------------------------------------------------------

        case fnccod_readOneChar:
        case fnccod_peek:
        {
            // arguments:
            // - read(): stream number: file number or 0, -1 (console resp. alternate IO)
            // - peek(): stream number: file number or 0, -1 (console resp. alternate IO)
            // read() does not wait if no (more) characters are available 

            // perform checks and set pointer to IO stream or file
            Stream* pStream{ _pConsole };
            int streamNumber{ 0 };
            execResult_type execResult = checkStream(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber);            // return stream pointer AND stream number 
            if (execResult != result_execOK) { return execResult; }

            // read character from file now 
            char c{};                                                                                         // init: no character read
            if (functionCode == fnccod_readOneChar) { c = pStream->read(); }
            else { c = pStream->peek(); }

            // save result
            fcnResultValueType = value_isStringPointer;
            if (c == 0xFF) { fcnResult.pStringConst = nullptr; }        // empty string
            else {
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[2];
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
            #endif            
                fcnResult.pStringConst[0] = c;
                fcnResult.pStringConst[1] = '\0';                                                                                           // terminating \0
            }
        }
        break;


        // - read characters: read characters from stream (file or IO) until buffer is full, optional terminator character is found or time out occurs
        // - read line      : read characters from stream (file or IO) until buffer is full, a '\n' (new line) character is read or time out occurs
        // --------------------------------------------------------------------------------------------------------------------------------------------

        case fnccod_readChars:
        case fnccod_readLine:
        {
            // arguments:
            // - read:      file number[, terminator character], length (number of bytes to read - upon return:chars read)
            // - read line: file number

            // terminator character: first character of given string (if empty string: error)
            // if the 'length' argument is a variable, it returns the count of bytes read
            // these functions time out 
            // functions return a character string variable or a nullptr (empty string)


            // perform checks and set pointer to IO stream or file
            Stream* pStream{ _pConsole };
            int streamNumber{ 0 };
            execResult_type execResult = checkStream(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber);            // return stream pointer AND stream number 
            if (execResult != result_execOK) { return execResult; }

            // check terminator charachter: first character in char * 
            char terminator{ '\n' };                                // init (if read line)
            if (suppliedArgCount == 3) {                // if 3 arguments supplied, then second argument is terminator (not for read line)
                if (!(argIsStringBits & (0x1 << 1))) { return result_arg_stringExpected; }
                if (args[1].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
                terminator = args[1].pStringConst[0];
            }

            // limit string length, because variable for it is created on the heap
            int maxLineLength = MAX_ALPHA_CONST_LEN - 1;        // init: input line -> line terminator '('\n') will be added as well
            // input & read: check length (optional argument)
            if (functionCode == fnccod_readChars) {     // last argument supplied (specifies maximum length)
                int lengthArgIndex = suppliedArgCount - 1;            // base 0
                if ((!(argIsLongBits & (0x1 << lengthArgIndex))) && (!(argIsFloatBits & (0x1 << lengthArgIndex)))) { return result_arg_numberExpected; }      // number of bytes to read
                maxLineLength = (argIsLongBits & (0x1 << lengthArgIndex)) ? (args[lengthArgIndex].longConst) : (args[lengthArgIndex].floatConst);
                if ((maxLineLength < 1) || (maxLineLength > MAX_ALPHA_CONST_LEN)) { return result_arg_outsideRange; }
            }

            // prepare to read characters
            _intermediateStringObjectCount++;
            // buffer, long enough to receive maximum line length and (input line only) line terminator ('\n')
            char* buffer = new char[((functionCode == fnccod_readLine) ? maxLineLength + 2 : maxLineLength + 1)];
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)buffer, HEX);
        #endif

            // read characters now
            int charsRead{};                          // init: set count of characters read to max. number of characters that can be read
            if (functionCode == fnccod_readChars) {    // read number of characters or read line
                charsRead = (suppliedArgCount == 2) ? pStream->readBytes(buffer, maxLineLength) : pStream->readBytesUntil(terminator, buffer, maxLineLength);
            }
            else { charsRead = pStream->readBytesUntil(terminator, buffer, maxLineLength); }        // input line function (no length limit)
            buffer[charsRead] = ((functionCode == fnccod_readLine) ? '\n' : '\0');       // add terminating '\0'
            if (functionCode == fnccod_readLine) { buffer[charsRead + 1] = '\0'; }

            // return number of characters read into last argument, if it's not a constant
            bool isConstant = (!(argIsVarBits & (0x1 << (suppliedArgCount - 1))) || (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));         // constant ?
            if (!isConstant) { // if last argument is constant: skip saving value in last argument WITHOUT error  

                // array with floats ? convert long value (charsRead) into float and store float
                bool returnArgIsArray = (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isArray);
                if ((argIsFloatBits & (0x1 << (suppliedArgCount - 1))) && returnArgIsArray) {        // convert value to float (to match with array's fixed value type)
                    *_pEvalStackTop->varOrConst.value.pFloatConst = (float)charsRead;
                }
                else {      // array with integers, or scalar (any numeric type): store integer value in it, and set type 
                    *_pEvalStackTop->varOrConst.value.pLongConst = charsRead;
                    *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isLong;     // if not yet long
                }
            }

            // save result
            // -----------
            fcnResultValueType = value_isStringPointer;

            // no characters read ? simply delete buffer
            if (charsRead == 0) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)buffer, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] buffer;
                fcnResult.pStringConst = nullptr;
            }

            // less characters read than maximum ? move string to a smaller character array to save space
            else if (charsRead < maxLineLength) {
                _intermediateStringObjectCount++;
                char* smallerBuffer = new char[charsRead + 1]; // including space for terminating '\0'
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)smallerBuffer, HEX);
            #endif
                strcpy(smallerBuffer, buffer);
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)buffer, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] buffer;
                fcnResult.pStringConst = smallerBuffer;
            }
            else { fcnResult.pStringConst = buffer; }
        }
        break;


        //
        // -----

        case fnccod_parseList:
        case fnccod_parseListFromVar:
        {
            // arguments: file number or string, variable[, variable ...]
            char* buffer{};
            execResult_type execResult{ result_execOK };
            int valuesSaved{ 0 };

            // check receiving arguments: must be variables
            for (int argIndex = 1; argIndex < suppliedArgCount; ++argIndex) { if (!(argIsVarBits & (1 << argIndex))) { return result_arg_varExpected; } }   // i=1: second argument, ...

            if (functionCode == fnccod_parseList) {
                // perform checks and set pointer to IO stream or file
                Stream* pStream{ _pConsole };
                int streamNumber{ 0 };
                execResult_type execResult = checkStream(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber);            // return stream pointer AND stream number 
                if (execResult != result_execOK) { return execResult; }

                // prepare to read characters
                _intermediateStringObjectCount++;
                // limit buffer length, because it's created on the heap
                buffer = new char[MAX_ALPHA_CONST_LEN + 1];                     // buffer, long enough to receive maximum line length + null (create AFTER last error check)
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)buffer, HEX);
            #endif

                // read line from file
                int charsRead = pStream->readBytesUntil('\n', buffer, MAX_ALPHA_CONST_LEN);      // note: empty strings: buffer contains one character ('\0')
                buffer[charsRead] = '\0';       // add terminating '\0'
            }
            else {      // parse from string
                if (!(argIsStringBits & (1 << 0))) { return result_arg_stringExpected; };
                buffer = (argIsVarBits & (1 << 0)) ? *pFirstArgStackLvl->varOrConst.value.ppStringConst : pFirstArgStackLvl->varOrConst.value.pStringConst;
            }

            // parse constants in buffer
            parseTokenResult_type parsingResult{ result_tokenFound };

            char* pNext = buffer;   // init
            int commaLength = strlen(term_comma);
            bool intermediateStringCreated{ false };
            Val value; char valueType{};
            LE_evalStack* pStackLvl = pFirstArgStackLvl;                                // now points to file number argument


            // iterate through all value-receiving variables and separators between them
            // -------------------------------------------------------------------------

            // second argument, ... last argument
            for (int argIndex = 1; argIndex < suppliedArgCount; ++argIndex, intermediateStringCreated = false) {       //initialise 'intermediateStringCreated' to false after each loop

                // move to the first non-space character of next token 
                while (pNext[0] == ' ') { pNext++; }                                         // skip leading spaces
                if (isSpace(pNext[0])) { break; }                // end of instruction: prepare to quit parsing  

                char* pch = pNext;

                do {    // one loop only
                    if (argIndex > 1) {      // first look for a separator
                        // do not look for trailing space, to use strncmp() wih number of non-space characters found, because a space is not required after an operator
                        bool isComma = (strncmp(term_comma, pch, commaLength) == 0);      // token corresponds to terminal name ? Then exit loop    
                        if (!isComma) { parsingResult = result_separatorExpected;  break; }
                        pNext += commaLength;                                                                            // move to next character
                        while (pNext[0] == ' ') { pNext++; }                                         // skip leading spaces
                        if (isSpace(pNext[0])) { parsingResult = result_parseList_stringNotComplete; break; }
                    }

                    // parsing functions below return...
                    //  - true: no parsing error. parsingResult determines whether token recognised (result_tokenFound) or not (result_tokenNotFound) - in which case it can still be another token type
                    //  - false: parsing error. parsingResult indicates which error.

                    // float or integer ?
                    _initVarOrParWithUnaryOp = 0;       // needs to be zero before calling parseIntFloat()
                    if (!parseIntFloat(pNext, pch, value, valueType, parsingResult)) { break; }                                     // break with error
                    if (parsingResult == result_tokenFound) { break; }                                                              // is this token type: look no further
                    // string ? if string and net empty, a string object is created by routine parseString()
                    if (!parseString(pNext, pch, value.pStringConst, valueType, parsingResult, true)) { break; }                    // break with error
                    if (parsingResult == result_tokenFound) { break; }                                                              // is this token type: look no further
                    parsingResult = result_parseList_valueToParseExpected;
                } while (false);        // one loop only

                if (parsingResult != result_tokenFound) { execResult = result_list_parsingError;   break; }                             // exit loop if token error (syntax, ...)

                // if a valid token was parsed: if it's a non-empty string (stored in value.pString), remember thar it will have to be deleted in case an errors occurs in what follows
                if ((valueType == value_isStringPointer) && (value.pStringConst != nullptr)) { intermediateStringCreated = true; }      // parsed string is created on the heap


                // parsing OK: assign value to receiving variable
                // ----------------------------------------------

                    // retrieve stack level for receiving variable
                pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);         // now ponts to variable argument to receive a value (or nullptr if last was reached before)
                // reached last variable and input buffer not completely parsed ? break with error
                if (pStackLvl == nullptr) { break; }        // no more variables to save values into: quit parsing remainder of string / file

                // if variable is an array element, it's variable type is fixed. Compatible with provided value ?
                bool returnArgIsArray = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isArray);
                bool oldArgIsLong = (argIsLongBits & (0x1 << argIndex));
                bool oldArgIsFloat = (argIsFloatBits & (0x1 << argIndex));
                bool oldArgIsString = (argIsStringBits & (0x1 << argIndex));
                char oldArgValueType = (oldArgIsLong ? value_isLong : oldArgIsFloat ? value_isFloat : value_isStringPointer);

                // if receiving variable is array, both of the old and new values must be string OR both must be numeric (array value type is fixed)
                if (returnArgIsArray && (oldArgIsString != (valueType == value_isStringPointer))) { execResult = result_array_valueTypeIsFixed; break; }

                // if currently the variable contains a string object, delete it
                if (oldArgIsString) { execResult = deleteVarStringObject(pStackLvl); if (execResult != result_execOK) { break; } }      // (if not empty)

                // save new value and value type
                if (!returnArgIsArray || (oldArgValueType == valueType)) {
                    *pStackLvl->varOrConst.value.pLongConst = value.longConst;                    // valid for all value types
                    *pStackLvl->varOrConst.varTypeAddress = (*pStackLvl->varOrConst.varTypeAddress & ~value_typeMask) | valueType;
                }
                else {      // is array and new and old value have different numeric types: convert to array value type
                    if (oldArgValueType == value_isLong) { *pStackLvl->varOrConst.value.pLongConst = value.floatConst; }
                    else { *pStackLvl->varOrConst.value.pFloatConst = value.longConst; }
                }

                ++valuesSaved;      // numberof values saved will be returned

                // if the new value is a non-empty (intermediate) string, simply reference it in the Justina variable 
                if ((valueType == value_isStringPointer) && (value.pStringConst != nullptr)) {
                    intermediateStringCreated = false;                                               // it's becoming a Justina variable value now
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)value.pStringConst, HEX);
                #endif
                    _intermediateStringObjectCount--;
                    // do NOT delete the object: it became a variable string

                    char varScope = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
                    Serial.println((uint32_t)*pStackLvl->varOrConst.value.ppStringConst, HEX);
                #endif
                    (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;
                }
            }

            // delete input temporary buffer
            if (functionCode == fnccod_parseList) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)buffer, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] buffer;
            }

            if (intermediateStringCreated) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)value.pStringConst, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] value.pStringConst;

            }


            // execution error ?
            if (execResult != result_execOK) {
                _evalParseErrorCode = parsingResult;            // only relevant in case a parsing error occured
                return execResult;
            }

            // save result: number of values that were actually saved 
            fcnResultValueType = value_isLong;
            fcnResult.longConst = valuesSaved;
        }
        break;



        // evaluatie expression contained within quotes
        // --------------------------------------------

        case fnccod_eval:
        {
            // only one argument possible (eval() string)
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            char resultValueType;
            execResult_type execResult = launchEval(pFunctionStackLvl, args[0].pStringConst);
            if (execResult != result_execOK) { return execResult; }
            // an 'EXTERNAL function' (executing the parsed eval() expressions) has just been 'launched' (and will start after current (right parenthesis) token is processed)
            // because eval function name token and single argument will be removed from stack now (see below, at end of this procedure), adapt CALLER evaluation stack levels
            _activeFunctionData.callerEvalStackLevels -= 2;
        }
        break;


        // switch and ifte functions
        // -------------------------

        case fnccod_switch:
        case fnccod_ifte:
        {
            // switch() arguments: switch expression, test expression 1 , result 1 [, ... [, test expression 7 , result 7]] [, default result expression]
            // ifte() arguments  : test expression 1, true part, false part 1 (simple if, then, else form)
            //               or  : test expression 1, true part 1, test expression 2, true part 2 [, test expression 3 , true part 3 ... [, test expression 7 , true part 7]]...] [, false part]
            // no preliminary restriction on type of arguments

            // set default value
            bool isSwitch = (functionCode == fnccod_switch);
            fcnResultValueType = (suppliedArgCount % 2 == (isSwitch ? 0 : 1)) ? argValueType[suppliedArgCount - 1] : value_isLong;      // init
            fcnResult.longConst = 0; if (suppliedArgCount % 2 == (isSwitch ? 0 : 1)) { fcnResult = args[suppliedArgCount - 1]; }        // OK if default value is not a string or an empty string

            bool testValueIsNumber = (argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0));                                                                     // (for switch function only)
            bool match{ false };
            int matchIndex{ 0 };
            int matchResultPairs = (suppliedArgCount - (isSwitch ? 1 : 0)) / 2;
            for (int pair = (isSwitch ? 1 : 0); pair <= matchResultPairs - (isSwitch ? 0 : 1); ++pair) {
                matchIndex = (pair << 1) - (isSwitch ? 1 : 0);                                                                          // index in argument array
                match = false;      // init

                if (isSwitch) {
                    if ((argIsStringBits & (0x1 << 0)) && (argIsStringBits & (0x1 << matchIndex))) {                                                                    // test value and mmtch value are both strings
                        if ((args[0].pStringConst == nullptr) || (args[matchIndex].pStringConst == nullptr)) {
                            match = ((args[0].pStringConst == nullptr) && (args[matchIndex].pStringConst == nullptr));                  // equal
                        }
                        else { match = (strcmp(args[0].pStringConst, args[matchIndex].pStringConst) == 0); }
                    }
                    else if (testValueIsNumber && (((argIsLongBits & (0x1 << matchIndex))) || ((argIsFloatBits & (0x1 << matchIndex))))) {                              // test value and match value are both numeric
                        if ((argIsLongBits & (0x1 << 0)) && (argIsLongBits & (0x1 << matchIndex))) { match = ((args[0].longConst == args[matchIndex].longConst)); }
                        else { match = ((argIsFloatBits & (0x1 << 0)) ? args[0].floatConst : (float)args[0].longConst) == ((argIsFloatBits & (0x1 << matchIndex)) ? args[matchIndex].floatConst : (float)args[matchIndex].longConst); }
                    }
                }
                else {
                    if (!(argIsLongBits & (0x1 << matchIndex)) && !(argIsFloatBits & (0x1 << matchIndex))) { return result_testexpr_numberExpected; }               // test value and match value are both strings
                    match = ((argIsFloatBits & (0x1 << matchIndex)) ? (args[matchIndex].floatConst != 0.) : (args[matchIndex].longConst == !0));
                }

                if (match) {
                    fcnResultValueType = argValueType[matchIndex + 1];
                    fcnResult = args[matchIndex + 1];                                                                                   // OK if not string or empty string
                    break;
                }
            }

            // result is a non-empty string ? an object still has to be created on the heap
            if ((fcnResultValueType == value_isStringPointer) && (fcnResult.pStringConst != nullptr)) {
                int resultIndex = match ? matchIndex + 1 : suppliedArgCount - 1;
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[strlen(args[resultIndex].pStringConst) + 1];
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
            #endif            
                strcpy(fcnResult.pStringConst, args[resultIndex].pStringConst);
            }
        }
        break;


        // choose function
        // ---------------

        case fnccod_choose:
        {
            // arguments: expression, test expression 1, test expression 2 [... [, test expression 15]]
            // no preliminary restriction on type of arguments

            // the first expression is an index into the test expressions: return  
            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int index = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : args[0].floatConst;
            if ((index <= 0) || (index >= suppliedArgCount)) { return result_arg_outsideRange; }
            fcnResultValueType = argValueType[index];
            fcnResult = args[index];                                                                                                    // OK if not string or empty string

            // result is a non-empty string ? an object still has to be created on the heap
            if ((fcnResultValueType == value_isStringPointer) && (fcnResult.pStringConst != nullptr)) {
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[strlen(args[index].pStringConst) + 1];
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
            #endif            
                strcpy(fcnResult.pStringConst, args[index].pStringConst);
            }
        }
        break;


        // index function
        // --------------

        case fnccod_index:
        {
            // arguments : expression, test expression 1, test expression 2 [... [, test expression 15]]
            // no preliminary restriction on type of arguments

            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;                                                                                                    // init: not found

            bool testValueIsNumber = (argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0));                                                                     // (for switch function only)
            bool match{ false };
            for (int i = 1; i <= suppliedArgCount - 1; ++i) {
                if ((argIsStringBits & (0x1 << 0)) && (argIsStringBits & (0x1 << i))) {            // test value and mmtch value are both strings
                    if ((args[0].pStringConst == nullptr) || (args[i].pStringConst == nullptr)) {
                        match = ((args[0].pStringConst == nullptr) && (args[i].pStringConst == nullptr));                               // equal
                    }
                    else { match = (strcmp(args[0].pStringConst, args[i].pStringConst) == 0); }
                }
                else if (testValueIsNumber && (((argIsLongBits & (0x1 << i))) || ((argIsFloatBits & (0x1 << i))))) {                                                    // test value and match value are both numeric
                    if ((argIsLongBits & (0x1 << 0)) && (argIsLongBits & (0x1 << i))) { match = ((args[0].longConst == args[i].longConst)); }
                    else { match = ((argIsFloatBits & (0x1 << 0)) ? args[0].floatConst : (float)args[0].longConst) == ((argIsFloatBits & (0x1 << i)) ? args[i].floatConst : (float)args[i].longConst); }
                }

                if (match) {
                    fcnResult.longConst = i;
                    break;
                }
            }
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
            if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
            void* pArray = *pFirstArgStackLvl->varOrConst.value.ppArray;
            int arrayDimCount = ((char*)pArray)[3];
            int dimNo = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : int(args[1].floatConst);
            if ((argIsFloatBits & (0x1 << 1))) { if (args[1].floatConst != dimNo) { return result_arg_integerDimExpected; } }   // if float, fractional part should be zero
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
            int FiFoElement = 1;                                                                                                        // init: newest FiFo element
            if (suppliedArgCount == 1) {                                                                                                // FiFo element specified
                if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
                FiFoElement = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);
                if ((FiFoElement < 1) || (FiFoElement > MAX_LAST_RESULT_DEPTH)) { return result_arg_outsideRange; }
            }
            if (FiFoElement > _lastValuesCount) { return result_arg_invalid; }
            --FiFoElement;

            fcnResultValueType = lastResultTypeFiFo[FiFoElement];
            bool fcnResultIsLong = (lastResultTypeFiFo[FiFoElement] == value_isLong);
            bool fcnResultIsFloat = (lastResultTypeFiFo[FiFoElement] == value_isFloat);
            if (fcnResultIsLong || fcnResultIsFloat || (!fcnResultIsLong && !fcnResultIsFloat && (lastResultValueFiFo[FiFoElement].pStringConst == nullptr))) {
                fcnResult = lastResultValueFiFo[FiFoElement];
            }
            else {                              // string
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[strlen(lastResultValueFiFo[FiFoElement].pStringConst + 1)];
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
            #endif            
                strcpy(fcnResult.pStringConst, lastResultValueFiFo[FiFoElement].pStringConst);
            }
        }
        break;


        // format a number or a string into a destination string
        // -----------------------------------------------------

        case fnccod_format:
        {
            // mandatory argument 1: value to be formatted
            // optional arguments 2-5: width, precision, [specifier (F:fixed, E:scientific, G:general, D: long integer, X:hex)], flags, characters printed (return value)
            // note that specifier argument can be left out, flags argument taking its place
            // behaviour corresponds to c++ printf, sprintf, ..., result is a formatted string

            // width, precision, specifier and flags are used as defaults for next calls to this function, if they are not provided again
            // if the value to be formatted is a string, the precision argument is interpreted as 'maximum characters to print', otherwise it indicates numeric precision (both values retained seperately)
            // the specifier is only relevant for formatting numbers (ignored for formatting strings), but can be set while formatting a string

            // numeric precision: function depends on the specifier:
            // - with specifiers 'D'and 'X': specifies the minimum number of digits to be written. Shorter values are padded with leading zeros. Longer values are not truncated. 
            //   If the precision is zero, a zero value will not be printed.
            // - with 'E' and 'F' specifiers: number of decimals to be printed after the decimal point
            // - with 'G' specifier:maximum number of significant numbers to be printed

            // flags: value 1 = left justify, 2 = force sign, 4 = insert a space if no sign, 8: the use depends on the precision specifier:
            // - used with 'F', 'E', 'G' specifiers: always add a decimal point, even if no digits follow 
            // - used with 'X' (hex) specifier: preceed non-zero numbers with '0x'
            // - no function with 'D' (decimal) specifier
            // flag value 16 = pad with zeros 

            bool isIntFmt{ false };
            int charsPrinted{ 0 };

            // INIT print width, precision, specifier, flags
            int& width = _printWidth, & precision = (((argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0))) ? _printNumPrecision : _printCharsToPrint), & flags = _printFmtFlags;

            // optional argument returning #chars that were printed is present ? Variable expected
            bool hasSpecifierArg = false; // init
            if (suppliedArgCount >= 3) { hasSpecifierArg = (!(argIsLongBits & (0x1 << 3)) && !(argIsFloatBits & (0x1 << 3))); }             // third argument is either a specifier (string) or set of flags (number)
            bool returnArgIsArray{};
            if (suppliedArgCount == (hasSpecifierArg ? 6 : 5)) {      // optional argument returning #chars that were printed is present
                // if array has a non-numeric type (string), produce error (consistent with the way operators deal with it)
                returnArgIsArray = (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isArray);
                if ((!(argIsLongBits & (0x1 << (suppliedArgCount - 1))) && !(argIsFloatBits & (0x1 << (suppliedArgCount - 1)))) && returnArgIsArray) { return result_array_valueTypeIsFixed; }
            }

            // test arguments and ADAPT print width, precision, specifier, flags
            // -----------------------------------------------------------------

            execResult_type execResult = checkFmtSpecifiers(false, (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))), suppliedArgCount, argValueType, args, _printNumSpecifier[0], width, precision, flags);
            if (execResult != result_execOK) { return execResult; }

            // prepare format specifier string and format
            // ------------------------------------------

            char  fmtString[20];                                                                                                        // long enough to contain all format specifier parts
            char* specifier = "s";
            if ((argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0))) {
                specifier = _printNumSpecifier;
                isIntFmt = (specifier[0] == 'X') || (specifier[0] == 'x') || (specifier[0] == 'D') || (specifier[0] == 'd');
            }
            makeFormatString(flags, isIntFmt, specifier, fmtString);
            printToString(width, precision, (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))), isIntFmt, argValueType, args, fmtString, fcnResult, charsPrinted);
            fcnResultValueType = value_isStringPointer;

            // return number of characters printed into (variable) argument if it was supplied
            // -------------------------------------------------------------------------------

            if (suppliedArgCount == (hasSpecifierArg ? 6 : 5)) {      // optional argument returning #chars that were printed is present
                bool isConstant = (!(argIsVarBits & (0x1 << (suppliedArgCount - 1))) || (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));
                if (!isConstant) { // if last argument is constant: skip saving value in last argument WITHOUT error  
                    // if  last argument's variable currently holds a non-empty string (indicated by a nullptr), delete char string object (it will be replaced by the number of characters printed)
                    execResult_type execResult = deleteVarStringObject(_pEvalStackTop); if (execResult != result_execOK) { return execResult; }
                    // save value in variable now, and set variable value type to long 
                    // note: if variable reference, then value type on the stack indicates 'variable reference' which should not be changed (but stack level will be deleted now anyway)
                    if ((argIsFloatBits & (0x1 << (suppliedArgCount - 1))) && returnArgIsArray) {        // convert value to float (to match with array's fixed value type)
                        *_pEvalStackTop->varOrConst.value.pFloatConst = (float)charsPrinted;
                    }
                    else {      // either variable has integer type, or the variable is scalar (any type): store integer value in it, and set type 
                        *_pEvalStackTop->varOrConst.value.pLongConst = charsPrinted;
                        *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isLong;     // if not yet long
                    }
                }
            }
        }
        break;


        // type conversion functions
        // -------------------------

        case fnccod_cint:
        {
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
            if ((argIsLongBits & (0x1 << 0))) { fcnResult.longConst = args[0].longConst; }
            else if ((argIsFloatBits & (0x1 << 0))) { fcnResult.longConst = (long)args[0].floatConst; }
            else if ((argIsStringBits & (0x1 << 0))) { fcnResult.longConst = strtol(args[0].pStringConst, nullptr, 0); }
        }
        break;

        case fnccod_cfloat:
        {
            fcnResultValueType = value_isFloat;
            fcnResult.floatConst = 0.;
            if ((argIsLongBits & (0x1 << 0))) { fcnResult.floatConst = (float)args[0].longConst; }
            else if ((argIsFloatBits & (0x1 << 0))) { fcnResult.floatConst = args[0].floatConst; }
            else if ((argIsStringBits & (0x1 << 0))) { fcnResult.floatConst = strtof(args[0].pStringConst, nullptr); }
        }
        break;

        case fnccod_cstr:
        {
            fcnResultValueType = value_isStringPointer;                                                                                 // init
            fcnResult.pStringConst = nullptr;
            if ((argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0))) {
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[30];                                                                                  // provide sufficient length to store a number
                (argIsLongBits & (0x1 << 0)) ? sprintf(fcnResult.pStringConst, "%ld", args[0].longConst) : sprintf(fcnResult.pStringConst, "%G", args[0].floatConst);

            }
            else if ((argIsStringBits & (0x1 << 0))) {
                if (args[0].pStringConst != nullptr) {
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[strlen(args[0].pStringConst) + 1];
                    strcpy(fcnResult.pStringConst, args[0].pStringConst);       // just copy the string provided as argument
                }
            }
            if (fcnResult.pStringConst != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
            #endif
            }
        }
        break;

        // math functions 
        // --------------

        case fnccod_sqrt:
        case fnccod_sin:
        case fnccod_cos:
        case fnccod_tan:
        case fnccod_asin:
        case fnccod_acos:
        case fnccod_atan:
        case fnccod_ln:
        case fnccod_log10:
        case fnccod_exp:
        case fnccod_expm1:
        case fnccod_lnp1:
        case fnccod_round:
        case fnccod_ceil:
        case fnccod_floor:
        case fnccod_trunc:
        case fnccod_abs:
        case fnccod_sign:
        case fnccod_min:
        case fnccod_max:
        case fnccod_fmod:
        {
            for (int i = 0; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
            }
            float arg1float = (argIsLongBits & (0x1 << 0)) ? (float)args[0].longConst : args[0].floatConst;                                             // keep original value in args[0]

            fcnResultValueType = value_isFloat;                                                                                         // init: return a float
            fcnResult.floatConst = 0.;                                                                                                  // init: return 0. if the Arduino function doesn't return anything

            // test arguments
            if (functionCode == fnccod_sqrt) { if (arg1float < 0.) { return result_arg_outsideRange; } }
            else if ((functionCode == fnccod_asin) || (functionCode == fnccod_acos)) { if ((arg1float < -1.) || (arg1float > 1.)) { return result_arg_outsideRange; } }
            else if ((functionCode == fnccod_ln) || (functionCode == fnccod_log10)) { if (arg1float <= 0.) { return result_arg_outsideRange; } }
            else if (functionCode == fnccod_lnp1) { if (arg1float <= -1.) { return result_arg_outsideRange; } }

            // calculate
            if (functionCode == fnccod_sqrt) { fcnResult.floatConst = sqrt(arg1float); }
            else if (functionCode == fnccod_sin) { fcnResult.floatConst = sin(arg1float); }
            else if (functionCode == fnccod_cos) { fcnResult.floatConst = cos(arg1float); }
            else if (functionCode == fnccod_tan) { fcnResult.floatConst = tan(arg1float); }
            else if (functionCode == fnccod_asin) { fcnResult.floatConst = asin(arg1float); }
            else if (functionCode == fnccod_acos) { fcnResult.floatConst = acos(arg1float); }
            else if (functionCode == fnccod_atan) { fcnResult.floatConst = atan(arg1float); }
            else if (functionCode == fnccod_ln) { fcnResult.floatConst = log(arg1float); }
            else if (functionCode == fnccod_lnp1) { fcnResult.floatConst = log1p(arg1float); }
            else if (functionCode == fnccod_exp) { fcnResult.floatConst = exp(arg1float); }
            else if (functionCode == fnccod_expm1) { fcnResult.floatConst = expm1(arg1float); }
            else if (functionCode == fnccod_log10) { fcnResult.floatConst = log10(arg1float); }
            else if (functionCode == fnccod_round) { fcnResult.floatConst = round(arg1float); }
            else if (functionCode == fnccod_trunc) { fcnResult.floatConst = trunc(arg1float); }
            else if (functionCode == fnccod_floor) { fcnResult.floatConst = floor(arg1float); }
            else if (functionCode == fnccod_ceil) { fcnResult.floatConst = ceil(arg1float); }
            // Arduino min(long 0, something greater than 0) returns a double very close to zero, but not zero (same for max()). Avoid this.
            else if ((functionCode == fnccod_min) || (functionCode == fnccod_max)) {
                if ((argIsLongBits & (0x1 << 0)) && (argIsLongBits & (0x1 << 1))) {
                    fcnResultValueType = value_isLong;
                    fcnResult.longConst = (functionCode == fnccod_min) ? min(args[0].longConst, args[1].longConst) : max(args[0].longConst, args[1].longConst);
                }
                else {
                    float arg2float = (argIsLongBits & (0x1 << 1)) ? (float)args[1].longConst : args[1].floatConst;
                    fcnResult.floatConst = ((arg1float <= arg2float) == (functionCode == fnccod_min)) ? arg1float : arg2float;
                }
            }
            else if (functionCode == fnccod_abs) {
                // avoid -0. as Arduino abs() result: use fabs() if result = float value
                if ((argIsLongBits & (0x1 << 0))) { fcnResultValueType = value_isLong; };
                (argIsLongBits & (0x1 << 0)) ? fcnResult.longConst = abs(args[0].longConst) : fcnResult.floatConst = fabs(args[0].floatConst);
            }
            else if (functionCode == fnccod_sign) { fcnResultValueType = value_isLong; fcnResult.longConst = (argIsLongBits & (0x1 << 0)) ? (args[0].longConst < 0 ? 1 : 0) : signbit(arg1float); }
            else if (functionCode == fnccod_fmod) { fcnResult.floatConst = fmod(arg1float, (argIsLongBits & (0x1 << 1)) ? args[1].longConst : args[1].floatConst); }     // second argument cast to float anyway


            // test result (do net vtest for subnormal numbers here)
            if (fcnResultValueType == value_isFloat) {
                if (isnan(fcnResult.floatConst)) { return result_undefined; }
                if (!isfinite(fcnResult.floatConst)) { return result_overflow; }
            }
        }
        break;


        // bit and byte manipulation functions
        // -----------------------------------

        // Arduino bit manipulation functions
        // arguments and return values: same as the corresponding Arduino functions
        // all arguments need to be long integers; if a value is returned, it's always a long integer
        // except for the 'read' functions (and the 'bit number to value' function), if the first argument is a variable; it's value is adapted as well

        case fnccod_bit:                // bit number -> value 

        case fnccod_bitRead:            // 2 arguments: long value, bit (0 to 31) to read. Returned: 0 or 1
        case fnccod_bitClear:           // 2 arguments: long value, bit (0 to 31) to clear. New value is returned
        case fnccod_bitSet:             // 2 arguments: long value, bit (0 to 31) to set. New value is returned 
        case fnccod_bitWrite:           // 3 arguments: long value, bit (0 to 31), new bit value (0 or 1). New value is returned 

            // extra Justina bit manipulation functons. Mask argument indicates which bits to read, set, clear or write
        case fnccod_bitsMaskedRead:     // 2 arguments: long value, mask. Returns masked value 
        case fnccod_bitsMaskedClear:    // 2 arguments: long value, mask = bits to clear: bits indicated by mask are cleared. New value is returned
        case fnccod_bitsMaskedSet:      // 2 arguments: long value, mask = bits = bits to set: bits indicated by mask are set. New value is returned 
        case fnccod_bitsMaskedWrite:    // 3 arguments: long value, mask, bits to write: value bits indicated by mask are changed. New value is returned

            // extra Justina byte manipulation functons. Byte argument indicates which byte to read or write
        case fnccod_byteRead:           // 2 arguments: long, byte to read (0 to 3). Value returned is between 0x00 and 0xFF.     
        case fnccod_byteWrite:          // 3 arguments: long, byte to write (0 to 3), value to write (lowest 8 bits of argument). New value is returned    
        {
            if (!(argIsLongBits & (0x1 << 0)) && (functionCode != fnccod_bit)) { return result_arg_integerTypeExpected; }
            for (int i = 0; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = (long)args[i].floatConst; }          // all arguments have long type now
            }

            if (functionCode == fnccod_bit && ((args[0].longConst < 0) || (args[0].longConst > 31))) { return result_arg_outsideRange; }
            if (((functionCode == fnccod_bitRead) || (functionCode == fnccod_bitClear) || (functionCode == fnccod_bitSet) || (functionCode == fnccod_bitWrite)) && ((args[1].longConst < 0) || (args[1].longConst > 31))) {
                return result_arg_outsideRange;
            }
            if (((functionCode == fnccod_byteRead) || (functionCode == fnccod_byteWrite)) && ((args[1].longConst < 0) || (args[1].longConst > 3))) { return result_arg_outsideRange; }

            fcnResultValueType = value_isLong;                                                                                          // init: return a long
            fcnResult.longConst = 0;                                                                                                    // init: return 0 if the Arduino function doesn't return anything
            uint8_t* pBytes = (uint8_t*)&(args[0].longConst);                                                                           // access individual bytes of a value

            if (functionCode == fnccod_bit) { fcnResult.longConst = 1 << args[0].longConst; }                                           // requires no variable

            else if (functionCode == fnccod_bitRead) { fcnResult.longConst = (args[0].longConst & (1 << args[1].longConst)) != 0; }     // requires no variable
            else if (functionCode == fnccod_bitClear) { fcnResult.longConst = args[0].longConst & ~(1 << args[1].longConst); }
            else if (functionCode == fnccod_bitSet) { fcnResult.longConst = args[0].longConst | (1 << args[1].longConst); }
            else if (functionCode == fnccod_bitWrite) { fcnResult.longConst = (args[2].longConst == 0) ? args[0].longConst & ~(1 << args[1].longConst) : args[0].longConst | (1 << args[1].longConst); }

            else if (functionCode == fnccod_bitsMaskedRead) { fcnResult.longConst = (args[0].longConst & args[1].longConst); }          // requires no variable; second argument is considered mask
            else if (functionCode == fnccod_bitsMaskedClear) { fcnResult.longConst = args[0].longConst & ~args[1].longConst; }
            else if (functionCode == fnccod_bitsMaskedSet) { fcnResult.longConst = args[0].longConst | args[1].longConst; }
            else if (functionCode == fnccod_bitsMaskedWrite) { fcnResult.longConst = args[0].longConst & (~args[1].longConst | args[2].longConst) | (args[1].longConst & args[2].longConst); }

            else if (functionCode == fnccod_byteRead) { fcnResult.longConst = pBytes[args[1].longConst]; }                              // requires variable; contents of 1 byte is returned
            else if (functionCode == fnccod_byteWrite) { pBytes[args[1].longConst] = args[2].longConst; fcnResult.longConst = args[0].longConst; }  // new variable value is returned
        }


        // function modifies variable (first argument) ?
        if ((functionCode == fnccod_bitClear) || (functionCode == fnccod_bitSet) || (functionCode == fnccod_bitWrite) ||
            (functionCode == fnccod_bitsMaskedClear) || (functionCode == fnccod_bitsMaskedSet) || (functionCode == fnccod_bitsMaskedWrite) ||
            (functionCode == fnccod_byteWrite)) {

            bool isConstant = (!(argIsVarBits & (0x1 << 0)) || (_pEvalStackMinus2->varOrConst.sourceVarScopeAndFlags & var_isConstantVar)); // first argument is variable ? then store result
            if (!isConstant) { *_pEvalStackMinus2->varOrConst.value.pLongConst = fcnResult.longConst; }                                      // (note: it's a long already - tested above)
        }
        break;


        // hardware memory address 8 bit and 32 bit read and write
        // -------------------------------------------------------

        // intended to directly read and write to memory locations mapped to peripheral registers (I/O, counters, ...)
        // !!! dangerous if you don't know what you're doing 

        case    fnccod_mem32Read:
        case    fnccod_mem32Write:
        case    fnccod_mem8Read:
        case    fnccod_mem8Write:
        {
            if (!(argIsLongBits & (0x1 << 0))) { return result_arg_integerTypeExpected; }           // memory address must be integer
            for (int i = 1; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = (long)args[i].floatConst; }          // all arguments have long type now
            }

            if (((functionCode == fnccod_mem8Read) || (functionCode == fnccod_mem8Write)) && ((args[1].longConst < 0) || (args[1].longConst > 3))) { return result_arg_outsideRange; }

            fcnResultValueType = value_isLong;                                                                                          // init: return a long
            fcnResult.longConst = 0;                                                                                                    // init: return 0 if the Arduino function doesn't return anything

            args[0].longConst &= ~0x3;                                                                                                  // align with word size

            // write functions: the memory / peripheral register location is not read afterwards (because this could trigger a specific hardware action),
            // so write functions return zero)  
            if (functionCode == fnccod_mem32Read) { fcnResult.longConst = *(uint32_t*)args[0].longConst; }                              // 32 bit register value is returned
            else if (functionCode == fnccod_mem8Read) { fcnResult.longConst = ((uint8_t*)(args[0].longConst))[args[1].longConst]; }     // 8 bit register value is returned
            else if (functionCode == fnccod_mem32Write) { *(uint32_t*)args[0].longConst = args[1].longConst; }                          // register contents has not been read: zero is returned
            else if (functionCode == fnccod_mem8Write) { ((uint8_t*)(args[0].longConst))[args[1].longConst] = args[2].longConst; }      // register contents has not been read: zero is returned

        }
        break;


        // Arduino timing and digital I/O functions 
        // ----------------------------------------

        // all arguments can be long or float; if a value is returned, it's always a long integer
        // Note that, as Justina 'integer' constants and variables are internally represented by (signed) long values, large values returned by certain functions 
        // may show up as negative values (if greater then or equal to 2^31)
        // arguments and return values: same as the corresponding Arduino functions

        case fnccod_millis:
        case fnccod_micros:
        case fnccod_delay:
        case fnccod_delayMicroseconds:
        case fnccod_digitalRead:
        case fnccod_digitalWrite:
        case fnccod_pinMode:
        case fnccod_analogRead:
        case fnccod_analogReference:
        case fnccod_analogWrite:
        case fnccod_analogReadResolution:
        case fnccod_analogWriteResolution:
        case fnccod_noTone:
        case fnccod_pulseIn:
        case fnccod_shiftIn:
        case fnccod_shiftOut:
        case fnccod_tone:
        case fnccod_random:
        case fnccod_randomSeed:
        {
            // for all arguments provided: check they are Justina integers or floats
            // no additional checks are done (e.g. floats with fractions)
            for (int i = 0; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = int(args[i].floatConst); }                                                     // all these functions need integer values
            }
            fcnResultValueType = value_isLong;      // init: return a long
            fcnResult.longConst = 0;                // init: return 0 if the Arduino function doesn't return anything

            if (functionCode == fnccod_millis) { fcnResult.longConst = millis(); }
            else if (functionCode == fnccod_micros) { fcnResult.longConst = micros(); }
            else if (functionCode == fnccod_delay) { delay((unsigned long)args[0].longConst); }                                         // args: milliseconds    
            else if (functionCode == fnccod_delayMicroseconds) { delayMicroseconds((unsigned long)args[0].longConst); }                 // args: milliseconds 
            else if (functionCode == fnccod_digitalRead) { fcnResult.longConst = digitalRead(args[0].longConst); }                      // arg: pin
            else if (functionCode == fnccod_digitalWrite) { digitalWrite(args[0].longConst, args[1].longConst); }                       // args: pin, value
            else if (functionCode == fnccod_pinMode) { pinMode(args[0].longConst, args[1].longConst); }                                 // args: pin, pin mode
            else if (functionCode == fnccod_analogRead) { fcnResult.longConst = analogRead(args[0].longConst); }                        // arg: pin
        #if !defined(ARDUINO_ARCH_RP2040)                                                                                               // Arduino RP2040: prevent linker error
            else if (functionCode == fnccod_analogReference) { analogReference(args[0].longConst); }                                    // arg: reference type (0 to 5: see ARduino ref - 2 is external)
        #endif
            else if (functionCode == fnccod_analogWrite) { analogWrite(args[0].longConst, args[1].longConst); }                         // args: pin, value
            else if (functionCode == fnccod_analogReadResolution) { analogReadResolution(args[0].longConst); }                          // arg: bits
            else if (functionCode == fnccod_analogWriteResolution) { analogWriteResolution(args[0].longConst); }                        // arg: bits
            else if (functionCode == fnccod_noTone) { noTone(args[0].longConst); }                                                      // arg: pin
            else if (functionCode == fnccod_pulseIn) {                                                                                  // args: pin, value, (optional) time out
                fcnResult.longConst = (suppliedArgCount == 2) ? pulseIn(args[0].longConst, args[1].bytes[0]) :
                    pulseIn(args[0].longConst, args[1].bytes[0], (uint32_t)args[2].longConst);
            }
            else if (functionCode == fnccod_shiftIn) {                                                                                  // args: data pin, clock pin, bit order
                fcnResult.longConst = shiftIn(args[0].longConst, args[1].longConst, (BitOrder)args[2].longConst);
            }
            else if (functionCode == fnccod_shiftOut) {                                                                                 // args: data pin, clock pin, bit order, value
                shiftOut(args[0].longConst, args[1].longConst, (BitOrder)args[2].longConst, args[3].longConst);
            }
            else if (functionCode == fnccod_tone) {                                                                                     // args: pin, frequency, (optional) duration
                (suppliedArgCount == 2) ? tone(args[0].longConst, args[1].longConst) : tone(args[0].longConst, args[1].longConst, args[2].longConst);
            }
            else if (functionCode == fnccod_random) {                                                                                   //args: bondaries
                fcnResult.longConst = (suppliedArgCount == 1) ? random(args[0].longConst) : random(args[0].longConst, args[1].longConst);
            }
            else if (functionCode == fnccod_randomSeed) { randomSeed(args[0].longConst); }                                              // arg: seed
        }
        break;


        // 'character' functions
        // ---------------------

        // first argument must be a non-empty string; optional argument must point to a character in the string (1 to string length)
        // if a value is returned, it's always a long integer (if boolean: 0 (false) or not 0 (true)) 

        case fnccod_isAlpha:
        case fnccod_isAlphaNumeric:
        case fnccod_isAscii:
        case fnccod_isControl:
        case fnccod_isDigit:
        case fnccod_isGraph:
        case fnccod_isHexadecimalDigit:
        case fnccod_isLowerCase:
        case fnccod_isPrintable:
        case fnccod_isPunct:
        case fnccod_isSpace:
        case fnccod_isUpperCase:
        case fnccod_isWhitespace:
        case fnccod_asc:

        {
            // check that non-empty string is provided; if second argument is given, check it's within range
            // no additional checks are done (e.g. floats with fractions)
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
            int length = strlen(args[0].pStringConst);
            int charPos = 1;                                                                                                            // first character in string
            if (suppliedArgCount == 2) {
                if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
                charPos = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : int(args[1].floatConst);
                if ((args[1].longConst < 1) || (args[1].longConst > length)) { return result_arg_outsideRange; }
            }
            fcnResultValueType = value_isLong;                                                                                          // init: return a long
            fcnResult.longConst = 0;                                                                                                    // init: return 0 if the Arduino function doesn't return anything

            if (functionCode == fnccod_isAlpha) { fcnResult.longConst = isalpha(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isAlphaNumeric) { fcnResult.longConst = isAlphaNumeric(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isAscii) { fcnResult.longConst = isAscii(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isControl) { fcnResult.longConst = isControl(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isDigit) { fcnResult.longConst = isDigit(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isGraph) { fcnResult.longConst = isGraph(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isHexadecimalDigit) { fcnResult.longConst = isHexadecimalDigit(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isLowerCase) { fcnResult.longConst = isLowerCase(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isPrintable) { fcnResult.longConst = isPrintable(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isPunct) { fcnResult.longConst = isPunct(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isSpace) { fcnResult.longConst = isSpace(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isUpperCase) { fcnResult.longConst = isUpperCase(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isWhitespace) { fcnResult.longConst = isWhitespace(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_asc) { fcnResult.longConst = args[0].pStringConst[--charPos]; }
        }
        break;


        // string functions
        // ----------------

        case fnccod_char:                                                                                                               // convert ASCII code (argument) to 1-character string
        {
            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int asciiCode = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);
            if ((asciiCode < 0) || (asciiCode > 0xFE)) { return result_arg_outsideRange; }                                              // do not accept 0xFF

            // result is string
            fcnResultValueType = value_isStringPointer;
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[2];
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            
            fcnResult.pStringConst[0] = asciiCode;
            fcnResult.pStringConst[1] = '\0';                                                                                           // terminating \0
        }
        break;


        case fnccod_len:                                                                                                                // return length of a string
        {
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;                                                                                                    // init
            if (args[0].pStringConst != nullptr) { fcnResult.longConst = strlen(args[0].pStringConst); }
        }
        break;


        case fnccod_nl:                                                                                                                 // return CR and LF character string
        {
            // result is string
            fcnResultValueType = value_isStringPointer;
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[3];
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            
            fcnResult.pStringConst[0] = '\r';
            fcnResult.pStringConst[1] = '\n';
            fcnResult.pStringConst[2] = '\0';                                                                                           // terminating \0
        }
        break;

        case fnccod_space:                                                                                                              // create a string with n spaces
        case fnccod_repchar:                                                                                                            // create a string with n times the first character in the argument string
        {
            fcnResultValueType = value_isStringPointer;                                                                                 // init
            fcnResult.pStringConst = nullptr;

            char c{ ' ' };                                                                                                              // init
            if (functionCode == fnccod_repchar) {
                if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
                if (args[0].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
                c = args[0].pStringConst[0];                                                                                            // only first character in string will be repeated
            }

            int lengthArg = (functionCode == fnccod_repchar) ? 1 : 0;                                                                   // index for argument containing desired length of result string
            if (!(argIsLongBits & (0x1 << lengthArg)) && !(argIsFloatBits & (0x1 << lengthArg))) { return result_arg_numberExpected; }
            int len = ((argIsLongBits & (0x1 << lengthArg))) ? args[lengthArg].longConst : (long)args[lengthArg].floatConst;                            // convert to long if needed
            if ((len <= 0) || (len > MAX_ALPHA_CONST_LEN)) { return result_arg_outsideRange; }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len + 1];                                                                                 // space for terminating '0'
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            

            for (int i = 0; i <= len - 1; ++i) { fcnResult.pStringConst[i] = c; }
            fcnResult.pStringConst[len] = '\0';
        }
        break;


        case fnccod_strcmp:
        case fnccod_strcasecmp:
        {
            // arguments: string, substring to search for
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;                                                                                                    // init: nothing found

            if (!(argIsStringBits & (0x1 << 0)) || !(argIsStringBits & (0x1 << 1))) { return result_arg_stringExpected; }
            if ((args[0].pStringConst == nullptr) || (args[1].pStringConst == nullptr)) {
                if ((args[0].pStringConst == nullptr) && (args[1].pStringConst == nullptr)) { break; }                                  // equal
                else { fcnResult.longConst = (args[0].pStringConst == nullptr) ? -1 : 1; break; }
            }

            fcnResult.longConst = (functionCode == fnccod_strcmp) ? strcmp(args[0].pStringConst, args[1].pStringConst) : strcasecmp(args[0].pStringConst, args[1].pStringConst);                                                   // none of the strings is an empty string
            if (fcnResult.longConst < 0) { fcnResult.longConst = -1; }
            else if (fcnResult.longConst > 0) { fcnResult.longConst = 1; }
        }
        break;


        case fnccod_strstr:
        {
            // arguments: string, substring to search for [, start]
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;                        // init: nothing found

            if (!(argIsStringBits & (0x1 << 0)) || !(argIsStringBits & (0x1 << 1))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { break; }                                                                             // string is empty: a substring (even if empty) can not be found
            else if (args[1].pStringConst == nullptr) { fcnResult.longConst = 1; break; }                                               // string is empty: an immediate match at start of string

            char* startSearchChar = args[0].pStringConst;                                                                               // init: search for a match from start of string
            if (suppliedArgCount == 3) {
                if (!(argIsLongBits & (0x1 << 2)) && !(argIsFloatBits & (0x1 << 2))) { return result_arg_numberExpected; }
                int offset = ((argIsLongBits & (0x1 << 2)) ? args[2].longConst : (long)args[2].floatConst) - 1;
                if ((offset < 0) || (offset >= strlen(args[0].pStringConst))) { return result_arg_outsideRange; }
                startSearchChar += offset;
            }

            char* substringStart = strstr(startSearchChar, args[1].pStringConst);
            if (substringStart != nullptr) { fcnResult.longConst = substringStart - args[0].pStringConst + 1; }
        }
        break;

        case fnccod_toupper:
        case fnccod_tolower:
        {
            // arguments: string [, start [, end]])
            // if string only as argument, start = first character, end = last character
            // if start is specified, and end is not, then end = start
            fcnResultValueType = value_isStringPointer;            // init
            fcnResult.pStringConst = nullptr;

            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { if (suppliedArgCount > 1) { return  result_arg_outsideRange; } else { break; } }     // result is empty string, but only one argument accepted

            int len = strlen(args[0].pStringConst);
            int first = 0, last = len - 1;                                                                                              // init: complete string
            for (int i = 1; i < suppliedArgCount; ++i) {                                                                                // skip if only one argument (string)
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = int(args[i].floatConst); }                                                     // all these functions need integer values
                if (i == 1) { first = args[i].longConst - 1; last = 1; }
                else { last = args[i].longConst - 1; }
            }
            if ((first > last) || (first < 0) || (last >= len)) { return result_arg_outsideRange; }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len + 1];                                                                                 // same length as original, space for terminating 
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            
            strcpy(fcnResult.pStringConst, args[0].pStringConst);   // copy original string
            for (int i = first; i <= last; i++) { fcnResult.pStringConst[i] = ((functionCode == fnccod_toupper) ? toupper(fcnResult.pStringConst[i]) : tolower(fcnResult.pStringConst[i])); }
        }
        break;


        case fnccod_left:       // arguments: string, number of characters, starting from left, to return
        case fnccod_right:      // arguments: string, number of characters, starting from right, to return
        case fnccod_mid:        // arguments: string, first character to return (starting from left), number of characters to return
        {

            fcnResultValueType = value_isStringPointer;                                                                                 // init
            fcnResult.pStringConst = nullptr;

            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }

            for (int i = 1; i < suppliedArgCount; ++i) {                                                                                // skip first argument (string)
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = int(args[i].floatConst); }                                                     // all these functions need integer values
            }
            int len = strlen(args[0].pStringConst);

            int first = (functionCode == fnccod_left) ? 0 : (functionCode == fnccod_mid) ? args[1].longConst - 1 : len - args[1].longConst;
            int last = (functionCode == fnccod_left) ? args[1].longConst - 1 : (functionCode == fnccod_mid) ? first + args[2].longConst - 1 : len - 1;

            if ((first > last) || (first < 0) || (last >= len)) { return result_arg_outsideRange; }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[last - first + 1];                                                                        // space for terminating '0'
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            
            memcpy(fcnResult.pStringConst, args[0].pStringConst + first, last - first + 1);
            fcnResult.pStringConst[last - first + 1] = '\0';
        }
        break;


        case fnccod_ltrim:                                                                                                              // left trim, right trim, left & right trim
        case fnccod_rtrim:
        case fnccod_trim:
        {
            fcnResultValueType = value_isStringPointer;                                                                                 // init
            fcnResult.pStringConst = nullptr;

            int spaceCnt{ 0 };                                                                                                          // init
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { break; }                                                                             // original string is empty: return with empty result string

            int len = strlen(args[0].pStringConst);
            char* p = args[0].pStringConst;

            // trim leading spaces ?
            if ((functionCode == fnccod_ltrim) || (functionCode == fnccod_trim)) {                                                      // trim leading spaces ?
                while (*p == ' ') { p++; };
                spaceCnt = p - args[0].pStringConst;                                                                                    // subtraction of two pointers
            }
            if (spaceCnt == len) { break; }                                                                                             // trimmed string is empty: return with empty result string

            // trim trailing spaces ? (string does not only contain spaces)
            char* q = args[0].pStringConst + len - 1;                                                                                   // last character
            if ((functionCode == fnccod_rtrim) || (functionCode == fnccod_trim)) {
                while (*q == ' ') { q--; };
                spaceCnt += (args[0].pStringConst + len - 1 - q);
            }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len - spaceCnt + 1];                                                                      // space for terminating '0'
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            
            memcpy(fcnResult.pStringConst, p, len - spaceCnt);
            fcnResult.pStringConst[len - spaceCnt] = '\0';
        }
        break;

        case fnccod_strhex:
        {
            fcnResultValueType = value_isStringPointer;                                                                                 // init
            fcnResult.pStringConst = nullptr;

            int spaceCnt{ 0 };                                                                                                          // init
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { break; }                                                                             // original string is empty: return with empty result string

            int len = strlen(args[0].pStringConst);

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[2 * len + 1];                                                                               // 2 hex digits per character, space for terminating '0'
        #if PRINT_HEAP_OBJ_CREA_DEL
            Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
        #endif            
            for (int i = 0, j = 0; i < len; i++, j += 2) { sprintf(fcnResult.pStringConst + j, "%x", args[0].pStringConst[i]); }
            fcnResult.pStringConst[2 * len] = '\0';
        }
        break;

        case fnccod_quote:
        {
            fcnResultValueType = value_isStringPointer;                                                                                 // init
            fcnResult.pStringConst = nullptr;

            if ((argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0))) {
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[30];                                                                                  // provide sufficient length to store a number
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
            #endif            
                (argIsLongBits & (0x1 << 0)) ? sprintf(fcnResult.pStringConst, "%ld", args[0].longConst) : sprintf(fcnResult.pStringConst, "%G", args[0].floatConst);
            }

            else if (argIsStringBits & (0x1 << 0)) {
                fcnResult.pStringConst = args[0].pStringConst;
                quoteAndExpandEscSeq(fcnResult.pStringConst);     // returns a new intermediate string on the heap (never a null pointer)
            }
        }
        break;


        // retrieve a system variable
        // --------------------------

        case fnccod_sysVal:
        {
            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int sysVal = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);

            fcnResultValueType = value_isLong;                                                                                          // default for most system values

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
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[2];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
                #endif
                    strcpy(fcnResult.pStringConst, (sysVal == 4) ? _dispNumSpecifier : _printNumSpecifier);
                }
                break;

                case 10: fcnResult.longConst = _promptAndEcho; break;
                case 11: fcnResult.longConst = _printLastResult; break;
                case 12: fcnResult.longConst = _userCBprocStartSet_count; break;
                case 13: fcnResult.longConst = _userCBprocAliasSet_count; break;

                case 14:
                {
                    fcnResultValueType = value_isStringPointer;
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[MAX_IDENT_NAME_LEN + 1];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
                #endif
                    strcpy(fcnResult.pStringConst, _programName);
                }
                break;

                case 15:    // product name
                case 16:    // legal copy right
                case 17:    // product version 
                case 18:    // build date
                {
                    fcnResultValueType = value_isStringPointer;
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[((sysVal == 15) ? strlen(ProductName) : (sysVal == 16) ? strlen(LegalCopyright) : (sysVal == 17) ? strlen(ProductVersion) : strlen(BuildDate)) + 1];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
                #endif
                    strcpy(fcnResult.pStringConst, (sysVal == 15) ? ProductName : (sysVal == 16) ? LegalCopyright : (sysVal == 17) ? ProductVersion : BuildDate);
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
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[13 * 5];                              // includes place for 13 times 5 characters (3 digits max. for each number, max. 2 extra in between) and terminating \0
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
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

                case 25:                                                                                                                // return trace string
                {
                    fcnResultValueType = value_isStringPointer;
                    fcnResult.pStringConst = nullptr;                                                                                   // init (empty string)
                    if (_pTraceString != nullptr) {
                        _intermediateStringObjectCount++;
                        fcnResult.pStringConst = new char[strlen(_pTraceString) + 1];
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
                    #endif
                        strcpy(fcnResult.pStringConst, _pTraceString);
                    }
                }
                break;

                case 26:                                        // created list object count (across linked lists)
                {
                    fcnResultValueType = value_isLong;
                    fcnResult.longConst = evalStack.getCreatedObjectCount();        // pick any list: count is across lists
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

        _pEvalStackTop->varOrConst.value = fcnResult;                                                                                   // long, float or pointer to string
        _pEvalStackTop->varOrConst.valueType = fcnResultValueType;                                                                      // value type of second operand  
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;                                                                          // use generic constant type
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;                                                                           // not an array, not an array element (it's a constant) 
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
            if ((valueType[argNo - 1] != value_isLong) && (valueType[argNo - 1] != value_isFloat)) { return result_arg_numberExpected; }                                               // numeric ?
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
    fmtString[strPos] = '*'; ++strPos; fmtString[strPos] = '.'; ++strPos; fmtString[strPos] = '*'; ++strPos;             // width and precision specified with additional arguments (*.*)
    if (isIntFmt) { fmtString[strPos] = 'l'; ++strPos; fmtString[strPos] = numFmt[0]; ++strPos; }
    else { fmtString[strPos] = numFmt[0]; ++strPos; }
    fmtString[strPos] = '%'; ++strPos; fmtString[strPos] = 'n'; ++strPos; fmtString[strPos] = '\0'; ++strPos;            // %n specifier (return characters printed)

    return;
}


// -----------------------------------------------------------------------
// format number or string according to format string (result is a string)
// -----------------------------------------------------------------------

void  Justina_interpreter::printToString(int width, int precision, bool inputIsString, bool isIntFmt, char* valueType, Val* value, char* fmtString,
    Val& fcnResult, int& charsPrinted, bool expandStrings) {
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

    _intermediateStringObjectCount++;
    fcnResult.pStringConst = new char[resultStrLen + 1];
#if PRINT_HEAP_OBJ_CREA_DEL
    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)fcnResult.pStringConst, HEX);
#endif

    if (inputIsString) {
        if (expandStrings) {
            if ((*value).pStringConst != nullptr) {
                char* pString = (*value).pStringConst;                          // remember pointer to original string
                quoteAndExpandEscSeq((*value).pStringConst);                    // creates new string
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)pString, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] pString;                                               // delete old string
            }
        }
        sprintf(fcnResult.pStringConst, fmtString, width, precision, ((*value).pStringConst == nullptr) ? (expandStrings ? "\"\"" : "") : (*value).pStringConst, &charsPrinted);
    }
    else if (isIntFmt) { sprintf(fcnResult.pStringConst, fmtString, width, precision, (*valueType == value_isLong) ? (*value).longConst : (long)(*value).floatConst, &charsPrinted); }     // hex output for floating point numbers not provided (Arduino)
    else { sprintf(fcnResult.pStringConst, fmtString, width, precision, (*valueType == value_isLong) ? (float)(*value).longConst : (*value).floatConst, &charsPrinted); }

    return;
}


// -------------------------------------------------------------------------
// delete a variable string object referenced in an evaluation stack element
// -------------------------------------------------------------------------

// if not a string, then do nothing. If not a string, or string is empty, then exit WITH error

Justina_interpreter::execResult_type Justina_interpreter::deleteVarStringObject(LE_evalStack* pStackLvl) {
    if (pStackLvl->varOrConst.tokenType != tok_isVariable) { return result_execOK; };                            // not a variable
    if ((*pStackLvl->varOrConst.varTypeAddress & value_typeMask) != value_isStringPointer) { return result_execOK; }      // not a string object
    if (*pStackLvl->varOrConst.value.ppStringConst == nullptr) { return result_execOK; }

    char varScope = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);

    // delete variable string object
#if PRINT_HEAP_OBJ_CREA_DEL
    Serial.print((varScope == var_isUser) ? "----- (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
    Serial.println((uint32_t)*pStackLvl->varOrConst.value.ppStringConst, HEX);
#endif
    (varScope == var_isUser) ? _userVarStringObjectCount-- : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount-- : _localVarStringObjectCount--;
    delete[] * pStackLvl->varOrConst.value.ppStringConst;
    return result_execOK;
}


// ------------------------------------------------------------------------------
// delete an intermediate string object referenced in an evaluation stack element
// ------------------------------------------------------------------------------

// if not a string, then do nothing. If not an intermediate string object, then exit WITHOUT error

Justina_interpreter::execResult_type Justina_interpreter::deleteIntermStringObject(LE_evalStack* pStackLvl) {

    if ((pStackLvl->varOrConst.valueAttributes & constIsIntermediate) != constIsIntermediate) { return result_execOK; }       // not an intermediate constant
    if (pStackLvl->varOrConst.valueType != value_isStringPointer) { return result_execOK; }                                   // not a string object
    if (pStackLvl->varOrConst.value.pStringConst == nullptr) { return result_execOK; }
#if PRINT_HEAP_OBJ_CREA_DEL
    Serial.print("----- (Intermd str) ");   Serial.println((uint32_t)_pEvalStackTop->varOrConst.value.pStringConst, HEX);
#endif
    _intermediateStringObjectCount--;
    delete[] pStackLvl->varOrConst.value.pStringConst;

    return result_execOK;
}

// ---------------------------------------------------------------------------
// copy command arguments or internal function arguments from evaluation stack
// ---------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::copyValueArgsFromStack(LE_evalStack*& pStackLvl, int argCount, bool* argIsNonConstantVar, bool* argIsArray, char* valueType, Val* args, bool prepareForCallback, Val* dummyArgs) {
    execResult_type execResult;


    for (int i = 0; i < argCount; i++) {
        bool argIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);                                                        // could be a constant variable
        argIsNonConstantVar[i] = argIsVar && (!(pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));                 // is a constant variable
        bool argIsConstant = !(argIsNonConstantVar[i] && argIsVar);                                                                 // constant variable or pure constant

        argIsArray[i] = argIsVar ? (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isArray) : false;
        valueType[i] = argIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;

        args[i].longConst = (argIsVar ? (*pStackLvl->varOrConst.value.pLongConst) : pStackLvl->varOrConst.value.longConst);         // retrieve value (valid for ALL value types)
        if (prepareForCallback) {                                                                                                   // preparing for callback function 
            // numeric argument ?
            if (((valueType[i] & value_typeMask) == value_isLong) || ((valueType[i] & value_typeMask) == value_isFloat)) {
                // numeric CONSTANT argument: make a copy of the actual data (not the pointers to it: these will be copied, for safety as well, upon return of the present function)
                if (argIsConstant) { dummyArgs[i].longConst = args[i].longConst; args[i].pLongConst = &(dummyArgs[i].longConst); }  // pure or variable constant: pass address of copied value
                else { args[i].pLongConst = pStackLvl->varOrConst.value.pLongConst; }                                               // changeable variable: retrieve address of original value
            }

            // string argument ?
            else if ((valueType[i] & value_typeMask) == value_isStringPointer) {        // for callback calls only      
                char* pOriginalArg = args[i].pStringConst;                              // pointer to Justina variable or constant string
                int strLength{ 0 };
                // empty (null pointer) and constant strings: create a temporary string (empty but null-terminated or copy of the non-empty string)
                if ((args[i].pStringConst == nullptr) || argIsConstant) {               // note: non-empty variable strings (only): pointer keeps pointing to variable string (no copy)           
                    valueType[i] |= passCopyToCallback;                                 // flag that a copy has been made (it will have to be deleted afterwards))
                    strLength = (args[i].pStringConst == nullptr) ? 0 : strlen(args[i].pStringConst);

                    _intermediateStringObjectCount++;                                   // temporary string object will be deleted right after return from call to user callback routine
                    args[i].pStringConst = new char[strLength + 1];                     // change pointer to copy of string
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (Intermd str) ");   Serial.println((uint32_t)args[i].pStringConst, HEX);
                #endif

                    if (strLength == 0) { args[i].pStringConst[0] = '\0'; }             // empty string (sole character is null-character as terminator)
                    else { strcpy(args[i].pStringConst, pOriginalArg); }                // non-empty constant string
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
        _localVarValueAreaCount++;
        _activeFunctionData.pLocalVarValues = new Val[localVarCount];              // local variable value: real, pointer to string or array, or (if reference): pointer to 'source' (referenced) variable
        _activeFunctionData.ppSourceVarTypes = new char* [localVarCount];           // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        _activeFunctionData.pVariableAttributes = new char[localVarCount];         // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) 

    #if PRINT_HEAP_OBJ_CREA_DEL
        Serial.print("+++++ (LOCAL STORAGE) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues, HEX);
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


// ------------------------------------------------
// launch parsing and execution of an eval() string
// ------------------------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::launchEval(LE_evalStack*& pFunctionStackLvl, char* parsingInput) {

    execResult_type execResult{ result_execOK };

    if (parsingInput == nullptr) { return result_eval_nothingToEvaluate; }


    // push current command line storage to command line stack, to make room for the evaluation string (to parse) 
    // ----------------------------------------------------------------------------------------------------------

    // the parsed command line pushed, contains the parsed statements 'calling' (parsing and executing) the eval() string 
    // this is either an outer level parsed eval() string, or the parsed command line where execution started  

#if PRINT_PARSED_STAT_STACK
    Serial.print("  >> PUSH parsed statements (launch eval): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + _progMemorySize));
#endif
    long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
    _pImmediateCmdStackTop = (char*)immModeCommandStack.appendListElement(sizeof(char*) + parsedUserCmdLen);
    *(char**)_pImmediateCmdStackTop = _lastUserCmdStep;
    memcpy(_pImmediateCmdStackTop + sizeof(char*), (_programStorage + _progMemorySize), parsedUserCmdLen);

    // parse eval() string
    // -------------------
    char* pDummy{};
    char* holdProgramCounter = _programCounter;
    _programCounter = _programStorage + _progMemorySize;                                  // parsed statements go to immediate mode program memory
    _parsingEvalString = true;

    // create a temporary string to hold expressions to parse, with an extra semicolon added at the end (in case it's missing)
    _systemVarStringObjectCount++;
    char* pEvalParsingInput = new char[strlen(parsingInput) + 2]; // room for additional semicolon (in case string is not ending with it) and terminating '\0'
#if PRINT_HEAP_OBJ_CREA_DEL
    Serial.print("+++++ (system var str) "); Serial.println((uint32_t)pEvalParsingInput, HEX);
#endif

    strcpy(pEvalParsingInput, parsingInput);              // copy the actual string
    pEvalParsingInput[strlen(parsingInput)] = term_semicolon[0];
    pEvalParsingInput[strlen(parsingInput) + 1] = '\0';
    char* pParsingInput_temp = pEvalParsingInput;        // temp, because value will be changed upon return (preserve original pointer value)
    // note: application flags are not adapted (would not be passed to caller immediately)
    int dummy{};
    parseTokenResult_type result = parseStatement(pParsingInput_temp, pDummy, dummy);           // parse all eval() expressions in ONE go (which is not the case for standard parsing and trace string parsing)
#if PRINT_HEAP_OBJ_CREA_DEL
    Serial.print("----- (system var str) "); Serial.println((uint32_t)pEvalParsingInput, HEX);
#endif
    _systemVarStringObjectCount--;
    delete[] pEvalParsingInput;

    // last step of just parsed eval() string. Note: adding sizeof(tok_no_token) because not yet added
    _lastUserCmdStep = ((result == result_tokenFound) ? _programCounter + sizeof(tok_no_token) : nullptr);    // if parsing error, store nullptr as last token position

    _parsingEvalString = false;
    if (result != result_tokenFound) {
        // immediate mode program memory now contains a PARTIALLY parsed eval() expression string (up to the token producing a parsing error) and a few string constants may have been created in the process.
        // restore the situation from BEFORE launching the parsing of this now partially parsed eval() expression:   
        // delete any newly parsed string constants created in the parsing attempt
        // pop the original imm.mode parsed statement stack top level again to imm. mode program memory
        // a corresponding entry in flow ctrl stack has not yet been created either)
        deleteConstStringObjects(_programStorage + _progMemorySize);      // string constants that were created just now 
        memcpy((_programStorage + _progMemorySize), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);
        immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
        _pImmediateCmdStackTop = (char*)immModeCommandStack.getLastListElement();
    #if PRINT_PARSED_STAT_STACK
        Serial.print("  >> POP parsed statements (launch eval parse error): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + _progMemorySize));
    #endif

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

    _activeFunctionData.pNextStep = _programStorage + _progMemorySize;                     // first step in first statement in parsed eval() string
    _activeFunctionData.errorStatementStartStep = _programStorage + _progMemorySize;
    _activeFunctionData.errorProgramCounter = _programStorage + _progMemorySize;

    return  result_execOK;
}


// ------------------------------------------------------------------------------------------------
// *   init parameter variables with supplied arguments (scalar parameters with default values)   *
// ------------------------------------------------------------------------------------------------

void Justina_interpreter::initFunctionParamVarWithSuppliedArg(int suppliedArgCount, LE_evalStack*& pFirstArgStackLvl) {
    // save function caller's arguments to function's local storage and remove them from evaluation stack
    if (suppliedArgCount > 0) {
        LE_evalStack* pStackLvl = pFirstArgStackLvl;                                                                        // pointing to first argument on stack
        for (int i = 0; i < suppliedArgCount; i++) {
            int valueType = pStackLvl->varOrConst.valueType;
            bool operandIsLong = (valueType == value_isLong);
            bool operandIsFloat = (valueType == value_isFloat);
            bool operandIsVariable = (pStackLvl->varOrConst.tokenType == tok_isVariable);
            bool opIsConstantVar = operandIsVariable ? (*pStackLvl->varOrConst.varTypeAddress & var_isConstantVar) : false;

            // non_constant variable (could be an array) passed ?
            if (operandIsVariable && !opIsConstantVar) {                                                                    // function argument is a variable => local value is a reference to 'source' variable
                _activeFunctionData.pLocalVarValues[i].pBaseValue = pStackLvl->varOrConst.value.pBaseValue;                 // pointer to 'source' variable
                _activeFunctionData.ppSourceVarTypes[i] = pStackLvl->varOrConst.varTypeAddress;                             // pointer to 'source' variable value type
                _activeFunctionData.pVariableAttributes[i] = value_isVarRef |
                    (pStackLvl->varOrConst.sourceVarScopeAndFlags & (var_scopeMask | var_isArray | var_isConstantVar));     // local 'ref var' value type + source variable scope, 'is array' and 'is constant' flags
            }

            // parsed, or intermediate, constant OR constant variable, passed as argument (constant: never an array)
            else {
                _activeFunctionData.pVariableAttributes[i] = valueType;                                                     // local variable value type (long, float, char*)
                if (operandIsLong || operandIsFloat) {
                    _activeFunctionData.pLocalVarValues[i].floatConst = operandIsVariable ? *pStackLvl->varOrConst.value.pFloatConst : pStackLvl->varOrConst.value.floatConst;
                }
                else {                                                                                                      // function argument is string constant: create a local copy
                    _activeFunctionData.pLocalVarValues[i].pStringConst = nullptr;                                          // init (empty string)
                    char* tempString{};
                    tempString = operandIsVariable ? *pStackLvl->varOrConst.value.ppStringConst : pStackLvl->varOrConst.value.pStringConst;
                    if (tempString != nullptr) {
                        int stringlen = strlen(tempString);
                        _localVarStringObjectCount++;
                        _activeFunctionData.pLocalVarValues[i].pStringConst = new char[stringlen + 1];
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        Serial.print("+++++ (loc var str) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues[i].pStringConst, HEX);
                    #endif
                        strcpy(_activeFunctionData.pLocalVarValues[i].pStringConst, tempString);
                    }
                };
            }

            // if intermediate constant string, then delete char string object (tested within called routine)            
            deleteIntermStringObject(pStackLvl);
            pStackLvl = (LE_evalStack*)evalStack.deleteListElement(pStackLvl);                                              // argument saved: remove argument from stack and point to next argument
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
        while (count < suppliedArgCount) { tokenType = findTokenStep(pStep, tok_isTerminalGroup1, termcod_comma); count++; }

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
                    _localVarStringObjectCount++;
                    _activeFunctionData.pLocalVarValues[count].pStringConst = new char[stringlen + 1];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    Serial.print("+++++ (loc var str) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues[count].pStringConst, HEX);
                #endif
                    strcpy(_activeFunctionData.pLocalVarValues[count].pStringConst, s);
                }
            }
            count++;
        }
    }

    // skip (remainder of) function definition
    findTokenStep(pStep, tok_isTerminalGroup1, termcod_semicolon);
};



// --------------------------------------------
// *   init local variables (non-parameter)   *
// --------------------------------------------

void Justina_interpreter::initFunctionLocalNonParamVariables(char* pStep, int paramCount, int localVarCount) {
    // upon entry, positioned at first token after FUNCTION statement

    int tokenType{}, terminalCode{};

    int count = paramCount;         // sum of mandatory and optional parameters

    while (count != localVarCount) {
        findTokenStep(pStep, tok_isReservedWord, cmdcod_var, cmdcod_constVar);     // find local 'var' or 'const' keyword (always there)

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
                _localArrayObjectCount++;
                float* pArray = new float[arrayElements + 1];
            #if PRINT_HEAP_OBJ_CREA_DEL
                Serial.print("+++++ (loc ar stor) "); Serial.println((uint32_t)pArray, HEX);
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
                            _localVarStringObjectCount++;
                            char* pVarString = new char[length + 1];          // create char array on the heap to store alphanumeric constant, including terminating '\0'
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            Serial.print("+++++ (loc var str) "); Serial.println((uint32_t)pVarString, HEX);
                        #endif

                            // store alphanumeric constant in newly created character array
                            strcpy(pVarString, pString);              // including terminating \0
                            _activeFunctionData.pLocalVarValues[count].pStringConst = pVarString;       // store pointer to string
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
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    }
    else { makeIntermediateConstant(_pEvalStackTop); }             // if not already an intermediate constant

    // delete local variable arrays and strings (only if local variable is not a reference)

    int localVarCount = extFunctionData[_activeFunctionData.functionIndex].localVarCountInFunction;      // of function to be terminated
    int paramOnlyCount = extFunctionData[_activeFunctionData.functionIndex].paramOnlyCountInFunction;      // of function to be terminated

    if (localVarCount > 0) {
        deleteStringArrayVarsStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);
        deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);

    #if PRINT_HEAP_OBJ_CREA_DEL
        Serial.print("----- (LOCAL STORAGE) ");   Serial.println((uint32_t)_activeFunctionData.pLocalVarValues, HEX);
    #endif
        _localVarValueAreaCount--;
        // release local variable storage for function that has been called
        delete[] _activeFunctionData.pLocalVarValues;
        delete[] _activeFunctionData.pVariableAttributes;
        delete[] _activeFunctionData.ppSourceVarTypes;
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


    if ((_activeFunctionData.pNextStep >= (_programStorage + _progMemorySize)) && (_callStackDepth == 0)) {   // not within a function, not within eval() execution, and not in debug mode       
        if (_localVarValueAreaCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            Serial.print("*** Local variable storage area objects cleanup error. Remaining: "); Serial.println(_localVarValueAreaCount);
        #endif
            _localVarValueAreaErrors += abs(_localVarValueAreaCount);
            _localVarValueAreaCount = 0;
        }

        if (_localVarStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            Serial.print("*** Local variable string objects cleanup error. Remaining: "); Serial.println(_localVarStringObjectCount);
        #endif
            _localVarStringObjectErrors += abs(_localVarStringObjectCount);
            _localVarStringObjectCount = 0;
        }

        if (_localArrayObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            Serial.print("*** Local array objects cleanup error. Remaining: "); Serial.println(_localArrayObjectCount);
        #endif
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
    long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
    deleteConstStringObjects(_programStorage + _progMemorySize);
    memcpy((_programStorage + _progMemorySize), _pImmediateCmdStackTop + sizeof(char*), parsedUserCmdLen);        // size berekenen
    immModeCommandStack.deleteListElement(_pImmediateCmdStackTop);
    _pImmediateCmdStackTop = (char*)immModeCommandStack.getLastListElement();
#if PRINT_PARSED_STAT_STACK
    Serial.print("  >> POP parsed statements (terminate eval): last step: "); Serial.println(_lastUserCmdStep - (_programStorage + _progMemorySize));
#endif

    if (evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels >= 1) {
        execResult = execAllProcessedOperators();
        if (execResult != result_execOK) { return execResult; }
    }
    return execResult;
}


// -----------------------------------
// *   fetch variable base address   *
// -----------------------------------

void* Justina_interpreter::fetchVarBaseAddress(TokenIsVariable* pVarToken, char*& sourceVarTypeAddress, char& selfValueType, char& sourceVarScopeAndFlags) {

    // this function fetches the data stored in a parsed token, which should have token type 'tok_isVariable' and then returns a number of data elements to be pushed on the...
    // ... evaluation stack by the caller of this function

    // Justina function parameters receiving a variable (not an expression result) as a caller's argument:
    // ->  the function's local 'parameter type' variable will not contain a copy of the 'source' variable, but a reference to it (value type 'value_isVarRef')
    //     the prefix 'self' then refers to the Justina function 'parameter' variable(indicated by the token), the prefix 'source' refers to the Jusina caller's argument variable   
    // All other cases: Justina global, static and user variables, local variables, 'parameter' variables receiving an expression result or not receiving a caller's argument (default initialisation):
    // ->  the prefix 'self' and 'source' both refer to the Justina variable itself (indicated by the token)

    // upon entry, pVarToken argument must point to a variable token in Justina PROGRAM memory 
    // upon return:
    // - selfValueType contains the value type (long, float, char* or reference) of Justina the variable indicated by the token
    // - sourceVarScopeAndFlags contains the SOURCE variable's scope, 'is array' and 'is constant variable' (declared with 'const') flags, respectively
    // - sourceVarTypeAddress points to (contains the address of) source variable's attributes (value type, ...) 
    // - return pointer will point to (contain the address of) the 'self' variable base address (Justina variable indicated by the token): the address where the variable's value is stored
    //   note that this 'value' can be an address itself: for referenced variables, but alse for arrays and strings (char*)

    int varNameIndex = pVarToken->identNameIndex;
    uint8_t varScope = pVarToken->identInfo & var_scopeMask;                                                // global, user, local, static or parameter
    bool isUserVar = (varScope == var_isUser);
    bool isGlobalVar = (varScope == var_isGlobal);
    bool isStaticVar = (varScope == var_isStaticInFunc);

    int valueIndex = pVarToken->identValueIndex;

    if (isUserVar) {
        selfValueType = userVarType[valueIndex] & value_typeMask;                                           // value type (indicating long, float, char* or 'variable reference')
        sourceVarTypeAddress = userVarType + valueIndex;                                                    // pointer to source variable's attributes          
        sourceVarScopeAndFlags = pVarToken->identInfo & (var_scopeMask | var_isArray | var_isConstantVar);
        return &userVarValues[valueIndex];                                                                  // pointer to value (long, float, char* or (array variables, strings) pointer to array or string)
    }
    else if (isGlobalVar) {
        selfValueType = globalVarType[valueIndex] & value_typeMask;                                         // value type (indicating long, float, char* or 'variable reference')
        sourceVarTypeAddress = globalVarType + valueIndex;                                                  // pointer to source variable's attributes
        sourceVarScopeAndFlags = pVarToken->identInfo & (var_scopeMask | var_isArray | var_isConstantVar);
        return &globalVarValues[valueIndex];                                                                // pointer to value (long, float, char* or (array variables, strings) pointer to array or string)
    }
    else if (isStaticVar) {
        selfValueType = staticVarType[valueIndex] & value_typeMask;                                         // value type (indicating long, float, char* or 'variable reference')
        sourceVarTypeAddress = staticVarType + valueIndex;                                                  // pointer to source variable's attributes
        sourceVarScopeAndFlags = pVarToken->identInfo & (var_scopeMask | var_isArray | var_isConstantVar);
        return &staticVarValues[valueIndex];                                                                // pointer to value (long, float, char* or (array variables, strings) pointer to array or string)
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

        int blockType = _activeFunctionData.blockType;                                                      // init
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                                                       // one level below _activeFunctionData

        // variable is a local (including parameter) value: if the current flow control stack level does not refer to a function, but to a command line or eval() block type,
        // then the variable is a local variable of a stopped program's open function 
        bool isStoppedFunctionVar = (blockType == block_extFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + _progMemorySize)) : true;     // command line or eval() block type

        if (isStoppedFunctionVar) {
            bool isDebugCmdLevel = (blockType == block_extFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + _progMemorySize)) : false;

            if (!isDebugCmdLevel) {       // find debug level in flow control stack instead
                do {
                    blockType = *(char*)pFlowCtrlStackLvl;
                    isDebugCmdLevel = (blockType == block_extFunction) ? (((OpenFunctionData*)pFlowCtrlStackLvl)->pNextStep >= (_programStorage + _progMemorySize)) : false;
                    pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                } while (!isDebugCmdLevel);                                                                 // stack level for open function found immediate below debug line found (always match)
            }

            blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
            while (blockType != block_extFunction) {
                pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
            }
        }
        else {                                                                                              // the variable is a local variable of the function referenced in _activeFunctionData
            pFlowCtrlStackLvl = &_activeFunctionData;
        }


        // note (function parameter variables only): when a function is called with a variable argument (always passed by reference), 
        // the parameter value type has been set to 'reference' when the function was called
        selfValueType = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes[valueIndex] & value_typeMask;           // local variable value type (indicating long, float, string or REFERENCE)

        if (selfValueType == value_isVarRef) {
            sourceVarTypeAddress = ((OpenFunctionData*)pFlowCtrlStackLvl)->ppSourceVarTypes[valueIndex];                    // pointer to 'source' variable attributes
            // 'SOURCE'source' variable scope (user, global, static; local, param), 'is array' and 'is constant variable' flags
            sourceVarScopeAndFlags = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes[valueIndex] & ~value_typeMask;
            return   ((Val**)((OpenFunctionData*)pFlowCtrlStackLvl)->pLocalVarValues)[valueIndex];                          // pointer to 'source' variable value 
        }

        // local variable OR parameter variable that received the result of an expression (or constant) as argument (passed by value); optional parameter variable that received no value (default initialization) 
        else {
            sourceVarTypeAddress = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes + valueIndex;                // pointer to variable attributes
            // local variable value type; 'is array'  and 'is constant var' flags
            sourceVarScopeAndFlags = pVarToken->identInfo & (var_scopeMask | var_isArray | var_isConstantVar);
            return (Val*)&(((OpenFunctionData*)pFlowCtrlStackLvl)->pLocalVarValues[valueIndex]);
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

    void* pArray = varBaseAddress;                                                                          // will point to float or string pointer (both can be array elements)
    int arrayDimCount = ((char*)pArray)[3];

    int arrayElement{ 0 };
    for (int i = 0; i < arrayDimCount; i++) {
        int arrayDim = ((char*)pArray)[i];
        if ((subscripts[i] < 1) || (subscripts[i] > arrayDim)) { return nullptr; }                          // is outside array boundaries

        int arrayNextDim = (i < arrayDimCount - 1) ? ((char*)pArray)[i + 1] : 1;
        arrayElement = (arrayElement + (subscripts[i] - 1)) * arrayNextDim;
    }
    arrayElement++;                                                                                         // add one (first array element contains dimensions and dimension count)
    return (Val*)pArray + arrayElement;                                                                     // pointer to a 4-byte array element (long, float or pointer to string)
}


// -----------------------------------------------
// *   push terminal token to evaluation stack   *
// -----------------------------------------------

void Justina_interpreter::pushTerminalToken(int& tokenType) {                                               // terminal token is assumed

    // push internal or external function index to stack

    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(TerminalTokenLvl));
    _pEvalStackTop->terminal.tokenType = tokenType;
    _pEvalStackTop->terminal.tokenAddress = _programCounter;                                                // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;                                        // terminal token only: calculate from partial index stored in high 4 bits of token type 
    _pEvalStackTop->terminal.index += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
};


// ------------------------------------------------------------------------
// *   push internal or external function name token to evaluation stack   *
// ------------------------------------------------------------------------

void Justina_interpreter::pushFunctionName(int& tokenType) {                                                // function token is assumed (internal or external)

    // push internal or external function index to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(FunctionLvl));
    _pEvalStackTop->function.tokenType = tokenType;
    _pEvalStackTop->function.tokenAddress = _programCounter;                                                // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->function.index = ((TokenIsIntFunction*)_programCounter)->tokenIndex;
};


// -------------------------------------------------------------
// *   push real or string constant token to evaluation stack   *
// -------------------------------------------------------------

void Justina_interpreter::pushConstant(int& tokenType) {                                                                                    // float or string constant token is assumed

    // push real or string parsed constant, value type and array flag (false) to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;                                                                                  // use generic constant type
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;                                                                              // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->varOrConst.valueType = ((*(char*)_programCounter) >> 4) & value_typeMask;                                               // for constants, upper 4 bits contain the value type
    _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;
    _pEvalStackTop->varOrConst.valueAttributes = 0x00;

    if ((_pEvalStackTop->varOrConst.valueType & value_typeMask) == value_isLong) {
        memcpy(&_pEvalStackTop->varOrConst.value.longConst, ((TokenIsConstant*)_programCounter)->cstValue.longConst, sizeof(long));         // float  not necessarily aligned with word size: copy memory instead
    }
    else if ((_pEvalStackTop->varOrConst.valueType & value_typeMask) == value_isFloat) {
        memcpy(&_pEvalStackTop->varOrConst.value.floatConst, ((TokenIsConstant*)_programCounter)->cstValue.floatConst, sizeof(float));      // float  not necessarily aligned with word size: copy memory instead
    }
    else {
        memcpy(&_pEvalStackTop->varOrConst.value.pStringConst, ((TokenIsConstant*)_programCounter)->cstValue.pStringConst, sizeof(void*));  // char pointer not necessarily aligned with word size: copy pointer instead
    }
};


// ---------------------------------------------------
// *   push generic name token to evaluation stack   *
// ---------------------------------------------------

void Justina_interpreter::pushGenericName(int& tokenType) {                                                 // float or string constant token is assumed

    // push real or string parsed constant, value type and array flag (false) to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    // just push the string pointer to the generic name (no indexes, ...)
    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(GenNameLvl));
    _pEvalStackTop->varOrConst.tokenType = tok_isGenericName;                                               // use generic constant type
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;                                              // only for finding source error position during unparsing (for printing)

    char* pAnum{ nullptr };
    memcpy(&pAnum, ((TokenIsConstant*)_programCounter)->cstValue.pStringConst, sizeof(pAnum)); // char pointer not necessarily aligned with word size: copy pointer instead
    _pEvalStackTop->genericName.pStringConst = pAnum;                                  // store char* in stack 
};


// ----------------------------------------------
// *   push variable token to evaluation stack   *
// ----------------------------------------------

void Justina_interpreter::pushVariable(int& tokenType) {                                                    // with variable token type

    // push variable base address, variable value type (real, string) and array flag to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackTop->varOrConst.tokenType = tokenType;
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;

    void* varAddress = fetchVarBaseAddress((TokenIsVariable*)_programCounter, _pEvalStackTop->varOrConst.varTypeAddress, _pEvalStackTop->varOrConst.valueType,
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags);
    _pEvalStackTop->varOrConst.value.pBaseValue = varAddress;                                               // base address of variable
    _pEvalStackTop->varOrConst.valueAttributes = 0;                                                         // init
    /*
    Serial.println("**    PUSHING variable to eval stack top");
    Serial.print("      source vartype address: "); Serial.println((uint32_t)_pEvalStackTop->varOrConst.varTypeAddress, HEX);
    Serial.print("      local value type: "); Serial.println(_pEvalStackTop->varOrConst.valueType, HEX);
    Serial.print("      source variable attributes: "); Serial.println(_pEvalStackTop->varOrConst.sourceVarScopeAndFlags, HEX);
    Serial.print("      var base value address: "); Serial.println((uint32_t)_pEvalStackTop->varOrConst.value.pBaseValue, HEX);
    */
}
