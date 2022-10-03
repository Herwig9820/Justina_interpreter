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

#define printCreateDeleteHeapObjects 0
#define debugPrint 0


// ******************************************************************
// ***         class Justina_interpreter - implemantation           *
// ******************************************************************

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

#if printCreateDeleteHeapObjects
    Serial.print("(LIST) Create elem # "); Serial.print(_listElementCount);
    Serial.print(", list ID "); Serial.print(_listID);
    Serial.print(", name "); Serial.print(_listName);
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

    // before deleting object, remove from list:
    // change pointers from previous element (or _pFirstPointer, if no previous element) and next element (or _pLastPointer, if no next element)
    ((pElem->pPrev == nullptr) ? _pFirstElement : pElem->pPrev->pNext) = pElem->pNext;
    ((pElem->pNext == nullptr) ? _pLastElement : pElem->pNext->pPrev) = pElem->pPrev;

#if printCreateDeleteHeapObjects
    Serial.print("(LIST) Delete elem # "); Serial.print(_listElementCount);
    Serial.print(", list ID "); Serial.print(_listID);
    Serial.print(", name "); Serial.print(_listName);
    Serial.print(", list elem address: "); Serial.println((uint32_t)pElem - RAMSTART);
#endif
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

    _quitJustinaAtEOF = false;
    _isPrompt = false;

    _instructionCharCount = 0;
    _lineCount = 0;                             // taking into account new line after 'load program' command ////
    _flushAllUntilEOF = false;
    _StarCmdCharCount = 0;

    _programMode = false;
    _currenttime = millis();
    _previousTime = _currenttime;
    _lastCallBackTime = _currenttime;

    parsingStack.setListName("parsing ");
    evalStack.setListName("eval    ");
    flowCtrlStack.setListName("flowCtrl");
    immModeCommandStack.setListName("cmd line");

    // purely execution related counters
    _intermediateStringObjectCount = 0;
    _localVarValueAreaCount = 0;
    _localVarStringObjectCount = 0;
    _localArrayObjectCount = 0;


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
    bool kill{ false };                                       // kill is true: request from caller, kill is false: quit command executed
    bool quitNow{ false };
    char c;

    _pConsole->println();
    for (int i = 0; i < 13; i++) { _pConsole->print("*"); } _pConsole->print("____");
    for (int i = 0; i < 4; i++) { _pConsole->print("*"); } _pConsole->print("__");
    for (int i = 0; i < 14; i++) { _pConsole->print("*"); } _pConsole->print("_");
    for (int i = 0; i < 10; i++) { _pConsole->print("*"); }_pConsole->println();

    _pConsole->print("    "); _pConsole->println(ProductName);
    _pConsole->print("    "); _pConsole->println(LegalCopyright);
    _pConsole->print("    Version: "); _pConsole->print(ProductVersion); _pConsole->print(" ("); _pConsole->print(BuildDate); _pConsole->println(")");
    for (int i = 0; i < 48; i++) { _pConsole->print("*"); } _pConsole->println();

    _programMode = false;                                   //// te checken of er dan nog iets moet gereset worden
    *_programStart = '\0';                                      //  current end of program (immediate mode)
    _pConsole = pConsole;//// ??? constructor toch ?
    _isPrompt = false;                 // end of parsing
    _pTerminal = pTerminal;
    _definedTerminals = definedTerms;

    _coldStart = false;             // can be used if needed in this procedure, to determine whether this was a cold or warm start

    do {
        // while waiting for characters, continuously do a housekeeping callback (if function defined)
        if (_housekeepingCallback != nullptr) {
            _currenttime = millis();                                                        // while reading from console, continuously call housekeeping callback routine
            _previousTime = _currenttime;                                                   // keep up to date (needed during parsing and evaluation)
            _lastCallBackTime = _currenttime;
            _housekeepingCallback(quitNow, _appFlags);
            if (quitNow) { kill = true; break; }                // return true if kill request received from calling program OR Justina Quit command executed 
        }

        if (_pConsole->available() > 0) {     // if terminal character available for reading
            c = _pConsole->read();
            quitNow = processCharacter(c, kill);        // process one character. Kill request from calling program and 
            if (quitNow) { ; break; }                        // user gave quit command
        }
    } while (true);

    _appFlags = 0x0000L;
    _housekeepingCallback(quitNow, _appFlags);      // only to pass application flags to caller

    if (kill) { _pConsole->println("\r\n\r\n>>>>> Justina: kill request received from calling program <<<<<"); }
    if (_keepInMemory) { _pConsole->println("\r\nJustina: bye\r\n"); }        // if remove from memory: message given in destructor
    _quitJustinaAtEOF = false;         // if interpreter stays in memory: re-init

    return _keepInMemory;
}


// ----------------------------------
// *   process an input character   *
// ----------------------------------

bool Justina_interpreter::processCharacter(char c, bool& kill) {
    // process character
    static parseTokenResult_type result{};
    static bool requestMachineReset{ false };
    static bool withinStringEscSequence{ false };
    static bool instructionsParsed{ false };
    static bool lastCharWasWhiteSpace{ false };
    static bool lastCharWasSemiColon{ false };

    static bool withinComment{ false };
    static bool withinString{ false };

    static char* pErrorPos{};

    const char quitCalc[8] = "*quit*";

    char EOFchar = 0x1A;
    char commentStartChar = '$';

    bool redundantSpaces = false;                                       // init
    bool redundantSemiColon = false;

    bool isEndOfFile = (!_programMode && (c == '\n')) || (c == EOFchar);                                      // end of input: EOF in program mode, LF or EOF in immediate mode
    bool isCommentStartChar = (c == '$');                               // character can also be part of comment

    //// tijdelijk
    bool isProgramCtrl = (c == 2);                                   // switch between program and immediate mode ?
    bool isParserReset = (c == 3);                                     // reset parser ?


    if (isProgramCtrl) {
        // do not touch program memory itself: there could be a program in it 
        _programMode = !_programMode;
        _programStart = _programStorage + (_programMode ? 0 : PROG_MEM_SIZE);
        _programSize = _programSize + (_programMode ? PROG_MEM_SIZE : IMM_MEM_SIZE);
        _programCounter = _programStart;                          // start of 'immediate mode' program area

        requestMachineReset = _programMode;                         // reset machine when parsing starts, not earlier (in case there is a program in memory)

        _instructionCharCount = 0;
        _lineCount = 0;                             // taking into account new line after 'load program' command ////
        _StarCmdCharCount = 0;
        _flushAllUntilEOF = false;

        lastCharWasWhiteSpace = false;
        lastCharWasSemiColon = false;

        withinString = false; withinStringEscSequence = false;
        withinComment = false;

        if (_isPrompt) { _pConsole->println(); }
        _pConsole->print(_programMode ? "Waiting for program...\r\n" : ((_promptAndEcho != 0) ? "Justina> " : ""));
        _isPrompt = (_promptAndEcho != 0) ? !_programMode : false;
        return false;
    }
    else if (isParserReset) {  // temporary
        _programMode = false;
        resetMachine(true);

        instructionsParsed = false;

        _instructionCharCount = 0;
        _lineCount = 0;                             // taking into account new line after 'load program' command ////
        _StarCmdCharCount = 0;
        _flushAllUntilEOF = false;

        lastCharWasWhiteSpace = false;
        lastCharWasSemiColon = false;

        withinString = false; withinStringEscSequence = false;
        withinComment = false;

        return false;
    }
    else if ((c < ' ') && (c != '\n') && (!isEndOfFile)) { return false; }                  // skip control-chars except new line and EOF character



    if (!isEndOfFile) {
        if (_flushAllUntilEOF) { return false; }                       // discard characters (after parsing error)

        bool isLeadingSpace = ((_StarCmdCharCount == 0) && (c == ' '));
        if (c == '\n') { _lineCount++; _StarCmdCharCount = 0; }                           // line number used when while reading program in input file

        // check for exit command if not in program mode
        if (!_programMode && !isLeadingSpace && !(c == '\n') && (_StarCmdCharCount >= 0)) {
            if (c == quitCalc[_StarCmdCharCount]) {
                _StarCmdCharCount++;
                if (quitCalc[_StarCmdCharCount] == '\0') { _flushAllUntilEOF = true; _quitJustinaAtEOF = true; return false; }         // perfect match: set flag to exit interpreter
                else  if (_StarCmdCharCount == strlen(quitCalc)) { _StarCmdCharCount = -1; }  // -1: no match: no further checking for now
            }
            else { _StarCmdCharCount = -1; };     // -1: no match: no further checking for now
        }

        // currently within a string or within a comment ?
        if (withinString) {
            if (c == '\\') { withinStringEscSequence = !withinStringEscSequence; }
            else if (c == '\"') { withinString = withinStringEscSequence; withinStringEscSequence = false; }
            else { withinStringEscSequence = false; }                 // any other character within string
            lastCharWasWhiteSpace = false;
            lastCharWasSemiColon = false;

        }
        else if (withinComment) {
            if (c == '\n') { withinComment = false; return false; }                // comment stops at end of line
        }
        else {                                                                                              // not within a string
            bool leadingWhiteSpace = (((c == ' ') || (c == '\n')) && (_instructionCharCount == 0));
            if (leadingWhiteSpace) { return false; };                        // but always process end of file character

            if (!withinComment && (c == '\"')) { withinString = true; }
            else if (!withinString && (c == commentStartChar)) { withinComment = true; return false; }
            else if (c == '\n') { c = ' '; }                       // not within string or comment: replace a new line with a space (white space in multi-line instruction)

            redundantSpaces = (_instructionCharCount > 0) && (c == ' ') && lastCharWasWhiteSpace;
            redundantSemiColon = (c == ';') && lastCharWasSemiColon;
            lastCharWasWhiteSpace = (c == ' ');                     // remember
            lastCharWasSemiColon = (c == ';');
        }

        // less than 3 positions available in buffer: discard character (keep 2 free positions to add optional ';' and for terminating '\0')  
        if ((_instructionCharCount <= _maxInstructionChars - 3) && !isEndOfFile && !redundantSpaces && !redundantSemiColon && !withinComment) {
            _instruction[_instructionCharCount] = c;                               // still room: add character
            _instructionCharCount++;
        }
    }

    if ((_instructionCharCount > 0) && isEndOfFile) {             // if last instruction before EOF does not contain a semicolon separator at the end, add it 
        if (_instruction[_instructionCharCount - 1] != ';') {
            _instruction[_instructionCharCount] = ';';                               // still room: add character
            _instructionCharCount++;
        }
    }




    bool isInstructionSeparator = (!withinString) && (!withinComment) && (c == ';') && !redundantSemiColon;   // only if before end of file character 
    isInstructionSeparator = isInstructionSeparator || (withinString && (c == '\n'));  // new line sent to parser as well
    bool instructionComplete = isInstructionSeparator || (isEndOfFile && (_instructionCharCount > 0));

    if (instructionComplete && !_quitJustinaAtEOF) {                                                // terminated by a semicolon if not end of input
        _instruction[_instructionCharCount] = '\0';                            // add string terminator

        if (requestMachineReset) {
            resetMachine(false);                                // prepare for parsing next program (stay in current mode )
            requestMachineReset = false;
        }

        char* pInstruction = _instruction;                                                 // because passed by reference 
        result = parseInstruction(pInstruction);                                 // parse one instruction (ending with ';' character, if found)
        pErrorPos = pInstruction;                                                      // in case of error
        if (result != result_tokenFound) { _flushAllUntilEOF = true; }
        if (result == result_parse_kill) { kill = true; _quitJustinaAtEOF = true; }     // _flushAllUntilEOF is true already (flush buffer before quitting)

        _instructionCharCount = 0;
        withinString = false; withinStringEscSequence = false;
        instructionsParsed = true;                                  // instructions found
    }


    if (isEndOfFile) {
        execResult_type execResult{ result_execOK };

        if (instructionsParsed) {
            int funcNotDefIndex;
            if (result == result_tokenFound) {
                // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
                if (_programMode && (!allExternalFunctionsDefined(funcNotDefIndex))) { result = result_undefinedFunctionOrArray; }
                if (_blockLevel > 0) { result = result_noBlockEnd; }

                if (result != result_tokenFound) { _appFlags |= 0x0001L; }              // if parsing error only occurs here, error condition flag can still be set here
            }

            if (result == result_tokenFound) {            // result could be altered in the meantime
                if (!_programMode) {

                    // evaluation comes here
                    if (_promptAndEcho == 2) { prettyPrintInstructions(0); }                    // immediate mode and result OK: pretty print input line
                    else if (_promptAndEcho == 1) { _pConsole->println(); _isPrompt = false; }

                    execResult = exec();                                 // execute parsed user statements

                    if ((execResult == result_eval_kill) || (execResult == result_eval_quit)) { _quitJustinaAtEOF = true; }
                    if (execResult == result_eval_kill) { kill = true; }
                }
            }

            // parsing OK message (program mode only - no message in immediate mode) or error message 
            printParsingResult(result, funcNotDefIndex, _instruction, _lineCount, pErrorPos);
            (immModeCommandStack.getElementCount() > 0) ? (_appFlags |= 0x0030L) : (_appFlags &= ~0x0030L);
        }
        else { _pConsole->println(); }

        // count of programs in debug:
        // - if an error occured in a RUNNING program, the program is terminated and the number of STOPPED programs ('in debug mode') does not change.
        // - if an error occured while executing a command line, then this count is not changed either
        // flow control stack:
        // - at this point, structure '_activeFunctionData' always contains flow control data for the main program level (command line - in debug mode if the count of open programs is not zero)
        // - the flow control stack maintains data about open block commands and open functions (call stack)
        // => skip stack elements for any command line open block commands and fetch the data for the function where control will resume when started again

        if ((immModeCommandStack.getElementCount() > 0) && (execResult != result_eval_kill) && (execResult != result_eval_quit)) {
            char* nextInstructionsPointer = _programCounter;
            OpenFunctionData* pDeepestOpenFunction = &_activeFunctionData;

            void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                    int blockType = block_none;
            do {                                                                // there is at least one open function in the call stack
                blockType = *(char*)pFlowCtrlStackLvl;
                if (blockType != block_extFunction) {
                    pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                    continue;
                };
                break;
            } while (true);
            pDeepestOpenFunction = (OpenFunctionData*)pFlowCtrlStackLvl;        // deepest level of nested functions
            nextInstructionsPointer = pDeepestOpenFunction->pNextStep;

            _pConsole->println(); for (int i = 1; i <= _dispWidth; i++) { _pConsole->print("-"); }
            char msg[150] = "";
            sprintf(msg, "\r\n*** DEBUG *** NEXT [%s: ", extFunctionNames[pDeepestOpenFunction->functionIndex]);
            _pConsole->print(msg);
            prettyPrintInstructions(10, nextInstructionsPointer);

            if (immModeCommandStack.getElementCount() > 1) {
                sprintf(msg, "*** this + %d other programs STOPPED ***", immModeCommandStack.getElementCount() - 1);
                _pConsole->println(msg);
            }
        }

        if (!_programMode && (_promptAndEcho != 0)) { _pConsole->print("Justina> "); _isPrompt = true; }                 // print new prompt

        bool wasReset = false;      // init
        if (_programMode) {               //// waarschijnlijk aan te passen als LOADPROG cmd implemented (-> steeds vanuit immediate mode)
            // end of file: always back to immediate mode
            // do not touch program memory itself: there could be a program in it 
            _programMode = false;

            // if program parsing error: reset machine, because variable storage is not consistent with program 
            if (result != result_tokenFound) {
                resetMachine(false);      // message not needed here
                wasReset = true;
            }
        }

        // in immediate mode; if stopping a program for debug, do not delete parsed strings included in the command line, because that command line has now been pushed on  ...
        // the parsed command line stack and included parsed constants will be deleted later (resetMachine routine)
        else if (execResult == result_eval_stopForDebug) { *_programStart = '\0'; }  ////

        // in immediate mode
        else if (instructionsParsed) {
            // execution finished: delete parsed strings in imm mode command (they are on the heap and not needed any more). Identifiers must stay avaialble
            deleteConstStringObjects(_programStorage + PROG_MEM_SIZE);  // always
            *_programStart = '\0';                                      //  current end of program (immediate mode)
        }

        if (!wasReset) {
            parsingStack.deleteList();                      // safety
            _blockLevel = 0;
            _extFunctionBlockOpen = false;

            _programStart = _programStorage + PROG_MEM_SIZE;        // already set immediate mode 
            _programSize = _programSize + IMM_MEM_SIZE;
            _programCounter = _programStart;                        // start of 'immediate mode' program area

        }

        instructionsParsed = false;
        lastCharWasSemiColon = false;

        _instructionCharCount = 0;
        _lineCount = 0;
        _StarCmdCharCount = 0;
        _flushAllUntilEOF = false;

        withinComment = false;

#if debugPrint
        Serial.print("\r\n** EOF stats:\r\n    parsed strings "); Serial.print(_parsedStringConstObjectCount);

        Serial.print(", prog name strings "); Serial.print(_identifierNameStringObjectCount);
        Serial.print(", prog var strings "); Serial.print(_globalStaticVarStringObjectCount);
        Serial.print(", prog arrays "); Serial.print(_globalStaticArrayObjectCount);

        Serial.print(", user var names "); Serial.print(_userVarNameStringObjectCount);
        Serial.print(", user var strings "); Serial.print(_userVarStringObjectCount);
        Serial.print(", user arrays "); Serial.print(_userArrayObjectCount);

        Serial.print(", last value strings "); Serial.print(_lastValuesStringObjectCount);


        Serial.print("\r\n    interim strings "); Serial.print(_intermediateStringObjectCount);

        Serial.print(", local var storage "); Serial.print(_localVarValueAreaCount);
        Serial.print(", local var strings "); Serial.print(_localVarStringObjectCount);
        Serial.print(", local arrays "); Serial.println(_localArrayObjectCount);

        Serial.println();
#endif
        return _quitJustinaAtEOF;


    }
    return false;  // and wait for next character
}
