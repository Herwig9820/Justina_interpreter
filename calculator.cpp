#include "myParser.h"

extern Stream* pTerminal;

Calculator::Calculator() {

    _instruction [0] = '\0';                // empty input
    _instructionCharCount = 0;
    _pretty [0] = '\0';                     // empty output
    _parsingInfo [0] = '\0';                // empty parsing info

    // init 'machine' (no call of 'resetMachine', because this clears heap objects for this calculator object, and there are none)
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

bool Calculator::processCharacter( char c ) {
    // process character
    bool isEOF { false };
    char EOFchar = 0x1A;

    bool isProgramCtrl = (c == 2);                                   // switch between program and immediate mode ?
    bool isParserReset = (c == 3);                                     // reset parser ?
    bool isInstructionSeparator = (c == ';');

    if ( isProgramCtrl ) {
        _instruction [0] = '\0';
        _instructionCharCount = 0;
        _pretty [0] = '\0';
        _parsingInfo [0] = '\0';

        _programMode = !_programMode;
        _programStart = _programStorage + (_programMode ? 0 : PROG_MEM_SIZE);
        _programSize = _programSize + (_programMode ? PROG_MEM_SIZE : IMM_MEM_SIZE);
        _programCounter = _programStart;                          // start of 'immediate mode' program area

        Serial.println( _programMode ? "program mode " : "immediate mode" );
        return true;
    }
    else if ( isParserReset ) {
        _instruction [0] = '\0';
        _instructionCharCount = 0;
        _pretty [0] = '\0';
        _parsingInfo [0] = '\0';

        myParser.resetMachine();
        Serial.println( "machine reset" );
        return true;
    }
    else if ( (c < ' ') && (c != '\n') && (c != EOFchar) ) { return false; }                  // skip control-chars except new line and EOF character

    // end of input detected ? (EOF in program mode, LF in immediate mode)
    bool inputTerminated = (isProgramCtrl && (c == EOFchar)) || ((!isProgramCtrl) && (c == '\n'));
    // add character to instruction buffer (if still romm)
    if ( (!inputTerminated) && (_instructionCharCount <= _maxInstructionChars - 2) ) {       // at least two positions available: fill the first position
        _instruction [_instructionCharCount] = c;                                             // at least last position available for terminating \n
        _instructionCharCount++;
    }

    bool instructionComplete = ((c == isInstructionSeparator) || inputTerminated);
    if ( instructionComplete ) {
        _instruction [_instructionCharCount] = '\0';
        uint8_t  result = myParser.parseSource( _instruction, _parsingInfo, _pretty, _maxCharsPretty );

        pTerminal->println( "--------------------------------------\r\npretty: " );
        if ( strcmp( _pretty, "" ) ) { pTerminal->println( _pretty ); }
        pTerminal->println( _parsingInfo );

        _instruction [0] = '\0';
        _instructionCharCount = 0;
        _pretty [0] = '\0';
        _parsingInfo [0] = '\0';
    }

    if ( inputTerminated ) {
        if (!_programMode) {myParser.deleteAllAlphanumStrValues( calculator._programStorage + calculator.PROG_MEM_SIZE );}

        _programMode = false; // always exit program mode when program is parsed 
        _programStart = _programStorage + PROG_MEM_SIZE;
        _programSize = IMM_MEM_SIZE;
        _programCounter = _programStart;                          // start of 'immediate mode' program area

        *_programStart = '\0';                                      //  current end of program (immediate mode)

    }
    return true;    //// return value wordt niet gebruikt
};


Calculator calculator;
