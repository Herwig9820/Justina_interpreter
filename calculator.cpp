#include "myParser.h"

// -------------------
// *   constructor   *
// -------------------

Calculator::Calculator( Stream* const pTerminal ) : _pTerminal( pTerminal ) {
    _pTerminal->println( "[calc] Starting calculator..." );
    _callbackFcn = nullptr;
    _pmyParser = new MyParser( this );              // pass the address of this Calculator object to the MyParser constructor

    // init 'machine' (not a complete reset, because this clears heap objects for this calculator object, and there are none)
    _varNameCount = 0;
    _staticVarCount = 0;
    _localVarCountInFunction = 0;
    _extFunctionCount = 0;

    _instructionCharCount = 0;
    _lineCount = 0;                             // taking into account new line after 'load program' command ////
    _flushAllUntilEOF = false;
    _StarCmdCharCount = 0;

    _programMode = false;
    _programStart = _programStorage + PROG_MEM_SIZE;
    _programSize = IMM_MEM_SIZE;
    _programCounter = _programStart;                          // start of 'immediate mode' program area

    *_programStorage = '\0';                                    //  current end of program 
    *_programStart = '\0';                                      //  current end of program (immediate mode)

    _pTerminal->println( "[calc] Ready>" );                  // end of parsing
};


// ---------------------
// *   deconstructor   *
// ---------------------

Calculator::~Calculator() {
    _pTerminal->println( "[calc] Quitting calculator... " );
    _programMode = false;                                   //// te checken of er dan nog iets moet gereset worden
    if ( !_keepInMemory ) {
        delete _pmyParser;
        _callbackFcn = nullptr;
    }
    _pTerminal->println( "[calc] bye" );
};


// ----------------------------
// *   calculator main loop   *
// ----------------------------

void Calculator::setCalcMainLoopCallback( void (*func)(bool& requestQuit) ) {
    // initialize callback function (e.g. to maintain a TCP connection, to implement a heartbeat, ...)
    _callbackFcn = func;
}


// ----------------------------
// *   calculator main loop   *
// ----------------------------

bool Calculator::run() {
    bool quitNow { false };
    char c;
    do {
        if ( _callbackFcn != nullptr ) { _callbackFcn( quitNow ); }
        if ( quitNow ) {
            _pTerminal->println( "[calc] Abort request received..." );
            break;
        }
        if ( _pTerminal->available() > 0 ) {     // if terminal character available for reading
            c = _pTerminal->read();
            quitNow = processCharacter( c );        // process one character
            if ( quitNow ) { break; }               // user gave quit command
        }
    } while ( true );

    return _keepInMemory;
}

// ----------------------------------
// *   process an input character   *
// ----------------------------------

bool Calculator::processCharacter( char c ) {
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

        _pTerminal->println( _programMode ? "[calc] Waiting for program..." : "[calc] Ready>" );
        return false;
    }
    else if ( isParserReset ) {  // temporary
        _programMode = false;
        _pmyParser->resetMachine();
        instructionsParsed = false;

        _instructionCharCount = 0;
        _lineCount = 0;                             // taking into account new line after 'load program' command ////
        _StarCmdCharCount = 0;
        _flushAllUntilEOF = false;

        lastCharWasWhiteSpace = false;
        lastCharWasSemiColon = false;

        withinString = false; withinStringEscSequence = false;
        withinComment = false;

        Serial.println( "(machine reset na manual parser reset)" );
        return false;
    }
    else if ( (c < ' ') && (c != '\n') && (!isEndOfFile) ) { return false; }                  // skip control-chars except new line and EOF character



    if ( !isEndOfFile ) {
        if ( _flushAllUntilEOF ) { return false; }                       // discard characters (after parsing error)

        bool isLeadingSpace = ((_StarCmdCharCount == 0) && (c == ' '));
        if ( c == '\n' ) { _lineCount++; _StarCmdCharCount = 0; }                           // while reading program in input file

        // check for exit command if not in program mode, a printable character (not a leading space) and checking still underway 
        if ( !_programMode && !isLeadingSpace && !(c == '\n') && (_StarCmdCharCount >= 0) ) {
            if ( c == quitCalc [_StarCmdCharCount] ) {
                _StarCmdCharCount++;
                if ( quitCalc [_StarCmdCharCount] == '\0' ) { return true; }         // perfect match: exit calculator
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

    if ( instructionComplete ) {                                                // terminated by a semicolon if not end of input
        _instruction [_instructionCharCount] = '\0';                            // add string terminator

        if ( requestMachineReset ) {
            _pmyParser->resetMachine();                                // prepare for parsing next program( stay in current mode )
            requestMachineReset = false;
            Serial.println( "(machine reset bij start parsen)" );
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
                if ( _programMode && (!_pmyParser->allExternalFunctionsDefined( funcNotDefIndex )) ) { result = MyParser::result_undefinedFunction; }
                if ( _pmyParser->_blockLevel > 0 ) { ; result = MyParser::result_noBlockEnd; }
                if ( !_programMode ) {
                    // evaluation comes here
                    _pmyParser->prettyPrintProgram();                    // immediate mode and result OK: pretty print input line
                    _pTerminal->println( "(hier komt resultaat)" );      // immediate mode: print evaluation result
                }
            }

            // parsing OK message (program mode only) or error message 
            _pmyParser->printParsingResult( result, funcNotDefIndex, _instruction, _lineCount, pErrorPos );
            _pTerminal->println( "[calc] Ready>" );                  // end of parsing
        }

        bool wasReset = false;      // init
        if ( _programMode ) {               //// waarschijnlijk aan te passen als LOADPROG cmd implemented (-> steeds vanuit immediate mode)
            // end of file: always back to immediate mode
            // do not touch program memory itself: there could be a program in it 
            _programMode = false;

            // if program parsing error: reset machine, because variable storage is not consistent with program 
            if ( result != MyParser::result_tokenFound ) {
                _pmyParser->resetMachine();      // message not needed here
                Serial.println( "(Machine reset na parsing error)" );       // program mode parsing only !
                wasReset = true;
            }
        }

        // was in immediate mode
        else if ( instructionsParsed ) {

            // delete alphanumeric constants because they are on the heap. Identifiers must stay avaialble
            _pmyParser->deleteAllAlphanumStrValues( _programStorage + PROG_MEM_SIZE );  // always
            *_programStorage = '\0';                                    //  current end of program 
            *_programStart = '\0';                                      //  current end of program (immediate mode)
        }


        if ( !wasReset ) {
            _pmyParser->myStack.deleteList();                      // safety
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

    return false;  // and wait for next character
}
