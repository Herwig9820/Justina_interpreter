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
#define PRINT_PARSED_CMD_STACK 0
#define PRINT_DEBUG_INFO 0


// *****************************************************************
// ***        class Justina_interpreter - implementation         ***
// *****************************************************************


// ----------------------------------------------------------------------
// *   execute a processed command  (statement starting with a keyword) *
// ----------------------------------------------------------------------

// structure of a command: keyword expression, expression, ... ;
// during parsing, preliminary checks have been done already: minimum, maximum number of expressions allowed, type of expressions allowed etc.
// further checks are performed at runtime: do expressions yield a result of the currect type, etc.
// the expression list as a whole is not put between parentheses (in contrast to function arguments)

Justina_interpreter::execResult_type Justina_interpreter::execProcessedCommand(bool& isFunctionReturn, bool& forcedStopRequest, bool& forcedAbortRequest) {

    // this c++ function is called when the END of a command statement (semicolon) is encountered during execution, and all arguments (expressions)...
    // ...have been evaluated and their results put on the evaluation stack
    // now it is time to use these results to execute a specific command and pop the stack afterwards

    // IMPORTANT: when adding code for new Justina functions, it must be written so that when a Justina  error occurs, a c++ RETURN <error code> statement is executed.
    // BUT prior to this, all 'intermediate character strings' which are NOT referenced within the evaluation stack MUST BE  DELETED (if referenced, they will be deleted automatically by error handling)

    isFunctionReturn = false;  // init
    execResult_type execResult = result_execOK;
    int cmdArgCount = evalStack.getElementCount() - _activeFunctionData.callerEvalStackLevels;

    // note supplied argument count and go to first argument (if any)
    LE_evalStack* pStackLvl = _pEvalStackTop;
    for (int i = 1; i < cmdArgCount; i++) {                                                                               // skipped if no arguments, or if one argument
        pStackLvl = (LE_evalStack*)evalStack.getPrevListElement(pStackLvl);                                                 // iterate to first argument
    }

    _activeFunctionData.errorProgramCounter = _activeFunctionData.activeCmd_tokenAddress;

#if PRINT_DEBUG_INFO
    _pDebugOut->print("                 process command code: "); _pDebugOut->println((int)_activeFunctionData.activeCmd_ResWordCode);
#endif

    switch (_activeFunctionData.activeCmd_ResWordCode) {                                                                    // command code 

        // -------------------------------------------------
        // Stop code execution (program only, for debugging)
        // -------------------------------------------------

        case cmdcod_stop:
        {
            // 'stop' behaves as if an error occurred, in order to follow the same processing logic  

            // skip non-executable commands
            do {
                int tokenType = *_activeFunctionData.pNextStep & 0x0F;
                if (tokenType != tok_isReservedWord) { break; }
                int tokenIndex = ((TokenIsResWord*)_activeFunctionData.pNextStep)->tokenIndex;
                if ((_resWords[tokenIndex].restrictions & cmd_skipDuringExec) == 0) { break; }
                findTokenStep(_activeFunctionData.pNextStep, true, tok_isTerminalGroup1, termcod_semicolon, termcod_semicolon_BPset, termcod_semicolon_BPallowed);             // find semicolon (always match)
                jumpTokens(1, _activeFunctionData.pNextStep);             // first token after semicolon
            } while (true);

            _activeFunctionData.errorStatementStartStep = _activeFunctionData.pNextStep;
            _activeFunctionData.errorProgramCounter = _activeFunctionData.pNextStep;

            // RETURN with 'event' error
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
            return result_stopForDebug;
        }
        break;


        // ------------------------
        // Quit Justina interpreter
        // ------------------------

        case cmdcod_quit:
        {

            // optional argument 1 clear all
            // - value is 1: keep interpreter in memory on quitting (retain data), value is 0: clear all and exit Justina 
            // 'quit' behaves as if an error occurred, in order to follow the same processing logic  

            if (cmdArgCount != 0) {                                                                                       // 'quit' command only                                                                                      
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];

                copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);                    // copy arguments from evaluationi stack
                if (((uint8_t)(valueType[0]) != value_isLong) && ((uint8_t)(valueType[0]) != value_isFloat)) { return result_arg_numberExpected; }
                if ((uint8_t)(valueType[0]) == value_isFloat) { args[0].longConst = (int)args[0].floatConst; }
                // specifying 'retain data' or 'release memory' argument: silent mode. Note: 'retain data' will only set if allowed by _justinaConstraints 
                _keepInMemory = ((args[0].longConst != 0) && ((_justinaConstraints & 0b0100) == 0b0100));                   // silent mode (even not possible to cancel)
                return result_quit;
            }

            else {      // keep in memory when quitting, cancel: ask user
                if ((_justinaConstraints & 0b0100) == 0b0100) {                                                             // retaining data is allowed: ask question and note answer
                    while (_pConsoleIn->available() > 0) { readFrom(0); }                                                   // empty console buffer first (to allow the user to start with an empty line)

                    do {
                        bool doStop{ false }, doAbort{ false }, doCancel{ false }, doDefault{ false };
                        printlnTo(0, "===== Quit Justina: keep in memory ? (please answer Y, N or \\c to cancel) =====");

                        // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                        // return flags doStop, doAbort, doCancel, doDefault if user included corresponding escape sequences in input string.
                        int length{ 2 };
                        char input[2 + 1] = "";                                                                          // init: empty string
                        // NOTE: quitting has higher priority than aborting or stopping, and quitting anyway, so not needed to check abort and stop flags
                        // NOTE: doDefault is a dummy argument here
                        if (getConsoleCharacters(doStop, doAbort, doCancel, doDefault, input, length, '\n')) { return result_kill; }  // kill request from caller ? 
                        if (doAbort) { forcedAbortRequest = true; break; }                                                  // abort running code (program or immediate mode statements)
                        else if (doStop) { forcedStopRequest = true; }                                                      // stop a running program (do not produce stop event yet, wait until program statement executed)
                        if (doCancel) { break; }                                                                            // '\c': cancel operation (lowest priority)

                        bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                        if (validAnswer) {
                            _keepInMemory = (tolower(input[0]) == 'y');
                            return result_quit;                                                                             // Justina quit command executed 
                        }
                    } while (true);
                }
                else {
                    _keepInMemory = false;                                                                                  // do not retain data on quitting (it's not allwed by caller)
                    return result_quit;
                }
            }

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // --------------------------------------
        // Restart or abort stopped program again
        // --------------------------------------

        // these commands behave as if an error occurred, in order to follow the same processing logic  
        // the commands are issued from the command line and restart a program stopped for debug (except the abort command)

        // step: executes one program step. If a 'parsing only' statement is encountered, it will simply be skipped
        // step over: if the statement is a function call, executes the function without stopping until control returns to the caller. For other statements, behaves like 'step'
        // step out: continues execution without stopping, until control is passed to the caller
        // step out of block: if in an open block (while, for, ...), continues execution until control passes to a statement outside the open block. Otherwise, behaves like 'step'
        // step to block end: if in an open block (while, for, ...), continues execution until the next statement to execute is the 'block end' statement...
        // ... this allows you to execute a 'for' loop one loop at the time, for instance. If outside an open block, behaves like 'step' 
        // go: continues execution until control returns to the user
       // abort a program while it is stopped

        case cmdcod_step:
        case cmdcod_stepOver:
        case cmdcod_stepOut:
        case cmdcod_stepOutOfBlock:
        case cmdcod_stepToBlockEnd:
        case cmdcod_go:
        case cmdcod_abort:
        {

            bool OpenBlock{ true };
            char nextStepBlockAction{ block_na };          // init

            if (_openDebugLevels == 0) { return result_noProgramStopped; }

            // is this a debugging command requiring an open block ? (-> step out of block, step to block end commands)
            if (((_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOutOfBlock) ||
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepToBlockEnd))) {

                // determine whether an open block exists within the active function:
                // to do that, locate flow control control stack level below the stopped function data 
                // note: because program is currently stopped, _activeFunctionData represents the debug level command line. FlowCtrlStack top levels contain...
                // ...any open blocks for the debug level, then the level representing the stopped function and only then any open blocks for that stopped function, ...
                // followed by levels for other open functions in the call stack with their open blocks... and finally a level representing the command line...
                // ...from where the program was called, with any open blocks (if any programs are stopped, additional levels are present, basically repeating the same scheme).
                void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;
                char blockType{};
                do {
                    // skip all debug level blocks and open function block (always there). Then, check the next control flow stack level (also always there)
                    blockType = ((openBlockGeneric*)pFlowCtrlStackLvl)->blockType;
                    pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                } while ((blockType != block_JustinaFunction) && (blockType != block_eval));

                // access the flow control stack level below the stack level for the active function, and check the blocktype: is it an open block within the function ?
                // (if not, then it's the stack level for the caller already)
                blockType = ((openBlockGeneric*)pFlowCtrlStackLvl)->blockType;
                if ((blockType != block_for) && (blockType != block_while) && (blockType != block_if)) { OpenBlock = false; }   // is it an open block ?
            }


            // overwrite the parsed command line (containing the 'step', 'go' or 'abort' command) with the command line stack top and pop the command line stack top
            // before removing, delete any parsed string constants for that command line

            _lastUserCmdStep = *(char**)_pParsedCommandLineStackTop;                                                             // pop program step of last user cmd token ('tok_no_token')
            long parsedUserCmdLen = _lastUserCmdStep - (_programStorage + _progMemorySize) + 1;
            deleteConstStringObjects(_programStorage + _progMemorySize);
            memcpy((_programStorage + _progMemorySize), _pParsedCommandLineStackTop + sizeof(char*), parsedUserCmdLen);          // size berekenen
            parsedCommandLineStack.deleteListElement(_pParsedCommandLineStackTop);
            _pParsedCommandLineStackTop = parsedCommandLineStack.getLastListElement();
        #if PRINT_PARSED_CMD_STACK
            _pDebugOut->print("  >> POP parsed statements (Go): steps = "); _pDebugOut->println(_lastUserCmdStep - (_programStorage + _progMemorySize));
        #endif
            --_openDebugLevels;

            // abort: all done
            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_abort) { return result_abort; }

            _stepCmdExecuted = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_step) ? db_singleStep :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOut) ? db_stepOut :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOver) ? db_stepOver :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepOutOfBlock) ? (OpenBlock ? db_stepOutOfBlock : db_singleStep) :
                (_activeFunctionData.activeCmd_ResWordCode == cmdcod_stepToBlockEnd) ? (OpenBlock ? db_stepToBlockEnd : db_singleStep) :
                db_continue;

            // currently, at least one program is stopped (we are in debug mode)
            // find the flow control stack entry for the stopped function and make it the active function again (remove the flow control stack level for the debugging command line)
            char blockType = block_none;            // init
            do {
                // always at least one open function (because returning to caller from it)
                blockType = ((openBlockGeneric*)_pFlowCtrlStackTop)->blockType;

                // load local storage pointers again for interrupted function and restore pending step & active function information for interrupted function
                if (blockType == block_JustinaFunction) { _activeFunctionData = *(OpenFunctionData*)_pFlowCtrlStackTop; }

                // delete FLOW CONTROL stack level that contained caller function storage pointers and return address (all just retrieved to _activeFunctionData)
                flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
            } while (blockType != block_JustinaFunction);
            --_callStackDepth;          // deepest open function removed from flow control stack (as well as optional debug command line open blocks) 

            // info needed to check when commands like step out, ... have finished executing, returning control to user
            _stepCallStackLevel = _callStackDepth;                                      // call stack levels at time of first program step to execute after step,... command
            _stepFlowCtrlStackLevels = flowCtrlStack.getElementCount();                 // all flow control stack levels at time of first program step to execute after step,... command (includes open blocks)

            // !!! DO NOT clean up: evaluation stack has been set correctly, and _activeFunctionData.activeCmd_ResWordCode:  _activeFunctionData just received its values from the flow control stack 
        }
        break;


        // ------------------------
        // Define Trace expressions
        // ------------------------

        case cmdcod_trace:
        {
            bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
            char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
            Val value;
            value.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst);    // line is valid for all value types  

            bool opIsString = ((uint8_t)valueType == value_isStringPointer);
            if (!opIsString) { return result_arg_stringExpected; }

            replaceSystemStringValue(_pTraceString, value.pStringConst);

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                                        // clear evaluation stack and intermediate strings
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
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // -----------------------------
        // switch breakpoints on are off
        // -----------------------------

        case cmdcod_BPon:
        case cmdcod_BPoff:
        {
            _pBreakpoints->_breakPontsAreOn = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_BPon);

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;

        // ------------------------------------------------------------------------
        // set next line to continue execution (pass control to that line and stop)
        // ------------------------------------------------------------------------

        case cmdcod_setNextLine:
        {
            // is at least one program stopped in debug mode ?
            if (_openDebugLevels == 0) { return result_noProgramStopped; }


            // A. check argument (source line)
            // -------------------------------
            bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
            int valueType = (int)(operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType);
            Val value;
            if ((valueType != value_isLong) && (valueType != value_isFloat)) { return result_BP_sourcelineNumberExpected; }
            value.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst);    // line is valid for all value types  
            long sourceLine = (valueType == value_isLong) ? value.longConst : (long)value.floatConst;


            // B. locate the parsed program step for the source line to go to
            // --------------------------------------------------------------

            // note: the FIRST statement STARTING on that source line must be an executable statement. Otherwise this sourceline cannot be set as the 'next line'
            char* nextStep_tobe{};
            execResult_type execResult = _pBreakpoints->findParsedStatementForSourceLine(sourceLine, nextStep_tobe);
            if (execResult != result_execOK) { return execResult; }


            // C. is the parsed program step the start of a statement WITHIN the currently stopped function ?
            // ----------------------------------------------------------------------------------------------

            // in the flow control stack, skip any optional debug command line open blocks (top of flow ctrl stack) and find stopped function data to find out
            int debugLineNestingLevel = 0;
            OpenFunctionData* pFlowCtrlStackLvl = (OpenFunctionData*)_pFlowCtrlStackTop;
            while (pFlowCtrlStackLvl->blockType != block_JustinaFunction) {
                debugLineNestingLevel++;
                pFlowCtrlStackLvl = (OpenFunctionData*)flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
            }
            // pFlowCtrlStackLvl now points to the stopped function's data
            int functionIndex = (int)pFlowCtrlStackLvl->functionIndex;
            // compare the address of the function start token with the address of the program step
            if (justinaFunctionData[functionIndex].pJustinaFunctionStartToken > nextStep_tobe) { return result_BP_sourceLineNotInStoppedFunction; }
            if (functionIndex < _justinaFunctionCount - 1) {        // is function NOT the last parsed function in program memory ? Check against next function then
                if (justinaFunctionData[functionIndex + 1].pJustinaFunctionStartToken < nextStep_tobe) { return result_BP_sourceLineNotInStoppedFunction; }
            }


            // D. Check that setting a next line doesn't pass control to an inner block 
            // ------------------------------------------------------------------------

            // control cannot pass to a block (e.g. for...end) that doesn't contain the deepest block where control was until now
            // however, it is allowed to quit inner blocks, setting control to an outer block or even the function level
            // during the scan, for each 'block start' statement, a corresponding 'block end' statement must be encountered as well

            // the check is done by scanning all steps between the currently set next statement (next step-as is) and the parsed statement...
            // ...that would be set as next statement (next step-to be)
            // the check always starts with the lowest numbered of the two statements, moving forward in memory (even if jumping backward)

            // examples (BS = block start statement, BE = block end statement, .. = other statements):
            // - jumping forward:  (statement jumped from)..BS..BE..BS..BS..BE..(statement jumped to)         NOT OK
            // - jumping forward:  (statement jumped from)..BS..BE..BS..BS..BE..BE..(statement jumped to)     OK
            // - jumping forward:  (statement jumped from)..BS..BE..BS..BS..BE..BE..BE..(statement jumped to) OK
            // - jumping forward:  (statement jumped from)..BE..BS..BS..BE..BE..BE..(statement jumped to)     OK

            // - jumping backward:   (statement jumped to)..BE (stop scan: result known)                      NOT OK
            // - jumping backward:   (statement jumped to)..BS..BE..BS..BS..BE..(statement jumped from)       OK

            char*& nextStep_asis = pFlowCtrlStackLvl->pNextStep;            // where control is currently
            int relativeNestingLevel = 0;                                   // block nesting level within the function, relative to the block nesting level at the start of this check

            // scan from the lower program step to the higher one (even if setting a next line moves control backward in the source file)
            if (nextStep_asis != nextStep_tobe) {
                bool AsIsBeforeToBe = nextStep_asis < nextStep_tobe;            // true: jumping forward
                char* lowStep = min(nextStep_asis, nextStep_tobe);              // lowest step to scan
                char* highStep = max(nextStep_asis, nextStep_tobe);             // highest step to scan
                int minimumRelativeNestingLevel = 0;                            // moving out of a block decreases this number, moving into a block increases it                       

                char* step = lowStep;                                               // start with the lowest step
                bool incRelativeLevel{ false }, decRelativeLvel{ false };           // flags: move into a block, move out of a block

                do {
                    int matchedCritNum = 0;
                    int tokenType = *step & 0x0F;
                    if (incRelativeLevel) { relativeNestingLevel++; }     // ok, if the corresponding block end statement will be encountered as well
                    else if (decRelativeLvel) {
                        relativeNestingLevel--;
                        if (relativeNestingLevel < minimumRelativeNestingLevel) {
                            minimumRelativeNestingLevel--;
                            // if jumping backward, the check for an invalid sourceline to jump to, can be made during each loop, because...
                            // ...if minimumRelativeNestingLevel is negative at any one time, this indicates that...
                            // ...control would pass to a block that doesn't contain the deepest block where control was until now
                            if (!AsIsBeforeToBe && (minimumRelativeNestingLevel < 0)) { { return result_BP_cannotMoveIntoBlocks; }
                            }
                        }
                    }
                    if (step == highStep) { break; }            // all done

                    incRelativeLevel = false, decRelativeLvel = false;

                    // check for start and end block statements
                    if (tokenType == tok_isReservedWord) {
                        int tokenIndex = (int)((TokenIsResWord*)step)->tokenIndex;
                        char resWordCode = _resWords[tokenIndex].resWordCode;

                        // during execution, it's only AFTER executing start / end of block statements that control moves in or out of a block: remember until next loop
                        if ((resWordCode == cmdcod_for) || (resWordCode == cmdcod_while) || (resWordCode == cmdcod_if)) { incRelativeLevel = true; }
                        else if (resWordCode == cmdcod_end) { decRelativeLvel = true; }       // also safe for setting next line to end of procedure
                    }

                    // find next statement. This statement starts after the first statement separator flagging a set or allowed breakpoint
                    do {
                        findTokenStep(step, true, tok_isTerminalGroup1, termcod_semicolon, termcod_semicolon_BPset, termcod_semicolon_BPallowed, &matchedCritNum);             // find semicolon (always match)
                    } while (matchedCritNum == 1);           // skip statements not allowing breakpoints
                    jumpTokens(1, step);             // first token after semicolon
                } while (true);

                // if jumping forward (relative nesting level of 'to be step' only known at the end), the check for an invalid sourceline to jump to, can be made only here...
                // ...because the relative nesting level of that statement is only known at the end (only then it's known whether that statement would jump into a block)
                if ((AsIsBeforeToBe) && (relativeNestingLevel > minimumRelativeNestingLevel)) { return result_BP_cannotMoveIntoBlocks; }
            }


            // E. set next step to statement corresponding to sourceline
            // ---------------------------------------------------------
            pFlowCtrlStackLvl->pNextStep = nextStep_tobe;           // points again to stopped function data level
            pFlowCtrlStackLvl->errorStatementStartStep = nextStep_tobe;
            pFlowCtrlStackLvl->errorProgramCounter = nextStep_tobe;


            // F. If moving out of open blocks, delete corresponding flow control stack levels
            // ------------------------------------------------------------------------------
            for (int i = 1; i <= (-relativeNestingLevel); i++) {
                pFlowCtrlStackLvl = (OpenFunctionData*)flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);     // inner open block to be deleted 
                pFlowCtrlStackLvl = (OpenFunctionData*)flowCtrlStack.deleteListElement(pFlowCtrlStackLvl);     // points again to stopped function data level
            }


            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // -----------------------------------------
        // set, clear, enable, disable breakpoint(s)
        // -----------------------------------------

        case cmdcod_setBP:
        case cmdcod_clearBP:
        case cmdcod_enableBP:
        case cmdcod_disableBP:
        {
            // all commands:   source line number [, source line number, ...]
            // set breakpoint: source line number, view string [, trigger string] - or -
            //                 source line number, view string , hitcount

            bool argIsVar;
            execResult_type execResult{ result_execOK };

            bool argIsArray;
            char valueType;
            Val arg;

            bool isSetBPwithHitcountViewExpr{ false };
            bool isWithViewExpression{ false };
            bool isWithHitCount{ false };
            long sourceLine{ 0 };
            long hitCountBP1{ 0 };
            char* triggerExprBP1{ nullptr }, * viewExprBP1{ nullptr };
            LE_evalStack* pArg1StackLvl = pStackLvl;

            if ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_setBP) && ((cmdArgCount == 2) || (cmdArgCount == 3))) {
                // possibly a single breakpoint with view expression (can be empty string) and hitcount,
                //      or,                                                                and optional view expression (both can be empty strings)
                for (int i = 1; i <= cmdArgCount; i++) {
                    copyValueArgsFromStack(pStackLvl, 1, &argIsVar, &argIsArray, &valueType, &arg);

                    if (i == 1) {
                        sourceLine = (valueType == value_isLong) ? arg.longConst : (long)arg.floatConst;
                    }
                    else if (i == 2) {       // if string, this is a view string
                        isWithViewExpression = (valueType == value_isStringPointer);
                        if (isWithViewExpression) { viewExprBP1 = arg.pStringConst; }
                        if (!isWithViewExpression) { break; }                                           // not a single breakpoint with view expression and optional hitcount or trigger expression
                    }
                    else if (i == 3) {
                        isWithHitCount = ((valueType == value_isLong) || (valueType == value_isFloat));                             // third arg must be string (view expression, if provided)
                        if (isWithHitCount) {
                            hitCountBP1 = (valueType == value_isLong ? arg.longConst : (long)arg.floatConst);
                            if ((hitCountBP1 < 1) || (hitCountBP1 > 100000)) { return result_BP_hitcountNotWithinRange; }
                        }
                        else { triggerExprBP1 = arg.pStringConst; }
                    }
                }
            }

            // set one breakpoint with view expression and, optionally, hit count or trigger expression ?
            if (isWithViewExpression) {
                int extraAttribCount = cmdArgCount - 1;
                execResult = _pBreakpoints->maintainBP(sourceLine, _activeFunctionData.activeCmd_ResWordCode, extraAttribCount, viewExprBP1, hitCountBP1, triggerExprBP1);
                if (execResult != result_execOK) { return execResult; }
            }

            // set/clear/enable/disable multiple breakpoints
            else {
                pStackLvl = pArg1StackLvl;          // points to first argument again
                for (int i = 1; i <= cmdArgCount; i++) {
                    copyValueArgsFromStack(pStackLvl, 1, &argIsVar, &argIsArray, &valueType, &arg);
                    // values have not been tested yet for numeric type
                    if ((valueType != value_isLong) && (valueType != value_isFloat)) { return result_BP_sourcelineNumberExpected; }
                    sourceLine = (valueType == value_isLong) ? arg.longConst : (long)arg.floatConst;

                    execResult = _pBreakpoints->maintainBP(sourceLine, _activeFunctionData.activeCmd_ResWordCode);
                    if (execResult != result_execOK) { return execResult; }
                }
            }

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ------------------------
        // Raise an execution error
        // ------------------------

        case cmdcod_raiseError:
        {
            bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
            char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
            Val value;
            value.longConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pLongConst) : pStackLvl->varOrConst.value.longConst);       // line is valid for all value types  

            bool opIsLong = ((uint8_t)valueType == value_isLong);
            bool opIsFloat = ((uint8_t)valueType == value_isFloat);
            if (!opIsLong && !opIsFloat) { break; }                                                                                        // ignore if not a number

            execResult_type errNum = (opIsLong) ? (execResult_type)value.longConst : (execResult_type)value.floatConst;
            // if error to be raised is not within this range, simply ignore it 
            if ((errNum != result_execOK) && (execResult < result_startOfEvents)) { return errNum; }

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // -------------------------------
        // switch error trapping on or off
        // -------------------------------

        case cmdcod_trapErrors:
        {
            bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
            char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
            if ((valueType != value_isLong) && (valueType != value_isFloat)) { return result_arg_numberExpected; }
            Val value;
            value.longConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pLongConst) : pStackLvl->varOrConst.value.longConst);    // line is valid for all value types  
            bool trapEnable = (valueType == value_isLong) ? (bool)value.longConst : (bool)value.floatConst;
            _activeFunctionData.trapEnable = trapEnable ? 1 : 0;                                                        // counts for currently executing procedure only                                                       
            if (trapEnable) { _trappedErrorNumber = (int)result_execOK; } // reset err() only when enabling, to allow testing for error after setting error trapping off

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ------------------------
        // Clear an execution error
        // ------------------------

        case cmdcod_clearError:
        {
            _trappedErrorNumber = (int)result_execOK;

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // -----------------------------------
        // read and parse program from stream
        // -----------------------------------

        case cmdcod_loadProg:
        {
            _loadProgFromStreamNo = 0;                                                                                      // init: load from console 
            if (cmdArgCount == 1) {                                                                                       // source specified (console, alternate input or file name)
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];
                copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

                // SD source file name specified ?
                if (valueType[0] == value_isStringPointer) {                                                                // load program from SD file
                    // open file and retrieve file number
                    execResult = SD_open(_loadProgFromStreamNo, args[0].pStringConst, O_READ);                              // this performs a few card & file checks as well
                    if (execResult == result_SD_couldNotOpenFile) {
                        if (!SD.exists(args[0].pStringConst)) { execResult = result_SD_fileNotFound; }                      // replace error code for clarity
                    }
                    if (execResult != result_execOK) { return execResult; }
                }

                // external source specified ?
                else if ((valueType[0] == value_isLong) || (valueType[0] == value_isFloat)) {                               // external source specified: console or alternate input
                    _loadProgFromStreamNo = ((valueType[0] == value_isLong) ? args[0].longConst : args[0].floatConst);
                    if (_loadProgFromStreamNo > 0) { return result_IO_invalidStreamNumber; }
                    else if ((-_loadProgFromStreamNo) > _externIOstreamCount) { return result_IO_invalidStreamNumber; }
                }
            }

            return result_initiateProgramLoad;                                                                              // not an error but an 'event'

            // no clean up to do (return statement executed already)
        }
        break;


        // ----------------------------------
        // set console input or output stream
        // ----------------------------------

        case cmdcod_setConsole:
        case cmdcod_setConsIn:
        case cmdcod_setConsOut:
        case cmdcod_setDebugOut:
        {
            bool argIsVar[1];
            bool argIsArray[1];
            char valueType[1];
            Val args[1];
            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);
            if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }
            int streamNumber = (valueType[0] == value_isLong) ? args[0].longConst : args[0].floatConst;

            // NOTE: set debug out: file number is acceptable, even if no open file is associated with it at this time
            if ((streamNumber >= MAX_OPEN_SD_FILES) || ((-streamNumber) > _externIOstreamCount) || (streamNumber == 0)) { return result_IO_invalidStreamNumber; }
            if ((streamNumber > 0) && (_activeFunctionData.activeCmd_ResWordCode != cmdcod_setDebugOut)) { return result_SD_fileNotAllowedHere; }

            // set debug out ? 
            bool setDebugOut = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_setDebugOut);
            if (setDebugOut) {
                if (streamNumber < 0) {
                    if (_pExternOutputStreams[(-streamNumber) - 1] == nullptr) { return result_IO_noDeviceOrNotForOutput; }
                    _debug_sourceStreamNumber = streamNumber;
                    _pDebugOut = _pExternOutputStreams[(-streamNumber) - 1];                              // external IO (stream number -1 => array index 0, etc.)
                    _pDebugPrintColumn = &_pPrintColumns[(-streamNumber) - 1];
                }
                else {
                    // NOTE: debug out (in contrast to console in & out) can point to an SD file
                    // NOTE: debug out will be automatically reset to console out if file is subsequently closed
                    File* pFile{};
                    execResult_type execResult = SD_fileChecks(pFile, streamNumber, 1);
                    if (execResult != result_execOK) { return execResult; }
                    _debug_sourceStreamNumber = streamNumber;
                    _pDebugOut = static_cast<Stream*> (pFile);
                    _pDebugPrintColumn = &openFiles[streamNumber - 1].currentPrintColumn;
                }
            }
            else {
                // set console in, out, in & out
                bool setConsIn = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_setConsIn);
                bool setConsOut = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_setConsOut);
                bool setConsole = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_setConsole);

                char streamTypes[15];
                char msg[80];

                // NOTE: in case of debug output change, the streams below are not actually used
                strcpy(streamTypes, setConsIn ? "input" : (setConsOut || setDebugOut) ? "output" : "I/O");
                sprintf(msg, "\r\nWARNING: please check first that the selected %s device is available\r\n  ", streamTypes);
                printlnTo(0, msg);
                strcpy(streamTypes, setConsIn ? "for input" : setConsOut ? "for output" : setDebugOut ? "for debug output" : "");
                sprintf(msg, "===== Change console %s ? (please answer Y or N) =====", streamTypes);

                do {
                    printlnTo(0, msg);
                    int length{ 2 };
                    char input[2 + 1] = "";                                                                                 // init: empty string
                    bool doStop{ false }, doAbort{ false }, doCancel{ false }, doDefault{ false };
                    // NOTE: doCancel and doDefault are dummy arguments here
                    if (getConsoleCharacters(doStop, doAbort, doCancel, doDefault, input, length, '\n')) { return result_kill; }  // kill request from caller ? 
                    if (doAbort) { forcedAbortRequest = true; break; }                                                      // abort running code (program or immediate mode statements)
                    else if (doStop) { forcedStopRequest = true; }                                                          // stop a running program (do not produce stop event yet, wait until program statement executed)

                    bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                    if (validAnswer) {
                        sprintf(msg, "---------- Changing console now %s ----------\r\n", streamTypes);
                        printlnTo(0, msg);
                        if (tolower(input[0]) == 'y') {
                            if (setConsIn || setConsole) {
                                if (_pExternInputStreams[(-streamNumber) - 1] == nullptr) { return result_IO_noDeviceOrNotForInput; }
                            }
                            if (setConsOut || setConsole) {
                                if (_pExternOutputStreams[(-streamNumber) - 1] == nullptr) { return result_IO_noDeviceOrNotForOutput; }
                                _consoleOut_sourceStreamNumber = streamNumber;
                                _pConsoleOut = _pExternOutputStreams[(-streamNumber) - 1];                // external IO (stream number -1 => array index 0, etc.)
                                _pConsolePrintColumn = &_pPrintColumns[(-streamNumber) - 1];
                            }
                            // only after all tests are done !
                            if (setConsIn || setConsole) {
                                _consoleIn_sourceStreamNumber = streamNumber;
                                _pConsoleIn = _pExternInputStreams[(-streamNumber) - 1];
                            }
                        }
                        break;
                    }
                } while (true);
            }

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }

        break;


        // ------------------------------------------------------------
        // send a file from SD card to external I/O stream
        // receive a file from external I/O stream and store on SD card
        // copy SD card file
        // ------------------------------------------------------------

        case cmdcod_sendFile:           // arguments: filename   -or-   filename, external I/O stream [, verbose]]
        case cmdcod_receiveFile:        // arguments: filename   -or-   external I/O stream, filename [, verbose] 
        case cmdcod_copyFile:           // arguments: source filename, destination filename 
        {
            // filename: in 8.3 format
            // external I/O stream: numeric constant, default is CONSOLE
            // verbose: default is 1. If verbose is not set, also "overwrite ?" question will not appear  

            if (cmdArgCount > 3) { return result_arg_tooManyArgs; }

            bool argIsVar[2];
            bool argIsArray[2];
            char valueType[2];
            Val args[2];
            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

            if (!_SDinitOK) { return result_SD_noCardOrCardError; }

            bool isSend = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_sendFile);
            bool isReceive = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_receiveFile);
            bool isCopy = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_copyFile);

            int sourceStreamNumber{ 0 }, destinationStreamNumber{ 0 };      // init: console

            // send or receive file: send or receive data to / from external IO stream 
            if ((isSend || isReceive) && (cmdArgCount >= 2)) {                                                            // source (receive) / destination (send) specified ?
                int IOstreamArgIndex = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_sendFile ? 1 : 0);              // init (default for send and receive only, if not specified)
                if ((valueType[IOstreamArgIndex] == value_isLong) || (valueType[IOstreamArgIndex] == value_isFloat)) {      // external source/destination specified (console or an alternate I/O stream)

                    // valid external IO number ?
                    int IOstreamNumber = ((valueType[IOstreamArgIndex] == value_isLong) ? args[IOstreamArgIndex].longConst : (long)args[IOstreamArgIndex].floatConst);      // zero or negative
                    if (IOstreamNumber > 0) { return result_IO_invalidStreamNumber; }
                    else if ((-IOstreamNumber) > _externIOstreamCount) { return result_IO_invalidStreamNumber; }
                    else { (isReceive ? sourceStreamNumber : destinationStreamNumber) = IOstreamNumber; }
                }
                else { return result_arg_numberExpected; }
            }

            // send or copy file: source is a file
            if ((isSend || isCopy)) {
                if (valueType[0] != value_isStringPointer) { return result_arg_stringExpected; }                            // mandatory file name
                if (!pathValid(args[0].pStringConst)) { return result_SD_pathIsNotValid; }

                // don't open source file yet: wait until all other checks are done
            }

            // verbose argument supplied ?
            bool verbose = true;
            if (cmdArgCount == 3) {
                if ((valueType[2] == value_isLong) || (valueType[2] == value_isFloat)) { verbose = (bool)((valueType[2] == value_isLong) ? args[2].longConst : (long)args[2].floatConst); }
                else { return result_arg_numberExpected; }
            }

            bool proceed{ true };        // init (in silent mode, overwrite without asking)

            // receive or copy file: destination is a file
            if ((isReceive) || (isCopy)) {
                int receivingFileArgIndex = (cmdArgCount == 1) ? 0 : 1;
                if (valueType[receivingFileArgIndex] != value_isStringPointer) { return result_arg_stringExpected; }                        // mandatory file name
                if (!pathValid(args[receivingFileArgIndex].pStringConst)) { return result_SD_pathIsNotValid; }

                if (isCopy) {
                    if (strcasecmp(args[0].pStringConst, args[1].pStringConst) == 0) { return result_SD_sourceIsDestination; }              // 8.3 file format: NOT case sensitive
                }
                // if file exists, ask if overwriting it is OK
                if (SD.exists(args[receivingFileArgIndex].pStringConst)) {
                    if (verbose) {
                        printlnTo(0, "\r\n===== File exists already. Overwrite ? (please answer Y or N) =====");
                        do {
                            // flush 'console in' characters  
                            // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                            int length{ 2 };                                // traps input > 1 character
                            char input[2 + 1] = "";                                                                                         // init: empty string
                            // NOTE: doCancel and doDefault are dummy arguments here
                            bool doStop{ false }, doAbort{ false }, doCancel{ false }, doDefault{ false };
                            if (getConsoleCharacters(doStop, doAbort, doCancel, doDefault, input, length, '\n')) { return result_kill; }    // kill request from caller ?
                            if (doAbort) { proceed = false; forcedAbortRequest = true; break; }                                             // ' abort running code (program or immediate mode statements)
                            if (doStop) { forcedStopRequest = true; }                                                                  // stop a running program (do not produce stop event yet, wait until program statement executed)

                            bool validAnswer = (strlen(input) == 1) && ((tolower(input[0]) == 'n') || (tolower(input[0]) == 'y'));
                            if (validAnswer) { proceed = (tolower(input[0]) == 'y');  break; }
                            printlnTo(0, "\r\nYour answer is not valid. Please answer Y or N");

                            bool kill{false};
                            flushConsoleBuffer(kill, doStop, doAbort);
                            if (kill) { return result_kill; }
                            if (doAbort) { proceed = false; forcedAbortRequest = true; break; }                                             // ' abort running code (program or immediate mode statements)
                            if (doStop) { forcedStopRequest = true; }                                                                  // stop a running program (do not produce stop event yet, wait until program statement executed)

                            printlnTo(0, "\r\n===== File exists already. Overwrite ? (please answer Y or N) =====");

                        } while (true);
                    }
                }

                // file does not yet exist ? check if directory exists. If not, create without asking
                else {
                    char* dirPath = new char[strlen(args[receivingFileArgIndex].pStringConst) + 1];
                    strcpy(dirPath, args[receivingFileArgIndex].pStringConst);
                    int pos{ 0 };
                    bool dirCreated{ true };
                    for (pos = strlen(args[receivingFileArgIndex].pStringConst) - 1; pos >= 0; pos--) { if (dirPath[pos] == '/') { dirPath[pos] = '\0'; break; } }      // isolate path

                    if (pos > 0) {    // pos > 0: is NOT a root folder file (pos = 0: root '/' character found; pos=-1: no root '/' character found)
                        if (!SD.exists(dirPath)) {   // if (sub-)directory path does not exist, create it now
                            dirCreated = SD.mkdir(dirPath);
                        }
                    }
                    delete[]dirPath;
                    if (!dirCreated) { return result_SD_couldNotCreateFileDir; }                                            // no success ? error
                }

                if (proceed) {
                    // open receiving file for writing. Create it if it doesn't exist yet, truncate it if it does 
                    execResult = SD_open(destinationStreamNumber, args[receivingFileArgIndex].pStringConst, O_WRITE + O_CREAT + O_TRUNC);
                    if (execResult != result_execOK) { return execResult; }         // file not opened
                }
            }

            // send or copy file: source is a file ? open it now
            if (proceed) {
                if ((isSend || isCopy)) {
                    execResult = SD_open(sourceStreamNumber, args[0].pStringConst, O_READ);                                 // this performs a few card & file checks as well
                    if (execResult == result_SD_couldNotOpenFile) {
                        if (!SD.exists(args[0].pStringConst)) { execResult = result_SD_fileNotFound; }                      // replace error code for clarity
                    }
                    if (execResult != result_execOK) {
                        if (isCopy) { SD_closeFile(destinationStreamNumber); }                                              // error opening source file: close destination file (already open)
                        return execResult;
                    }
                }

                // copy data from source stream to destination stream
                if (verbose) { printlnTo(0, isSend ? "\r\nSending file... please wait" : isReceive ? "\r\nReceiving file... please wait" : "\r\nCopying file..."); }

                execResult = setStream(sourceStreamNumber);                                                                     // set input stream
                if (execResult == result_execOK) { execResult = setStream(destinationStreamNumber, true); }                     // set output stream for output
                if (execResult != result_execOK) {
                    if (isSend || isCopy) { SD_closeFile(sourceStreamNumber); }
                    if (isReceive || isCopy) { SD_closeFile(destinationStreamNumber); }
                    return execResult;
                }

                bool kill{ false }, doStop{ false }, doAbort{ false }, stdConsDummy{ false };
                char c{};
                char buffer[128];
                int bufferCharCount{ 0 };
                bool waitForFirstChar = isReceive;
                int progressDotsByteCount{ 0 };
                long totalByteCount{ 0 };
                long dotCount{ 0 };
                bool newData{};

                do {
                    // read data from source stream
                    if (isSend || isCopy) {
                        execPeriodicHousekeeping(&kill, &doStop, &doAbort);                                                 // get housekeeping flags
                        bufferCharCount = read(buffer, 128);                                                                // if fewer bytes available, end reading WITHOUT time out
                        newData = (bufferCharCount > 0);
                        progressDotsByteCount += bufferCharCount;
                        totalByteCount += bufferCharCount;
                    }
                    else {
                        // receive: get a character if available and perform a regular housekeeping callback as well
                        c = getCharacter(kill, doStop, doAbort, stdConsDummy, isReceive, waitForFirstChar);
                        newData = (c != 0xff);
                        if (newData) { buffer[bufferCharCount++] = c; progressDotsByteCount++; totalByteCount++; }
                        waitForFirstChar = false;                                                                           // for all next characters
                    }
                    if (verbose && (progressDotsByteCount > 2000)) {
                        progressDotsByteCount = 0;  printTo(0, '.');
                        if ((++dotCount & 0x3f) == 0) { printlnTo(0); }                                                     // print a crlf each 64 dots
                    }

                    // handle kill, abort and stop requests
                    if (kill) {                                                                                                // kill request from caller ?
                        if (isSend || isCopy) { SD_closeFile(sourceStreamNumber); }
                        if (isReceive || isCopy) { SD_closeFile(destinationStreamNumber); }
                        return result_kill;
                    }
                    if (doAbort) {                                                                                          // abort running code (program or immediate mode statements) ?
                        if (isSend || isCopy) { forcedAbortRequest = true; break; }
                        else {                                                                                              // receive: process (flush) 
                            if (!forcedAbortRequest) {
                                printlnTo(0, "\r\nAbort: processing remainder of input file... please wait");
                                forcedAbortRequest = true;
                            }
                        }
                    }
                    else if (doStop) { forcedStopRequest = true; }                                                          // stop a running program (do not produce stop event yet, wait until program statement executed)

                    // write data to destination stream
                    if (!forcedAbortRequest) {                                                                              // (receive only): if abort is requested, incoming characters need to be flushed (so, not written anymore) 
                        bool doWrite = isReceive ? ((bufferCharCount == 128) || (!newData && (bufferCharCount > 0))) : newData;
                        if (newData) { write(buffer, bufferCharCount); bufferCharCount = 0; }
                    }
                } while (newData);

                // verbose ? provide user info
                if (verbose) {
                    if (forcedAbortRequest) {
                        printlnTo(0, isSend ? "\r\n+++ File partially sent +++\r\n" : isReceive ? (totalByteCount == 0 ? "\r\n+++ NO file received +++\r\n" :
                            "\r\n+++ File partially received +++\r\n") : "\r\n+++ File partially copied +++\r\n");
                    }
                    else {
                        char s[100];
                        sprintf(s, (isSend ? "\r\n+++ File sent, %ld bytes +++\r\n" : isReceive ? (totalByteCount == 0 ? "\r\n+++ NO file received +++\r\n" : "\r\n+++ File received, %ld bytes +++\r\n") :
                            "\r\n+++ File copied, %ld bytes +++\r\n"), totalByteCount);
                        printlnTo(0, s);
                    }
                }

                // close file(s)
                if (isSend || isCopy) { SD_closeFile(sourceStreamNumber); }
                if (isReceive || isCopy) { SD_closeFile(destinationStreamNumber); }
            }

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // --------------
        // SD card: start
        // --------------

        case cmdcod_startSD:
        {
            execResult = startSD();
            if (execResult != result_execOK) { return execResult; };

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
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
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // --------------------------------------------------------------------
        // Print information or question, requiring user confirmation or answer
        // --------------------------------------------------------------------

        case cmdcod_info:                                                                                                   // display message on CONSOLE and request response

            // mandatory argument 1: prompt (string expression)
            // optional argument 2: numeric variable
            // - on entry: value is 0 or argument not supplied: confirmation required by pressing ENTER (any preceding characters are skipped)
            //             value is 1: idem, but if '\c' encountered in input stream the operation is canceled by user 
            //             value is 2: only positive or negative answer allowed, by pressing 'y' or 'n' followed by ENTER   
            //             value is 3: idem, but if '\c' encountered in input stream the operation is canceled by user 
            // - on exit:  value is 0: operation was canceled by user, 1 if operation confirmed by user

            // NO BREAK here: continue with Input command code


        case cmdcod_input:                                                                                                  // request user to input a string
        {
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


            // the 'input' and 'info' statements only accept variables for specific arguments. IN contrast to functions, which can only test this at runtime,...
            // ... statements can test this during parsing. This is why there are no tests related to constants here. 

            bool argIsVar[3];
            bool argIsArray[3];
            char valueType[3];
            Val args[3];

            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

            if (valueType[0] != value_isStringPointer) { return result_arg_stringExpected; }                                    // prompt 

            bool isInput = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_input);                                         // init
            bool isInfoWithYesNo = false;

            bool checkForDefault = false;       // init
            bool checkForCancel = false;
            bool answerValid{ false };

            while (_pConsoleIn->available() > 0) { readFrom(0); }                                                               // empty console buffer first (to allow the user to start with an empty line)

            do {                                                                                                                // until valid answer typed
                if (isInput) {                                                                                                  // input command
                    if (((uint8_t)(valueType[2]) != value_isLong) && ((uint8_t)(valueType[2]) != value_isFloat)) { return result_arg_numberExpected; }    // flag: with default 
                    checkForDefault = (((uint8_t)(valueType[2]) == value_isLong) ? args[2].longConst != 0 : args[2].floatConst != 0.);
                    checkForCancel = true;

                    if ((argIsArray[1]) && (valueType[1] != value_isStringPointer)) { return result_array_valueTypeIsFixed; }   // an array cannot change type: it needs to be string to receive result
                    if (checkForDefault && (valueType[1] != value_isStringPointer)) { return result_arg_stringExpected; }       // default supplied: it needs to be string

                    char s[80] = "===== Input (\\c to cancel";                                                                  // title static text
                    char title[80 + MAX_ALPHA_CONST_LEN] = "";                                                                  // title static text + string contents of variable
                    strcat(s, checkForDefault ? ", \\d for default = '%s') =====" : "): =====");
                    sprintf(title, s, (args[1].pStringConst == nullptr) ? "" : args[1].pStringConst);
                    printlnTo(0, title);
                }

                else {                                                                                                          // info command
                    if (cmdArgCount == 2) {
                        if (((uint8_t)(valueType[1]) != value_isLong) && ((uint8_t)(valueType[1]) != value_isFloat)) { return result_arg_numberExpected; }
                        if ((uint8_t)(valueType[1]) == value_isFloat) { args[1].longConst = (int)args[1].floatConst; }
                        if ((args[1].longConst < 0) || (args[1].longConst > 3)) { execResult = result_arg_invalid; return execResult; };

                        isInfoWithYesNo = args[1].longConst & 0x02;
                        checkForCancel = args[1].longConst & 0x01;
                    }
                    checkForDefault = false;

                    char s[120] = "===== Information ";
                    strcat(s, isInfoWithYesNo ? "(please answer Y or N" : "(please confirm by pressing ENTER");
                    printlnTo(0, strcat(s, checkForCancel ? ", \\c to cancel): =====" : "): ====="));
                }

                printlnTo(0, args[0].pStringConst);       // user prompt 

                // read characters and store in 'input' variable. Return on '\n' (length is stored in 'length').
                // return flags doStop, doAbort, doCancel, doDefault if user included corresponding escape sequences in input string.
                bool doStop{ false }, doAbort{ false }, doCancel{ false }, doDefault{ false };
                int length{ MAX_USER_INPUT_LEN };
                char input[MAX_USER_INPUT_LEN + 1] = "";                                                                        // init: empty string
                if (getConsoleCharacters(doStop, doAbort, doCancel, doDefault, input, length, '\n')) { return result_kill; }
                if (doAbort) { forcedAbortRequest = true; break; }                                                              // abort running code (program or immediate mode statements)
                else if (doStop) { forcedStopRequest = true; }                                                                  // stop a running program (do not produce stop event yet, wait until program statement executed)

                doDefault = checkForDefault && doDefault;                                                                       // gate doDefault
                doCancel = checkForCancel && doCancel;                                                                          // gate doCancel

                // if request to stop received, first handle input data
                bool  answerIsNo{ false };
                answerValid = true;                                                                                             // init
                if (!doAbort && !doCancel && !doDefault) {                                                                      // doStop: continue execution for now (stop when current statement is executed)
                    if (isInfoWithYesNo) {                                                                                      // check validity of answer ('y' or 'n')
                        if (length != 1) { answerValid = false; }
                        if (answerValid) {
                            if ((input[0] != 'n') && (input[0] != 'N') && (input[0] != 'y') && (input[0] != 'Y')) { answerValid = false; }
                            answerIsNo = (input[0] == 'n') || (input[0] == 'N');
                        }
                        if (!answerValid) { printlnTo(0, "\r\nERROR: answer is not valid. Please try again"); }
                    }
                    else  if (isInput) {

                        LE_evalStack* pStackLvl = (cmdArgCount == 3) ? _pEvalStackMinus1 : _pEvalStackTop;
                        // if  variable currently holds a non-empty string (indicated by a nullptr), delete char string object
                        execResult = deleteVarStringObject(pStackLvl); if (execResult != result_execOK) { return execResult; }

                        if (strlen(input) == 0) { args[1].pStringConst = nullptr; }
                        else {
                            // note that for reference variables, the variable type fetched is the SOURCE variable type
                            int varScope = pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask;
                            int stringlen = min(strlen(input), MAX_ALPHA_CONST_LEN);

                            (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;
                            args[1].pStringConst = new char[stringlen + 1];
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            _pDebugOut->print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
                            _pDebugOut->println((uint32_t)args[1].pStringConst, HEX);
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


                if (cmdArgCount == (isInput ? 3 : 2)) {       // last argument (optional second if Info, third if Input statement) serves a dual purpose: allow cancel (on entry) and signal 'canceled' (on exit)
                    // store result in variable and adapt variable value type
                    // 0 if canceled, 1 if 'OK' or 'Yes',  -1 if 'No' (variable is already numeric: no variable string to delete)
                    *_pEvalStackTop->varOrConst.value.pLongConst = doCancel ? 0 : answerIsNo ? -1 : 1;                          // 1: 'OK' or 'Yes' (yes / no question) answer                       
                    *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isLong;

                    // if NOT a variable REFERENCE, then value type on the stack indicates the real value type and NOT 'variable reference' ...
                    // but it does not need to be changed, because in the next step, the respective stack level will be deleted 
                }
            } while (!answerValid);

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                                // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                            // command execution ended
        }
        break;


        // -----------------------------------------------------------------------------------------------
        // stop or pause a running program and wait for the user to continue (without entering debug mode)
        //------------------------------------------------------------------------------------------------

        case cmdcod_pause:
        case cmdcod_halt:
        {
            long pauseTime = 1000;                                                                      // default: 1 second
            if (cmdArgCount == 1) {                                                                   // copy pause delay, in seconds, from stack, if provided
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];
                copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

                if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }
                pauseTime = (valueType[0] == value_isLong) ? args[0].longConst : (int)args[0].floatConst;    // in seconds
                if (pauseTime < 1) { pauseTime = 1; }
                else if (pauseTime > 10) { pauseTime = 10; };
                pauseTime *= 1000; // to milliseconds
            }
            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_halt) {
                char s[100 + MAX_IDENT_NAME_LEN];
                bool isProgramFunction = (_activeFunctionData.pNextStep < (_programStorage + _progMemorySize));     // is this a program function ?
                if (isProgramFunction) { sprintf(s, "===== Program stopped in user function %s: press ENTER to continue =====", JustinaFunctionNames[_activeFunctionData.functionIndex]); }
                else { strcpy(s, "Press ENTER to continue"); }
                printlnTo(0, s);
            }

            bool kill{ false }, doStop{ false }, doAbort{ false }, stdConsDummy{ false };

            long startPauseAt = millis();                                                               // if pause, not stop;

            // set _pStreamIn to console, for use by Justina methods
            execResult = setStream(0); if (execResult != result_execOK) { return execResult; }
            while (_pConsoleIn->available() > 0) { read(); }                                            // empty console buffer first (to allow the user to type in a 'single' character)
            do {                                                                                        // until new line character encountered
                char c{};
                c = getCharacter(kill, doStop, doAbort, stdConsDummy);                                  // get a key (character from console) if available and perform a regular housekeeping callback as well
                if (kill) { execResult = result_kill; return execResult; }                              // kill Justina interpreter ? (buffer is now flushed until next line character)
                if (doAbort) { forcedAbortRequest = true; break; }                                      // stop a running Justina program (buffer is now flushed until nex line character) 
                if (doStop) { forcedStopRequest = true; }                                               // stop a running program (do not produce stop event yet, wait until program statement executed)

                if (c == '\n') { break; }                                                               // after other input characters flushed

                if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_pause) {
                    if (startPauseAt + pauseTime < millis()) { break; }                                 // if still characters in buffer, buffer will be flushed when processng of statement finalised
                }
            } while (true);

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                        // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                    // command execution ended
        }
        break;


        // -------------------------------------------------------------------------------------------------------------------------------------------------------------
        // print all arguments (longs, floats and strings) in succession. Floats are printed in compact format with maximum 3 digits / decimals and an optional exponent
        // -------------------------------------------------------------------------------------------------------------------------------------------------------------

        // note: the print command does not take into account the display format set to print the last calculation result
        // to format output produced with the print command, use the formatting function provided (function code: fnccod_format) 

        case cmdcod_dbout:
        case cmdcod_dboutLine:
        case cmdcod_cout:
        case cmdcod_coutLine:
        case cmdcod_coutList:
        case cmdcod_print:
        case cmdcod_printLine:
        case cmdcod_printList:
        case cmdcod_printToVar:
        case cmdcod_printLineToVar:
        case cmdcod_printListToVar:
        {
            // print to console, file or string ?
            bool isExplicitStreamPrint = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_print) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLine)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printList));
            bool isPrintToVar = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_printToVar) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLineToVar)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListToVar));
            bool isConsolePrint = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_cout) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_coutLine)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_coutList));                                         // for now, refers to 'cout...' commands (implicit console reference)
            bool isDebugPrint = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_dbout) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_dboutLine));
            int firstValueIndex = (isConsolePrint || isDebugPrint) ? 1 : 2;                                                 // print to file or string: first argument is file or string

            // normal or list print ?
            bool doPrintList = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_coutList) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printList)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListToVar));

            // print new line sequence ?
            bool doPrintLineEnd = ((_activeFunctionData.activeCmd_ResWordCode == cmdcod_dboutLine)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_coutLine) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLine)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printLineToVar)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_coutList) || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printList)
                || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printListToVar));

            if (isDebugPrint && (_pDebugOut == _pConsoleOut)) { isConsolePrint = true; }

            LE_evalStack* pFirstArgStackLvl = pStackLvl;
            char argSep[3] = "  "; argSep[0] = term_comma[0];

            int streamNumber{ 0 };                                  // init

            if (isDebugPrint) { _pStreamOut = _pDebugOut; }
            else {
                execResult = setStream(streamNumber, true); if (execResult != result_execOK) { return execResult; }         // init stream for output
            }
            // in case no stream argument provided (cout, ..., debugOut ...) , set stream print column pointer to current print column for default 'console' OR 'debug out' stream
            // pointer to print column for the current stream is used by tab() and col() functions 
            int* pStreamPrintColumn = _pLastPrintColumn;                                                                    // init (OK if no stream number provided)
            int varPrintColumn{ 0 };                                                                                        // only for printing to string variable: current print column
            char* assembledString{ nullptr };                                                                               // only for printing to string variable: intermediate string

            char intFmtStr[10] = "%#.*l";
            strcat(intFmtStr, doPrintList ? "d" : _dispIntegerSpecifier);
            char floatFmtStr[10] = "%#.*";
            strcat(floatFmtStr, doPrintList ? "e" : _dispFloatSpecifier);

            for (int i = 1; i <= cmdArgCount; i++) {
                bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
                char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
                bool opIsLong = ((uint8_t)valueType == value_isLong);
                bool opIsFloat = ((uint8_t)valueType == value_isFloat);
                bool opIsString = ((uint8_t)valueType == value_isStringPointer);
                char* printString = nullptr;
                Val operand;

                // next line is valid for values of all types (same memory locations are copied)
                operand.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst);

                // print to stream or variable: first argument is stream number or receiving variable
                if (i < firstValueIndex) {                                                                                  // cout, .... have an implicit stream: skip

                    if (isPrintToVar) {      // print to variable
                        if (!operandIsVar) { return result_arg_varExpected; }
                        bool isArray = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isArray);
                        if (isArray && !opIsString) { return result_array_valueTypeIsFixed; }
                        pStreamPrintColumn = &varPrintColumn;       // NOTE: '_pLastPrintColumn' (pointer to last print position of last printed stream) is not altered by variable print 
                        *pStreamPrintColumn = 0;    // reset each time a new print to variable command is executed, because each time you start with an empty string variable
                    }

                    else {     // print to given stream number
                        // check stream number (if file, also perform related file and SD card object checks)
                        if ((!opIsLong) && (!opIsFloat)) { return result_arg_numberExpected; }                              // file number
                        streamNumber = opIsLong ? operand.longConst : operand.floatConst;

                        Stream* p{};
                        execResult = setStream(streamNumber, p, true); if (execResult != result_execOK) { return execResult; }  // stream for output
                        if (p == _pConsoleOut) { isConsolePrint = true; }                                                   // !!! from here on, also for streams < 0, if they POINT to console
                        // set pointers to current print column value for stream
                        pStreamPrintColumn = _pLastPrintColumn;
                    }
                }

                // process a value for printing
                else {
                    // if argument is the result of a tab() or col() function, do NOT print the value these functions return, but advance the print position by the number of tabs specified by the ...
                    // ...  function result OR to the column specified by the function result (if greater then the current column)
                    // this only works if the tab() or col() functon itself is not part of a larger expression (otherwise values attributes 'isPrintTabRequest' and 'isPrintColumnRequest' are lost)
                    bool isTabFunction{ false }, isColFunction{ false }, isCurrentColFunction{ false };
                    if (!operandIsVar) {         // we are looking for an intermediate constant (result of a tab() function occuring as direct argument of the print command)
                        isTabFunction = (pStackLvl->varOrConst.valueAttributes & isPrintTabRequest);
                        isColFunction = (pStackLvl->varOrConst.valueAttributes & isPrintColumnRequest);

                        if (isTabFunction || isColFunction) {
                            int spaceLength{};
                            if (isTabFunction) {
                                int tabCount = pStackLvl->varOrConst.value.longConst;                                       // is an intermediate constant (function result), not a variable
                                spaceLength = _tabSize - (*pStreamPrintColumn % _tabSize) + (tabCount - 1) * _tabSize;
                            }
                            else {                                                                                          // goto print column function
                                int requestedColumn = pStackLvl->varOrConst.value.longConst;
                                spaceLength = (requestedColumn > *pStreamPrintColumn) ? requestedColumn - 1 - *pStreamPrintColumn : 0;
                            }

                            printString = nullptr;                                                                          // init
                            if (spaceLength > 0) {
                                _intermediateStringObjectCount++;
                                printString = new char[spaceLength + 1];
                                memset(printString, ' ', spaceLength);
                                printString[spaceLength] = '\0';
                            #if PRINT_HEAP_OBJ_CREA_DEL
                                _pDebugOut->print("+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)assembledString, HEX);
                            #endif
                            }
                        }
                            }

                    if (!isTabFunction && !isColFunction) {                                                                 // go for normal flow
                        // prepare one value for printing
                        if (opIsLong || opIsFloat) {
                            // at least long enough to print long values, or float values with "G" specifier, without leading characters
                            char s[20];
                            printString = s;                                                                                // pointer
                            // next line is valid for long values as well (same memory locations are copied)
                            operand.floatConst = (operandIsVar ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst);

                            // print (line): take into account precision and specifier; printList: print met max. accuracy, integers: base 10, float: general format.
                            // do not take into account formatting flags: integers: always precede hex with '0x', floats: always print decimal point.
                            if (opIsLong) { if (doPrintList) { sprintf(s, "%#ld", operand.longConst); } else { sprintf(s, intFmtStr, _dispIntegerPrecision, operand.longConst); } }
                            else { if (doPrintList) { sprintf(s, "%#G", operand.floatConst); } else { sprintf(s, floatFmtStr, _dispFloatPrecision, operand.floatConst); } }
                        }
                        else {
                            operand.pStringConst = operandIsVar ? (*pStackLvl->varOrConst.value.ppStringConst) : pStackLvl->varOrConst.value.pStringConst;
                            // no need to copy string - just print the original, directly from stack (it's still there)
                            printString = operand.pStringConst;                                                             // attention: null pointers not transformed into zero-length strings here
                            if (doPrintList) { quoteAndExpandEscSeq(printString); }
                        }
                    }

                    // print one value

                    // NOTE that there is no limit on the number of characters printed here (MAX_PRINT_WIDTH not checked)

                    if (isPrintToVar) {        // print to string ?
                        // remember 'old' string length and pointer to 'old' string
                        char* oldAssembString = assembledString;

                        // calculate length of new string: provide room for argument AND
                        // - if print list: for all value arguments except the last one: sufficient room for argument separator 
                        // - if print new line: if last argument, provide room for new line sequence
                        if (printString != nullptr) { varPrintColumn += strlen(printString); }                              // provide room for new string
                        if (doPrintList && (i < cmdArgCount)) { varPrintColumn += strlen(argSep); }                       // provide room for argument separator

                        // create new string object with sufficient room for argument AND extras (arg. separator and new line sequence, if applicable)
                        if (varPrintColumn > 0) {
                            _intermediateStringObjectCount++;
                            assembledString = new char[varPrintColumn + 1]; assembledString[0] = '\0';
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            _pDebugOut->print("+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)assembledString, HEX);
                        #endif
                        }

                        // copy string with all previous arguments (if not empty)
                        if (oldAssembString != nullptr) strcpy(assembledString, oldAssembString);
                        if (printString != nullptr) { strcat(assembledString, printString); }
                        // if applicable, copy argument separator or new line sequence
                        if (doPrintList && (i < cmdArgCount)) { strcat(assembledString, argSep); }

                        // delete previous assembled string
                        if (oldAssembString != nullptr) {
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            _pDebugOut->print("----- (Intermd str) "); _pDebugOut->println((uint32_t)oldAssembString, HEX);
                        #endif
                            _intermediateStringObjectCount--;
                            delete[] oldAssembString;
                        }
                        }

                    else {      // print to file or console ?
                        if (printString != nullptr) {
                            // if a direct argument of a print function ENDS with CR or LF, reset print column to 0
                            long printed = print(printString);                                                              // we need the position n the string of the last character printed
                            if ((printString[printed - 1] == '\r') || (printString[printed - 1] == '\n')) { *pStreamPrintColumn = 0; }      // reset print column for stream to 0
                            else { *pStreamPrintColumn += printed; }                                                        // not a CR or LF character at end of string ? adapt print column for stream 
                        }
                        if ((i < cmdArgCount) && doPrintList) { *pStreamPrintColumn += print(argSep); }
                    }

                    // if printString is an object on the heap, delete it (note: if printString is created above in quoteAndExpandEscSeq(): it's never a nullptr)
                    if (((isTabFunction || isColFunction) && !(printString == nullptr)) || (opIsString && doPrintList)) {
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        _pDebugOut->print("----- (Intermd str) "); _pDebugOut->println((uint32_t)printString, HEX);
                    #endif
                        _intermediateStringObjectCount--;
                        delete[] printString;
                    }
                    }

                pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
                        }

            // finalise
            if (isPrintToVar) {                                                                                             // print to string ? save in variable
                // receiving argument is a variable, and if it's an array element, it has string type 

                // if currently the variable contains a string object, delete it
                // NOTE: error can not occur, because 
                execResult = deleteVarStringObject(pFirstArgStackLvl);                                                      // if not empty; checks done above (is variable, is not a numeric array)     
                if (execResult != result_execOK) {
                    if (assembledString != nullptr) {
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        _pDebugOut->print("----- (Intermd str) "); _pDebugOut->println((uint32_t)assembledString, HEX);
                    #endif
                        _intermediateStringObjectCount--;
                        delete[] assembledString;
                    }
                    return execResult;
                }

                // print line end without supplied arguments for printing: a string object does not exist yet, so create it now
                if (doPrintLineEnd) {
                    *pStreamPrintColumn = 0;                                                                                // to be consistent with handling of printing line end for printing to non-variable streams, but initialised to zero already 
                    if (cmdArgCount == 1) {                                                                               // only receiving variable supplied: no string created yet     
                        _intermediateStringObjectCount++;
                        assembledString = new char[3]; assembledString[0] = '\r'; assembledString[1] = '\n'; assembledString[2] = '\0';
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        _pDebugOut->print("+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)assembledString, HEX);
                    #endif
                    }
                }

                // save new string in variable 
                *pFirstArgStackLvl->varOrConst.value.ppStringConst = assembledString;                                       // init: copy pointer (OK if string length not above limit)
                *pFirstArgStackLvl->varOrConst.varTypeAddress = (*pFirstArgStackLvl->varOrConst.varTypeAddress & ~value_typeMask) | value_isStringPointer;

                // string stored in variable: clip to maximum length
                if (strlen(assembledString) > MAX_ALPHA_CONST_LEN) {
                    char* clippedString = new char[MAX_ALPHA_CONST_LEN];
                    memcpy(clippedString, assembledString, MAX_ALPHA_CONST_LEN);                                            // copy the string, not the pointer
                    clippedString[MAX_ALPHA_CONST_LEN] = '\0';
                    *pFirstArgStackLvl->varOrConst.value.ppStringConst = clippedString;
                }

                if (assembledString != nullptr) {
                    // non-empty string, adapt object counters (change from intermediate to variable string)
                    _intermediateStringObjectCount--;        // but do not delete the object: it became a variable string
                    char varScope = (pFirstArgStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);
                    (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;

                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("----- (Intermd str) ");   _pDebugOut->println((uint32_t)assembledString, HEX);
                    _pDebugOut->print((varScope == var_isUser) ? "+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "+++++ (var string ) " : "+++++ (loc var str) ");
                    _pDebugOut->println((uint32_t)*pFirstArgStackLvl->varOrConst.value.ppStringConst, HEX);
                #endif              
                }

                if (strlen(assembledString) > MAX_ALPHA_CONST_LEN) { delete[] assembledString; }                            // not referenced in eval. stack (clippedString is), so will not be deleted as part of cleanup
                }

            else {      // print to file or external IO
                if (doPrintLineEnd) {
                    println();
                    *pStreamPrintColumn = 0;
                }
            }

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
                    }
        break;


        // -------------------------------------------------------------
        // print all variables (global and user), call stack or SD files
        // -------------------------------------------------------------

        case cmdcod_printVars:
        case cmdcod_printCallSt:
        case cmdcod_printBP:
        case cmdcod_listFiles:
        {
            bool isConsolePrint{ true };    // init
            int streamNumber = 0;                                                                                           // init: console
            execResult = setStream(streamNumber, true); if (execResult != result_execOK) { return execResult; }             // init output stream
            int* pStreamPrintColumn = _pConsolePrintColumn;                                                                 // init 

            if (cmdArgCount == 0) {
                *pStreamPrintColumn = 0;                                                                                    // will not be used here, but is zero at the end
            }
            else {                                                                                                          // file name specified
                bool argIsVar[1];
                bool argIsArray[1];
                char valueType[1];
                Val args[1];
                copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);
                if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }
                streamNumber = (valueType[0] == value_isLong) ? args[0].longConst : args[0].floatConst;

                // prepare for printing to stream
                Stream* p{};
                execResult = setStream(streamNumber, p, true); if (execResult != result_execOK) { return execResult; }       // stream for output
                isConsolePrint = (p == _pConsoleOut);
                pStreamPrintColumn = (streamNumber == 0) ? _pConsolePrintColumn : (streamNumber < 0) ? _pPrintColumns + (-streamNumber) - 1 : &(openFiles[streamNumber - 1].currentPrintColumn);
                *pStreamPrintColumn = 0;                                                                                    // will not be used here, but must be set to zero
            }

            println();

            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printVars) {
                printVariables(true);                                                                                       // print user variables
                println();
                printVariables(false);                                                                                      // print global program variables
            }
            else if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printCallSt) { printCallStack(); }

            else if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_printBP) { _pBreakpoints->printBreakpoints(); }

            else {
                execResult = SD_listFiles();
                if (execResult != result_execOK) { return execResult; };
            }

            println();
            *pStreamPrintColumn = 0;

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                                // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }

        break;


        // ---------------------------------------------------------
        // print all SD files, with 'last modified' dates, to Serial
        // ---------------------------------------------------------

        // to print to any output stream, look for command code cmdcod_listFiles

        case cmdcod_listFilesToSer:
        {
            if (!_SDinitOK) { return result_SD_noCardOrCardError; }

            // print to SERIAL (fixed in SD library), including date and time stamp
            SdVolume volume{};
            SdFile root{};
            // ===>>> to serial !!!
            Serial.println("\nSD card: files (name, date, size in bytes): ");

            volume.init(_SDcard);
            root.openRoot(volume);
            root.ls(LS_R | LS_DATE | LS_SIZE);                                                                              // to SERIAL (not to _console)

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings 
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ------------------------------------------------------
        // Set display width for printing last calculation result
        // ------------------------------------------------------

        case cmdcod_dispwidth:
        {
            bool argIsVar[1];
            bool argIsArray[1];
            char valueType[1];
            Val args[1];
            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);
            if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }    // numeric ?

            int width = (valueType[0] == value_isLong) ? (int)args[0].longConst : (int)args[0].floatConst;
            if ((width < MIN_CONSOLE_PRINT_WIDTH) || (width > MAX_CONSOLE_PRINT_WIDTH)) { return result_arg_outsideRange; }                     // positive ?
            _dispWidth = width;

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ---------------------------------------------------------
        // Set display format for floating point numbers or integers
        // ---------------------------------------------------------

        case cmdcod_floatfmt:
        case cmdcod_intfmt:
        {
            // floatFmt precision [, specifier]  [, flags] ]    : set formatting for floats 
            // intFmt precision [, specifier]  [, flags] ]      : set formatting for integers
            // NOTE: string printing : NOT affected

            // these settings are used for printing last calculation result, user input echo, print commands output and values traced in debug mode('trace' command)

            // precision: 
            // floatFmt command with 'f', 'e' or 'E' specifier: number of digits printed after the decimal point    
            //                  with 'g' or 'G' specifier: MAXIMUM number of significant digits to be printed 
            // intFmt command with 'd', 'x' and 'X': MINIMUM number of digits to be written (if the integer is shorter, it will be padded with leading zeros)

            // specifier (optional parameter):
            // floatFmt command: 'f', 'e', 'E', 'g' or 'G' specifiers allowed
            // =>  'f': fixed point, 'e' or 'E': scientific, 'g' ot 'G': shortest notation (fixed or scientific). 'E' or 'G': exponent character printed in capitals    
            // intFmt command: 'd', 'x' and 'X' specifiers allowed
            // =>  'd' signed integer, 'x' or 'X': unsigned hexadecimal integer. 'X': hex number is printed in capitals

            // flags (optional parameter): 
            // value 0x1 = left justify within print field, 0x2 = force sign, 0x4 = insert a space if no sign, 0x8: (1) floating point numbers: ALWAYS add a decimal point, even if no digits follow...
            // ...(2) integers:  precede non-zero numbers with '0x' or '0X' if printed in hexadecimal format, value 0x10 = pad with zeros within print field

            // once set, and if not provided again, specifier and flags are used as defaults for next calls to these commands

            // NOTE: strings are always printed unchanged, right justified. Use the fmt() function to format strings

            bool argIsVar[3];
            bool argIsArray[3];
            char valueType[3];
            Val args[3];

            if (cmdArgCount > 3) { execResult = result_arg_tooManyArgs; return execResult; }
            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

            // set format for numbers and strings

            bool isIntFmtCmd = (_activeFunctionData.activeCmd_ResWordCode == cmdcod_intfmt);
            char specifier = isIntFmtCmd ? _dispIntegerSpecifier[0] : _dispFloatSpecifier[0];
            int precision{ isIntFmtCmd ? _dispIntegerPrecision : _dispFloatPrecision };
            int fmtFlags{ isIntFmtCmd ? _dispIntegerFmtFlags : _dispFloatFmtFlags };
            char* fmtString{ isIntFmtCmd ? _dispIntegerFmtString : _dispFloatFmtString };

            // !!! the last 3 arguments return the values of 1st to max. 3rd argument of the command (widh, precision, specifier, flags). Optional last argument is characters printed -> not relevant here
            execResult = checkFmtSpecifiers(true, cmdArgCount, valueType, args, specifier, precision, fmtFlags);
            if (execResult != result_execOK) { return execResult; }

            if (specifier == 's') { return result_arg_invalid; }
            bool isIntSpecifier = (specifier == 'X') || (specifier == 'x') || (specifier == 'd');
            if (isIntFmtCmd != isIntSpecifier) { return result_arg_invalid; }

            precision = min(precision, isIntFmtCmd ? MAX_INT_PRECISION : MAX_FLOAT_PRECISION);

            // create format string for numeric values
            (isIntFmtCmd ? _dispIntegerPrecision : _dispFloatPrecision) = precision;
            (isIntFmtCmd ? _dispIntegerFmtFlags : _dispFloatFmtFlags) = fmtFlags;
            (isIntFmtCmd ? _dispIntegerSpecifier[0] : _dispFloatSpecifier[0]) = specifier;

            // adapt the format string for integers (intFmt cmd) or floats (floatFmt cmd); NOTE that the format string for strings is fixed
            makeFormatString(fmtFlags, isIntSpecifier, &specifier, fmtString);

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ------------------------
        // set console display mode
        // ------------------------

        case cmdcod_dispmod:      // takes two arguments
        {
            // mandatory argument 1: 0 = do not print prompt and do not echo user input; 1 = print prompt but no not echo user input; 2 = print prompt and echo user input 
            // mandatory argument 2: 0 = do not print last result; 1 = print last result; 2 = expand last result escape sequences and print last result

            bool argIsVar[2];
            bool argIsArray[2];
            char valueType[2];                                                                                              // 2 arguments
            Val args[2];

            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

            for (int i = 0; i < cmdArgCount; i++) {                                                                       // always 2 parameters
                bool argIsLong = (valueType[i] == value_isLong);
                bool argIsFloat = (valueType[i] == value_isFloat);
                if (!(argIsLong || argIsFloat)) { execResult = result_arg_numberExpected; return execResult; }

                if (argIsFloat) { args[i].longConst = (int)args[i].floatConst; }
                if ((args[i].longConst < 0) || (args[i].longConst > 2)) { execResult = result_arg_invalid; return execResult; };
            }
            if ((args[0].longConst == 0) && (args[1].longConst == 0)) { execResult = result_arg_invalid; return execResult; };   // no prompt AND no last result print: do not allow

            // if last result printing switched back on, then prevent printing pending last result (if any)
            _lastValueIsStored = false;                                                                                     // prevent printing last result (if any)

            _promptAndEcho = args[0].longConst, _printLastResult = args[1].longConst;

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ------------------------------------------------------------
        // set tab size (for print commands except print list commands)
        // ------------------------------------------------------------

        case cmdcod_tabSize:
        {
            bool argIsVar[1];
            bool argIsArray[1];
            char valueType[1];
            Val args[1];
            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

            if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }
            _tabSize = (valueType[0] == value_isLong) ? args[0].longConst : (int)args[0].floatConst;
            if ((_tabSize < 2) || (_tabSize > 30)) { _tabSize = (_tabSize < 2) ? 2 : 30; }                                  // limit tabSize range

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ------------------------------------
        // set angle mode for trig calculations
        // ------------------------------------

        case cmdcod_angle:
        {
            bool argIsVar[1];
            bool argIsArray[1];
            char valueType[1];
            Val args[1];
            copyValueArgsFromStack(pStackLvl, cmdArgCount, argIsVar, argIsArray, valueType, args);

            if ((valueType[0] != value_isLong) && (valueType[0] != value_isFloat)) { return result_arg_numberExpected; }
            _angleMode = (valueType[0] == value_isLong) ? args[0].longConst : (int)args[0].floatConst;
            if ((_angleMode < 0) || (_angleMode > 1)) { return result_arg_outsideRange; }                                   // 0 = radians, 1 = degrees

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                            // clear evaluation stack and intermediate strings
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // --------------------
        // block start commands
        // --------------------

        case cmdcod_for:
        case cmdcod_if:                                                                                                     // 'if' command
        case cmdcod_while:                                                                                                  // 'while' command
        {
            // start a new loop, or execute an existing loop ? 
            bool initNew{ true };        // IF...END: only one iteration (always new), FOR...END loop: always first itaration of a new loop, because only pass (command skipped for next iterations)
            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_while) {                                                // while block: start of an iteration
                if (flowCtrlStack.getElementCount() != 0) {                                                                 // at least one open (outer) block exists in current function (or main) ?
                    char blockType = ((openBlockGeneric*)_pFlowCtrlStackTop)->blockType;
                    if ((blockType == block_for) || (blockType == block_if)) { initNew = true; }
                    else if (blockType == block_while) {
                        // currently executing an iteration of an outer 'if', 'while' or 'for' loop ? Then this is the start of the first iteration of a new (inner) 'if' or 'while' loop
                        initNew = ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & withinIteration;                  // 'within iteration' flag set ?
                    }
                }
            }

            if (initNew) {
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
                    for (int i = 1; i <= cmdArgCount; i++) {        // skipped if no arguments
                        Val operand;                                                                                                // operand and result
                        bool operandIsVar = (pStackLvl->varOrConst.tokenType == tok_isVariable);
                        char valueType = operandIsVar ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
                        if ((valueType != value_isLong) && (valueType != value_isFloat)) { execResult = result_testexpr_numberExpected; return execResult; }
                        operand.floatConst = (operandIsVar ? *pStackLvl->varOrConst.value.pFloatConst : pStackLvl->varOrConst.value.floatConst);    // valid for long values as well

                        // store references to control variable and its value type
                        if (i == 1) {
                            controlVarIsLong = (valueType == value_isLong);                                                         // remember
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlVar = pStackLvl->varOrConst.value;                    // pointer to variable (containing a long or float constant)
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlValueType = pStackLvl->varOrConst.varTypeAddress;     // pointer to variable value type
                        }

                        // store final loop value
                        else if (i == 2) {
                            finalValueIsLong = (valueType == value_isLong);                                                         // remember
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue = operand;
                        }

                        // store loop step
                        else {                                                                                                      // third parameter
                            stepIsLong = (valueType == value_isLong);                                                               // store loop increment / decrement
                            ((OpenBlockTestData*)_pFlowCtrlStackTop)->step = operand;
                        }

                        pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
                    }

                    if (cmdArgCount < 3) {                                                                                        // step not specified: init with default (1.)  
                        stepIsLong = false;
                        ((OpenBlockTestData*)_pFlowCtrlStackTop)->step.floatConst = 1.;                                             // init as float
                    }

                    // determine value type to use for loop tests, promote final value and step to float if value type to use for loop tests is float
                    // the initial value type of the control variable and the value type of (constant) final value and step define the loop test value type
                    ((OpenBlockTestData*)_pFlowCtrlStackTop)->testValueType = (controlVarIsLong && finalValueIsLong && stepIsLong ? value_isLong : value_isFloat);
                    if (((OpenBlockTestData*)_pFlowCtrlStackTop)->testValueType == value_isFloat) {
                        if (finalValueIsLong) { ((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue.floatConst = (float)((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue.longConst; }
                        if (stepIsLong) { ((OpenBlockTestData*)_pFlowCtrlStackTop)->step.floatConst = (float)((OpenBlockTestData*)_pFlowCtrlStackTop)->step.longConst; }
                    }

                    ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl |= forLoopInit;                                           // init at the start of initial FOR loop iteration
                }

                ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl &= ~breakFromLoop;                                            // init at the start of initial iteration for any loop
            }

            ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl |= withinIteration;                                               // init at the start of an iteration for any loop
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
            if (testClauseCondition) {                                                                                              // result of test in preceding 'if' or 'elseif' clause FAILED ? Check this clause
                Val operand;                                                                                                        // operand and result
                bool operandIsVar = (_pEvalStackTop->varOrConst.tokenType == tok_isVariable);
                char valueType = operandIsVar ? (*_pEvalStackTop->varOrConst.varTypeAddress & value_typeMask) : _pEvalStackTop->varOrConst.valueType;
                if ((valueType != value_isLong) && (valueType != value_isFloat)) { execResult = result_testexpr_numberExpected; return execResult; }
                operand.floatConst = operandIsVar ? *_pEvalStackTop->varOrConst.value.pFloatConst : _pEvalStackTop->varOrConst.value.floatConst;    // valid for long values as well (same memory locations are copied)

                fail = (valueType == value_isFloat) ? (operand.floatConst == 0.) : (operand.longConst == 0);                                        // current test (elseif clause)
                ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl = fail ? ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl | testFail : ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & ~testFail;                                          // remember test result (true -> 0x1)
            }

            bool setNextToken = fail || (_activeFunctionData.activeCmd_ResWordCode == cmdcod_for);
            if (setNextToken) {                                                                                                     // skip this clause ? (either a preceding test passed, or it failed but the currrent test failed as well)
                TokenIsResWord* pToToken;
                uint16_t toTokenStep{ 0 };
                pToToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;
                memcpy(&toTokenStep, pToToken->toTokenStep, sizeof(char[2]));
                _activeFunctionData.pNextStep = _programStorage + toTokenStep;                                                      // prepare jump to 'else', 'elseif' or 'end' command
            }

            // clean up
            clearEvalStackLevels(cmdArgCount);                                                                                    // clear evaluation stack
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                                // command execution ended
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
                blockType = ((openBlockGeneric*)_pFlowCtrlStackTop)->blockType;
                // inner block(s) could be IF...END blocks (before reaching loop block)
                isLoop = ((blockType == block_while) || (blockType == block_for));
                if (isLoop) {
                    TokenIsResWord* pToken;
                    uint16_t toTokenStep{ 0 };
                    pToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;                                   // pointer to loop start command token
                    memcpy(&toTokenStep, pToken->toTokenStep, sizeof(char[2]));
                    pToken = (TokenIsResWord*)(_programStorage + toTokenStep);                                              // pointer to loop end command token
                    memcpy(&toTokenStep, pToken->toTokenStep, sizeof(char[2]));
                    _activeFunctionData.pNextStep = _programStorage + toTokenStep;                                          // prepare jump to 'END' command
                }

                else {          // inner IF...END block: remove from flow control stack 
                    flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                    _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
                }
            } while (!isLoop);

            if (_activeFunctionData.activeCmd_ResWordCode == cmdcod_break) { ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl |= breakFromLoop; }

            // clean up
            _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                        // command execution ended
        }
        break;


        // ----------------------------------------------
        // end block command (While, For, If) or Function
        // ----------------------------------------------

        case cmdcod_end:
        {
            char blockType = ((openBlockGeneric*)_pFlowCtrlStackTop)->blockType;

            if ((blockType == block_if) || (blockType == block_while) || (blockType == block_for)) {

                bool exitLoop{ true };

                if ((blockType == block_for) || (blockType == block_while)) {
                    exitLoop = ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & breakFromLoop;                       // BREAK command encountered: exit loop
                }

                if (!exitLoop) {                                                                                            // no BREAK encountered: loop terminated anyway ? (depends on test result) 
                    if (blockType == block_for) { execResult = testForLoopCondition(exitLoop); if (execResult != result_execOK) { return execResult; } }
                    else if (blockType == block_while) { exitLoop = (((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl & testFail); }       // false: test passed
                }

                if (!exitLoop) {        // continue with next loop
                    if (blockType == block_for) {
                        _activeFunctionData.pNextStep = ((OpenBlockTestData*)_pFlowCtrlStackTop)->nextTokenAddress;         // address of token following 'for' token in program memory        
                    }
                    else {      // WHILE...END block
                        TokenIsResWord* pToToken;
                        uint16_t toTokenStep{ 0 };
                        pToToken = (TokenIsResWord*)_activeFunctionData.activeCmd_tokenAddress;
                        memcpy(&toTokenStep, pToToken->toTokenStep, sizeof(char[2]));

                        _activeFunctionData.pNextStep = _programStorage + toTokenStep;                                      // prepare jump to start of new loop
                    }
                }

                ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl &= ~withinIteration;                                  // at the end of an iteration

                // do NOT reset in case of End Function: _activeFunctionData will receive its values in routine terminateJustinaFunction()
                _activeFunctionData.activeCmd_ResWordCode = cmdcod_none;                                                    // command execution ended

                if (exitLoop) {
                    flowCtrlStack.deleteListElement(_pFlowCtrlStackTop);
                    _pFlowCtrlStackTop = flowCtrlStack.getLastListElement();
                }
                break;                                                                                                      // break here: do not break if end function !
            }

        }

        // NO BREAK here: from here on, subsequent execution is the same for 'end' (function) and for 'return'


        // --------------------
        // return from function
        // --------------------

        case cmdcod_return:
        {
            isFunctionReturn = true;
            bool returnWithZero = (cmdArgCount == 0);                                                                     // RETURN statement without expression, or END statement: return a zero
            terminateJustinaFunction(returnWithZero);
            execResult_type execResult = execAllProcessedOperators();                                                              // continue in caller !!!
            if (execResult != result_execOK) { return execResult; }

            // DO NOT reset _activeFunctionData.activeCmd_ResWordCode: _activeFunctionData will receive its values in routine terminateJustinaFunction()
        }
        break;

                }       // end switch

    return result_execOK;
            }


// -------------------------------
// *   test for loop condition   *
// -------------------------------

Justina_interpreter::execResult_type Justina_interpreter::testForLoopCondition(bool& testFails) {

    char testTypeIsLong = (((OpenBlockTestData*)_pFlowCtrlStackTop)->testValueType == value_isLong);                                // loop final value and step have the initial control variable value type
    bool ctrlVarIsLong = ((*(uint8_t*)((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlValueType & value_typeMask) == value_isLong);
    bool ctrlVarIsFloat = ((*(uint8_t*)((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlValueType & value_typeMask) == value_isFloat);
    if (!ctrlVarIsLong && !ctrlVarIsFloat) { return result_testexpr_numberExpected; }                                               // value type changed to string within loop: error

    Val& pCtrlVar = ((OpenBlockTestData*)_pFlowCtrlStackTop)->pControlVar;                                                          // pointer to control variable
    Val& finalValue = ((OpenBlockTestData*)_pFlowCtrlStackTop)->finalValue;
    Val& step = ((OpenBlockTestData*)_pFlowCtrlStackTop)->step;
    char& loopControl = ((OpenBlockTestData*)_pFlowCtrlStackTop)->loopControl;


    if (ctrlVarIsLong) {                                                                                                            // current control variable value type is long
        if (testTypeIsLong) {                                                                                                       // loop final value and step are long
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pLongConst = *pCtrlVar.pLongConst + step.longConst; }
            if (step.longConst > 0) { testFails = (*pCtrlVar.pLongConst > finalValue.longConst); }
            else { testFails = (*pCtrlVar.pLongConst < finalValue.longConst); }
        }
        else {                                                                                                                      // loop final value and step are float: promote long values to float
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pLongConst = (long)((float)*pCtrlVar.pLongConst + step.floatConst); }     // store result back as LONG (do not change control variable value type)
            if (step.floatConst > 0.) { testFails = ((float)*pCtrlVar.pLongConst > finalValue.floatConst); }
            else { testFails = ((float)*pCtrlVar.pLongConst < finalValue.floatConst); }
        }
    }
    else {                                                                                                                          // current control variable value type is float
        if (testTypeIsLong) {                                                                                                       // loop final value and step are long: promote long values to float
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pFloatConst = (*pCtrlVar.pFloatConst + (float)step.longConst); }
            if ((float)step.longConst > 0.) { testFails = (*pCtrlVar.pFloatConst > (float)finalValue.longConst); }
            else { testFails = (*pCtrlVar.pFloatConst < (float)finalValue.longConst); }
        }
        else {                                                                                                                      // loop final value and step are float
            if (!(loopControl & forLoopInit)) { *pCtrlVar.pFloatConst = *pCtrlVar.pFloatConst + step.floatConst; }
            if (step.floatConst > 0.) { testFails = (*pCtrlVar.pFloatConst > finalValue.floatConst); }
            else { testFails = (*pCtrlVar.pFloatConst < finalValue.floatConst); }
        }
    }

    loopControl &= ~forLoopInit;                                                                                                    // reset 'FOR loop init' flag
    return result_execOK;
};


// ---------------------------------------------------------------------------------------
// *   copy command arguments or internal cpp function arguments from evaluation stack   *
// ---------------------------------------------------------------------------------------

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
            else if ((valueType[i] & value_typeMask) == value_isStringPointer) {                // for callback calls only      
                char* pOriginalArg = args[i].pStringConst;                                      // pointer to Justina variable or constant string
                int strLength{ 0 };
                // empty (null pointer) and constant strings: create a temporary string (empty but null-terminated or copy of the non-empty string)
                if ((args[i].pStringConst == nullptr) || argIsConstant) {                       // note: non-empty variable strings (only): pointer keeps pointing to variable string (no copy)           
                    valueType[i] |= passCopyToCallback;                                         // flag that a copy has been made (it will have to be deleted afterwards))
                    strLength = (args[i].pStringConst == nullptr) ? 0 : strlen(args[i].pStringConst);

                    _intermediateStringObjectCount++;                                           // temporary string object will be deleted right after return from call to user callback routine
                    args[i].pStringConst = new char[strLength + 1];                             // change pointer to copy of string
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)args[i].pStringConst, HEX);
                #endif

                    if (strLength == 0) { args[i].pStringConst[0] = '\0'; }                     // empty string (sole character is null-character as terminator)
                    else { strcpy(args[i].pStringConst, pOriginalArg); }                        // non-empty constant string
                }
            }
        }

        pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
    }

    return result_execOK;
}


// -----------------------------------------------------------------------
// *   replace a system string value with a copy of a new string value *** 
// -----------------------------------------------------------------------

void Justina_interpreter::replaceSystemStringValue(char*& systemString, const char* newString) {

    // delete current system string (if not nullptr)
    if (systemString != nullptr) {
    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print("----- (system var str) "); _pDebugOut->println((uint32_t)systemString, HEX);
    #endif
        _systemVarStringObjectCount--;
        delete[] systemString;
        systemString = nullptr;
    }

    // COPY new string in system variable (no move)
    if (newString != nullptr) {                                                                                        // new trace string
        _systemVarStringObjectCount++;
        systemString = new char[strlen(newString) + 2]; // room for additional semicolon (in case string is not ending with it) and terminating '\0'
    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print("+++++ (system var str) "); _pDebugOut->println((uint32_t)systemString, HEX);
    #endif

        strcpy(systemString, newString);                                                                                    // copy the actual string
        systemString[strlen(newString)] = term_semicolon[0];
        systemString[strlen(newString) + 1] = '\0';
    }
}


