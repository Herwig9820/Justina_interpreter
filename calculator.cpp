#include "myParser.h"

extern Stream* pTerminal;

Calculator::Calculator() {

    _instructionCharCount = 0;

    // init 'machine' (not a complete reset, because this clears heap objects for this calculator object, and there are none)
    _instructionCharCount = 0;
    _flushAllUntilEOF = false;

    _varNameCount = 0;
    _staticVarCount = 0;
    _localVarCountInFunction = 0;
    _extFunctionCount = 0;

    _programMode = false;
    _programStart = _programStorage + PROG_MEM_SIZE;
    _programSize = IMM_MEM_SIZE;
    _programCounter = _programStart;                          // start of 'immediate mode' program area

    *_programStorage = '\0';                                    //  current end of program 
    *_programStart = '\0';                                      //  current end of program (immediate mode)
};


// ----------------------------------
// *   process an input character   *
// ----------------------------------

void Calculator::processCharacter( char c ) {
    // process character
    static bool withinStringEscSequence { false };
    static char* pErrorPos {};
    static MyParser::parseTokenResult_type result {};
    static bool requestMachineReset { false };
    static bool instructionsParsed { false };
    static int lineCount {0};
    static bool lastCharWasWhiteSpace{false};

    char EOFchar = 0x1A;
    if ( !_programMode && (c == '\n') ) { c = EOFchar; }
    bool endOfFileChar = (c == EOFchar);                                      // end of input: EOF in program mode, LF or EOF in immediate mode

    bool isProgramCtrl = (c == 2);                                   // switch between program and immediate mode ?
    bool isParserReset = (c == 3);                                     // reset parser ?

    if ( isProgramCtrl ) {
        // do not touch program memory itself: there could be a program in it 
        _programMode = !_programMode;
        _programStart = _programStorage + (_programMode ? 0 : PROG_MEM_SIZE);
        _programSize = _programSize + (_programMode ? PROG_MEM_SIZE : IMM_MEM_SIZE);
        _programCounter = _programStart;                          // start of 'immediate mode' program area

        requestMachineReset = _programMode;                         // reset machine when parsing starts, not earlier (in case there is a program in memory)

        instructionsParsed = false;
        lastCharWasWhiteSpace = false;
        lineCount = _programMode ? 0 : -1;
        _instructionCharCount = 0;
        _flushAllUntilEOF = false;

        pTerminal->println( _programMode ? "+++ program mode +++" : "+++ immediate mode +++" );
        return;
    }
    else if ( isParserReset ) {  // temporary
        _programMode = false;
        myParser.resetMachine();

        instructionsParsed = false;
        lastCharWasWhiteSpace = false;
        lineCount = _programMode ? 0:-1;
        _instructionCharCount = 0;
        _flushAllUntilEOF = false;

        pTerminal->println( "+++ machine reset +++" );
        return;
    }
    else if ( (c < ' ') && (c != '\n') && (! endOfFileChar) ) { return; }                  // skip control-chars except new line and EOF character


    if ( !endOfFileChar ) {
        if ( _flushAllUntilEOF ) { return; }                       // discard characters (after parsing error)

        if ( c == '\n' ) { lineCount++; }                           // input file

        bool leadingWhiteSpace = (((c == ' ') || (c == '\n')) && (_instructionCharCount == 0));
        if ( leadingWhiteSpace ) { return; };                        // but always process end of file character

        if ( requestMachineReset ) {
            myParser.resetMachine();                                // prepare for parsing next program( stay in current mode mode )
            requestMachineReset = false;
            pTerminal->println( "+++ machine reset +++" );
        }

        bool redundantSpaces = false;;

        // currently within a string ?
        if ( _instructionCharCount == 0 ) { _withinString = false; withinStringEscSequence = false; }          // a string cannot be mlulti-line
        if ( _withinString ) {
            if ( c == '\\' ) { withinStringEscSequence = !withinStringEscSequence; }
            else if ( c == '\"' ) { _withinString = withinStringEscSequence; withinStringEscSequence = false; }
            else { withinStringEscSequence = false; }                 // any other character within string
            lastCharWasWhiteSpace = false;
        }
        else {                                                                                              // not within a string
            if ( c == '\"' ) { _withinString = true; }
            else if ( c == '\n' ) { c = ' '; }                       // not within string: replace a new line with a space (white space in multi-line instruction)
            redundantSpaces = (_instructionCharCount > 0) && (c==' ') && lastCharWasWhiteSpace;
            lastCharWasWhiteSpace = (c == ' ');                     // remember
        }

        // less than 2 positions available in buffer: discard character (last position will be for terminating '\n' then)  
        if ( (_instructionCharCount <= _maxInstructionChars - 2) && !endOfFileChar && !redundantSpaces) {
            _instruction [_instructionCharCount] = c;                               // still room: add character
            _instructionCharCount++;
        }
    }

    bool isInstructionSeparator = (!_withinString) && (c == ';');   // only if before end of file character 
    bool instructionComplete = isInstructionSeparator || (endOfFileChar && (_instructionCharCount > 0));

    if ( instructionComplete ) {                                                // terminated by a semicolon if not end of input
        _instruction [_instructionCharCount] = '\0';                            // add string terminator
        result = myParser.parseSource( _instruction, pErrorPos );
        if ( result != MyParser::result_tokenFound ) { _flushAllUntilEOF = true; }
        instructionsParsed = true;
        _instructionCharCount = 0;
    }

    if ( endOfFileChar ) {
        if ( instructionsParsed ) {
            int funcNotDefIndex;
            if ( result == MyParser::result_tokenFound ) {
                // checks at the end of parsing
                if ( calculator._programMode && (!myParser.allExternalFunctionsDefined( funcNotDefIndex )) ) { result = MyParser::result_undefinedFunction_ProgMode; }
                if ( myParser._blockLevel > 0 ) { result = MyParser::result_noBlockEnd; }
            }

            myParser.prettyPrintProgram();                    // append pretty printed instruction to string
            myParser.printParsingResult( result, funcNotDefIndex, _instruction, lineCount, pErrorPos );      
        }


        bool wasReset = false;
        if ( _programMode ) {
            // end of file: always back to immediate mode
            // do not touch program memory itself: there could be a program in it 
            _programMode = false;
            pTerminal->println( "+++ immediate mode +++" );

            if ( result != MyParser::result_tokenFound ) {
                myParser.resetMachine();      // message not needed here
                wasReset = true;
            }
        }
        // was in immediate mode
        else {
            if ( result != MyParser::result_tokenFound ) {
                *_programStorage = '\0';                                    //  current end of program 
                *_programStart = '\0';                                      //  current end of program (immediate mode)
            }
            else {
                pTerminal->println( "********** evaluation phase **********" );
            }
            // delete alphanumeric constants because they are in the heap
            myParser.deleteAllAlphanumStrValues( calculator._programStorage + calculator.PROG_MEM_SIZE );  // always
        }


        if ( !wasReset ) {
            myParser.myStack.deleteList();                      // safety
            myParser._blockLevel = 0;
            myParser._extFunctionBlockOpen = false;

            _programStart = _programStorage + PROG_MEM_SIZE;        // already set immediate mode 
            _programSize = _programSize + IMM_MEM_SIZE;
            _programCounter = _programStart;                          // start of 'immediate mode' program area
        
        }

        instructionsParsed = false;
        lineCount = 0;
        _instructionCharCount = 0;
        _flushAllUntilEOF = false;
    }
}

Calculator calculator;
