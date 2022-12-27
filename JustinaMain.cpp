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
#define debugPrint 0


// *****************************************************************
// ***            class LinkedList - implementation              ***
// *****************************************************************

// ---------------------------------------------
// *   initialisation of static class member   *
// ---------------------------------------------

int LinkedList::_listIDcounter = 0;

// -------------------
// *   constructor   *
// -------------------

LinkedList::LinkedList() {
    _listID = _listIDcounter;
    _listIDcounter++;       // static variable
    _pFirstElement = nullptr;
    _pLastElement = nullptr;
}


// ---------------------
// *   deconstructor   *
// ---------------------

LinkedList::~LinkedList() {
    _listIDcounter--;       // static variable
}


// --------------------------------------------------
// *   append a list element to the end of a list   *
// --------------------------------------------------

char* LinkedList::appendListElement(int size) {
    ListElemHead* p = (ListElemHead*)(new char[sizeof(ListElemHead) + size]);       // create list object with payload of specified size in bytes

    if (_pFirstElement == nullptr) {                                                  // not yet any elements
        _pFirstElement = p;
        p->pPrev = nullptr;                                                             // is first element in list: no previous element
    }
    else {
        _pLastElement->pNext = p;
        p->pPrev = _pLastElement;
    }
    _pLastElement = p;
    p->pNext = nullptr;                                                                 // because p is now last element
    _listElementCount++;

#if printCreateDeleteListHeapObjects
    Serial.print("(LIST) Create elem # "); Serial.print(_listElementCount);
    Serial.print(", list ID "); Serial.print(_listID);
    Serial.print(", stack: "); Serial.print(_listName);
    if (p == nullptr) { Serial.println("- list elem adres: nullptr"); }
    else {
        Serial.print(", list elem address: "); Serial.println((uint32_t)p - RAMSTART);
    }
#endif
    return (char*)(p + 1);                                          // pointer to payload of newly created element
}


// -----------------------------------------------------
// *   delete a heap object and remove it from list    *
// -----------------------------------------------------

char* LinkedList::deleteListElement(void* pPayload) {                              // input: pointer to payload of a list element

    ListElemHead* pElem = (ListElemHead*)pPayload;                                     // still points to payload: check if nullptr
    if (pElem == nullptr) { pElem = _pLastElement; }                                  // nullptr: delete last element in list (if it exists)
    else { pElem = pElem - 1; }                                                         // pointer to list element header

    if (pElem == nullptr) { return nullptr; }                                         // still nullptr: return (list is empty)

    ListElemHead* p = pElem->pNext;                                                     // remember return value

#if printCreateDeleteListHeapObjects
    // determine list element # by counting from the list start
    ListElemHead* q = _pFirstElement;
    int i{};
    for (i = 1; i <= _listElementCount; ++i) {
        if (q == pElem) { break; }            // always a match
        q = q->pNext;
    }

    Serial.print("(LIST) Delete elem # "); Serial.print(i); Serial.print(" (new # "); Serial.print(_listElementCount - 1);
    Serial.print("), list ID "); Serial.print(_listID);
    Serial.print(", stack: "); Serial.print(_listName);
    Serial.print(", list elem address: "); Serial.println((uint32_t)pElem - RAMSTART);
#endif

    // before deleting object, remove from list:
    // change pointers from previous element (or _pFirstPointer, if no previous element) and next element (or _pLastPointer, if no next element)
    ((pElem->pPrev == nullptr) ? _pFirstElement : pElem->pPrev->pNext) = pElem->pNext;
    ((pElem->pNext == nullptr) ? _pLastElement : pElem->pNext->pPrev) = pElem->pPrev;

    _listElementCount--;
    delete[]pElem;

    if (p == nullptr) { return nullptr; }
    else { return (char*)(p + 1); }                                           // pointer to payload of next element in list, or nullptr if last element deleted
}


// ------------------------------------------
// *   delete all list elements in a list   *
// ------------------------------------------

void LinkedList::deleteList() {
    if (_pFirstElement == nullptr) return;

    ListElemHead* pHead = _pFirstElement;
    while (true) {
        char* pNextPayload = deleteListElement((char*)(pHead + 1));
        if (pNextPayload == nullptr) { return; }
        pHead = ((ListElemHead*)pNextPayload) - 1;                                     // points to list element header 
    }
}


// ----------------------------------------------------
// *   get a pointer to the first element in a list   *
// ----------------------------------------------------

char* LinkedList::getFirstListElement() {
    return (char*)(_pFirstElement + (_pFirstElement == nullptr ? 0 : 1)); // add one header length
}


//----------------------------------------------------
// *   get a pointer to the last element in a list   *
//----------------------------------------------------

char* LinkedList::getLastListElement() {

    return (char*)(_pLastElement + (_pLastElement == nullptr ? 0 : 1)); // add one header length
}


// -------------------------------------------------------
// *   get a pointer to the previous element in a list   *
// -------------------------------------------------------

char* LinkedList::getPrevListElement(void* pPayload) {                                 // input: pointer to payload of a list element  
    if (pPayload == nullptr) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*)pPayload) - 1;                                     // points to list element header
    if (pElem->pPrev == nullptr) { return nullptr; }
    return (char*)(pElem->pPrev + 1);                                                      // points to payload of previous element
}


//----------------------------------------------------
// *   get a pointer to the next element in a list   *
//----------------------------------------------------

char* LinkedList::getNextListElement(void* pPayload) {
    if (pPayload == nullptr) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*)pPayload) - 1;                                     // points to list element header
    if (pElem->pNext == nullptr) { return nullptr; }
    return (char*)(pElem->pNext + 1);                                                      // points to payload of previous element
}


//-------------------------------------------------------------
// *   get the list ID (depends on the order of creation !)   *
//-------------------------------------------------------------

int LinkedList::getListID() {
    return _listID;
}


//--------------------------
// *   set the list name   *
//--------------------------

void LinkedList::setListName(char* listName) {
    strncpy(_listName, listName, listNameSize - 1);
    _listName[listNameSize - 1] = '\0';
    return;
}


//--------------------------
// *   get the list name   *
//--------------------------

char* LinkedList::getListName() {
    return _listName;
}


//-------------------------------
// *   get list element count   *
//-------------------------------

int LinkedList::getElementCount() {
    return _listElementCount;
}



/***********************************************************
*                 class Justina_interpreter                *
***********************************************************/

// -------------------
// *   constructor   *
// -------------------

Justina_interpreter::Justina_interpreter(Stream* const pConsole) : _pConsole(pConsole) {

    // settings to be initialized when cold starting interpreter only
    // --------------------------------------------------------------

    _coldStart = true;

    _housekeepingCallback = nullptr;
    for (int i = 0; i < _userCBarrayDepth; i++) { _callbackUserProcStart[i] = nullptr; }
    _userCBprocStartSet_count = 0;

    _resWordCount = (sizeof(_resWords)) / sizeof(_resWords[0]);
    _functionCount = (sizeof(_functions)) / sizeof(_functions[0]);
    _terminalCount = (sizeof(_terminals)) / sizeof(_terminals[0]);

    _quitJustina = false;
    _isPrompt = false;

    _statementCharCount = 0;
    _lineCount = 0;                             // taking into account new line after 'load program' command ////
    _flushAllUntilEOF = false;

    _programMode = false;
    _currenttime = millis();
    _previousTime = _currenttime;
    _lastCallBackTime = _currenttime;

    parsingStack.setListName("parsing ");
    evalStack.setListName("eval    ");
    flowCtrlStack.setListName("flowCtrl");
    immModeCommandStack.setListName("cmd line");

    // initialize interpreter object fields
    // ------------------------------------

    initInterpreterVariables(true);
};


// ---------------------
// *   deconstructor   *
// ---------------------

Justina_interpreter::~Justina_interpreter() {
    if (!_keepInMemory) {
        resetMachine(true);             // delete all objects created on the heap
        _housekeepingCallback = nullptr;
    }
    _pConsole->println("\r\nJustina: bye\r\n");
};


// ------------------------------
// *   set call back functons   *
// ------------------------------

bool Justina_interpreter::setMainLoopCallback(void (*func)(bool& requestQuit, long& appFlags)) {

    // a call from the user program initializes the address of a 'user callback' function.
    // Justina will call this user routine repeatedly and automatically, allowing  the user...
    // ...to execute a specific routine regularly (e.g. to maintain a TCP connection, to implement a heartbeat, ...)
    _housekeepingCallback = func;
    return true;
}

bool Justina_interpreter::setUserFcnCallback(void(*func) (const void** data, const char* valueType)) {

    // each call from the user program initializes a next 'user callback' function address in an array of function addresses 
    if (_userCBprocStartSet_count > +_userCBarrayDepth) { return false; }      // throw away if callback array full
    _callbackUserProcStart[_userCBprocStartSet_count++] = func;
    return true; // success
}



// ----------------------------
// *   interpreter main loop   *
// ----------------------------

bool Justina_interpreter::run(Stream* const pConsole, Stream** const pTerminal, int definedTerms) {
    // local static variables
    static bool withinStringEscSequence{ false };
    static bool lastCharWasSemiColon{ false };
    static bool within1LineComment{ false };
    static bool withinMultiLineComment{false};
    static bool withinString{ false };

    static char* pErrorPos{};

    static parseTokenResult_type result{ result_tokenFound };    // init
    static bool statementParsed{ false };                      // init
    static bool receivingProgram{ false };

    // local variables 
    bool kill{ false };                                       // kill is true: request from caller, kill is false: quit command executed
    bool quitNow{ false };
    char c;

    bool redundantSemiColon = false;
    bool isCommentStartChar = (c == '$');                               // character can also be part of comment

    _pConsole->println();
    for (int i = 0; i < 13; i++) { _pConsole->print("*"); } _pConsole->print("____");
    for (int i = 0; i < 4; i++) { _pConsole->print("*"); } _pConsole->print("__");
    for (int i = 0; i < 14; i++) { _pConsole->print("*"); } _pConsole->print("_");
    for (int i = 0; i < 10; i++) { _pConsole->print("*"); }_pConsole->println();

    _pConsole->print("    "); _pConsole->println(ProductName);
    _pConsole->print("    "); _pConsole->println(LegalCopyright);
    _pConsole->print("    Version: "); _pConsole->print(ProductVersion); _pConsole->print(" ("); _pConsole->print(BuildDate); _pConsole->println(")");
    for (int i = 0; i < 48; i++) { _pConsole->print("*"); } _pConsole->println();

    _programMode = false;
    _programCounter = _programStorage + PROG_MEM_SIZE;
    *(_programStorage + PROG_MEM_SIZE) = tok_no_token;                                      //  current end of program (immediate mode)
    _pConsole = pConsole;
    _isPrompt = false;                 // end of parsing
    _pTerminal = pTerminal;
    _definedTerminals = definedTerms;

    _coldStart = false;             // can be used if needed in this procedure, to determine whether this was a cold or warm start



    do {
        // when loading a program, as soon as first printable character of a PROGRAM is read, each subsequent character needs to follow after the previous one within a fixed time delay, handled by getKey().
        // program reading ends when no character is read within this time window.
        // when processing immediate mode statements (single line), reading ends when a New Line terminating character is received
        bool allowTimeOut = _programMode && !_initiateProgramLoad;          // _initiateProgramLoad is set during execution of the command to read a program source file from the console
        if (getKey(c, allowTimeOut)) { kill = true; break; }                // return true if kill request received from calling program
        if (c < 0xFF) { _initiateProgramLoad = false; }                     // reset _initiateProgramLoad after each printable character received
        bool programOrLineRead = _programMode ? ((c == 0xFF) && allowTimeOut) : (c == '\n');
        if ((c == 0xFF) && !programOrLineRead) { continue; }                // no character (except when program or line is read): start next loop


        do {        // one loop only
            quitNow = false;

            // if no character added: nothing to do, wait for next
            bool bufferOverrun{false};
            bool noCharAdded = !addCharacterToInput(lastCharWasSemiColon, withinString, withinStringEscSequence, within1LineComment, withinMultiLineComment, redundantSemiColon, programOrLineRead,bufferOverrun, c);
            if (bufferOverrun) { result = result_statementTooLong; }
            else if (noCharAdded) {  break ;}               // start a new outer loop (read a character if available, etc.)

            // if a statement is complete (terminated by a semicolon or end of input), parse it
            // --------------------------------------------------------------------------------
            bool isStatementSeparator = (!withinString) && (!within1LineComment) && (!withinMultiLineComment) && (c == ';') && !redundantSemiColon;
            isStatementSeparator = isStatementSeparator || (withinString && (c == '\n'));  // new line sent to parser as well
            bool statementComplete = !bufferOverrun && (isStatementSeparator || (programOrLineRead && (_statementCharCount > 0))) ;

            if (statementComplete && !_quitJustina) {                   // if quitting anyway, just skip                                               
                _statement[_statementCharCount] = '\0';                            // add string terminator

                char* pStatement = _statement;                                                 // because passed by reference 
                char* pDummy{};
                _parsingExecutingTraceString = false; _parsingEvalString = false;
                result = parseStatements(pStatement, pDummy);                                 // parse ONE statement only 
                pErrorPos = pStatement;                                                      // in case of error

                if (result != result_tokenFound) { _flushAllUntilEOF = true; }
                if (result == result_parse_kill) { kill = true; _quitJustina = true; }     // _flushAllUntilEOF is true already (flush buffer before quitting)

                _statementCharCount = 0;                                                        // reset after each statement read
                withinString = false; withinStringEscSequence = false; within1LineComment = false; withinMultiLineComment=false;
                statementParsed = true;                                                     // with or without error
            }


            if (!programOrLineRead) { break; }       // program mode: complete program read and parsed / imm. mode: 1 statement read and parsed (with or without error)
            quitNow = processAndExec(statementParsed, result, kill, pErrorPos);  // return value: quit Justina now

            // reset character input status variables
            lastCharWasSemiColon = false;
            _statementCharCount = 0;
            _lineCount = 0;
            _flushAllUntilEOF = false;
        } while (false);

        if (quitNow) { break; }                        // user gave quit command


    } while (true);

    _appFlags = 0x0000L;                            // clear all application flags
    _housekeepingCallback(quitNow, _appFlags);      // only to pass application flags to caller

    _statementCharCount = 0;
    _lineCount = 0;
    _flushAllUntilEOF = false;

    if (kill) {_keepInMemory = false; _pConsole->println("\r\n\r\n>>>>> Justina: kill request received from calling program <<<<<"); }
    if (_keepInMemory) { _pConsole->println("\r\nJustina: bye\r\n"); }        // if remove from memory: message given in destructor
    _quitJustina = false;         // if interpreter stays in memory: re-init

    return _keepInMemory;           // return to calling program
}


// -------------------
// *                 *
// -------------------

bool Justina_interpreter::addCharacterToInput(bool& lastCharWasSemiColon, bool& withinString, bool& withinStringEscSequence, bool& within1LineComment, bool & withinMultiLineComment, 
    bool& redundantSemiColon,bool programOrLineRead, bool &bufferOverrun, char c) {

    const char commentStartChar = '$';

    static bool lastCharWasWhiteSpace{ false };

    bool redundantSpaces = false;                                       // init

    bufferOverrun = false;
    if ((c < ' ') && (c != '\n')) { return false; }          // skip control-chars except new line and EOF character

    // if last statement in buffer does not contain a semicolon separator at the end, add it
    if (programOrLineRead) {
        if (_statementCharCount > 0) {              
            if (_statement[_statementCharCount - 1] != ';') {
                if(_statementCharCount == MAX_STATEMENT_LEN ){bufferOverrun = true; return false;}
                _statement[_statementCharCount] = ';';                               // still room: add character
                _statementCharCount++;
            }
        }
    }

    // process character
    else {                                                                  // not yet at end of program or imm. mode line
        if (_flushAllUntilEOF) { return false; }                       // discard characters (after parsing error)

        if (c == '\n') { _lineCount++; }                           // line number used when while reading program in input file

        // currently within a string or within a comment ?
        if (withinString) {
            if (c == '\\') { withinStringEscSequence = !withinStringEscSequence; }
            else if (c == '\"') { withinString = withinStringEscSequence; withinStringEscSequence = false; }
            else { withinStringEscSequence = false; }                 // any other character within string
            lastCharWasWhiteSpace = false;
            lastCharWasSemiColon = false;
        }

        else if (within1LineComment) {
            if (c == '\n') { within1LineComment = false; return false; }                // comment stops at end of line
        }

        else {                                                                                              // not within a string or comment
            bool leadingWhiteSpace = (((c == ' ') || (c == '\n')) && (_statementCharCount == 0));
            if (leadingWhiteSpace) { return false; };                        

            if (!within1LineComment && !withinMultiLineComment && (c == '\"')) { withinString = true; }
            else if (!withinString && (c == commentStartChar)) { within1LineComment = true; return false; }
            else if (c == '\n') { c = ' '; }                       // not within string or comment: replace a new line with a space (white space in multi-line statement)

            redundantSpaces = (_statementCharCount > 0) && (c == ' ') && lastCharWasWhiteSpace;
            redundantSemiColon = (c == ';') && lastCharWasSemiColon;
            lastCharWasWhiteSpace = (c == ' ');                     // remember
            lastCharWasSemiColon = (c == ';');
        }

        if (redundantSpaces || redundantSemiColon || within1LineComment) { return false; }            // no character added

        // add character  
        if (_statementCharCount == MAX_STATEMENT_LEN) { bufferOverrun = true; return false; }
        _statement[_statementCharCount] = c;                               // still room: add character
        _statementCharCount++;
    }

    return true;
}


// -------------------
// *                 *
// -------------------

bool Justina_interpreter::processAndExec(bool statementParsed, parseTokenResult_type result, bool& kill, char* pErrorPos) {

    execResult_type execResult{ result_execOK };

    if (statementParsed) {
        int funcNotDefIndex;
        if (result == result_tokenFound) {
            // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
            if (_programMode && (!allExternalFunctionsDefined(funcNotDefIndex))) { result = result_undefinedFunctionOrArray; }
            if (_blockLevel > 0) { result = result_noBlockEnd; }

            if (result != result_tokenFound) { _appFlags |= appFlag_errorConditionBit; }              // if parsing error only occurs here, error condition flag can still be set here (signal to caller)
        }

        (_programMode ? _lastProgramStep : _lastUserCmdStep) = _programCounter;

        if (result == result_tokenFound) {
            if (!_programMode) {

                // evaluation comes here
                if (_promptAndEcho == 2) { prettyPrintStatements(0); _pConsole->println(); }                    // immediate mode and result OK: pretty print input line
                else if (_promptAndEcho == 1) { _pConsole->println(); _isPrompt = false; }

                execResult = exec(_programStorage + PROG_MEM_SIZE);                                 // execute parsed user statements

                if ((execResult == result_kill) || (execResult == result_quit)) { _quitJustina = true; }
                if (execResult == result_kill) { kill = true; }
            }
        }

        // parsing OK message (program mode only - no message in immediate mode) or error message 
        printParsingResult(result, funcNotDefIndex, _statement, _lineCount, pErrorPos);
        statementParsed = false;      // reset
    }
    else { _pConsole->println(); }

    // if in debug mode, trace expressions (if defined) and print debug info 
    if ((_openDebugLevels > 0) && (execResult != result_kill) && (execResult != result_quit) && (execResult != result_initiateProgramLoad)) { traceAndPrintDebugInfo(); }

    // if program parsing error: reset machine, because variable storage might not be consistent with program any more
    if ((_programMode) && (result != result_tokenFound)) { resetMachine(false); }
    else if (execResult == result_initiateProgramLoad) { Serial.println("** reset machine "); resetMachine(false); }//// 1 lijn met vorige

    // no program error (could be immmediate mode error however): only reset a couple of items here 
    else {
        parsingStack.deleteList();
        _blockLevel = 0;
        _extFunctionBlockOpen = false;
    }

    if ((_promptAndEcho != 0) && (execResult != result_initiateProgramLoad)) { _pConsole->print("Justina> "); _isPrompt = true; }                 // print new prompt

    // execution finished (not stopping in debug mode), with or without error: delete parsed strings in imm mode command : they are on the heap and not needed any more. Identifiers must stay avaialble
    // -> if stopping a program for debug, do not delete parsed strings (in imm. mode command), because that command line has now been pushed on  ...
     // the parsed command line stack and included parsed constants will be deleted later (resetMachine routine)
    if (execResult != result_stopForDebug) { deleteConstStringObjects(_programStorage + PROG_MEM_SIZE); } // always

    // finalize 
    _programMode = false;
    _programCounter = _programStorage + PROG_MEM_SIZE;                 // start of 'immediate mode' program area
    *(_programStorage + PROG_MEM_SIZE) = tok_no_token;                                      //  current end of program (immediate mode)





    if (execResult == result_initiateProgramLoad) {
        _programMode = true;
        _programCounter = _programStorage;

        if (_isPrompt) { _pConsole->println(); }
        _pConsole->print("Waiting for program...\r\n");
        _isPrompt = false;

        _initiateProgramLoad = true;
    }






    statementParsed = false;

    (_appFlags &= ~appFlag_statusMask);
    (_openDebugLevels > 0) ? (_appFlags |= appFlag_stoppedInDebug) : (_appFlags |= appFlag_idle);     // signal 'debug mode' or 'idle' to caller

    return _quitJustina;
}

// -------------------
// *                 *
// -------------------

void Justina_interpreter::traceAndPrintDebugInfo() {
    // count of programs in debug:
    // - if an error occured in a RUNNING program, the program is terminated and the number of STOPPED programs ('in debug mode') does not change.
    // - if an error occured while executing a command line, then this count is not changed either
    // flow control stack:
    // - at this point, structure '_activeFunctionData' always contains flow control data for the main program level (command line - in debug mode if the count of open programs is not zero)
    // - the flow control stack maintains data about open block commands, open functions and eval() strings in execution (call stack)
    // => skip stack elements for any command line open block commands or eval() strings in execution, and fetch the data for the function where control will resume when started again

    char* nextStatementPointer = _programCounter;
    OpenFunctionData* pDeepestOpenFunction = &_activeFunctionData;

    void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;
    int blockType = block_none;
    do {                                                                // there is at least one open function in the call stack
        blockType = *(char*)pFlowCtrlStackLvl;
        if (blockType != block_extFunction) {
            pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
            continue;
        };
        break;
    } while (true);
    pDeepestOpenFunction = (OpenFunctionData*)pFlowCtrlStackLvl;        // deepest level of nested functions
    nextStatementPointer = pDeepestOpenFunction->pNextStep;

    _pConsole->println(); for (int i = 1; i <= _dispWidth; i++) { _pConsole->print("-"); } _pConsole->println();
    parseAndExecTraceString();     // trace string may not contain keywords, external functions, generic names
    char msg[150] = "";
    sprintf(msg, "DEBUG ==>> NEXT [%s: ", extFunctionNames[pDeepestOpenFunction->functionIndex]);
    _pConsole->print(msg);
    prettyPrintStatements(10, nextStatementPointer);

    if (_openDebugLevels > 1) {
        sprintf(msg, "*** this + %d other programs STOPPED ***", _openDebugLevels - 1);
        _pConsole->println(msg);
    }
}