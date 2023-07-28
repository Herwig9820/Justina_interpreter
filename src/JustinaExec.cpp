/************************************************************************************************************
*    Justina interpreter library for Arduino boards with 32 bit SAMD microconrollers                        *
*                                                                                                           *
*    Tested with Nano 33 IoT and Arduino RP2040                                                             *
*                                                                                                           *
*    Version:    v1.01 - 12/07/2023                                                                         *
*    Author:     Herwig Taveirne, 2021-2023                                                                 *
*                                                                                                           *
*    Justina is an interpreter which does NOT require you to use an IDE to write and compile programs.      *
*    Programs are written on the PC using any text processor and transferred to the Arduino using any       *
*    Serial or TCP Terminal program capable of sending files.                                               *
*    Justina can store and retrieve programs and other data on an SD card as well.                          *
*                                                                                                           *
*    See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                           *
*    This program is free software: you can redistribute it and/or modify                                   *
*    it under the terms of the GNU General Public License as published by                                   *
*    the Free Software Foundation, either version 3 of the License, or                                      *
*    (at your option) any later version.                                                                    *
*                                                                                                           *
*    This program is distributed in the hope that it will be useful,                                        *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of                                         *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                           *
*    GNU General Public License for more details.                                                           *
*                                                                                                           *
*    You should have received a copy of the GNU General Public License                                      *
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.                                  *
************************************************************************************************************/


#include "Justina.h"

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
    _pDebugOut->print("\r\n*** enter exec: eval stack depth: "); _pDebugOut->println(evalStack.getElementCount());
#endif
    // init

    _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_executing;     // status 'executing'

    int tokenType = *startHere & 0x0F;
    int tokenIndex{ 0 };
    bool isFunctionReturn = false;
    bool precedingIsComma = false;                                                      // used to detect prefix operators following a comma separator
    bool isEndOfStatementSeparator = false;                                             // false, because this is already the start of a new instruction
    bool lastWasEndOfStatementSeparator = false;                                        // false, because this is already the start of a new instruction

    bool doStopForDebugNow = false;
    bool appFlagsRequestStop = false, appFlagsRequestAbort = false;
    bool abortCommandReceived{ false };
    bool showStopmessage{ false };
    bool doSingleStep = false;
    bool lastTokenIsSemicolon = false;                                                  // do not stop a program after an 'empty' statement (occurs when returning to caller)
    bool doSkip = false;

    execResult_type execResult = result_execOK;
    char* holdProgramCnt_StatementStart{ nullptr }, * programCnt_previousStatementStart{ nullptr };
    char* holdErrorProgramCnt_StatementStart{ nullptr }, * errorProgramCnt_previousStatement{ nullptr };

    // switch single step mode OFF before starting to execute command line (even in debug mode). Step and Debug commands will switch it on again (to execute one step).
    _stepCmdExecuted = db_continue;
    _debugCmdExecuted = false;                                                          // Justina function to debug must be on same comand line as Debug command

    _programCounter = startHere;
    holdProgramCnt_StatementStart = _programCounter; programCnt_previousStatementStart = _programCounter;

    holdErrorProgramCnt_StatementStart = _programCounter, errorProgramCnt_previousStatement = _programCounter;
    bool execError{ false };                                                            // init

    _activeFunctionData.functionIndex = 0;                                              // main program level: not relevant
    _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                            // no command is being executed
    _activeFunctionData.activeCmd_tokenAddress = nullptr;
    _activeFunctionData.errorStatementStartStep = _programCounter;
    _activeFunctionData.errorProgramCounter = _programCounter;
    _activeFunctionData.blockType = block_JustinaFunction;                              // consider main as a Justina function      

    bool setCurrentPrintColumn{ false };                                                // for print commands only

    _lastValueIsStored = false;


    // -----------------
    // 1. process tokens
    // -----------------

    while (tokenType != tok_no_token) {                                                                 // for all tokens in token list
        // if terminal token, determine which terminal type
    #if PRINT_PROCESSED_TOKEN
        _pDebugOut->print(">>>> exec: token step = "); _pDebugOut->println(_programCounter - _programStorage);
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

        int  holdCommandStartEValStackLevels{ 0 };

        // 1.1 process by token type
        // -------------------------

        switch (tokenType) {                                                                            // process according to token type

            // Case: process keyword token
            // ---------------------------

            case tok_isReservedWord:
            {

                // parse-only statements (program, function, var, local, static, ...): skip for execution

                tokenIndex = ((TokenIsResWord*)_programCounter)->tokenIndex;

            #if PRINT_DEBUG_INFO
                _pDebugOut->print("eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->print(" - keyword: "); _pDebugOut->print(_resWords[tokenIndex]._resWordName);
                _pDebugOut->print("   ["); _pDebugOut->print(_programCounter - _programStorage); _pDebugOut->println("]");
            #endif
            #if PRINT_PROCESSED_TOKEN
                _pDebugOut->print("=== process keyword: address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [");
                _pDebugOut->print(_resWords[tokenIndex]._resWordName); _pDebugOut->println("]");
            #endif
                bool skipStatement = ((_resWords[tokenIndex].restrictions & cmd_skipDuringExec) != 0);
                if (skipStatement) {
                    findTokenStep(_programCounter, tok_isTerminalGroup1, termcod_semicolon);            // find semicolon (always match)
                    _activeFunctionData.pNextStep = _programCounter;
                    break;
                }

                // commands are executed when processing final semicolon statement (note: activeCmd_ResWordCode identifies individual commands; not command blocks)
                // first, all arguments of the command will be processed
                _activeFunctionData.activeCmd_ResWordCode = _resWords[tokenIndex].resWordCode;          // store command for now
                _activeFunctionData.activeCmd_tokenAddress = _programCounter;


                // If the command is a print command (printing to a particular stream), its arguments will be evaluated before the print command itself is executed.  
                // As soon as the stream to print to is known, store the current output 'cursor' position for the stream (last column printed to; zero if new line).
                // If any of the print command arguments contains a pos() function, the correct position will then be returned.

                switch (_activeFunctionData.activeCmd_ResWordCode) {
                    // Print to console out, print to debug out ? The stream is known at this point (which is the LAUNCH op the print command, not yet the execution). Store the last column printed to


                    case cmdcod_dbout:
                    case cmdcod_dboutLine:
                    { _pLastPrintColumn = _pDebugPrintColumn;  }
                    break;

                    case cmdcod_cout:
                    case cmdcod_coutLine:
                    case cmdcod_coutList:
                    { _pLastPrintColumn = _pConsolePrintColumn;  }
                    break;


                    // Other print to stream commands ? Stream is not known yet, because first command argument (stream) still needs to be evaluated. 
                    // Set a flag to store the last column for this stream as soon as it's known (after evaluation of the first command argument).

                    case cmdcod_print:
                    case cmdcod_printLine:
                    case cmdcod_printList:
                    {
                        setCurrentPrintColumn = true;                                                   // flag that current print column for current stream is not set yet
                        holdCommandStartEValStackLevels = evalStack.getElementCount();
                    }
                    break;

                    default: {}; break;     // other print commands: see comma separator token
                }
            }
            break;


            // Case: process internal cpp function token
            // -----------------------------------------

            case tok_isInternCppFunction:
            {

                pushInternCppFunctionName(tokenType);

            #if PRINT_PROCESSED_TOKEN
                _pDebugOut->print("=== process "); _pDebugOut->print(tokenType == tok_isInternCppFunction ? "internal cpp fcn" : tokenType == tok_isExternCppFunction ? "external cpp fcn" : "Justina fcn");
                _pDebugOut->print(": address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth ");
                _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [");

                int funcNameIndex = ((TokenIsInternCppFunction*)_programCounter)->tokenIndex;
                _pDebugOut->print(_internCppFunctions[funcNameIndex].funcName); _pDebugOut->println("]");
            #endif
            }
            break;


            // Case: process external cpp function token
            // -----------------------------------------

            case tok_isExternCppFunction:
            {

                pushExternCppFunctionName(tokenType);

            #if PRINT_PROCESSED_TOKEN
                _pDebugOut->print("=== process "); _pDebugOut->print(tokenType == tok_isInternCppFunction ? "internal cpp fcn" : tokenType == tok_isExternCppFunction ? "external cpp fcn" : "Justina fcn");
                _pDebugOut->print(": address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth ");
                _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [");

                int returnValueType = ((TokenIsExternCppFunction*)_programCounter)->returnValueType;
                int funcIndexInType = ((TokenIsExternCppFunction*)_programCounter)->funcIndexInType;
                const char* funcName = ((CppDummyVoidFunction*)_pExtCppFunctions[returnValueType])[funcIndexInType].cppFunctionName;
                _pDebugOut->print(funcName); _pDebugOut->println("]");
            #endif
            }
            break;


            // Case: process Justina function token
            // ------------------------------------

            case tok_isJustinaFunction:
            {

                pushJustinaFunctionName(tokenType);

            #if PRINT_PROCESSED_TOKEN
                _pDebugOut->print("=== process "); _pDebugOut->print(tokenType == tok_isInternCppFunction ? "internal cpp fcn" : tokenType == tok_isExternCppFunction ? "external cpp fcn" : "Justina fcn");
                _pDebugOut->print(": address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth ");
                _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [");

                int funcNameIndex = ((TokenIsJustinaFunction*)_programCounter)->identNameIndex;
                _pDebugOut->print(JustinaFunctionNames[funcNameIndex]); _pDebugOut->println("]");
            #endif

            #if PRINT_DEBUG_INFO
                int index = ((TokenIsJustinaFunction*)_programCounter)->identNameIndex;
                _pDebugOut->print("eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->print(" - function name: "); _pDebugOut->print(JustinaFunctionNames[index]);
                _pDebugOut->print("   ["); _pDebugOut->print(_programCounter - _programStorage); _pDebugOut->println("]");
            #endif
            }
            break;


            // Case: generic identifier token token
            // -------------------------------------------------

            case tok_isGenericName:
            {
                pushGenericName(tokenType);

            #if PRINT_PROCESSED_TOKEN
                _pDebugOut->print("=== process identif: address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [");
                _pDebugOut->print(_pEvalStackTop->varOrConst.value.pStringConst); _pDebugOut->print("]");
            #endif
            }
            break;


            // Case: parsed or intermediate constant value (long, float or string)
            // -------------------------------------------------------------------

            case tok_isConstant:
            {

                _activeFunctionData.errorProgramCounter = _programCounter;                              // in case an error occurs while processing token

                pushConstant(tokenType);

            #if PRINT_DEBUG_INFO
                _pDebugOut->print("eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->print(" - constant");
                _pDebugOut->print("   ["); _pDebugOut->print(_programCounter - _programStorage); _pDebugOut->println("]");
            #endif

            #if PRINT_PROCESSED_TOKEN
                _pDebugOut->print("=== process const  : address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [");
                char valueType = _pEvalStackTop->varOrConst.valueType;
                if (valueType == value_isLong) { _pDebugOut->print(_pEvalStackTop->varOrConst.value.longConst); _pDebugOut->println("]"); }
                else if (valueType == value_isFloat) { _pDebugOut->print(_pEvalStackTop->varOrConst.value.floatConst); _pDebugOut->println("]"); }
                else { _pDebugOut->print("'"); _pDebugOut->print(_pEvalStackTop->varOrConst.value.pStringConst); _pDebugOut->println("']"); }
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
                _activeFunctionData.errorProgramCounter = _programCounter;                              // in case an error occurs while processing token

                pushVariable(tokenType);

            #if PRINT_DEBUG_INFO
                _pDebugOut->print("eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->print(" - variable");
                _pDebugOut->print("   ["); _pDebugOut->print(_programCounter - _programStorage); _pDebugOut->println("]");
            #endif

            #if PRINT_PROCESSED_TOKEN
                _pDebugOut->print("=== process var name: address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [");
                int varNameIndex = ((TokenIsVariable*)_programCounter)->identNameIndex;
                char scopeMask = ((TokenIsVariable*)_programCounter)->identInfo & var_scopeMask;
                char* varName = (scopeMask == var_isUser) ? userVarNames[varNameIndex] : programVarNames[varNameIndex];
                _pDebugOut->print(varName); _pDebugOut->println("]");
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
                if (nextIsLeftPar) {                                                                    // array variable name (this token) is followed by subscripts (to be processed)
                    _pEvalStackTop->varOrConst.valueAttributes |= var_isArray_pendingSubscripts;        // flag that array element still needs to be processed
                }

                // check if (an) operation(s) can be executed. 
                // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)

                execResult = execAllProcessedOperators();
                if (execResult != result_execOK) { break; }

            #if PRINT_DEBUG_INFO
                _pDebugOut->println("=== processed var name");
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
                    _pDebugOut->print("eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->print(" - terminal: "); _pDebugOut->print(_terminals[_pEvalStackTop->terminal.index & 0x7F].terminalName);
                    _pDebugOut->print("   ["); _pDebugOut->print(_programCounter - _programStorage); _pDebugOut->println("]");
                #endif
                    if (precedingIsComma) { _pEvalStackTop->terminal.index |= 0x80;   doCaseBreak = true; }                             // flag that preceding token is comma separator 

                    if (!doCaseBreak) {
                        if (evalStack.getElementCount() < _activeFunctionData.callerEvalStackLevels + 2) { doCaseBreak = true; }        // no preceding token exist on the stack      
                    }
                    if (!doCaseBreak) {
                        if (!(_pEvalStackMinus1->genericToken.tokenType == tok_isConstant) && !(_pEvalStackMinus1->genericToken.tokenType == tok_isVariable)) { doCaseBreak = true;; };
                    }
                    if (!doCaseBreak) {
                        // previous token is constant or variable: check if current token is an infix or a postfix operator (it cannot be a prefix operator)
                        // if postfix operation, execute it first (it always has highest priority)
                        bool isPostfixOperator = (_terminals[_pEvalStackTop->terminal.index & 0x7F].postfix_priority != 0);
                        if (isPostfixOperator) {
                            execResult = execUnaryOperation(false);                                                                     // flag postfix operation
                            if (execResult == result_execOK) { execResult = execAllProcessedOperators(); }
                            if (execResult != result_execOK) { doCaseBreak = true;; }
                        }
                    }

                #if PRINT_PROCESSED_TOKEN        // after evaluation stack has been updated and before breaking 
                    _pDebugOut->print("=== processed termin : address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [ ");
                    _pDebugOut->print(_terminals[tokenIndex].terminalName);   _pDebugOut->println(" ]");
                #endif

                    if (doCaseBreak) { break; }
                }


                // comma separator ?
                // -----------------

                else if (isComma) {

                    // if currently executing a print... command, the first argument is always the stream number to print to. After it has been evaluated, it must be used to...
                    // ... set variable '_pLastPrintColumn' to the current last print column of this particular stream BEFORE the other arguments are evaluated, because...
                    // ...these arguments may use the pos() function to know at what column  printing the first argument starts.
                    // for command printLine with only one argument (stream), this is not relevant (because no arguments)

                    if (setCurrentPrintColumn && ((holdCommandStartEValStackLevels + 1) == evalStack.getElementCount()))                                    // stream argument has been processed
                    {
                        // retrieve stream number
                        // if stream is valid, will be tested when all print command arguments have been evaluated and the print command itself is processed
                        Val value{};
                        bool argIsVar = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);                                                           // could be a constant variable, but this is not relevant here
                        char valueType = argIsVar ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
                        bool opIsLong = ((uint8_t)valueType == value_isLong);
                        value.floatConst = (argIsVar ? (*_pEvalStackTop->varOrConst.value.pFloatConst) : _pEvalStackTop->varOrConst.value.floatConst);      // works for all value types
                        int streamNumber = opIsLong ? value.longConst : value.floatConst;                                                                   // always a long or a float value

                        // set pointer to last printed column (current position: start of line = 0) for this stream
                        _pLastPrintColumn = (streamNumber == 0) ? _pConsolePrintColumn : (streamNumber < 0) ? _pIOprintColumns + (-streamNumber) - 1 : &(openFiles[streamNumber - 1].currentPrintColumn);
                        setCurrentPrintColumn = false;      // reset
                    }

                #if PRINT_PROCESSED_TOKEN
                    _pDebugOut->print("=== process termin : address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [ ");
                    _pDebugOut->print(_terminals[tokenIndex].terminalName);   _pDebugOut->println(" ]");
                #endif
                }


                // right parenthesis ?
                // -------------------

                else if (isRightPar) {

                #if PRINT_DEBUG_INFO
                    _pDebugOut->print("eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->print(" - right par");
                    _pDebugOut->print("   ["); _pDebugOut->print(_programCounter - _programStorage); _pDebugOut->println("]");
                #endif

                    bool doCaseBreak{ false };
                    int argCount = 0;                                                                                               // init number of supplied arguments (or array subscripts) to 0
                    LE_evalStack* pStackLvl = _pEvalStackTop;     // stack level of last argument / array subscript before right parenthesis, or left parenthesis (if function call and no arguments supplied)

                    // set pointer to stack level for left parenthesis and pointer to stack level for preceding token (if any)
                    while (true) {
                        bool isTerminalLvl = ((pStackLvl->genericToken.tokenType == tok_isTerminalGroup1) || (pStackLvl->genericToken.tokenType == tok_isTerminalGroup2) || (pStackLvl->genericToken.tokenType == tok_isTerminalGroup3));
                        bool isLeftParLvl = isTerminalLvl ? (_terminals[pStackLvl->terminal.index & 0x7F].terminalCode == termcod_leftPar) : false;
                        if (isLeftParLvl) { break; }   // break if left parenthesis found 
                        pStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);
                        argCount++;
                    }

                    LE_evalStack* pPrecedingStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);                      // stack level PRECEDING left parenthesis (or null pointer)

                    // remove left parenthesis stack level
                    pStackLvl = (LE_evalStack*)evalStack.deleteListElement(pStackLvl);                                              // pStackLvl now pointing to first function argument or array subscript (or nullptr if none)
                #if PRINT_DEBUG_INFO
                    _pDebugOut->println("REMOVE left parenthesis from stack");
                    _pDebugOut->print("                 "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->println(" - first argument");
                #endif

                    // correct pointers (now wrong, if from 0 to 2 arguments)
                    _pEvalStackTop = (LE_evalStack*)evalStack.getLastListElement();                                                 // this line needed if no arguments
                    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
                    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

                    // execute internal cpp, external cpp or Justina function, OR (if array closing parenthesis) calculate array element address OR remove parenthesis around single argument 
                    execResult = execParenthesesPair(pPrecedingStackLvl, pStackLvl, argCount, appFlagsRequestStop, appFlagsRequestAbort);
                #if PRINT_DEBUG_INFO
                    _pDebugOut->print("    right par.: exec result "); _pDebugOut->println(execResult);
                #endif
                    if (execResult != result_execOK) { doCaseBreak = true; }

                    // the left parenthesis and the argument(s) are now removed and replaced by a single scalar (function result, array element, single argument)
                    // check if additional operators preceding the left parenthesis can now be executed.
                    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)
                    if (!doCaseBreak) {
                        execResult = execAllProcessedOperators(); if (execResult != result_execOK) { doCaseBreak = true; }
                    }



                #if PRINT_PROCESSED_TOKEN        // after evaluation stack has been updated and before breaking because of error
                    _pDebugOut->print("=== processed termin : address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [ ");
                    _pDebugOut->print(_terminals[tokenIndex].terminalName);   _pDebugOut->println(" ]");
                #endif
                    if (doCaseBreak) { break; }
                }


                // statement separator ?
                // ---------------------

                else if (isSemicolon) {
                #if PRINT_DEBUG_INFO
                    _pDebugOut->print("=== process semicolon : eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->print(" - semicolon");
                    _pDebugOut->print("   ["); _pDebugOut->print(_programCounter - _programStorage); _pDebugOut->println("]");
                #endif
                    bool doCaseBreak{ false };

                    lastTokenIsSemicolon = true;
                    isEndOfStatementSeparator = true;

                    if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_none) {                                                 // currently not executing a command, but a simple expression
                        if (evalStack.getElementCount() > (_activeFunctionData.callerEvalStackLevels + 1)) {
                            // if tracing, message would not be correct. Eval stack levels will be deleted right after printing a traced value (or trace execution error)
                            if (!_parsingExecutingTraceString) {
                            #if PRINT_OBJECT_COUNT_ERRORS
                                _pDebugOut->print("*** Evaluation stack error. Remaining stack levels for current program level: "); _pDebugOut->println(evalStack.getElementCount() - (_activeFunctionData.callerEvalStackLevels + 1));
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
                                if (tokenType != tok_isEvalEnd) { clearEvalStackLevels(1); }                                        // a next expression is found: delete the current expression's result
                            }

                            else if (_parsingExecutingTraceString) {                                                                // keep result for now (do nothing)
                            }

                            else {                                                                                                  // not an eval() block, not tracing

                                // in main program level ? store as last value (for now, we don't know if it will be followed by other 'last' values)
                                if (_programCounter >= (_programStorage + _progMemorySize)) {
                                    saveLastValue(_lastValueIsStored);
                                }                                                                                                   // save last result in FIFO and delete stack level
                                else {
                                    clearEvalStackLevels(1);
                                }                                                                                                   // NOT main program level: we don't need to keep the statement result

                            }
                        }
                    }
                    // command with optional expression(s) processed ? Execute command
                    else {
                        setCurrentPrintColumn = false;                                                                              // reset (used by print commands only)

                        // NOTE: STOP and ABORT commands do NOT set 'appFlagsRequestStop' & 'appFlagsRequestAbort', but return a 'stop' or 'abort' error instead
                        execResult = execProcessedCommand(isFunctionReturn, appFlagsRequestStop, appFlagsRequestAbort);
                        if (execResult == result_abort) { abortCommandReceived = true; }                                            // user typed
                        if (execResult != result_execOK) { doCaseBreak = true; }                                                    // error: break (case label) immediately
                    }

                #if PRINT_DEBUG_INFO
                    _pDebugOut->print("                 "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->println(" - semicolon processed");
                #endif

                #if PRINT_PROCESSED_TOKEN        // after evaluation stack has been updated and before breaking 
                    _pDebugOut->print("=== processed semicolon : address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [ ");
                    _pDebugOut->print(_terminals[tokenIndex].terminalName);   _pDebugOut->println(" ]");
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
                if (execResult != result_execOK) { break; }                                                                         // other error: break (case label) immediately

                // after evaluation stack has been updated and before breaking because of error
            #if PRINT_PROCESSED_TOKEN        
                _pDebugOut->print("=== processed 'eval end' token : address "); _pDebugOut->print(_programCounter - _programStorage);  _pDebugOut->print(", eval stack depth "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(" [ ");
                _pDebugOut->print("(eval end token)");   _pDebugOut->println(" ]");
            #endif
            }
            break;

        } // end 'switch (tokenType)'


        // 1.2. a token has been processed (with or without error): advance to next token
        // ------------------------------------------------------------------------------

        _programCounter = _activeFunctionData.pNextStep;                                                    // note: will be altered when calling a Justina function and upon return of a called function
        tokenType = *_activeFunctionData.pNextStep & 0x0F;                                                  // next token type (could be token within caller, if returning now)
        precedingIsComma = isComma;                                                                         // remember if this was a comma

    #if PRINT_DEBUG_INFO
        _pDebugOut->print("** token has been processed: next step = "); _pDebugOut->println(_activeFunctionData.pNextStep - _programStorage);
        _pDebugOut->print("                             eval stack depth: "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(", list element address: "); _pDebugOut->println((uint32_t)_pEvalStackTop, HEX); _pDebugOut->println();
    #endif


        // 1.3 last token processed was a statement separator ? 
        // ----------------------------------------------------

        // this code executes after a statement was executed (simple expression or command)

        if (isEndOfStatementSeparator) {                                                                    // after expression AND after command

        #if PRINT_PROCESSED_TOKEN        
            _pDebugOut->println("\r\n");
            _pDebugOut->println("**** is 'end of statement separator'\r\n");
        #endif

            // keep track of Justina program counters
            // --------------------------------------
            programCnt_previousStatementStart = holdProgramCnt_StatementStart;
            holdProgramCnt_StatementStart = _programCounter;

            if (execResult == result_execOK) {                                                              // no error ? 
                if (!isFunctionReturn) {                                                                    // adapt error program step pointers
                    // note: if returning from user function, error statement pointers retrieved from flow control stack 
                    _activeFunctionData.errorStatementStartStep = _programCounter;
                    _activeFunctionData.errorProgramCounter = _programCounter;
                }
            }

            // examine kill, stop and abort requests from Justina caller ('shell')
            // -------------------------------------------------------------------

            bool kill, forcedStop{}, forcedAbort{};
            execPeriodicHousekeeping(&kill, &forcedStop, &forcedAbort);
            if (kill) { execResult = result_kill; return execResult; }                                      // kill Justina interpreter ? (buffer is now flushed until next line character)
            appFlagsRequestStop = appFlagsRequestStop || forcedStop;
            appFlagsRequestAbort = appFlagsRequestAbort || forcedAbort;
            if (appFlagsRequestStop) { showStopmessage = true; }                                            // relevant for message only

            // process debugging commands (entered from the command line, or forced abort / stop requests received while a program is running  
            // ------------------------------------------------------------------------------------------------------------------------------

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

                // stop for debug now ? (program was interrupted, one of the 'step...' commands was executed (imm. mode), or the program encountered a STOP command
                doStopForDebugNow = (appFlagsRequestStop || _debugCmdExecuted ||
                    (_stepCmdExecuted == db_singleStep) ||
                    ((_stepCmdExecuted == db_stepOut) && (_callStackDepth < _stepCallStackLevel)) ||
                    ((_stepCmdExecuted == db_stepOver) && (_callStackDepth <= _stepCallStackLevel)) ||
                    ((_stepCmdExecuted == db_stepOutOfBlock) && (flowCtrlStack.getElementCount() < _stepFlowCtrlStackLevels)) ||
                    ((_stepCmdExecuted == db_stepToBlockEnd) && ((flowCtrlStack.getElementCount() < _stepFlowCtrlStackLevels) || nextIsSameLvlEnd)))

                    && executedStepIsprogram && nextStepIsprogram && !isFunctionReturn;


                // skipping a statement and stopping again for debug ?
                doSkip = (_stepCmdExecuted == db_skip) && nextStepIsprogram && !isFunctionReturn;
                if (doSkip) {
                    // check if next step is start of a command (reserved word) and that it is the end of a block command
                    int tokenType = (_activeFunctionData.pNextStep[0] & 0x0F);                                                      // always first character (any token)
                    if (tokenType == tok_isReservedWord) {                                                                          // ok
                        int tokenindex = ((TokenIsResWord*)_activeFunctionData.pNextStep)->tokenIndex;
                        // if the command to be skipped is an 'end' block command, remove the open block from the flow control stack
                        if (_resWords[tokenindex].resWordCode == cmdcod_end) {                                                      // it's an open block end (already tested before)
                            flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);                                                    // delete open block stack level
                            _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
                        }
                    }
                    // skip a statement in program memory: adapt program step pointers
                    findTokenStep(_programCounter, tok_isTerminalGroup1, termcod_semicolon);  // find next semicolon (always match)
                    _activeFunctionData.pNextStep = _programCounter += sizeof(TokenIsTerminal);
                    tokenType = *_activeFunctionData.pNextStep & 0x0F;                                                              // next token type (could be token within caller, if returning now)
                    precedingIsComma = false;
                    _activeFunctionData.errorStatementStartStep = _activeFunctionData.pNextStep;
                    _activeFunctionData.errorProgramCounter = _activeFunctionData.pNextStep;
                }

                if (doStopForDebugNow) { appFlagsRequestStop = false; _debugCmdExecuted = false; }                                  // reset request to stop program

                if (appFlagsRequestAbort) { execResult = result_abort; }
                else if (doStopForDebugNow || doSkip) { execResult = result_stopForDebug; }

                isFunctionReturn = false;
            }
        }


        // 1.4 did an execution error occur within token ? signal error
        // ------------------------------------------------------------

        // do not print / handle error message / event if currently executing trace expressions

        if (!_parsingExecutingTraceString) {
            if (execResult != result_execOK) {
                if (*_pConsolePrintColumn != 0) { printlnTo(0);  *_pConsolePrintColumn = 0; }
                execError = true;

                bool isEvent = (execResult >= result_startOfEvents);                                                                // not an error but an event ?
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
                    // then the info pointing to the correct statement ('caller' of the eval() function) is still available in the active function data structure (block type 'block_JustinaFunction'),  
                    // because the data has not yet been pushed to the flow ctrl stack

                    // [2] If a PARSING error occurs while parsing a NESTED eval() string, or an EXECUTION error occurs while executing ANY parsed eval() string (nested or not),
                    // the info pointing to the correct statement has been pushed to the flow ctrl stack already 

                    char* errorStatementStartStep = _activeFunctionData.errorStatementStartStep;
                    char* errorProgramCounter = _activeFunctionData.errorProgramCounter;
                    int functionIndex = _activeFunctionData.functionIndex;          // init

                    // info to identify and print the statement where the error occured is on the flow ctrl stack ? find it there
                    if (_activeFunctionData.blockType == block_eval) {
                        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                                                               // one level below _activeFunctionData
                        char* pImmediateCmdStackLvl = _pParsedCommandLineStackTop;

                        while (((OpenFunctionData*)pFlowCtrlStackLvl)->blockType == block_eval) {
                            pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                            pImmediateCmdStackLvl = parsedCommandLineStack.getPrevListElement(pImmediateCmdStackLvl);
                        }

                        // retrieve error statement pointers and function index (in case the 'function' block type is referring to immediate mode statements)
                        errorStatementStartStep = ((OpenFunctionData*)pFlowCtrlStackLvl)->errorStatementStartStep;
                        errorProgramCounter = ((OpenFunctionData*)pFlowCtrlStackLvl)->errorProgramCounter;
                        functionIndex = ((OpenFunctionData*)pFlowCtrlStackLvl)->functionIndex;

                        // if the error statement pointers refer to immediate mode code (not to a program), pretty print directly from the imm.mode parsed command stack: add an offset to the pointers 
                        bool isImmMode = (errorStatementStartStep >= (_programStorage + _progMemorySize));
                        if (isImmMode) { programCounterOffset = pImmediateCmdStackLvl + sizeof(char*) - (_programStorage + _progMemorySize); }
                    }

                    printTo(0, "\r\n  ");
                    prettyPrintStatements(1, errorStatementStartStep + programCounterOffset, errorProgramCounter + programCounterOffset, &sourceErrorPos);
                    for (int i = 1; i <= sourceErrorPos; ++i) { printTo(0, " "); }

                    sprintf(execInfo, "  ^\r\n  Exec error %d", execResult);                                                        // in main program level 
                    printTo(0, execInfo);

                    // errorProgramCounter is never pointing to a token directly contained in a parsed() eval() string 
                    if (errorProgramCounter >= (_programStorage + _progMemorySize)) { sprintf(execInfo, ""); }
                    else { sprintf(execInfo, " in user function %s", JustinaFunctionNames[functionIndex]); }
                    printTo(0, execInfo);

                    if (execResult == result_eval_parsingError) { sprintf(execInfo, " (eval() parsing error %ld)\r\n", _evalParseErrorCode); }
                    else if (execResult == result_list_parsingError) { sprintf(execInfo, " (list input parsing error %ld)\r\n", _evalParseErrorCode); }
                    else { sprintf(execInfo, "\r\n"); }
                    printTo(0, execInfo);
                }

                else if (execResult == result_quit) {
                    strcpy(execInfo, "\r\nExecuting 'quit' command, ");
                    printTo(0, strcat(execInfo, _keepInMemory ? "data retained\r\n" : "memory released\r\n"));
                }
                else if (execResult == result_kill) {}      // do nothing
                else if (execResult == result_abort) { printTo(0, "\r\n+++ Abort: code execution terminated +++\r\n"); }
                else if (execResult == result_stopForDebug) { if (showStopmessage) { printTo(0, "\r\n+++ Program stopped +++\r\n"); } }
                else if (execResult == result_initiateProgramLoad) {}                                                               // (nothing to do here for this event)

                _lastValueIsStored = false;                                                                                         // prevent printing last result (if any)
                break;
            }
        }
    }   // end 'while ( tokenType != tok_no_token )'                                                                                       


    // -----------
    // 2. finalize
    // -----------

    // 2.1 did the execution produce a result ? print it
    // -------------------------------------------------

    if (!_parsingExecutingTraceString) {
        if (*_pConsolePrintColumn != 0) { printlnTo(0);    *_pConsolePrintColumn = 0; }
        if (_lastValueIsStored && (_printLastResult > 0)) {

            // print last result
            bool isLong = (lastResultTypeFiFo[0] == value_isLong);
            bool isFloat = (lastResultTypeFiFo[0] == value_isFloat);
            int charsPrinted{  };                                                                                                   // required but not used
            Val toPrint;
            char* fmtString = (isLong || isFloat) ? _dispNumberFmtString : _dispStringFmtString;

            printToString(_dispWidth, (isLong || isFloat) ? _dispNumPrecision : MAX_STRCHAR_TO_PRINT,
                (!isLong && !isFloat), _dispIsIntFmt, lastResultTypeFiFo, lastResultValueFiFo, fmtString, toPrint, charsPrinted, (_printLastResult == 2));
            printlnTo(0, toPrint.pStringConst);

            if (toPrint.pStringConst != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("----- (Intermd str) "); _pDebugOut->println((uint32_t)toPrint.pStringConst, HEX);
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
        *((OpenFunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                                                             // push caller function data to stack
        ++_callStackDepth;      // user function level added to flow control stack

        _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                                                    // store evaluation stack levels in use by callers (call stack)

        // push current command line storage to command line stack, to make room for debug commands
    #if PRINT_PARSED_STAT_STACK
        _pDebugOut->print("  >> PUSH parsed statements (stop for debug): last step: "); _pDebugOut->println(_lastUserCmdStep - (_programStorage + _progMemorySize));
    #endif
        long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
        _pParsedCommandLineStackTop = (char*)parsedCommandLineStack.appendListElement(sizeof(char*) + parsedUserCmdLen);
        *(char**)_pParsedCommandLineStackTop = _lastUserCmdStep;
        memcpy(_pParsedCommandLineStackTop + sizeof(char*), (_programStorage + _progMemorySize), parsedUserCmdLen);

        ++_openDebugLevels;
    }

    // no programs in debug: always; otherwise: only if error is in fact quit or kill event 
    else if ((_openDebugLevels == 0) || (execResult == result_quit) || (execResult == result_kill)) {                               // do not clear flow control stack while in debug mode
        int dummy{};
        _openDebugLevels = 0;       // (if not yet zero)
        clearParsedCommandLineStack(parsedCommandLineStack.getElementCount());
        clearFlowCtrlStack(dummy);                                                                                                  // and remaining local storage + local variable string and array values
        clearEvalStack();
    }

    // tracing (this means at least one program is stopped; if no exec error then one evaluation stack level (with the result) needs to be deleted
    else if (_parsingExecutingTraceString) {
        int charsPrinted{  };                                                                                                       // required but not used
        Val toPrint;
        if (execResult == result_execOK) {
            Val value;
            bool isVar = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);
            char valueType = isVar ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
            bool isLong = (valueType == value_isLong);
            bool isFloat = (valueType == value_isFloat);
            char* fmtString = (isLong || isFloat) ? _dispNumberFmtString : _dispStringFmtString;
            // printToString() expects long, float or char*: remove extra level of indirection (variables only)
            value.floatConst = isVar ? *_pEvalStackTop->varOrConst.value.pFloatConst : _pEvalStackTop->varOrConst.value.floatConst; // works for long and string as well
            printToString(0, (isLong || isFloat) ? _dispNumPrecision : MAX_STRCHAR_TO_PRINT,
                (!isLong && !isFloat), _dispIsIntFmt, &valueType, &value, fmtString, toPrint, charsPrinted);
        }
        else {
            char valTyp = value_isStringPointer;
            char  errStr[12];                                                                                                       // includes place for terminating '\0'
            sprintf(errStr, "<ErrE%d>", (int)execResult);
            Val temp;
            temp.pStringConst = errStr;
            printToString(0, MAX_STRCHAR_TO_PRINT, true, false, &valTyp, &temp, _dispStringFmtString, toPrint, charsPrinted);
        }

        if (toPrint.pStringConst == nullptr) { printlnTo(0); }
        else {
            printTo(0, toPrint.pStringConst);
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("----- (Intermd str) "); _pDebugOut->println((uint32_t)toPrint.pStringConst, HEX);
        #endif
            _intermediateStringObjectCount--;
            delete[] toPrint.pStringConst;
        }


        // note: flow control stack and immediate command stack (program code) are not affected: only need to clear evaluation stack
        clearEvalStackLevels(evalStack.getElementCount() - (int)_activeFunctionData.callerEvalStackLevels);
    }

    // program or command line exec error (could be an app flags abort request), OR abort command entered by the user,
    // while at least one other program is currently stopped in debug mode ?
    else if (execResult != result_execOK) {
        int deleteImmModeCmdStackLevels{ 0 };
        clearFlowCtrlStack(deleteImmModeCmdStackLevels, true, abortCommandReceived);                                                // returns imm. mode command stack levels to delete
        clearParsedCommandLineStack(deleteImmModeCmdStackLevels);                                                                   // do not delete all stack levels but only supplied level count
        clearEvalStackLevels(evalStack.getElementCount() - (int)_activeFunctionData.callerEvalStackLevels);
    }

    // adapt application flags
    _appFlags = (_appFlags & ~appFlag_statusMask) | appFlag_idle;                                                                   // status 'idle'

#if PRINT_DEBUG_INFO
    _pDebugOut->print("*** exit exec: eval stack depth: "); _pDebugOut->println(evalStack.getElementCount());
    _pDebugOut->print("** EXEC: return error code: "); _pDebugOut->println(execResult);
    _pDebugOut->println("**** returning to main");
#endif
    _activeFunctionData.pNextStep = _programStorage + _progMemorySize;                                                              // only to signal 'immediate mode command level'

    return execResult;                                                                                                              // return result, in case it's needed by caller
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
        if (tokenType == tok_no_token) { return tok_no_token; }                                                 // end of program reached
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
    char& tokenCode1_spec = criterium1;                                                                         // keyword index or terminal index to look for
    char& tokenCode2_spec = criterium2;                                                                         // optional second index (-1 if only one index to look for)

    // if looking for a specific variable
    char& varScope_spec = criterium1;                                                                           //  variable scope to look for (user, global, ...)
    char& valueIndex_spec = criterium2;                                                                         // value index to look for

    // exclude current token step
    int tokenType = *pStep & 0x0F;
    // terminals and constants: token length is NOT stored in token type
    int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
        (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;                         // fetch next token 
    pStep = pStep + tokenLength;

    do {
        tokenType = *pStep & 0x0F;
        if (tokenType == '\0') { return tokenType; }                                                            // signal 'not found'

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

                case tok_isTerminalGroup1:                                                                      // actual token can be part of any of the three terminal groups
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
            if (tokenCodeMatch) { return tokenType; }                                                           // if terminal, then return exact group (entry: use terminalGroup1) 
        }

        int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) :
            (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) : (*pStep >> 4) & 0x0F;    // fetch next token 
        pStep = pStep + tokenLength;
    } while (true);
}


// --------------------------------------------------------
// *   Save last value for future reuse by calculations   * 
// --------------------------------------------------------

void Justina_interpreter::saveLastValue(bool& overWritePrevious) {
    if (!(evalStack.getElementCount() > _activeFunctionData.callerEvalStackLevels)) { return; }                 // safety: data available ?
    // if overwrite 'previous' last result, then replace first item (if there is one); otherwise replace last item if FiFo full (-1 if nothing to replace)
    int itemToRemove = overWritePrevious ? ((_lastValuesCount >= 1) ? 0 : -1) :
        ((_lastValuesCount == MAX_LAST_RESULT_DEPTH) ? MAX_LAST_RESULT_DEPTH - 1 : -1);

    // remove a previous item ?
    if (itemToRemove != -1) {
        // if item to remove is a string: delete heap object
        if (lastResultTypeFiFo[itemToRemove] == value_isStringPointer) {

            if (lastResultValueFiFo[itemToRemove].pStringConst != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("----- (FiFo string) ");   _pDebugOut->println((uint32_t)lastResultValueFiFo[itemToRemove].pStringConst, HEX);
            #endif 
                _lastValuesStringObjectCount--;
                delete[] lastResultValueFiFo[itemToRemove].pStringConst;                                        // note: this is always an intermediate string
            }
        }
    }
    else {
        _lastValuesCount++;                                                                                     // only adding an item, without removing previous one
    }

    // move older last results one place up in FIFO, except when just overwriting 'previous' last result
    if (!overWritePrevious && (_lastValuesCount > 1)) {                                                         // if 'new' last result count is 1, no old results need to be moved  
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
        int stringlen = min(strlen(lastvalue.value.pStringConst), MAX_ALPHA_CONST_LEN);                         // excluding terminating \0
        _lastValuesStringObjectCount++;
        lastResultValueFiFo[0].pStringConst = new char[stringlen + 1];
    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print("+++++ (FiFo string) ");   _pDebugOut->println((uint32_t)lastResultValueFiFo[0].pStringConst, HEX);
    #endif            

        memcpy(lastResultValueFiFo[0].pStringConst, lastvalue.value.pStringConst, stringlen);                   // copy the actual string (not the pointer); do not use strcpy
        lastResultValueFiFo[0].pStringConst[stringlen] = '\0';

        if (lastValueIntermediate) {
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("----- (intermd str) ");   _pDebugOut->println((uint32_t)lastvalue.value.pStringConst, HEX);
        #endif
            _intermediateStringObjectCount--;
            delete[] lastvalue.value.pStringConst;
        }
    }

    // store new last value type
    lastResultTypeFiFo[0] = sourceValueType;                                                                    // value type

    // delete the stack level containing the result
    evalStack.deleteListElement(_pEvalStackTop);
    _pEvalStackTop = (LE_evalStack*)evalStack.getLastListElement();
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

    overWritePrevious = true;

    return;
}

// ------------------------------------------------------------------------
// *   Clear evaluation stack and associated intermediate string object   * 
// ------------------------------------------------------------------------

void Justina_interpreter::clearEvalStack() {

    clearEvalStackLevels(evalStack.getElementCount());
    _pEvalStackTop = nullptr;  _pEvalStackMinus1 = nullptr; _pEvalStackMinus2 = nullptr;                        // should be already

    // error if not all intermediate string objects deleted (this does NOT indicate a user or Justina program error but instead it points to an internal Justina issue)
    if (_intermediateStringObjectCount != 0) {
    #if PRINT_OBJECT_COUNT_ERRORS
        _pDebugOut->print("*** Intermediate string cleanup error. Remaining: "); _pDebugOut->println(_intermediateStringObjectCount);
    #endif
        _intermediateStringObjectErrors += abs(_intermediateStringObjectCount);
        _intermediateStringObjectCount = 0;
    }
    return;
}


// ----------------------------------------------------------------------------------
// *   Clear n evaluation stack levels and associated intermediate string objects   *
// ----------------------------------------------------------------------------------

void Justina_interpreter::clearEvalStackLevels(int n) {

    if (n <= 0) { return; }             // nothing to do

    LE_evalStack* pStackLvl = _pEvalStackTop, * pPrecedingStackLvl{};

    for (int i = 1; i <= n; i++) {
        // if intermediate constant string, then delete char string object (test op non-empty intermediate string object in called routine)  
        if (pStackLvl->genericToken.tokenType == tok_isConstant) { deleteIntermStringObject(pStackLvl); }       // exclude non-constant tokens (terminals, keywords, functions, ...)

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


// --------------------------------
// *   Clear flow control stack   *
// --------------------------------

void Justina_interpreter::clearFlowCtrlStack(int& deleteImmModeCmdStackLevels, bool terminateOneProgramOnly, bool isAbortCommand) {

    // if argument 'terminateOneProgramOnly' is false, delete all stack levels
    // if 'terminateOneProgramOnly' is true: at least one program is currently stopped (debug mode)
    // - if argument 'isAbortCommand' = false, an error has been encountered in a RUNNING program (or this program received an 'abort request' via the application flags) and needs to be terminated
    // - if 'abortCommand' is true, the user has entered the 'abort' command, which means the most recent STOPPED program needs to be terminated. But first (because higher up in the flow control stack),
    //   the 'immediate mode program' (an abort statement can not be contained in a program) containing the abort statement needs to be terminated as well. So, TWO programs need to be terminated

    deleteImmModeCmdStackLevels = 0;                                                // init number of immediate mode parsed command stack levels to delete afterwards (parsedCommandLineStack)

    int totalProgramsToTerminate = (isAbortCommand) ? 2 : 1;                        // abort command: terminate the running program containing the abort command AND the most recent stopped program
    int programsYetToTerminate = totalProgramsToTerminate;                          // counter

    if (flowCtrlStack.getElementCount() > 0) {                                      // skip if only main level (no program running)
        bool isInitialLoop{ true };
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;

        do {
            // first loop: retrieve block type of currently active function (could be 'main' level = immediate mode instruction as well)
            char blockType = isInitialLoop ? _activeFunctionData.blockType : *(char*)pFlowCtrlStackLvl;             // first character of structure is block type

            if (blockType == block_JustinaFunction) {                               // block type: function (can be a real program function, or an implicit 'immediate mode' function (the start of ANY program) 
                if (!isInitialLoop) { _activeFunctionData = *((OpenFunctionData*)pFlowCtrlStackLvl); }              // after first loop, load from stack

                if (terminateOneProgramOnly && (programsYetToTerminate == 0)) { break; }                            // all done

                bool isProgramFunction = (_activeFunctionData.pNextStep < (_programStorage + _progMemorySize));     // is this a program function ?
                if (isProgramFunction) {                                                                            // program function: delete local storage
                    {
                        int functionIndex = _activeFunctionData.functionIndex;
                        int localVarCount = justinaFunctionData[functionIndex].localVarCountInFunction;
                        int paramOnlyCount = justinaFunctionData[functionIndex].paramOnlyCountInFunction;
                        if (localVarCount > 0) {
                            deleteStringArrayVarsStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);
                            deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);

                        #if PRINT_HEAP_OBJ_CREA_DEL
                            _pDebugOut->print("----- (LOCAL STORAGE) ");   _pDebugOut->println((uint32_t)(_activeFunctionData.pLocalVarValues), HEX);
                        #endif
                            _localVarValueAreaCount--;
                            // release local variable storage for function that has been called
                            delete[] _activeFunctionData.pLocalVarValues;
                            delete[] _activeFunctionData.pVariableAttributes;
                            delete[] _activeFunctionData.ppSourceVarTypes;
                        }
                    }
                }
                else {                                                              // immediate mode 'function' (the start of ANY program)
                    if (terminateOneProgramOnly) { programsYetToTerminate--; }
                }
                if (!isInitialLoop) { --_callStackDepth; }                          // one function removed from the call stack
            }

            else if (blockType == block_eval) {
                if (!isInitialLoop) { --_callStackDepth; }
                ++deleteImmModeCmdStackLevels;                                      // update count of 'parsed command line' stack levels to be deleted (parsedCommandLineStack)
            }

            if (!isInitialLoop) {                                                   // delete stack top
                flowCtrlStack.deleteListElement(nullptr);
                pFlowCtrlStackLvl = flowCtrlStack.getLastListElement();
            }

            if (pFlowCtrlStackLvl == nullptr) { break; }       // all done
            isInitialLoop = false;
        } while (true);
    }

    _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
    _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
    _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);
}


// ------------------------------------------------------------
// *   Clear 'immediate command' parsed command line' stack   *  
// ------------------------------------------------------------

// deletes parsed strings referenced in program storage for the current command line statements, and subsequently pops a parsed command line from the stack to program storage
// the parameter n specifies the number of stack levels to pop

void Justina_interpreter::clearParsedCommandLineStack(int n) {
    // note: ensure that only entries are cleared for eval() levels for currently running program (if there is one) and current debug level (if at least one program is stopped)
    _pParsedCommandLineStackTop = parsedCommandLineStack.getLastListElement();

    while (n-- > 0) {
        // copy command line stack top to command line program storage and pop command line stack top
        _lastUserCmdStep = *(char**)_pParsedCommandLineStackTop;                                                     // pop parsed user cmd length
        long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
        deleteConstStringObjects(_programStorage + _progMemorySize);                                            // current parsed user command statements in immediate mode program memory
        memcpy((_programStorage + _progMemorySize), _pParsedCommandLineStackTop + sizeof(char*), parsedUserCmdLen);
        parsedCommandLineStack.deleteListElement(_pParsedCommandLineStackTop);
        _pParsedCommandLineStackTop = parsedCommandLineStack.getLastListElement();
    #if PRINT_PARSED_STAT_STACK
        _pDebugOut->print("  >> POP parsed statements (clr imm cmd stack): last step: "); _pDebugOut->println(_lastUserCmdStep - (_programStorage + _progMemorySize));
    #endif
    }

    // do NOT delete parsed string constants for original command line (last copied to program storage) - handled later 
}


// --------------------------------------------------------------------------------------------------------------------------------------------
// *   execute internal cpp, external cpp or Justina function, calculate array element address or remove parentheses around single argument   *
// --------------------------------------------------------------------------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::execParenthesesPair(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& firstArgStackLvl, int argCount, bool& forcedStopRequest, bool& forcedAbortRequest) {

    // NOTE: removing a parenthesis pair around a variable, constant or expression results in an intermediate constant

    // no lower stack levels before left parenthesis (removed in the meantime) ? Is a simple parentheses pair
    if (pPrecedingStackLvl == nullptr) {
        makeIntermediateConstant(_pEvalStackTop);                                                               // left parenthesis already removed from evaluation stack
        return result_execOK;
    }

    // stack level preceding left parenthesis is internal cpp function ? execute function
    else if (pPrecedingStackLvl->genericToken.tokenType == tok_isInternCppFunction) {
        execResult_type execResult = execInternalCppFunction(pPrecedingStackLvl, firstArgStackLvl, argCount, forcedStopRequest, forcedAbortRequest);

        return execResult;
    }

    // stack level preceding left parenthesis is external cpp function ? execute function
    else if (pPrecedingStackLvl->genericToken.tokenType == tok_isExternCppFunction) {
        execResult_type execResult = execExternalCppFunction(pPrecedingStackLvl, firstArgStackLvl, argCount);

        return execResult;
    }

    // stack level preceding left parenthesis is Justina function ? execute function
    else if (pPrecedingStackLvl->genericToken.tokenType == tok_isJustinaFunction) {
        execResult_type execResult = launchJustinaFunction(pPrecedingStackLvl, firstArgStackLvl, argCount);
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
    makeIntermediateConstant(_pEvalStackTop);                                                                   // left parenthesis already removed from evaluation stack
    return result_execOK;
}

// -----------------------------------------------------------------------------------------------------------------
// *   replace array variable base address and subscripts with the array element address on the evaluation stack   *
// -----------------------------------------------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::arrayAndSubscriptsToarrayElement(LE_evalStack*& pPrecedingStackLvl, LE_evalStack*& pStackLvl, int argCount) {
    void* pArray = *pPrecedingStackLvl->varOrConst.value.ppArray;
    _activeFunctionData.errorProgramCounter = pPrecedingStackLvl->varOrConst.tokenAddress;                      // token adress of array name (in the event of an error)

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
    pPrecedingStackLvl->varOrConst.valueAttributes &= ~var_isArray_pendingSubscripts;                           // remove 'pending subscripts' flag 
    // note: other data does not change (array attributes, value type, token type, intermediate constant, variable type address)


    // Remove array subscripts from evaluation stack
    // ----------------------------------------------

    clearEvalStackLevels(argCount);

    return result_execOK;
}

// ---------------------------------------
// *   calculate array element address   *
// ---------------------------------------

void* Justina_interpreter::arrayElemAddress(void* varBaseAddress, int* subscripts) {

    // varBaseAddress argument must be base address of an array variable (containing itself a pointer to the array)
    // subscripts array must specify an array element (max. 3 dimensions)
    // return pointer will point to a long, float or a string pointer (both can be array elements) - nullptr if outside boundaries

    void* pArray = varBaseAddress;                                                                              // will point to float or string pointer (both can be array elements)
    int arrayDimCount = ((char*)pArray)[3];

    int arrayElement{ 0 };
    for (int i = 0; i < arrayDimCount; i++) {
        int arrayDim = ((char*)pArray)[i];
        if ((subscripts[i] < 1) || (subscripts[i] > arrayDim)) { return nullptr; }                              // is outside array boundaries

        int arrayNextDim = (i < arrayDimCount - 1) ? ((char*)pArray)[i + 1] : 1;
        arrayElement = (arrayElement + (subscripts[i] - 1)) * arrayNextDim;
    }
    arrayElement++;                                                                                             // add one (first array element contains dimensions and dimension count)
    return (Val*)pArray + arrayElement;                                                                         // pointer to a 4-byte array element (long, float or pointer to string)
}


// -----------------------------------------------------
// *   turn stack operand into intermediate constant   *
// -----------------------------------------------------

void Justina_interpreter::makeIntermediateConstant(LE_evalStack* pEvalStackLvl) {
    // if a (scalar) variable or a parsed constant: replace by an intermediate constant

    if ((pEvalStackLvl->varOrConst.valueAttributes & constIsIntermediate) == 0) {                               // not an intermediate constant (variable or parsed constant)
        Val operand, result;                                                                                    // operands and result
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
            _pDebugOut->print("+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)result.pStringConst, HEX);
        #endif

            strcpy(result.pStringConst, operand.pStringConst);                                                  // copy the actual strings 
        }
        pEvalStackLvl->varOrConst.value = result;                                                               // float or pointer to string (type: no change)
        pEvalStackLvl->varOrConst.valueType = valueType;
        pEvalStackLvl->varOrConst.tokenType = tok_isConstant;                                                   // use generic constant type
        pEvalStackLvl->varOrConst.valueAttributes = constIsIntermediate;                                        // is an intermediate result (intermediate constant strings must be deleted when not needed any more)
        pEvalStackLvl->varOrConst.sourceVarScopeAndFlags = 0x00;                                                // not an array, not an array element (it's a constant) 
    }

    pEvalStackLvl->varOrConst.valueAttributes &= ~(isPrintTabRequest | isPrintColumnRequest);                   // clear tab() and col() function flags
}


// ----------------------------------------
// *   execute all processed operations   *
// ----------------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::execAllProcessedOperators() {                                // prefix and infix

    // _pEvalStackTop should point to an operand on entry (parsed constant, variable, expression result)

    int pendingTokenIndex{ 0 };
    int pendingTokenType{ tok_no_token }, pendingTokenPriority{};
    bool currentOpHasPriority{ false };

#if PRINT_DEBUG_INFO
    _pDebugOut->print("** exec processed infix operators -stack levels: "); _pDebugOut->println(evalStack.getElementCount());
#endif
    // check if (an) operation(s) can be executed 
    // when an operation is executed, check whether lower priority operations can now be executed as well (example: 3+5*7: first execute 5*7 yielding 35, then execute 3+35)

    while (evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 2) {                              // at least one preceding token exists on the stack

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
            if (evalStack.getElementCount() >= _activeFunctionData.callerEvalStackLevels + 3) {                         // TWO preceding tokens exist on the stack               
                isPrefixOperator = (!(_pEvalStackMinus2->genericToken.tokenType == tok_isConstant) && !(_pEvalStackMinus2->genericToken.tokenType == tok_isVariable));
                // comma separators are not pushed to the evaluation stack, but if it is followed by a (prefix) operator, a flag is set in order not to mistake a token sequence as two operands and an infix operation
                if (_pEvalStackMinus1->terminal.index & 0x80) { isPrefixOperator = true; }                              // e.g. print 5, -6 : prefix operation on second expression ('-6') and not '5-6' as infix operation
            }

            // check operator priority
            int priority{ 0 };
            if (isPrefixOperator) { priority = _terminals[terminalIndex].prefix_priority & 0x1F; }                      // bits b4..0 = priority
            else { priority = _terminals[terminalIndex].infix_priority & 0x1F; }
            bool RtoLassociativity = isPrefixOperator ? true : _terminals[terminalIndex].infix_priority & op_RtoL;


            // pending (not yet processed) token (always present and always a terminal token after a variable or constant token)
            // pending token can be any terminal token: infix operator, left or right parenthesis, comma or semicolon 
            // it can not be a prefix operator because it follows an operand (on top of stack)
            pendingTokenType = *_activeFunctionData.pNextStep & 0x0F;                                                   // there's always minimum one token pending (even if it is a semicolon)
            pendingTokenIndex = (*_activeFunctionData.pNextStep >> 4) & 0x0F;                                           // terminal token only: index stored in high 4 bits of token type 
            pendingTokenIndex += ((pendingTokenType == tok_isTerminalGroup2) ? 0x10 : (pendingTokenType == tok_isTerminalGroup3) ? 0x20 : 0);
            bool pendingIsPostfixOperator = (_terminals[pendingTokenIndex].postfix_priority != 0);                      // postfix or infix operator ?

            // check pending operator priority
            pendingTokenPriority = (pendingIsPostfixOperator ? (_terminals[pendingTokenIndex].postfix_priority & 0x1F) :        // bits b4..0 = priority
                (_terminals[pendingTokenIndex].infix_priority) & 0x1F);                                                 // pending terminal is either an infix or a postfix operator


            // determine final priority
            currentOpHasPriority = (priority >= pendingTokenPriority);
            if ((priority == pendingTokenPriority) && (RtoLassociativity)) { currentOpHasPriority = false; }

            if (!currentOpHasPriority) { break; }                                                                       // exit while() loop

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

    Val operand, opResult;                                                                                      // operand and result

    // what are the stack levels for operator and operand ?
    LE_evalStack* pOperandStackLvl = isPrefix ? _pEvalStackTop : _pEvalStackMinus1;
    LE_evalStack* pUnaryOpStackLvl = isPrefix ? _pEvalStackMinus1 : _pEvalStackTop;
    _activeFunctionData.errorProgramCounter = pUnaryOpStackLvl->terminal.tokenAddress;                          // in the event of an error


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


    execResult_type execResult = result_execOK;                                                                 // init  

    if (!opIsLong && !opIsFloat) { execResult = result_numberExpected; }                                        // value is numeric ? (no prefix / postfix operators for strings)
    if (!opIsLong && requiresLongOp) { execResult = result_integerTypeExpected; }                               // only integer value type allowed
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
    else if (terminalCode == termcod_bitCompl) { opResult.longConst = ~operand.longConst; }                     // prefix: bit complement


    // float values: extra value tests

    int resultValueType = resultCastLong ? value_isLong : opValueType;

    if (resultValueType == value_isFloat) {                                                                     // floats only
        if (isnan(opResult.floatConst)) { return result_undefined; }
        else if (!isfinite(opResult.floatConst)) { return result_overflow; }
    }


    // (5) post process
    // ----------------

    // decrement or increment operation: store value in variable (variable type does not change) 

    bool isIncrDecr = ((terminalCode == termcod_incr) || (terminalCode == termcod_decr));
    if (isIncrDecr) { *pOperandStackLvl->varOrConst.value.pFloatConst = opResult.floatConst; }                  // line is valid for long integers as well (same size)


    // if a prefix increment / decrement, then keep variable reference on the stack
    // if a postfix increment / decrement, replace variable reference in stack by UNMODIFIED value as intermediate constant
    //  if not a decrement / increment, replace value in stack by a new value (intermediate constant)

    if (!(isIncrDecr && isPrefix)) {                                                                            // prefix increment / decrement: keep variable reference (skip)
        pOperandStackLvl->varOrConst.value = isIncrDecr ? operand : opResult;                                   // replace stack entry with unmodified or modified value as intermediate constant
        pOperandStackLvl->varOrConst.valueType = resultValueType;
        pOperandStackLvl->varOrConst.tokenType = tok_isConstant;                                                // use generic constant type
        pOperandStackLvl->varOrConst.valueAttributes = constIsIntermediate;
        pOperandStackLvl->varOrConst.sourceVarScopeAndFlags = 0x00;                                             // not an array, not an array element (it's a constant) 
    }

    pOperandStackLvl->varOrConst.valueAttributes &= ~(isPrintTabRequest | isPrintColumnRequest);                // clear tab() and col() function flags

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

    Val operand1, operand2, opResult;                                                                           // operands and result

    _activeFunctionData.errorProgramCounter = _pEvalStackMinus1->terminal.tokenAddress;                         // in the event of an error


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

    bool opResultLong = op2isLong || requiresLongOp || resultCastLong;                                      // before checking array value type, if assigning to array, ...
    bool opResultFloat = op2isFloat && !(requiresLongOp || resultCastLong);                                 // ...operand value types: after promotion, if promoted
    bool opResultString = op2isString && !requiresLongOp || resultCastLong;

    switch (operatorCode) {                                                                                 // operation to execute

        case termcod_assign:
            opResult = operand2;
            break;

        case termcod_plus:                                                                                  // also for concatenation
        case termcod_plusAssign:
            if (opResultString) {      // then operands are strings as well
                bool op1emptyString = (operand1.pStringConst == nullptr);
                bool op2emptyString = (operand2.pStringConst == nullptr);

                // concatenate two operand strings objects and store pointer to it in result
                int stringlen = 0;                                                                          // is both operands are empty strings
                if (!op1emptyString) { stringlen = strlen(operand1.pStringConst); }
                if (!op2emptyString) { stringlen += strlen(operand2.pStringConst); }

                if (stringlen == 0) { opResult.pStringConst = nullptr; }                                    // empty strings are represented by a nullptr (conserve heap space)
                else {                                                                                      // string to be assigned is not empty
                    _intermediateStringObjectCount++;
                    opResult.pStringConst = new char[stringlen + 1];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)opResult.pStringConst, HEX);
                #endif
                    opResult.pStringConst[0] = '\0';                                                        // init: in case first operand is nullptr
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
            if ((operand1.floatConst == 0) && (operand2.floatConst == 0)) { return result_undefined; }      // C++ pow() provides 1 as result
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
            _pDebugOut->print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
            _pDebugOut->println((uint32_t)opResult.pStringConst, HEX);
        #endif

            memcpy(opResult.pStringConst, pUnclippedResultString, stringlen);                                   // copy the actual string (not the pointer); do not use strcpy
            opResult.pStringConst[stringlen] = '\0';                                                            // add terminating \0

            // compound statement ? then an intermediate string has been created (not pushed to the stack) and needs to be deleted now
            if (operatorCode != termcod_assign) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("----- (Intermd str) "); _pDebugOut->println((uint32_t)pUnclippedResultString, HEX);
            #endif
                _intermediateStringObjectCount--;
                // compound assignment: pointing to the unclipped result WHICH IS NON-EMPTY: so it's a heap object and must be deleted now
                delete[] pUnclippedResultString;
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
    evalStack.deleteListElement(_pEvalStackTop);                                                                // operand 2 
    evalStack.deleteListElement(_pEvalStackMinus1);                                                             // operator
    _pEvalStackTop = _pEvalStackMinus2;
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);


    //  if operation did not include an assignment, store result in stack as an intermediate constant

    // if assignment, then result is already stored in variable and the stack top still contains the reference to the variable
    if (!operationIncludesAssignment) {
        _pEvalStackTop->varOrConst.value = opResult;                        // float or pointer to string
        _pEvalStackTop->varOrConst.valueType = opResultLong ? value_isLong : opResultFloat ? value_isFloat : value_isStringPointer;     // value type of second operand  
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;                                                  // use generic constant type
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;                                               // not an array, not an array element (it's a constant) 
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    }

    _pEvalStackTop->varOrConst.valueAttributes &= ~(isPrintTabRequest | isPrintColumnRequest);                  // clear tab() and col() function flags

#if PRINT_DEBUG_INFO
    _pDebugOut->print("eval stack depth "); _pDebugOut->print(evalStack.getElementCount());  _pDebugOut->println(" - infix operation done");
    _pDebugOut->print("                 result = "); _pDebugOut->println(_pEvalStackTop->varOrConst.value.longConst);

    _pDebugOut->print("    eval stack depth: "); _pDebugOut->print(evalStack.getElementCount()); _pDebugOut->print(", list element address: "); _pDebugOut->println((uint32_t)_pEvalStackTop, HEX);
    if (opResultLong) { _pDebugOut->print("    'op': stack top long value "); _pDebugOut->println(_pEvalStackTop->varOrConst.value.longConst); }
    else if (opResultFloat) { _pDebugOut->print("    'op': stack top float value "); _pDebugOut->println(_pEvalStackTop->varOrConst.value.floatConst); }
    else { _pDebugOut->print("    'op': stack top string value "); _pDebugOut->println(_pEvalStackTop->varOrConst.value.pStringConst); }
#endif

    return result_execOK;
}


// ------------------------------------------------
// *   execute external cpp (user cpp) function   *
// ------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::execExternalCppFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount) {
    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;

    int returnValueType = pFunctionStackLvl->function.returnValueType;
    int funcIndexInType = pFunctionStackLvl->function.funcIndexInType;

    char fcnResultValueType{};  // init
    Val fcnResult;

    // variable references to store (arguments 2[..4]) 
    const char isVariable = 0x80;                                                                                   // mask: is variable (not a constant) 

    // if more than 8 arguments are supplied, excess arguments are discarded
    // to keep it simple for the c++ user writing the user routine, we always pass const void pointers, to variables and constants
    // but for constants, the pointer will point to a copy of the data

    Val args[8]{}, dummyArgs[8]{};                                                                                                  // values to be passed to user routine
    char valueType[8]{ };                                                                                                           // value types (long, float, char string)
    char varScope[8]{};                                                                                                             // if variable: variable scope (user, program global, static, local)
    bool argIsNonConstantVar[8]{};                                                                                                  // flag: is variable (scalar or aray)
    bool argIsArray[8]{};                                                                                                           // flag: is array element

    const void* pValues_copy[8]{};                                                                                                  // copies for safety
    char valueTypes_copy[8];
    int cmdParamCount_copy{ suppliedArgCount };

    LE_evalStack* pStackLvl = pFirstArgStackLvl;

    // any data to pass ? (optional arguments 1 to 8: data)
    if (suppliedArgCount >= 1) {                                                                                                    // first argument (callback procedure) processed (but still on the stack)
        copyValueArgsFromStack(pStackLvl, suppliedArgCount, argIsNonConstantVar, argIsArray, valueType, args, true, dummyArgs);
        pStackLvl = pFirstArgStackLvl;                                                                                              // set stack level again to first value argument
        for (int i = 0; i < suppliedArgCount; i++) {
            if (argIsNonConstantVar[i]) {                                                                                           // is this a 'changeable' variable ? (not a constant & not a constant variable)
                valueType[i] |= isVariable;                                                                                         // flag as 'changeable' variable (scalar or array element)
                varScope[i] = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);                                       // remember variable scope (user, program global, local, static) 
            }
            pValues_copy[i] = args[i].pBaseValue;                                                                                   // copy pointers for safety (protect original pointers from changes by c++ routine) 
            valueTypes_copy[i] = valueType[i];
            pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
        }
    }

    int intExecResult{ (int)result_execOK };

    switch (returnValueType) {
        case 0: fcnResult.longConst = (long)((CppBoolFunction*)_pExtCppFunctions[0])[funcIndexInType].func(pValues_copy, valueTypes_copy, cmdParamCount_copy, intExecResult); break;        // bool-> long
        case 1: fcnResult.longConst = (long)((CppCharFunction*)_pExtCppFunctions[1])[funcIndexInType].func(pValues_copy, valueTypes_copy, cmdParamCount_copy, intExecResult); break;        // char to long   
        case 2: fcnResult.longConst = (long)((CppIntFunction*)_pExtCppFunctions[2])[funcIndexInType].func(pValues_copy, valueTypes_copy, cmdParamCount_copy, intExecResult); break;         // int to long   
        case 3: fcnResult.longConst = ((CppLongFunction*)_pExtCppFunctions[3])[funcIndexInType].func(pValues_copy, valueTypes_copy, cmdParamCount_copy, intExecResult); break;              // long   
        case 4: fcnResult.floatConst = ((CppFloatFunction*)_pExtCppFunctions[4])[funcIndexInType].func(pValues_copy, valueTypes_copy, cmdParamCount_copy, intExecResult); break;            // float
        case 5: fcnResult.pStringConst = ((Cpp_pCharFunction*)_pExtCppFunctions[5])[funcIndexInType].func(pValues_copy, valueTypes_copy, cmdParamCount_copy, intExecResult); break;         // char*   
        case 6:((CppVoidFunction*)_pExtCppFunctions[6])[funcIndexInType].func(pValues_copy, valueTypes_copy, cmdParamCount_copy, intExecResult); fcnResult.longConst = 0; break;            // void -> returns zero
    }
    if (intExecResult != (int)result_execOK) { return (execResult_type)intExecResult; }
    fcnResultValueType = (returnValueType == 4) ? value_isFloat : (returnValueType == 5) ? value_isStringPointer : value_isLong;    // long: for bool, char, int, long return types


    // postprocess: check any strings RETURNED by callback procedure
    // -------------------------------------------------------------

    pStackLvl = pFirstArgStackLvl;                                                                             // set stack level again to first value argument
    for (int i = 0; i < suppliedArgCount; i++) {
        if ((valueType[i] & value_typeMask) == value_isStringPointer) {
            // string argument was a constant (including a CONST variable) - OR it was empty (null pointer) ?  
            // => a string copy or a new string solely consisting of a '\0' terminator (intermediate string) was passed to user routine and needs to be deleted 
            if (valueType[i] & passCopyToCallback) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("----- (Intermd str) "); _pDebugOut->println((uint32_t)args[i].pStringConst, HEX);
            #endif
                _intermediateStringObjectCount--;
                delete[] args[i].pStringConst;                                                                      // delete temporary string
            }

            // string argument was a (NON-CONSTANT) variable string: no copy was made, the string itself was passed to the user routine
            // did the user routine change it to an empty, '\0' terminated string ?
            // then this variable string object needs to be deleted and the pointer to it needs to be replaced by a null pointer (Justnia convention)
            else if (strlen(args[i].pStringConst) == 0) {

            #if PRINT_HEAP_OBJ_CREA_DEL 
                _pDebugOut->print((varScope[i] == var_isUser) ? "----- (usr var str) " : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? "----- (var string ) " : "----- (loc var str) ");
                _pDebugOut->println((uint32_t)args[i].pStringConst, HEX);
            #endif
                (varScope[i] == var_isUser) ? _userVarStringObjectCount-- : ((varScope[i] == var_isGlobal) || (varScope[i] == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount-- : _localVarStringObjectCount--;
                delete[]args[i].pStringConst;                                                                       // delete original variable string
                *pStackLvl->varOrConst.value.ppStringConst = nullptr;                                               // change pointer to string (in variable) to null pointer
            }
        }
        pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
    }


    clearEvalStackLevels(suppliedArgCount + 1);                                                                         // clean up: delete evaluation stack elements for supplied arguments

    // push result to stack
    // --------------------

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

    _pEvalStackTop->varOrConst.value = fcnResult;                                                                       // long, float or pointer to string
    _pEvalStackTop->varOrConst.valueType = fcnResultValueType;                                                          // value type of second operand  
    _pEvalStackTop->varOrConst.tokenType = tok_isConstant;                                                              // use generic constant type
    _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;                                                           // not an array, not an array element (it's a constant) 
}


// -------------------------------
// *   launch Justina function   *
// -------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::launchJustinaFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount) {

    // remember token address of the Justina function token (this is where the Justina function is called), in case an error occurs (while passing arguments etc.)   
    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;


    // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
    // ----------------------------------------------------------------------------------------------

    _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
    _pFlowCtrlStackTop = (OpenFunctionData*)flowCtrlStack.appendListElement(sizeof(OpenFunctionData));
    *((OpenFunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                                 // push caller function data to stack
    ++_callStackDepth;                                                                              // caller can be main, another Justina function or an eval() string

    _activeFunctionData.functionIndex = pFunctionStackLvl->function.index;                          // index of Justina function to call
    _activeFunctionData.blockType = block_JustinaFunction;
    _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                        // command execution ended      


    // create local variable storage for Justina function to be called
    // ---------------------------------------------------------------

    int localVarCount = justinaFunctionData[_activeFunctionData.functionIndex].localVarCountInFunction;
    int paramCount = justinaFunctionData[_activeFunctionData.functionIndex].paramOnlyCountInFunction;

    if (localVarCount > 0) {
        _localVarValueAreaCount++;
        _activeFunctionData.pLocalVarValues = new Val[localVarCount];                               // local variable value: real, pointer to string or array, or (if reference): pointer to 'source' (referenced) variable
        _activeFunctionData.ppSourceVarTypes = new char* [localVarCount];                           // only if local variable is reference to variable or array element: pointer to 'source' variable value type  
        _activeFunctionData.pVariableAttributes = new char[localVarCount];                          // local variable: value type (float, local string or reference); 'source' (if reference) or local variable scope (user, global, static; local, param) 

    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print("+++++ (LOCAL STORAGE) ");   _pDebugOut->println((uint32_t)_activeFunctionData.pLocalVarValues, HEX);
    #endif
    }


    // init local variables: parameters with supplied arguments (scalar and array var refs) and with default values (scalars only), local variables (scalar and array)
    // ---------------------------------------------------------------------------------------------------------------------------------------------------------------

    initFunctionParamVarWithSuppliedArg(suppliedArgCount, pFirstArgStackLvl);
    char* calledFunctionTokenStep = justinaFunctionData[_activeFunctionData.functionIndex].pJustinaFunctionStartToken;
    initFunctionDefaultParamVariables(calledFunctionTokenStep, suppliedArgCount, paramCount);       // return with first token after function definition...
    initFunctionLocalNonParamVariables(calledFunctionTokenStep, paramCount, localVarCount);         // ...and create storage for local array variables


    // delete function name token from evaluation stack
    // ------------------------------------------------
    _pEvalStackTop = (LE_evalStack*)evalStack.getPrevListElement(pFunctionStackLvl);
    _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
    _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);
    evalStack.deleteListElement(pFunctionStackLvl);

    _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                        // store evaluation stack levels in use by callers (call stack)


    // set next step to start of called function
    // -----------------------------------------

    _activeFunctionData.pNextStep = calledFunctionTokenStep;                                        // first step in first statement in called function
    _activeFunctionData.errorStatementStartStep = calledFunctionTokenStep;
    _activeFunctionData.errorProgramCounter = calledFunctionTokenStep;

    return  result_execOK;
}


// --------------------------------------------------------
// *   launch parsing and execution of an eval() string   *
// --------------------------------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::launchEval(LE_evalStack*& pFunctionStackLvl, char* parsingInput) {

    execResult_type execResult{ result_execOK };

    if (parsingInput == nullptr) { return result_eval_nothingToEvaluate; }


    // push current command line storage to command line stack, to make room for the evaluation string (to parse) 
    // ----------------------------------------------------------------------------------------------------------

    // the parsed command line pushed, contains the parsed statements 'calling' (parsing and executing) the eval() string 
    // this is either an outer level parsed eval() string, or the parsed command line where execution started  

#if PRINT_PARSED_STAT_STACK
    _pDebugOut->print("  >> PUSH parsed statements (launch eval): last step: "); _pDebugOut->println(_lastUserCmdStep - (_programStorage + _progMemorySize));
#endif
    long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
    _pParsedCommandLineStackTop = (char*)parsedCommandLineStack.appendListElement(sizeof(char*) + parsedUserCmdLen);
    *(char**)_pParsedCommandLineStackTop = _lastUserCmdStep;
    memcpy(_pParsedCommandLineStackTop + sizeof(char*), (_programStorage + _progMemorySize), parsedUserCmdLen);

    // parse eval() string
    // -------------------
    char* pDummy{};
    char* holdProgramCounter = _programCounter;
    _programCounter = _programStorage + _progMemorySize;                                                    // parsed statements go to immediate mode program memory
    _parsingEvalString = true;

    // create a temporary string to hold expressions to parse, with an extra semicolon added at the end (in case it's missing)
    _systemVarStringObjectCount++;
    char* pEvalParsingInput = new char[strlen(parsingInput) + 2]; // room for additional semicolon (in case string is not ending with it) and terminating '\0'
#if PRINT_HEAP_OBJ_CREA_DEL
    _pDebugOut->print("+++++ (system var str) "); _pDebugOut->println((uint32_t)pEvalParsingInput, HEX);
#endif

    strcpy(pEvalParsingInput, parsingInput);                                                                // copy the actual string
    pEvalParsingInput[strlen(parsingInput)] = term_semicolon[0];
    pEvalParsingInput[strlen(parsingInput) + 1] = '\0';
    char* pParsingInput_temp = pEvalParsingInput;                                                           // temp, because value will be changed upon return (preserve original pointer value)
    // note: application flags are not adapted (would not be passed to caller immediately)
    int dummy{};
    parseTokenResult_type result = parseStatement(pParsingInput_temp, pDummy, dummy);                       // parse all eval() expressions in ONE go (which is not the case for standard parsing and trace string parsing)
#if PRINT_HEAP_OBJ_CREA_DEL
    _pDebugOut->print("----- (system var str) "); _pDebugOut->println((uint32_t)pEvalParsingInput, HEX);
#endif
    _systemVarStringObjectCount--;
    delete[] pEvalParsingInput;

    // last step of just parsed eval() string. Note: adding sizeof(tok_no_token) because not yet added
    _lastUserCmdStep = ((result == result_tokenFound) ? _programCounter + sizeof(tok_no_token) : nullptr);  // if parsing error, store nullptr as last token position

    _parsingEvalString = false;
    if (result != result_tokenFound) {
        // immediate mode program memory now contains a PARTIALLY parsed eval() expression string...
        // ...(up to the token producing a parsing error) and a few string constants may have been created in the process.
        // restore the situation from BEFORE launching the parsing of this now partially parsed eval() expression:   
        // delete any newly parsed string constants created in the parsing attempt
        // pop the original imm.mode parsed statement stack top level again to imm. mode program memory
        // a corresponding entry in flow ctrl stack has not yet been created either)

        deleteConstStringObjects(_programStorage + _progMemorySize);      // string constants that were created just now 
        memcpy((_programStorage + _progMemorySize), _pParsedCommandLineStackTop + sizeof(char*), parsedUserCmdLen);
        parsedCommandLineStack.deleteListElement(_pParsedCommandLineStackTop);
        _pParsedCommandLineStackTop = (char*)parsedCommandLineStack.getLastListElement();
    #if PRINT_PARSED_STAT_STACK
        _pDebugOut->print("  >> POP parsed statements (launch eval parse error): last step: "); _pDebugOut->println(_lastUserCmdStep - (_programStorage + _progMemorySize));
    #endif

        _evalParseErrorCode = result;       // remember
        return result_eval_parsingError;
    }

    *(_programCounter) = tok_isEvalEnd | 0x10;                                                              // replace '\0' after parsed statements with 'end eval ()' token (length 1 in upper 4 bits)
    *(_programCounter + 1) = tok_no_token;
    _programCounter = holdProgramCounter;                                                                   // original program counter (points to closing par. of eval() function)



    // note current token (Justina function 'eval' token) position, in case an error happens IN THE CALLER immediately upon return from function to be called
    // ------------------------------------------------------------------------------------------------------------------------------------------------------

    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;                     // CALLER function 'eval' token position, before pushing caller function data to stack   


    // push caller function data (or main = user entry level in immediate mode) on FLOW CONTROL stack 
    // ----------------------------------------------------------------------------------------------

    _pFlowCtrlStackMinus2 = _pFlowCtrlStackMinus1; _pFlowCtrlStackMinus1 = _pFlowCtrlStackTop;
    _pFlowCtrlStackTop = (OpenFunctionData*)flowCtrlStack.appendListElement(sizeof(OpenFunctionData));
    *((OpenFunctionData*)_pFlowCtrlStackTop) = _activeFunctionData;                                         // push caller function data to stack
    ++_callStackDepth;                                                                                      // caller can be main, another Justina function or an eval() string

    _activeFunctionData.functionIndex = pFunctionStackLvl->function.index;                                  // index of (Justina) eval() function - but will not be used
    _activeFunctionData.blockType = block_eval;                                                             // now executing parsed 'eval' string
    _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                // command execution ended 

    _activeFunctionData.callerEvalStackLevels = evalStack.getElementCount();                                // store evaluation stack levels in use by callers (call stack)

    // set next step to start of called function
    // -----------------------------------------

    _activeFunctionData.pNextStep = _programStorage + _progMemorySize;                                      // first step in first statement in parsed eval() string
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
                        _pDebugOut->print("+++++ (loc var str) ");   _pDebugOut->println((uint32_t)_activeFunctionData.pLocalVarValues[i].pStringConst, HEX);
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
    int tokenType = *pStep & 0x0F;                                                                                          // function name token of called function

    if (suppliedArgCount < paramCount) {                                                                                    // missing arguments: use parameter default values to init local variables
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

            _activeFunctionData.pVariableAttributes[count] = valueType;                                                     // long, float or string (array flag is reset here)

            if (operandIsLong) {                                                                                            // operand is float constant
                memcpy(&_activeFunctionData.pLocalVarValues[count].longConst, ((TokenIsConstant*)pStep)->cstValue.longConst, sizeof(long));
            }
            else if (operandIsFloat) {                                                                                      // operand is float constant
                memcpy(&_activeFunctionData.pLocalVarValues[count].floatConst, ((TokenIsConstant*)pStep)->cstValue.floatConst, sizeof(float));
            }
            else {                                                                                                          // operand is parsed string constant: create a local copy and store in variable
                char* s{ nullptr };
                memcpy(&s, ((TokenIsConstant*)pStep)->cstValue.pStringConst, sizeof(char*));                                // copy the pointer, NOT the string  

                _activeFunctionData.pLocalVarValues[count].pStringConst = nullptr;                                          // init (if empty string)
                if (s != nullptr) {
                    int stringlen = strlen(s);
                    _localVarStringObjectCount++;
                    _activeFunctionData.pLocalVarValues[count].pStringConst = new char[stringlen + 1];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("+++++ (loc var str) ");   _pDebugOut->println((uint32_t)_activeFunctionData.pLocalVarValues[count].pStringConst, HEX);
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

    int count = paramCount;                                                                                                 // sum of mandatory and optional parameters

    while (count != localVarCount) {
        findTokenStep(pStep, tok_isReservedWord, cmdcod_var, cmdcod_constVar);                                              // find local 'var' or 'const' keyword (always there)

        do {
            // in case variable is not an array and it does not have an initializer: init now as zero (float). Arrays without initializer will be initialized later
            _activeFunctionData.pLocalVarValues[count].floatConst = 0;
            _activeFunctionData.pVariableAttributes[count] = value_isFloat;                                                 // for now, assume scalar

            tokenType = jumpTokens(2, pStep, terminalCode);                                                                 // either left parenthesis, assignment, comma or semicolon separator (always a terminal)

            // handle array definition dimensions 
            // ----------------------------------

            int dimCount = 0, arrayElements = 1;
            int arrayDims[MAX_ARRAY_DIMS]{ 0 };

            if (terminalCode == termcod_leftPar) {                                                                          // array opening parenthesis
                do {
                    tokenType = jumpTokens(1, pStep);                                                                       // dimension

                    // increase dimension count and calculate elements (checks done during parsing)
                    char valueType = ((*(char*)pStep) >> 4) & value_typeMask;
                    bool isLong = (valueType == value_isLong);                                                              // or float (checked during parsing)
                    Val dimSubscript{};
                    if (isLong) { memcpy(&dimSubscript, ((TokenIsConstant*)pStep)->cstValue.longConst, sizeof(long)); }
                    else { memcpy(&dimSubscript, ((TokenIsConstant*)pStep)->cstValue.floatConst, sizeof(float)); dimSubscript.longConst = (long)dimSubscript.floatConst; }
                    arrayElements *= dimSubscript.longConst;
                    arrayDims[dimCount] = dimSubscript.longConst;
                    dimCount++;

                    tokenType = jumpTokens(1, pStep, terminalCode);                                                         // comma (dimension separator) or right parenthesis
                } while (terminalCode != termcod_rightPar);

                // create array (init later)
                _localArrayObjectCount++;
                float* pArray = new float[arrayElements + 1];
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("+++++ (loc ar stor) "); _pDebugOut->println((uint32_t)pArray, HEX);
            #endif
                _activeFunctionData.pLocalVarValues[count].pArray = pArray;
                _activeFunctionData.pVariableAttributes[count] |= var_isArray;                                              // set array bit

                // store dimensions in element 0: char 0 to 2 is dimensions; char 3 = dimension count 
                for (int i = 0; i < MAX_ARRAY_DIMS; i++) {
                    ((char*)pArray)[i] = arrayDims[i];
                }
                ((char*)pArray)[3] = dimCount;                                                                              // (note: for param arrays, set to max dimension count during parsing)

                tokenType = jumpTokens(1, pStep, terminalCode);                                                             // assignment, comma or semicolon
            }


            // handle initialisation (if initializer provided)
            // -----------------------------------------------

            if (terminalCode == termcod_assign) {
                tokenType = jumpTokens(1, pStep);                                                                           // constant

                // fetch constant
                tokenType = *pStep & 0x0F;

                Val initializer{ };                                                                                         // last token is a number constant: dimension
                char* pString{ nullptr };

                char valueType = ((*(char*)pStep) >> 4) & value_typeMask;
                bool isLong = (valueType == value_isLong);
                bool isFloat = (valueType == value_isFloat);

                if (isLong) { memcpy(&initializer, ((TokenIsConstant*)pStep)->cstValue.longConst, sizeof(long)); }
                if (isFloat) { memcpy(&initializer, ((TokenIsConstant*)pStep)->cstValue.floatConst, sizeof(float)); }
                else { memcpy(&pString, ((TokenIsConstant*)pStep)->cstValue.pStringConst, sizeof(pString)); }               // copy pointer to string (not the string itself)
                int length = (isLong || isFloat) ? 0 : (pString == nullptr) ? 0 : strlen(pString);                          // only relevant for strings
                _activeFunctionData.pVariableAttributes[count] =
                    (_activeFunctionData.pVariableAttributes[count] & ~value_typeMask) | valueType;

                // array: initialize (note: test for non-empty string - which are not allowed as initializer - done during parsing)
                if ((_activeFunctionData.pVariableAttributes[count] & var_isArray) == var_isArray) {
                    void* pArray = ((void**)_activeFunctionData.pLocalVarValues)[count];                                    // void pointer to an array 
                    // fill up with numeric constants or (empty strings:) null pointers
                    if (isLong) { for (int elem = 1; elem <= arrayElements; elem++) { ((long*)pArray)[elem] = initializer.longConst; } }
                    else if (isFloat) { for (int elem = 1; elem <= arrayElements; elem++) { ((float*)pArray)[elem] = initializer.floatConst; } }
                    else { for (int elem = 1; elem <= arrayElements; elem++) { ((char**)pArray)[elem] = nullptr; } }
                }
                // scalar: initialize
                else {
                    if (isLong) { _activeFunctionData.pLocalVarValues[count].longConst = initializer.longConst; }           // store numeric constant
                    else if (isFloat) { _activeFunctionData.pLocalVarValues[count].floatConst = initializer.floatConst; }   // store numeric constant
                    else {
                        if (length == 0) { _activeFunctionData.pLocalVarValues[count].pStringConst = nullptr; }             // an empty string does not create a heap object
                        else { // create string object and store string
                            _localVarStringObjectCount++;
                            char* pVarString = new char[length + 1];                                                        // create char array on the heap to store alphanumeric constant, including terminating '\0'
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            _pDebugOut->print("+++++ (loc var str) "); _pDebugOut->println((uint32_t)pVarString, HEX);
                        #endif

                            // store alphanumeric constant in newly created character array
                            strcpy(pVarString, pString);                                                                    // including terminating \0
                            _activeFunctionData.pLocalVarValues[count].pStringConst = pVarString;                           // store pointer to string
                        }
                    }
                }

                tokenType = jumpTokens(1, pStep, terminalCode);                                                             // comma or semicolon
            }

            else {  // no initializer: if array, initialize it now (scalar has been initialized already)
                if ((_activeFunctionData.pVariableAttributes[count] & var_isArray) == var_isArray) {
                    void* pArray = ((void**)_activeFunctionData.pLocalVarValues)[count];                                    // void pointer to an array 
                    for (int elem = 1; elem <= arrayElements; elem++) { ((float*)pArray)[elem] = 0.; }                      // float (by default)
                }
            }
            count++;

        } while (terminalCode == termcod_comma);

    }
};


// ----------------------------------
// *   terminate Justina function   *
// ----------------------------------

Justina_interpreter::execResult_type Justina_interpreter::terminateJustinaFunction(bool addZeroReturnValue) {
    if (addZeroReturnValue) {
        _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;
        _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;                                                              // use generic constant type
        _pEvalStackTop->varOrConst.value.longConst = 0;                                                                     // default return value (long)
        _pEvalStackTop->varOrConst.valueType = value_isLong;
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate;
    }
    else { makeIntermediateConstant(_pEvalStackTop); }                                                                      // if not already an intermediate constant

    // delete local variable arrays and strings (only if local variable is not a reference)

    int localVarCount = justinaFunctionData[_activeFunctionData.functionIndex].localVarCountInFunction;                     // of function to be terminated
    int paramOnlyCount = justinaFunctionData[_activeFunctionData.functionIndex].paramOnlyCountInFunction;                   // of function to be terminated

    if (localVarCount > 0) {
        deleteStringArrayVarsStringObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);
        deleteVariableValueObjects(_activeFunctionData.pLocalVarValues, _activeFunctionData.pVariableAttributes, localVarCount, paramOnlyCount, false, false, true);

    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print("----- (LOCAL STORAGE) ");   _pDebugOut->println((uint32_t)_activeFunctionData.pLocalVarValues, HEX);
    #endif
        _localVarValueAreaCount--;
        // release local variable storage for function that has been called
        delete[] _activeFunctionData.pLocalVarValues;
        delete[] _activeFunctionData.pVariableAttributes;
        delete[] _activeFunctionData.ppSourceVarTypes;
    }
    char blockType = block_none;                                                                                            // init
    do {
        blockType = *(char*)_pFlowCtrlStackTop;                                                                             // always at least one level present for caller (because returning to it)

        // load local storage pointers again for caller function and restore pending step & active function information for caller function
        if ((blockType == block_JustinaFunction) || (blockType == block_eval)) { _activeFunctionData = *(OpenFunctionData*)_pFlowCtrlStackTop; }    // caller level

        // delete FLOW CONTROL stack level (any optional CALLED function open block stack level) 
        flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
        _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);

    } while ((blockType != block_JustinaFunction) && (blockType != block_eval));                                            // caller level can be caller eval() or caller Justina function
    --_callStackDepth;                                                                                                      // caller reached: call stack depth decreased by 1


    if ((_activeFunctionData.pNextStep >= (_programStorage + _progMemorySize)) && (_callStackDepth == 0)) {                 // not within a function, not within eval() execution, and not in debug mode       
        if (_localVarValueAreaCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** Local variable storage area objects cleanup error. Remaining: "); _pDebugOut->println(_localVarValueAreaCount);
        #endif
            _localVarValueAreaErrors += abs(_localVarValueAreaCount);
            _localVarValueAreaCount = 0;
        }

        if (_localVarStringObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** Local variable string objects cleanup error. Remaining: "); _pDebugOut->println(_localVarStringObjectCount);
        #endif
            _localVarStringObjectErrors += abs(_localVarStringObjectCount);
            _localVarStringObjectCount = 0;
        }

        if (_localArrayObjectCount != 0) {
        #if PRINT_OBJECT_COUNT_ERRORS
            _pDebugOut->print("*** Local array objects cleanup error. Remaining: "); _pDebugOut->println(_localArrayObjectCount);
        #endif
            _localArrayObjectErrors += abs(_localArrayObjectCount);
            _localArrayObjectCount = 0;
        }
    }

    execResult_type execResult = execAllProcessedOperators();                                                               // continue in caller !!!

    return execResult;
}


// -----------------------------------------------
// *   terminate execution of an eval() string   *
// -----------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::terminateEval() {
    execResult_type execResult{ result_execOK };                                                                            // init

    if (evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels >= 1) {
        makeIntermediateConstant(_pEvalStackTop);
    }
    else { return result_eval_nothingToEvaluate; }


    char blockType = block_none;                                                                                            // init
    do {
        blockType = *(char*)_pFlowCtrlStackTop;                                                                             // always at least one level present for caller (because returning to it)

        // load local storage pointers again for caller function and restore pending step & active function information for caller function
        if ((blockType == block_JustinaFunction) || (blockType == block_eval)) { _activeFunctionData = *(OpenFunctionData*)_pFlowCtrlStackTop; }

        // delete FLOW CONTROL stack level (any optional CALLED function open block stack level) 
        flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
        _pFlowCtrlStackMinus1 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackTop);
        _pFlowCtrlStackMinus2 = flowCtrlStack.getPrevListElement(_pFlowCtrlStackMinus1);

    } while ((blockType != block_JustinaFunction) && (blockType != block_eval));                                            // caller level can be caller eval() or caller Justina function
    --_callStackDepth;                                                                                                      // caller reached: call stack depth decreased by 1

    // overwrite the parsed 'EVAL' string expressions
    // before removing, delete any parsed strng constants for that command line

    _lastUserCmdStep = *(char**)_pParsedCommandLineStackTop;                                                                     // pop parsed user cmd length
    long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
    deleteConstStringObjects(_programStorage + _progMemorySize);
    memcpy((_programStorage + _progMemorySize), _pParsedCommandLineStackTop + sizeof(char*), parsedUserCmdLen);                  // size berekenen
    parsedCommandLineStack.deleteListElement(_pParsedCommandLineStackTop);
    _pParsedCommandLineStackTop = (char*)parsedCommandLineStack.getLastListElement();
#if PRINT_PARSED_STAT_STACK
    _pDebugOut->print("  >> POP parsed statements (terminate eval): last step: "); _pDebugOut->println(_lastUserCmdStep - (_programStorage + _progMemorySize));
#endif

    if (evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels >= 1) {
        execResult = execAllProcessedOperators();
        if (execResult != result_execOK) { return execResult; }
    }
    return execResult;
}


// -----------------------------------------------
// *   push terminal token to evaluation stack   *
// -----------------------------------------------

void Justina_interpreter::pushTerminalToken(int& tokenType) {                                                               // terminal token is assumed

    // push terminal index to stack

    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(TerminalTokenLvl));
    _pEvalStackTop->terminal.tokenType = tokenType;
    _pEvalStackTop->terminal.tokenAddress = _programCounter;                                                                // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->terminal.index = (*_programCounter >> 4) & 0x0F;                                                        // terminal token only: calculate from partial index stored in high 4 bits of token type 
    _pEvalStackTop->terminal.index += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
};


// -----------------------------------------------------------------
// *   push internal cpp function name token to evaluation stack   *
// -----------------------------------------------------------------

void Justina_interpreter::pushInternCppFunctionName(int& tokenType) {                                                       // internal cpp function token is assumed

    // push internal cpp function index to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(FunctionLvl));
    _pEvalStackTop->function.tokenType = tokenType;
    _pEvalStackTop->function.tokenAddress = _programCounter;                                                                // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->function.index = ((TokenIsInternCppFunction*)_programCounter)->tokenIndex;                              // internal cpp functions only
};


// -----------------------------------------------------------------
// *   push external cpp function name token to evaluation stack   *
// -----------------------------------------------------------------

void Justina_interpreter::pushExternCppFunctionName(int& tokenType) {                                                       // external cpp function token is assumed

    // push external cpp function return value type and index within functions for a specific return value type to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(FunctionLvl));
    _pEvalStackTop->function.tokenType = tokenType;
    _pEvalStackTop->function.tokenAddress = _programCounter;                                                                // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->function.returnValueType = ((TokenIsExternCppFunction*)_programCounter)->returnValueType;               // 0 = bool, 1 = char, 2 = int, 3 = long, 4 = float, 5 = char*, 6 = void (but returns zero to Justina)               
    _pEvalStackTop->function.funcIndexInType = ((TokenIsExternCppFunction*)_programCounter)->funcIndexInType;
};


// ------------------------------------------------------------
// *   push Justina function name token to evaluation stack   *
// ------------------------------------------------------------

void Justina_interpreter::pushJustinaFunctionName(int& tokenType) {                                                         // Justina function token is assumed

    // push Justina function index to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(FunctionLvl));
    _pEvalStackTop->function.tokenType = tokenType;
    _pEvalStackTop->function.tokenAddress = _programCounter;                                                                // only for finding source error position during unparsing (for printing)

    _pEvalStackTop->function.index = ((TokenIsJustinaFunction*)_programCounter)->identNameIndex;
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

void Justina_interpreter::pushGenericName(int& tokenType) {                                                                 // float or string constant token is assumed

    // push real or string parsed constant, value type and array flag (false) to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    // just push the string pointer to the generic name (no indexes, ...)
    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(GenericNameLvl));
    _pEvalStackTop->varOrConst.tokenType = tok_isGenericName;                                                               // use generic constant type
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;                                                              // only for finding source error position during unparsing (for printing)

    char* pAnum{ nullptr };
    memcpy(&pAnum, ((TokenIsConstant*)_programCounter)->cstValue.pStringConst, sizeof(pAnum));                              // char pointer not necessarily aligned with word size: copy pointer instead
    _pEvalStackTop->genericName.pStringConst = pAnum;                                                                       // store char* in stack 
};


// ----------------------------------------------
// *   push variable token to evaluation stack   *
// ----------------------------------------------

void Justina_interpreter::pushVariable(int& tokenType) {                                                                    // with variable token type

    // push variable base address, variable value type (real, string) and array flag to stack
    _pEvalStackMinus2 = _pEvalStackMinus1; _pEvalStackMinus1 = _pEvalStackTop;

    _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
    _pEvalStackTop->varOrConst.tokenType = tokenType;
    _pEvalStackTop->varOrConst.tokenAddress = _programCounter;

    void* varAddress = fetchVarBaseAddress((TokenIsVariable*)_programCounter, _pEvalStackTop->varOrConst.varTypeAddress, _pEvalStackTop->varOrConst.valueType,
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags);
    _pEvalStackTop->varOrConst.value.pBaseValue = varAddress;                                                               // base address of variable
    _pEvalStackTop->varOrConst.valueAttributes = 0;                                                                         // init
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
    // - selfValueType contains the value type (long, float, char* or reference) of the Justina variable indicated by the token
    // - sourceVarScopeAndFlags contains the SOURCE variable's scope, 'is array' and 'is constant variable' (declared with 'const') flags, respectively
    // - sourceVarTypeAddress points to (contains the address of) source variable's attributes (value type, ...) 
    // - return pointer will point to (contain the address of) the 'self' variable base address (Justina variable indicated by the token): the address where the variable's value is stored
    //   note that this 'value' can be an address itself: for referenced variables, but alse for arrays and strings (char*)

    int varNameIndex = pVarToken->identNameIndex;
    uint8_t varScope = pVarToken->identInfo & var_scopeMask;                                                                // global, user, local, static or parameter
    bool isUserVar = (varScope == var_isUser);
    bool isGlobalVar = (varScope == var_isGlobal);
    bool isStaticVar = (varScope == var_isStaticInFunc);

    int valueIndex = pVarToken->identValueIndex;

    if (isUserVar) {
        selfValueType = userVarType[valueIndex] & value_typeMask;                                                           // value type (indicating long, float, char* or 'variable reference')
        sourceVarTypeAddress = userVarType + valueIndex;                                                                    // pointer to source variable's attributes          
        sourceVarScopeAndFlags = pVarToken->identInfo & (var_scopeMask | var_isArray | var_isConstantVar);
        return &userVarValues[valueIndex];                                                                                  // pointer to value (long, float, char* or (array variables, strings) pointer to array or string)
    }
    else if (isGlobalVar) {
        selfValueType = globalVarType[valueIndex] & value_typeMask;                                                         // value type (indicating long, float, char* or 'variable reference')
        sourceVarTypeAddress = globalVarType + valueIndex;                                                                  // pointer to source variable's attributes
        sourceVarScopeAndFlags = pVarToken->identInfo & (var_scopeMask | var_isArray | var_isConstantVar);
        return &globalVarValues[valueIndex];                                                                                // pointer to value (long, float, char* or (array variables, strings) pointer to array or string)
    }
    else if (isStaticVar) {
        selfValueType = staticVarType[valueIndex] & value_typeMask;                                                         // value type (indicating long, float, char* or 'variable reference')
        sourceVarTypeAddress = staticVarType + valueIndex;                                                                  // pointer to source variable's attributes
        sourceVarScopeAndFlags = pVarToken->identInfo & (var_scopeMask | var_isArray | var_isConstantVar);
        return &staticVarValues[valueIndex];                                                                                // pointer to value (long, float, char* or (array variables, strings) pointer to array or string)
    }

    // local variables (including parameters)    
    else {
        // first locate the debug command level (either in active function data or down in the flow control stack)
        // from there onwards, find the first flow control stack level containing a 'function' block type  
        // The open function data (function where the program was stopped) needed to retrieve function variable data will referenced in that flow control stack level
        //
        // note: levels in between debug command level and open function level may exist, containing open block data for the debug command level
        // these levels can NOT refer to an eval() string execution level, because a program can not be stopped during the execution of an eval() string
        // (although it can during a Justina function called from an eval() string)

        int blockType = _activeFunctionData.blockType;                                                                      // init
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                                                                       // one level below _activeFunctionData

        // variable is a local (including parameter) value: if the current flow control stack level does not refer to a function, but to a command line or eval() block type,
        // then the variable is a local variable of a stopped program's open function 
        bool isStoppedFunctionVar = (blockType == block_JustinaFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + _progMemorySize)) : true;     // command line or eval() block type

        if (isStoppedFunctionVar) {
            bool isDebugCmdLevel = (blockType == block_JustinaFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + _progMemorySize)) : false;

            if (!isDebugCmdLevel) {       // find debug level in flow control stack instead
                do {
                    blockType = *(char*)pFlowCtrlStackLvl;
                    isDebugCmdLevel = (blockType == block_JustinaFunction) ? (((OpenFunctionData*)pFlowCtrlStackLvl)->pNextStep >= (_programStorage + _progMemorySize)) : false;
                    pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                } while (!isDebugCmdLevel);                                                                                 // stack level for open function found immediate below debug line found (always match)
            }

            blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
            while (blockType != block_JustinaFunction) {
                pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
            }
        }
        else {                                                                                                              // the variable is a local variable of the function referenced in _activeFunctionData
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


