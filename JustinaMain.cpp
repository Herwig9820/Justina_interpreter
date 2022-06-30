#include "Justina.h"

#define printCreateDeleteHeapObjects 0

/***********************************************************
*                    class LinkedList                   *
*    append and remove list elements from linked list      *
***********************************************************/

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


//-------------------------------
// *   get list element count   *
//-------------------------------

int LinkedList::getElementCount() {
    return _listElementCount;
}



/***********************************************************
*                      class Interpreter                    *
***********************************************************/

// -------------------
// *   constructor   *
// -------------------

Interpreter::Interpreter(Stream* const pConsole) : _pConsole(pConsole) {
    _pConsole->println("Justina: starting...");
    _callbackFcn = nullptr;
    for (int i=0; i< _userCBarrayDepth; i++) { _callbackUserProcStart[i] = nullptr;}    
    _userCBprocStartSet_count = 0;

    _pmyParser = new MyParser(this);              // pass the address of this Interpreter object to the MyParser constructor
    _quitCalcAtEOF = false;
    _isPrompt = false;

    // init 'machine' (not a complete reset, because this clears heap objects for this Interpreter object, and there are none)
    _programVarNameCount = 0;
    _staticVarCount = 0;
    _localVarCountInFunction = 0;
    _paramOnlyCountInFunction = 0;
    _extFunctionCount = 0;

    _instructionCharCount = 0;
    _lineCount = 0;                             // taking into account new line after 'load program' command ////
    _flushAllUntilEOF = false;
    _StarCmdCharCount = 0;

    _programMode = false;
    _programStart = _programStorage + PROG_MEM_SIZE;            // start in immediate mode
    _programSize = IMM_MEM_SIZE;
    _programCounter = _programStart;                          // start of 'immediate mode' program area

    // name strings for variables and functions
    identifierNameStringObjectCount = 0;
    userVarNameStringObjectCount = 0;

    // constant strings
    parsedStringConstObjectCount = 0;
    intermediateStringObjectCount = 0;
    lastValuesStringObjectCount = 0;

    // strings as value of variables
    globalStaticVarStringObjectCount = 0;
    userVarStringObjectCount = 0;
    localVarStringObjectCount = 0;

    // array storage 
    globalStaticArrayObjectCount = 0;
    userArrayObjectCount = 0;
    localArrayObjectCount = 0;

    // current last result FiFo depth (values currently stored)
    _lastResultCount = 0;

    // calculation result print
    _dispWidth = _defaultPrintWidth, _dispNumPrecision = _defaultNumPrecision, _dispCharsToPrint = _defaultCharsToPrint, _dispFmtFlags = _defaultPrintFlags;
    _dispNumSpecifier[0] = 'G'; _dispNumSpecifier[1] = '\0';
    _dispIsHexFmt = false;
    makeFormatString(_dispFmtFlags, false, _dispNumSpecifier, _dispNumberFmtString);       // for numbers
    strcpy(_dispStringFmtString, "%*.*s%n");                                                           // for strings

     // for print command
    _printWidth = _defaultPrintWidth, _printNumPrecision = _defaultNumPrecision, _printCharsToPrint = _defaultCharsToPrint, _printFmtFlags = _defaultPrintFlags;
    _printNumSpecifier[0] = 'G'; _printNumSpecifier[1] = '\0';

    // display output settings
    _promptAndEcho = 2, _printLastResult = true;

    *_programStorage = '\0';                                    //  current end of program 
    *_programStart = '\0';                                      //  current end of program (immediate mode)
};


// ---------------------
// *   deconstructor   *
// ---------------------

Interpreter::~Interpreter() {
    _pConsole->println("Justina: quitting...");
    if (!_keepInMemory) {
        delete _pmyParser;
        _callbackFcn = nullptr;
    }
    _pConsole->println("Justina: bye\r\n");
};


// ------------------------------
// *   set call back functons   *
// ------------------------------

void Interpreter::setMainLoopCallback(void (*func)(bool& requestQuit)) {

    // a call from the user program initializes the address of a 'user callback' function.
    // Justina will call this user routine repeatedly and automatically, allowing  the user...
    // ...to execute a specific routine regularly (e.g. to maintain a TCP connection, to implement a heartbeat, ...)
    _callbackFcn = func;
}

void Interpreter::setUserFcnCallback(void(*func) (const void* data)) {

    // each call from the user program initializes a next 'user callback' function address in an array of function addresses 
    if (_userCBprocStartSet_count < _userCBarrayDepth) { _callbackUserProcStart[_userCBprocStartSet_count++] = func; }      // throw away if callback array full
}



// ----------------------------
// *   interpreter main loop   *
// ----------------------------

bool Interpreter::run(Stream* const pConsole, Stream** const pTerminal, int definedTerms) {
    bool quitNow{ false };
    char c;

    _programMode = false;                                   //// te checken of er dan nog iets moet gereset worden
    _pConsole = pConsole;
    _isPrompt = false;                 // end of parsing
    _pTerminal = pTerminal;
    _definedTerminals = definedTerms;

    //// ***** test hierna

    long aaa = 5, bbb = 6;
    void* paaa = &aaa, * pbbb = &bbb;

    long dataArray[5]{ 10,11,12,13,14 };

    if (_callbackUserProcStart[0] != nullptr) { _callbackUserProcStart[0](paaa); }      //// test: roep 3 CB functies op
    if (_callbackUserProcStart[1] != nullptr) { _callbackUserProcStart[1](pbbb); }
    if (_callbackUserProcStart[2] != nullptr) { _callbackUserProcStart[2](dataArray); }

    //// ***** test tot hier

    do {
        if (_callbackFcn != nullptr) { _callbackFcn(quitNow); }
        if (quitNow) { _pConsole->println("\r\nAbort request received"); break; }
        if (_pConsole->available() > 0) {     // if terminal character available for reading
            c = _pConsole->read();
            quitNow = processCharacter(c);        // process one character
            if (quitNow) { _pConsole->println(); break; }                        // user gave quit command
        }
    } while (true);

    if (_keepInMemory) { _pConsole->println("Justina: bye\r\n"); }        // if remove from memory: message given in destructor
    _quitCalcAtEOF = false;         // if interpreter stays in memory: re-init
    return _keepInMemory;
}

// ----------------------------------
// *   process an input character   *
// ----------------------------------

bool Interpreter::processCharacter(char c) {
    // process character
    static MyParser::parseTokenResult_type result{};
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
        _pmyParser->resetMachine(true);

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
                if (quitCalc[_StarCmdCharCount] == '\0') { _flushAllUntilEOF = true; _quitCalcAtEOF = true; return false; }         // perfect match: set flag to exit interpreter
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

    if (instructionComplete && !_quitCalcAtEOF) {                                                // terminated by a semicolon if not end of input
        _instruction[_instructionCharCount] = '\0';                            // add string terminator

        if (requestMachineReset) {
            _pmyParser->resetMachine(false);                                // prepare for parsing next program( stay in current mode )
            requestMachineReset = false;
        }

        char* pInstruction = _instruction;                                                 // because passed by reference 
        result = _pmyParser->parseInstruction(pInstruction);                                 // parse one instruction (ending with ';' character, if found)
        pErrorPos = pInstruction;                                                      // in case of error
        if (result != MyParser::result_tokenFound) { _flushAllUntilEOF = true; }
        _instructionCharCount = 0;
        withinString = false; withinStringEscSequence = false;

        instructionsParsed = true;                                  // instructions found
    }




    if (isEndOfFile) {
        if (instructionsParsed) {
            int funcNotDefIndex;
            if (result == MyParser::result_tokenFound) {
                // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
                if (_programMode && (!_pmyParser->allExternalFunctionsDefined(funcNotDefIndex))) { result = MyParser::result_undefinedFunctionOrArray; }
                if (_pmyParser->_blockLevel > 0) { result = MyParser::result_noBlockEnd; }
                if (!_programMode) {

                    // evaluation comes here
                    if (_promptAndEcho == 2) { _pmyParser->prettyPrintInstructions(false); }                    // immediate mode and result OK: pretty print input line
                    else if (_promptAndEcho == 1) { _pConsole->println(); _isPrompt = false; }
                    exec();                                 // execute parsed user statements

                }
            }
            // parsing OK message (program mode only - no message in immediate mode) or error message 
            _pmyParser->printParsingResult(result, funcNotDefIndex, _instruction, _lineCount, pErrorPos);
        }
        else { _pConsole->println(); }                                       // empty line: advance to next line only
        if (_promptAndEcho != 0) { _pConsole->print("Justina> "); _isPrompt = true; }                 // print new prompt



        bool wasReset = false;      // init
        if (_programMode) {               //// waarschijnlijk aan te passen als LOADPROG cmd implemented (-> steeds vanuit immediate mode)
            // end of file: always back to immediate mode
            // do not touch program memory itself: there could be a program in it 
            _programMode = false;

            // if program parsing error: reset machine, because variable storage is not consistent with program 
            if (result != MyParser::result_tokenFound) {
                _pmyParser->resetMachine(false);      // message not needed here
                wasReset = true;
            }
        }

        // was in immediate mode
        else if (instructionsParsed) {

            // delete alphanumeric constants because they are on the heap. Identifiers must stay avaialble
            _pmyParser->deleteConstStringObjects(_programStorage + PROG_MEM_SIZE);  // always
            *_programStart = '\0';                                      //  current end of program (immediate mode)
        }


        if (!wasReset) {
            _pmyParser->parsingStack.deleteList();                      // safety
            _pmyParser->_blockLevel = 0;
            _pmyParser->_extFunctionBlockOpen = false;

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
    }

    return _quitCalcAtEOF;  // and wait for next character
}
