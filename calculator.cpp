#include "myParser.h"

Calculator::Calculator() {
    _programCounter = calculator._programStorage;
    *_programCounter = '\0';                                                 // is currently end of program

    _varNameCount = 0;
    _staticVarCount = 0;
    _localVarCountInFunction = 0;
    _extFunctionCount = 0;

};

int8_t Calculator::processSource( char* const inputLine, char* info, char* pretty, int charsPrettyLine ) {
    myParser.parseSource( inputLine, info, pretty, charsPrettyLine );
}


Calculator calculator;
