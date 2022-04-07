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
    _listIDcounter++;
}


// --------------------------------------------------
// *   append a list element to the end of a list   *
// --------------------------------------------------

char* LinkedList::appendListElement( int size ) {
    ListElemHead* p = (ListElemHead*) (new char [sizeof( ListElemHead ) + size]);       // create list object with payload of specified size in bytes

    if ( _pFirstElement == nullptr ) {                                                  // not yet any elements
        _pFirstElement = p;
        p->pPrev = nullptr;                                                             // is first element in list: no previous element
    }
    else {
        _pLastElement->pNext = p;
        p->pPrev = _pLastElement;
    }
    _pLastElement = p;
    p->pNext = nullptr;
    _listElementCount++;
#if printCreateDeleteHeapObjects
    Serial.print( "(LIST) Create elem # " ); Serial.print( _listElementCount );
    Serial.print( "- list ID " ); Serial.print( _listID );
    if ( p == nullptr ) { Serial.println( "- list elem adres: nullptr" ); }
    else {
        Serial.print( "- list elem adres: " ); Serial.println( (uint32_t) p - RAMSTART );
    }
#endif
    return (char*) (p + 1);                                          // pointer to payload of newly created element
}


// -----------------------------------------------------
// *   delete a heap object and remove it from list    *
// -----------------------------------------------------

char* LinkedList::deleteListElement( void* pPayload ) {                              // input: pointer to payload of a list element

    ListElemHead* pElem = (ListElemHead*) pPayload;                                     // still points to payload: check if nullptr
    if ( pElem == nullptr ) { pElem = _pLastElement; }                                  // nullptr: delete last element in list (if it exists)
    else { pElem = pElem - 1; }                                                         // pointer to list element header

    if ( pElem == nullptr ) { return nullptr; }                                         // still nullptr: return

    ListElemHead* p = pElem->pNext;                                                     // remember return value

    // before deleting object, remove from list:
    // change pointers from previous element (or _pFirstPointer, if no previous element) and next element (or _pLastPointer, if no next element)
    ((pElem->pPrev == nullptr) ? _pFirstElement : pElem->pPrev->pNext) = pElem->pNext;
    ((pElem->pNext == nullptr) ? _pLastElement : pElem->pNext->pPrev) = pElem->pPrev;

#if printCreateDeleteHeapObjects
    Serial.print( "(LIST) Delete elem # " ); Serial.print( _listElementCount );
    Serial.print( "- list ID " ); Serial.print( _listID );
    Serial.print( "- list elem adres: " ); Serial.println( (uint32_t) pElem - RAMSTART );
#endif
    _listElementCount--;
    delete []pElem;
    //// 
    if ( p == nullptr ) { return nullptr; }
    else { return (char*) (p + 1); }                                           // pointer to payload of next element in list, or nullptr if last element deleted
}


// ------------------------------------------
// *   delete all list elements in a list   *
// ------------------------------------------

void LinkedList::deleteList() {
    if ( _pFirstElement == nullptr ) return;

    ListElemHead* pHead = _pFirstElement;
    while ( true ) {
        char* pNextPayload = deleteListElement( (char*) (pHead + 1) );
        if ( pNextPayload == nullptr ) { return; }
        pHead = ((ListElemHead*) pNextPayload) - 1;                                     // points to list element header 
    }
}


// ----------------------------------------------------
// *   get a pointer to the first element in a list   *
// ----------------------------------------------------

char* LinkedList::getFirstListElement() {
    return (char*) (_pFirstElement + (_pFirstElement == nullptr, 0, 1));
}


//----------------------------------------------------
// *   get a pointer to the last element in a list   *
//----------------------------------------------------

char* LinkedList::getLastListElement() {
    return (char*) (_pLastElement + (_pFirstElement == nullptr, 0, 1));
}


// -------------------------------------------------------
// *   get a pointer to the previous element in a list   *
// -------------------------------------------------------

char* LinkedList::getPrevListElement( void* pPayload ) {                                 // input: pointer to payload of a list element  
    if ( pPayload == nullptr ) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*) pPayload) - 1;                                     // points to list element header
    if ( pElem->pPrev == nullptr ) { return nullptr; }
    return (char*) (pElem->pPrev + 1);                                                      // points to payload of previous element
}


//----------------------------------------------------
// *   get a pointer to the next element in a list   *
//----------------------------------------------------

char* LinkedList::getNextListElement( void* pPayload ) {
    if ( pPayload == nullptr ) { return nullptr; }                                          // nullptr: return
    ListElemHead* pElem = ((ListElemHead*) pPayload) - 1;                                     // points to list element header
    if ( pElem->pNext == nullptr ) { return nullptr; }
    return (char*) (pElem->pNext + 1);                                                      // points to payload of previous element
}


/***********************************************************
*                      class Interpreter                    *
***********************************************************/

// -------------------
// *   constructor   *
// -------------------

Interpreter::Interpreter( Stream* const pConsole ) : _pConsole( pConsole ) {
    _pConsole->println( "Justina: starting..." );
    _callbackFcn = nullptr;
    _pmyParser = new MyParser( this );              // pass the address of this Interpreter object to the MyParser constructor
    _quitCalcAtEOF = false;
    _isPrompt = false;

    // init 'machine' (not a complete reset, because this clears heap objects for this calculator object, and there are none)
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

    _lastResultCount = 0;

    *_programStorage = '\0';                                    //  current end of program 
    *_programStart = '\0';                                      //  current end of program (immediate mode)
};


// ---------------------
// *   deconstructor   *
// ---------------------

Interpreter::~Interpreter() {
    _pConsole->println( "Justina: quitting..." );
    if ( !_keepInMemory ) {
        delete _pmyParser;
        _callbackFcn = nullptr;
    }
    _pConsole->println( "Justina: bye\r\n" );
};


// ----------------------------
// *   calculator main loop   *
// ----------------------------

void Interpreter::setCalcMainLoopCallback( void (*func)(bool& requestQuit) ) {
    // initialize callback function (e.g. to maintain a TCP connection, to implement a heartbeat, ...)
    _callbackFcn = func;
}


// ----------------------------
// *   calculator main loop   *
// ----------------------------

bool Interpreter::run( Stream* const pConsole, Stream** const pTerminal, int definedTerms ) {
    bool quitNow { false };
    char c;

    _programMode = false;                                   //// te checken of er dan nog iets moet gereset worden
    _pConsole = pConsole;
    _isPrompt = false;                 // end of parsing
    _pTerminal = pTerminal;
    _definedTerminals = definedTerms;

    do {
        if ( _callbackFcn != nullptr ) { _callbackFcn( quitNow ); }
        if ( quitNow ) { _pConsole->println( "\r\nAbort request received" ); break; }
        if ( _pConsole->available() > 0 ) {     // if terminal character available for reading
            c = _pConsole->read();
            quitNow = processCharacter( c );        // process one character
            if ( quitNow ) { _pConsole->println(); break; }                        // user gave quit command
        }
    } while ( true );

    if ( _keepInMemory ) { _pConsole->println( "Justina: bye\r\n" ); }        // if remove from memory: message given in destructor
    _quitCalcAtEOF = false;         // if calculator stays in memory: re-init
    return _keepInMemory;
}

// ----------------------------------
// *   process an input character   *
// ----------------------------------

bool Interpreter::processCharacter( char c ) {
    // process character
    static MyParser::parseTokenResult_type result {};
    static bool requestMachineReset { false };
    static bool withinStringEscSequence { false };
    static bool instructionsParsed { false };
    static bool lastCharWasWhiteSpace { false };
    static bool lastCharWasSemiColon { false };

    static bool withinComment { false };
    static bool withinString { false };

    static char* pErrorPos {};

    const char quitCalc [8] = "*quit*";

    char EOFchar = 0x1A;
    char commentStartChar = '$';

    bool redundantSpaces = false;                                       // init
    bool redundantSemiColon = false;

    bool isEndOfFile = (!_programMode && (c == '\n')) || (c == EOFchar);                                      // end of input: EOF in program mode, LF or EOF in immediate mode
    bool isCommentStartChar = (c == '$');                               // character can also be part of comment

    //// tijdelijk
    bool isProgramCtrl = (c == 2);                                   // switch between program and immediate mode ?
    bool isParserReset = (c == 3);                                     // reset parser ?


    if ( isProgramCtrl ) {
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

        if ( _isPrompt ) { _pConsole->println(); }
        _pConsole->print( _programMode ? "Waiting for program...\r\n" : "Justina> " ); _isPrompt = !_programMode;
        return false;
    }
    else if ( isParserReset ) {  // temporary
        _programMode = false;
        _pmyParser->resetMachine( true );

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
    else if ( (c < ' ') && (c != '\n') && (!isEndOfFile) ) { return false; }                  // skip control-chars except new line and EOF character



    if ( !isEndOfFile ) {
        if ( _flushAllUntilEOF ) { return false; }                       // discard characters (after parsing error)

        bool isLeadingSpace = ((_StarCmdCharCount == 0) && (c == ' '));
        if ( c == '\n' ) { _lineCount++; _StarCmdCharCount = 0; }                           // line number used when while reading program in input file

        // check for exit command if not in program mode
        if ( !_programMode && !isLeadingSpace && !(c == '\n') && (_StarCmdCharCount >= 0) ) {
            if ( c == quitCalc [_StarCmdCharCount] ) {
                _StarCmdCharCount++;
                if ( quitCalc [_StarCmdCharCount] == '\0' ) { _flushAllUntilEOF = true; _quitCalcAtEOF = true; return false; }         // perfect match: set flag to exit calculator
                else  if ( _StarCmdCharCount == strlen( quitCalc ) ) { _StarCmdCharCount = -1; }  // -1: no match: no further checking for now
            }
            else { _StarCmdCharCount = -1; };     // -1: no match: no further checking for now
        }

        // currently within a string or within a comment ?
        if ( withinString ) {
            if ( c == '\\' ) { withinStringEscSequence = !withinStringEscSequence; }
            else if ( c == '\"' ) { withinString = withinStringEscSequence; withinStringEscSequence = false; }
            else { withinStringEscSequence = false; }                 // any other character within string
            lastCharWasWhiteSpace = false;
            lastCharWasSemiColon = false;

        }
        else if ( withinComment ) {
            if ( c == '\n' ) { withinComment = false; return false; }                // comment stops at end of line
        }
        else {                                                                                              // not within a string
            bool leadingWhiteSpace = (((c == ' ') || (c == '\n')) && (_instructionCharCount == 0));
            if ( leadingWhiteSpace ) { return false; };                        // but always process end of file character

            if ( !withinComment && (c == '\"') ) { withinString = true; }
            else if ( !withinString && (c == commentStartChar) ) { withinComment = true; return false; }
            else if ( c == '\n' ) { c = ' '; }                       // not within string or comment: replace a new line with a space (white space in multi-line instruction)

            redundantSpaces = (_instructionCharCount > 0) && (c == ' ') && lastCharWasWhiteSpace;
            redundantSemiColon = (c == ';') && lastCharWasSemiColon;
            lastCharWasWhiteSpace = (c == ' ');                     // remember
            lastCharWasSemiColon = (c == ';');
        }

        // less than 3 positions available in buffer: discard character (keep 2 free positions to add optional ';' and for terminating '\0')  
        if ( (_instructionCharCount <= _maxInstructionChars - 3) && !isEndOfFile && !redundantSpaces && !redundantSemiColon && !withinComment ) {
            _instruction [_instructionCharCount] = c;                               // still room: add character
            _instructionCharCount++;
        }
    }

    if ( (_instructionCharCount > 0) && isEndOfFile ) {             // if last instruction before EOF does not contain a semicolon separator at the end, add it 
        if ( _instruction [_instructionCharCount - 1] != ';' ) {
            _instruction [_instructionCharCount] = ';';                               // still room: add character
            _instructionCharCount++;
        }
    }




    bool isInstructionSeparator = (!withinString) && (!withinComment) && (c == ';') && !redundantSemiColon;   // only if before end of file character 
    isInstructionSeparator = isInstructionSeparator || (withinString && (c == '\n'));  // new line sent to parser as well
    bool instructionComplete = isInstructionSeparator || (isEndOfFile && (_instructionCharCount > 0));

    if ( instructionComplete && !_quitCalcAtEOF ) {                                                // terminated by a semicolon if not end of input
        _instruction [_instructionCharCount] = '\0';                            // add string terminator

        if ( requestMachineReset ) {
            _pmyParser->resetMachine( false );                                // prepare for parsing next program( stay in current mode )
            requestMachineReset = false;
        }

        char* pInstruction = _instruction;                                                 // because passed by reference 
        result = _pmyParser->parseInstruction( pInstruction );                                 // parse one instruction (ending with ';' character, if found)
        pErrorPos = pInstruction;                                                      // in case of error
        if ( result != MyParser::result_tokenFound ) { _flushAllUntilEOF = true; }
        _instructionCharCount = 0;
        withinString = false; withinStringEscSequence = false;

        instructionsParsed = true;                                  // instructions found
    }




    if ( isEndOfFile ) {
        if ( instructionsParsed ) {
            int funcNotDefIndex;
            if ( result == MyParser::result_tokenFound ) {
                // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
                if ( _programMode && (!_pmyParser->allExternalFunctionsDefined( funcNotDefIndex )) ) { result = MyParser::result_undefinedFunctionOrArray; }
                if ( _pmyParser->_blockLevel > 0 ) { result = MyParser::result_noBlockEnd; }
                if ( !_programMode ) {

                    // evaluation comes here
                    _pmyParser->prettyPrintInstructions( false );                    // immediate mode and result OK: pretty print input line

                    exec();                                 // execute parsed user statements
                }
            }
            // parsing OK message (program mode only - no message in immediate mode) or error message 
            _pmyParser->printParsingResult( result, funcNotDefIndex, _instruction, _lineCount, pErrorPos );
        }
        else {_pConsole->println(); }                                       // empty line: advance to next line only
        _pConsole->print( "Justina> " ); _isPrompt = true;                 // print new prompt



        bool wasReset = false;      // init
        if ( _programMode ) {               //// waarschijnlijk aan te passen als LOADPROG cmd implemented (-> steeds vanuit immediate mode)
            // end of file: always back to immediate mode
            // do not touch program memory itself: there could be a program in it 
            _programMode = false;

            // if program parsing error: reset machine, because variable storage is not consistent with program 
            if ( result != MyParser::result_tokenFound ) {
                _pmyParser->resetMachine( false );      // message not needed here
                wasReset = true;
            }
        }

        // was in immediate mode
        else if ( instructionsParsed ) {

            // delete alphanumeric constants because they are on the heap. Identifiers must stay avaialble
            _pmyParser->deleteConstStringObjects( _programStorage + PROG_MEM_SIZE );  // always
            *_programStart = '\0';                                      //  current end of program (immediate mode)
        }


        if ( !wasReset ) {
            _pmyParser->parsingStack.deleteList();                      // safety
            _pmyParser->_blockLevel = 0;
            _pmyParser->_extFunctionBlockOpen = false;

            _programStart = _programStorage + PROG_MEM_SIZE;        // already set immediate mode 
            _programSize = _programSize + IMM_MEM_SIZE;
            _programCounter = _programStart;                        // start of 'immediate mode' program area

        }

        instructionsParsed = false;

        _instructionCharCount = 0;
        _lineCount = 0;
        _StarCmdCharCount = 0;
        _flushAllUntilEOF = false;
    }

    return _quitCalcAtEOF;  // and wait for next character
}


