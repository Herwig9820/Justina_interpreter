#include "myParser.h"

extern Stream* pTerminal;

Calculator::Calculator() {

    _instructionCharCount = 0;

    // init 'machine' (not a complete reset, because this clears heap objects for this calculator object, and there are none)
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
    static MyParser::parseTokenResult_type result {};
    static bool requestMachineReset { false };
    static bool withinStringEscSequence { false };
    static bool instructionsParsed { false };
    static bool lastCharWasWhiteSpace { false };
    static bool lastCharWasSemiColon { false };

    static bool withinComment { false };
    static bool withinString { false };

    static char* pErrorPos {};
    static int lineCount { 0 };                             // taking into account new line after 'load program' command ////


    char EOFchar = 0x1A;
    char commentStartChar = '$';

    bool redundantSpaces = false;                                       // init
    bool redundantSemiColon = false;

    if ( !_programMode && (c == '\n') ) { c = EOFchar; }
    bool isEndOfFile = (c == EOFchar);                                      // end of input: EOF in program mode, LF or EOF in immediate mode
    bool isProgramCtrl = (c == 2);                                   // switch between program and immediate mode ?
    bool isParserReset = (c == 3);                                     // reset parser ?
    bool isCommentStartChar = (c == '$');                               // character can also be part of comment


    if ( isProgramCtrl ) {
        // do not touch program memory itself: there could be a program in it 
        _programMode = !_programMode;
        _programStart = _programStorage + (_programMode ? 0 : PROG_MEM_SIZE);
        _programSize = _programSize + (_programMode ? PROG_MEM_SIZE : IMM_MEM_SIZE);
        _programCounter = _programStart;                          // start of 'immediate mode' program area

        requestMachineReset = _programMode;                         // reset machine when parsing starts, not earlier (in case there is a program in memory)

        instructionsParsed = false;
        lastCharWasWhiteSpace = false;
        lastCharWasSemiColon = false;
        lineCount = 0;
        _instructionCharCount = 0;
        _flushAllUntilEOF = false;

        withinString = false; withinStringEscSequence = false;
        withinComment = false;

        pTerminal->println( _programMode ? "Waiting for program..." : "Ready >" );
        return;
    }
    else if ( isParserReset ) {  // temporary
        _programMode = false;
        myParser.resetMachine();

        instructionsParsed = false;
        lastCharWasWhiteSpace = false;
        lastCharWasSemiColon = false;
        lineCount = 0;
        _instructionCharCount = 0;
        _flushAllUntilEOF = false;

        withinString = false; withinStringEscSequence = false;
        withinComment = false;

        Serial.println( "(machine reset na manual parser reset)" );
        return;
    }
    else if ( (c < ' ') && (c != '\n') && (!isEndOfFile) ) { return; }                  // skip control-chars except new line and EOF character



    if ( !isEndOfFile ) {
        if ( _flushAllUntilEOF ) { return; }                       // discard characters (after parsing error)

        if ( c == '\n' ) { lineCount++; }                           // while reading program in input file

        // currently within a string or within a comment ?
        if ( withinString ) {
            if ( c == '\\' ) { withinStringEscSequence = !withinStringEscSequence; }
            else if ( c == '\"' ) { withinString = withinStringEscSequence; withinStringEscSequence = false; }
            else { withinStringEscSequence = false; }                 // any other character within string
            lastCharWasWhiteSpace = false;
            lastCharWasSemiColon = false;

        }
        else if ( withinComment ) {
            if ( c == '\n' ) { withinComment = false; return; }                // comment stops at end of line
        }
        else {                                                                                              // not within a string
            bool leadingWhiteSpace = (((c == ' ') || (c == '\n')) && (_instructionCharCount == 0));
            if ( leadingWhiteSpace ) { return; };                        // but always process end of file character

            if ( !withinComment && (c == '\"') ) { withinString = true; }
            else if ( !withinString && (c == commentStartChar) ) { withinComment = true; return; }
            else if ( c == '\n' ) { c = ' '; }                       // not within string or comment: replace a new line with a space (white space in multi-line instruction)

            redundantSpaces = (_instructionCharCount > 0) && (c == ' ') && lastCharWasWhiteSpace;
            redundantSemiColon = (c == ';') && lastCharWasSemiColon;
            lastCharWasWhiteSpace = (c == ' ');                     // remember
            lastCharWasSemiColon = (c == ';');
        }

        instructionsParsed = true;                                  // instructions found
        if ( requestMachineReset ) {
            myParser.resetMachine();                                // prepare for parsing next program( stay in current mode )
            requestMachineReset = false;
            Serial.println( "(machine reset bij start parsen)" );
        }

        // less than 3 positions available in buffer: discard character (keep 2 free positions to add optional ';' and for terminating '\0')  
        if ( (_instructionCharCount <= _maxInstructionChars - 3) && !isEndOfFile && !redundantSpaces && !redundantSemiColon && !withinComment ) {
            _instruction [_instructionCharCount] = c;                               // still room: add character
            _instructionCharCount++;
        }
    }

    if (( _instructionCharCount > 0 ) && isEndOfFile) {             // if last instruction before EOF does not contain a semicolon separator at the end, add it 
        if(_instruction [_instructionCharCount-1] != ';'){
            _instruction [_instructionCharCount] = ';';                               // still room: add character
            _instructionCharCount++;
        }
    }

    bool isInstructionSeparator = (!withinString) && (!withinComment) && (c == ';') && !redundantSemiColon;   // only if before end of file character 
    isInstructionSeparator = isInstructionSeparator || (withinString && (c == '\n'));  // new line sent to parser as well
    bool instructionComplete = isInstructionSeparator || (isEndOfFile && (_instructionCharCount > 0));


    if ( instructionComplete ) {                                                // terminated by a semicolon if not end of input
        _instruction [_instructionCharCount] = '\0';                            // add string terminator

        char* pInstruction = _instruction;                                                 // because passed by reference 
        result = myParser.parseInstruction( pInstruction );                                 // parse one instruction (ending with ';' character, if found)
        pErrorPos = pInstruction;                                                      // in case of error
        if ( result != MyParser::result_tokenFound ) { _flushAllUntilEOF = true; }
        _instructionCharCount = 0;
        withinString = false; withinStringEscSequence = false;

    }




    if ( isEndOfFile ) {
        if ( instructionsParsed ) {
            int funcNotDefIndex;
            if ( result == MyParser::result_tokenFound ) {
                // checks at the end of parsing: any undefined functions (program mode only) ?  any open blocks ?
                if ( calculator._programMode && (!myParser.allExternalFunctionsDefined( funcNotDefIndex )) ) { result = MyParser::result_undefinedFunction; }
                if ( myParser._blockLevel > 0 ) { ; result = MyParser::result_noBlockEnd; }
            }

            myParser.prettyPrintProgram();                    // append pretty printed instruction to string
            myParser.printParsingResult( result, funcNotDefIndex, _instruction, lineCount, pErrorPos );
        }

        bool wasReset = false;      // init
        if ( _programMode ) {               //// waarschijnlijk aan te passen als LOADPROG cmd implemented (-> steeds vanuit immediate mode)
            // end of file: always back to immediate mode
            // do not touch program memory itself: there could be a program in it 
            _programMode = false;

            // if program parsing error: reset machine, because variable storage is not consistent with program 
            if ( result != MyParser::result_tokenFound ) {
                myParser.resetMachine();      // message not needed here
                Serial.println( "(Machine reset na parsing error)" );       // program mode parsing only !
                wasReset = true;
            }
            pTerminal->println( "Ready >" );                  // end of parsing

        }
        // was in immediate mode
        else if ( instructionsParsed ) {

            if ( result == MyParser::result_tokenFound ) {
                pTerminal->println( "------------------ (hier komt evaluatie) --------------------------" );
            }
            // delete alphanumeric constants because they are on the heap. Identifiers must stay avaialble
            myParser.deleteAllAlphanumStrValues( calculator._programStorage + calculator.PROG_MEM_SIZE );  // always
            *_programStorage = '\0';                                    //  current end of program 
            *_programStart = '\0';                                      //  current end of program (immediate mode)
            pTerminal->println( "Ready >" );                  // end of parsing
        }


        if ( !wasReset ) {
            myParser.myStack.deleteList();                      // safety
            myParser._blockLevel = 0;
            myParser._extFunctionBlockOpen = false;

            _programStart = _programStorage + PROG_MEM_SIZE;        // already set immediate mode 
            _programSize = _programSize + IMM_MEM_SIZE;
            _programCounter = _programStart;                        // start of 'immediate mode' program area

        }

        instructionsParsed = false;
        lineCount = 0;
        _instructionCharCount = 0;
        _flushAllUntilEOF = false;


    }
}

Calculator calculator;
