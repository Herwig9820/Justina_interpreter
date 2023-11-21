#include "Justina.h"

#define PRINT_HEAP_OBJ_CREA_DEL 1
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
                        int length{ 1 };
                        char input[1 + 1] = "";                                                                          // init: empty string
                        // NOTE: quitting has higher priority than aborting or stopping, and quitting anyway, so not needed to check abort and stop flags
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


    }       // end switch

    return result_execOK;
}


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


