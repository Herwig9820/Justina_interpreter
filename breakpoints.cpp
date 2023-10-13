/************************************************************************************************************
*    Justina interpreter library for Arduino boards with 32 bit SAMD microconrollers                        *
*                                                                                                           *
*    Tested with Nano 33 IoT and Arduino RP2040                                                             *
*                                                                                                           *
*    Version:    v1.01 - 12/07/2023                                                                         *
*    Author:     Herwig Taveirne, 2021-2023                                                                 *
*                                                                                                           *
*    Justina is an interpreter which does NOT require you to use an IDE to write and compile programs.      *
*    Programs are written on the PC using any text processor and transferred to the Arduino using any       *
*    Serial or TCP Terminal program capable of sending files.                                               *
*    Justina can store and retrieve programs and other data on an SD card as well.                          *
*                                                                                                           *
*    See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                           *
*    This program is free software: you can redistribute it and/or modify                                   *
*    it under the terms of the GNU General Public License as published by                                   *
*    the Free Software Foundation, either version 3 of the License, or                                      *
*    (at your option) any later version.                                                                    *
*                                                                                                           *
*    This program is distributed in the hope that it will be useful,                                        *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of                                         *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                           *
*    GNU General Public License for more details.                                                           *
*                                                                                                           *
*    You should have received a copy of the GNU General Public License                                      *
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.                                  *
************************************************************************************************************/


#include "Justina.h"


// *****************************************************************
// ***            class Breakpoints - implementation             ***
// *****************************************************************

// -----------------------------------------------------------------------------------------------------------------
// *   breakpoints: store info during program parsing to allow setting breakpoints during execution (debug mode)   *
// -----------------------------------------------------------------------------------------------------------------

Justina_interpreter::parsingResult_type Breakpoints::addBreakpointData(const char semiColonBPallowed_token, bool& parsedStatementStartsOnNewLine, bool& parsedStatementStartLinesAdjacent,
    long& statementStartsAtLine, long& parsedStatementStartsAtLine, long& BPstartLine, long& BPendLine, long& BPpreviousEndLine) {

    static long gapLineRange{ 0 };

    // if the statement yet to parse starts on the same line as the previous statement, it doesn't start on a new line
    parsedStatementStartsOnNewLine = (statementStartsAtLine != parsedStatementStartsAtLine);

    if (!(_pJustina->_programMode && (parsedStatementStartsOnNewLine))) { return Justina_interpreter::parsingResult_type::result_parsing_OK; }          // nothing to do

    // 1. for each statement starting at the beginning of a line: alter the 'end of statement' token of the...
    //    ...previously parsed (preceding) statement to indicate 'setting breakpoint allowed' for the statement being parsed.
    // ----------------------------------------------------------------------------------------------------------------------

    if (_pJustina->_programCounter - 1 >= _pJustina->_programStorage) {
        *(_pJustina->_programCounter - 1) = semiColonBPallowed_token;
    }

    // 2. maintain source line ranges having statements starting at the beginning of a line
    // -------------------------------------------------------------------------------------

    // start of previous source line also contained start of a parsed statement ? Also rectify for beginning of file

    parsedStatementStartLinesAdjacent = (parsedStatementStartsAtLine == statementStartsAtLine - 1) && (statementStartsAtLine != 1);
    parsedStatementStartsAtLine = statementStartsAtLine;

    // still in a source line range with a new statement starting at the beginning of each line (disregarding white space) 
    if (parsedStatementStartLinesAdjacent) {
        BPendLine = parsedStatementStartsAtLine;
    }

    // first line of an 'adjacent source line range': time to calculate length of last gap range and previous 'adjacent' source line range
    else {
        long adjacentLineRange = (BPendLine == 0) ? 0 : BPendLine - BPstartLine + 1;                       // PREVIOUS adjacent range

        // if valid, store in _BPlineRangeStorage array
        if (adjacentLineRange != 0) {            // skip first (invalid) gap/adjacent line range pair (produced at beginning of file)
            Justina_interpreter::parsingResult_type result = addSourceLineRangePair(gapLineRange, adjacentLineRange);
            if (result != Justina_interpreter::result_parsing_OK) { return result; }
        }
        BPpreviousEndLine = BPendLine;

        gapLineRange = parsedStatementStartsAtLine - BPendLine - 1;            // GAP range between previous and this start of new gap range 
        BPstartLine = parsedStatementStartsAtLine; BPendLine = BPstartLine;
    }

    return Justina_interpreter::result_parsing_OK;
}


// ---------------------------------------------------------------------------------------------------------------------------------
// *   store a pair of program file line range lengths: gap line range length and adjacent line range length                     ***
// *   - gap source line range length: number of source file lines between previous and this adjacent line range                 ***
// *   - 'adjacent' source line range length: number of adjacent source file lines with a new statement starting at line start   *** 
// *   purpose: keep track of source lines where setting a breakpoint (debug mode) is allowed, in a relatively dense format      *** 
// ---------------------------------------------------------------------------------------------------------------------------------

Justina_interpreter::parsingResult_type Breakpoints::addSourceLineRangePair(long gapLineRange, long adjacentLineRange) {

    // 'gap' source line range length requires 3 bits or less, 'adjacent' source line range length requires 4 bits or less: use 1 byte (1 flag bit + 3 + 4 bits = 8 bits)
    // byte 0 -> bit 0 = 0 (flag), bits 3..1: 'gap' source line range length (value range 0..7), bits 7..4: 'adjacent' source line range length (value range 0-15)
    if ((gapLineRange < 0x08) && (adjacentLineRange < 0x10)) {
        if (_BPlineRangeStorageUsed >= _BPLineRangeMemorySize - 1) { return Justina_interpreter::result_BPlineTableMemoryFull; } // memory full
        _BPlineRangeStorage[_BPlineRangeStorageUsed++] = (adjacentLineRange << 4) | (gapLineRange << 1);        // flag bit 0 = 0: 1 byte used
    }

    // 'gap' source line range and 'adjacent' source line range lengths both require 7 bits (value range 0 - 127): use 2 bytes (2 flag bits + 2 x 7 bits = 16 bits)
    // - byte 0-> bits 1..0 = 0b01 (flag),  bits b7..2: 6 LSB's 'gap' source line range length
    // - byte 1-> bit 0: MSB 'gap' source line range length, bits 7..1: 7 bits 'adjacent' source line range length
    else if ((gapLineRange < 0x80) && (adjacentLineRange < 0x80)) {
        if (_BPlineRangeStorageUsed >= _BPLineRangeMemorySize - 2) { return Justina_interpreter::result_BPlineTableMemoryFull; } // memory full
        _BPlineRangeStorage[_BPlineRangeStorageUsed++] = (gapLineRange << 2) | 0x01;                            // flag bits 1..0 = 0b01: 2 bytes used
        _BPlineRangeStorage[_BPlineRangeStorageUsed++] = (adjacentLineRange << 1) | (char)(gapLineRange >= 0x40);
    }

    // 'gap' source line range and 'adjacent' source line range lengths both require 11 bits or less (value range 0 - 2047): use 3 bytes (2 flag bits + 2 x 11 bits = 24 bits)
    // - byte 0-> bits 1..0 = 0b11 (flag),  bits b7..2: 6 LSB's 'gap' source line range length
    // - byte 1-> bits 4..0: 5 MSB's 'gap' source line range length, bits 7..5: 3 LSB's 'adjacent' source line range length
    // - byte 2-> bits 7..0: 7 MSB's 'adjacent' source line range length
    else if ((gapLineRange < 0x800) && (adjacentLineRange < 0x800)) {
        if (_BPlineRangeStorageUsed >= _BPLineRangeMemorySize - 3) { return Justina_interpreter::result_BPlineTableMemoryFull; } // memory full
        uint32_t temp = ((gapLineRange << 2) & ~((0b01 << 2) - 1)) | ((adjacentLineRange << 13) & ~((0b01 << 13) - 1)) | 0x03;
        memcpy(_BPlineRangeStorage + _BPlineRangeStorageUsed, &temp, 3);
        _BPlineRangeStorageUsed += 3;
    }
    else { return Justina_interpreter::result_BPlineRangeTooLong; } // gap range or adjacent source line range length too long

    return Justina_interpreter::result_parsing_OK;
}


// ------------------------------
// *   set pointer to Justina   *
// ------------------------------

void Breakpoints::setJustinaRef(Justina_interpreter* pJustina) {
    _pJustina = pJustina;
}


// --------------------------------------------
// *   adapt a breakpoint for a source line   *
// --------------------------------------------

Justina_interpreter::execResult_type Breakpoints::maintainBPdata(long breakpointLine, char actionCmdCode, const char* viewString, long hitCount, const char* triggerString) {

    // 1. find source line sequence number (base 0) 
    // --------------------------------------------

    // note: line sequence number = line index in the set of source lines having a statement STARTING AT THE START of the source line (discarding spaces)
    long lineSequenceNum = BPgetsourceLineSequenceNumber(breakpointLine);
    if (lineSequenceNum == -1) { return  Justina_interpreter::result_BP_notAllowedForSourceLine; }                // not a valid source line (not within source line range or doesn't start with a Justina statement)


    // 2. find parsed program statement and current breakpoint state (set or 'allowed') for source line; if setBP or clearBP, adapt in program memory
    // ----------------------------------------------------------------------------------------------------------------------------------------------

    // parsed statements for statements STARTING AT THE START of a source line: 
    // the semicolon step (statement separator) preceding these parsed statements is altered to indicate that a breakpoint is either set or allowed for this statement

    char* pProgramStep{ nullptr };
    bool BPwasSetInProgMem{};
    bool doSet = (actionCmdCode == Justina_interpreter::cmdcod_setBP);
    bool doClear = (actionCmdCode == Justina_interpreter::cmdcod_clearBP);
    bool doEnable = (actionCmdCode == Justina_interpreter::cmdcod_enableBP);
    bool doDisable = (actionCmdCode == Justina_interpreter::cmdcod_disableBP);

    Justina_interpreter::execResult_type execResult = progMem_getSetClearBP(lineSequenceNum, pProgramStep, BPwasSetInProgMem, doSet, doClear);
    if (execResult != Justina_interpreter::result_execOK) { return execResult; }
    if (!BPwasSetInProgMem && !doSet) { return doClear ? Justina_interpreter::result_execOK : Justina_interpreter::result_BP_wasNotSet; }  // if BP not yet set, nothing to do

    // 3. Maintain breakpoint settings in breakpoint data table, for all breakpoints currently set  
    // -------------------------------------------------------------------------------------------

    // the count of source lines with a corresponding line sequence number is identical to the count of parsed statements with an altered preceding semicolon token (altered to indicate...
    // ...that a breakpoint is either set or allowed for the parsed statement).
    // in other words, for each source line with a valid line sequence number, there is exactly one parsed statement where a breakpoint is either set or allowed, and vice versa (1-to-1). 

    execResult = maintainBreakpointTable(breakpointLine, pProgramStep, BPwasSetInProgMem, doSet, doClear, doEnable, doDisable, viewString, hitCount, triggerString);

    return execResult;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------
// *   return the sequence number of a source line in the set of source lines with statements STARTING at the START of that line (discarding spaces).    *
// *   if the source line does not contain a statement STARTING at the START of that line (discarding spaces), return -1                                 * 
// -------------------------------------------------------------------------------------------------------------------------------------------------------

long Breakpoints::BPgetsourceLineSequenceNumber(long BPsourceLine) {

    long breakpointSourceLineIndex{ 0 };
    if (BPsourceLine <= 0) { return -1; }                           // signal range error

    int i = 0;
    long BPpreviousEndLine{ 0 };                        // introduce offset 1 here
    while (i < _BPlineRangeStorageUsed) {
        long gapLineRange{}, adjacentLineRange{};

        if (!(_BPlineRangeStorage[i] & 0x01)) {       // gap and adjacent source line ranges stored in one byte
            gapLineRange = (((uint32_t)_BPlineRangeStorage[i]) >> 1) & 0x7;                 // 3 bits long
            adjacentLineRange = (((uint32_t)_BPlineRangeStorage[i]) >> 4) & 0xF;            // 4 bits long
            i++;
        }

        else if ((_BPlineRangeStorage[i] & 0x11) == 0x01) {       // gap and adjacent source line ranges stored in two bytes
            uint32_t temp{};
            memcpy(&temp, _BPlineRangeStorage + i, 2);
            gapLineRange = (temp >> 2) & 0x7F;                                              // each 7 bits long
            adjacentLineRange = (temp >> 9) & 0x7F;
            i += 2;
        }

        else if ((_BPlineRangeStorage[i] & 0x11) == 0x11) {       // gap and adjacent source line ranges stored in three bytes
            uint32_t temp{};
            memcpy(&temp, _BPlineRangeStorage + i, 3);
            gapLineRange = (temp >> 2) & 0x7FF;                                             // each 11 bits long
            adjacentLineRange = (temp >> 13) & 0x7FF;
            i += 3;
        }
        long BPstartLine = BPpreviousEndLine + gapLineRange + 1;
        long BPendLine = BPstartLine + adjacentLineRange - 1;
        BPpreviousEndLine = BPendLine;
        if ((BPsourceLine >= BPstartLine) && (BPsourceLine <= BPendLine)) { return breakpointSourceLineIndex += (BPsourceLine - BPstartLine); }                // breakpoint index: base 0
        breakpointSourceLineIndex += BPendLine - BPstartLine + 1;
    }

    return -1;             // signal range error
}


// --------------------------------------------------------------------------------------------------------------------------------------
// *  find program step and current breakpoint state (set or 'allowed') for source line; if setBP or clearBP, adapt in program memory   *
// --------------------------------------------------------------------------------------------------------------------------------------

Justina_interpreter::execResult_type Breakpoints::progMem_getSetClearBP(long lineSequenceNum, char*& pProgramStep, bool& BPwasSet, bool doSet, bool doClear) {

    // 1. find parsed statement corresponding to source line sequence number
    // ---------------------------------------------------------------------

    // NOTE: there is a 1-TO-1 relationship between each source line with a valid line sequence number AND a parsed statements

    pProgramStep = _pJustina->_programStorage;
    long i{ 0 };
    int matchedCriteriumNumber{};
    int matchedSemiColonTokenIndex{ 0 };

    if (lineSequenceNum == 0) { return Justina_interpreter::result_BP_statementIsNonExecutable; }         // first statement is not preceded by a semicolon (statement separator)
    while (i < lineSequenceNum) {                                               // parsed statement corresponding to line sequence number (and source line statement) has been found (always a matching entry): exit          
        // find next semicolon token. It flags whether a breakpoint is allowed for the NEXT statement (pending further tests)   
        _pJustina->findTokenStep(pProgramStep, true, Justina_interpreter::tok_isTerminalGroup1, Justina_interpreter::termcod_semicolon, 
            Justina_interpreter::termcod_semicolon_BPset, Justina_interpreter::termcod_semicolon_BPallowed, &matchedCriteriumNumber, &matchedSemiColonTokenIndex);    // always match
        if ((matchedCriteriumNumber >= 2)) { i++; }                               // if parsed statement can receive a breakpoint (or breakpoint is set already), increase loop counter
    }

    // 2. in program memory, get / [set / clear] breakpoint for parsed statement by altering preceding statement separator      
    // -------------------------------------------------------------------------------------------------------------------
                                                                                                                          // breakpoint allowed or already set based on statement position in source file 
    int statementTokenType = _pJustina->jumpTokens(1, pProgramStep);                                                    // first character of first token of next statement (following the 'flagged' semicolon token)
    // if a command, check that setting a breakpoint is allowed by the command attributes
    if ((statementTokenType & 0x0F) == Justina_interpreter::tok_isReservedWord) {                                                            // statement is a command ? check whether it's executable
        int resWordIndex = ((Justina_interpreter::TokenIsResWord*)pProgramStep)->tokenIndex;
        if (Justina_interpreter::_resWords[resWordIndex].restrictions & Justina_interpreter::cmd_skipDuringExec) { return Justina_interpreter::result_BP_statementIsNonExecutable; }     // because not executable
    }
    BPwasSet = (((Justina_interpreter::TokenIsTerminal*)(pProgramStep - 1))->tokenTypeAndIndex == _pJustina->_semicolonBPset_token);             // if set, an entry is present in _breakpointData
    if (doSet || doClear) { ((Justina_interpreter::TokenIsTerminal*)(pProgramStep - 1))->tokenTypeAndIndex = doSet ? _pJustina->_semicolonBPset_token : _pJustina->_semicolonBPallowed_token; }                                 // flag 'breakpoint set'

    return Justina_interpreter::result_execOK;
}


// ----------------------------------------------------------------------
// *   Maintain breakpoint settings for all breakpoints currently set   *
// ----------------------------------------------------------------------

Justina_interpreter::execResult_type Breakpoints::maintainBreakpointTable(long sourceLine, char* pProgramStep, bool BPwasSet, bool doSet, bool doClear, bool doEnable, bool doDisable,
    const char* viewString, long hitCount, const char* triggerString) {

    constexpr int maxBPentries = sizeof(_breakpointData) / sizeof(_breakpointData[0]);

    int entry{ 0 };
    bool doInsertNewBP{ false };

    if (BPwasSet) {
        // if BP was set, all actions are allowed. Find breakpoint entry in breakpoint data array
        for (entry = 0; entry < maxBPentries; entry++) {
            if ((_breakpointData[entry].hasBPdata == 0b1) && (_breakpointData[entry].pProgramStep == pProgramStep)) { break; }       // always match 
        }

        if (doSet) { _breakpointData[entry].BPenabled = 0b01; }             // 'has data' attribute is set already, init 'enabled' flag
        // clear ,enable, disable: if BP was not set, then control returned to caller already
        if (doClear) { _breakpointData[entry].hasBPdata = 0b0; _breakpointsUsed--; }       // make table entry available again     
        else if (doEnable) { _breakpointData[entry].BPenabled = 0b1; }
        else if (doDisable) { _breakpointData[entry].BPenabled = 0b0; }
    }

    else {      // BP was not set: action is 'set BP'. Create breakpoint entry in breakpoint data array
        if (_breakpointsUsed == maxBPentries) { return Justina_interpreter::result_BP_maxBPentriesReached; }
        entry = _breakpointsUsed;                                    // first free entry 
        _breakpointData[entry].hasBPdata = 0b1;
        _breakpointData[entry].BPenabled = 0b1;
        _breakpointData[entry].sourceLine = sourceLine;
        _breakpointData[entry].pProgramStep = pProgramStep;
        _breakpointData[entry].BPwithViewExpr = 0b0;
        _breakpointData[entry].pView = nullptr;
        _breakpointData[entry].pTrigger = nullptr;
        _breakpointsUsed++;
        doInsertNewBP = true;
    }


    // save view string and hitcount value / trigger string
    _breakpointData[entry].BPwithViewExpr = (viewString == nullptr) ? 0b0 : 0b1;
    _breakpointData[entry].BPwithHitCount = (hitCount > 0) ? 0b1 : 0b0;
    _breakpointData[entry].BPwithTriggerExpr = (triggerString == nullptr) ? 0b0 : 0b1;

    _breakpointData[entry].hitCount = hitCount;
    _breakpointData[entry].hitCounter = 0;
    _pJustina->replaceSystemStringValue(_breakpointData[entry].pView, viewString);
    _pJustina->replaceSystemStringValue(_breakpointData[entry].pTrigger, triggerString);

    // insert the newly added entry to keep the set of entries sorted by source line number
    if (doInsertNewBP && (_breakpointsUsed >= 2)) {                                       // pre-existing breakpoints are sorted; just insert the new breakpoint in the right place in order to keep a sorted set
        BreakpointData breakpointBuffer = _breakpointData[entry];                           // new entry
        int index{};
        for (index = entry - 1; index >= 0; index--) {                                      // start with last sorted entry (preceding new entry), end with first
            if (breakpointBuffer.sourceLine > _breakpointData[index].sourceLine) { break; }
            else { _breakpointData[index + 1] = _breakpointData[index]; }
        }
        _breakpointData[index + 1] = breakpointBuffer;                                      // store new entry 
    }

    // remove the cleared entry to keep the set of entries sorted by source line number
    else if (doClear && (_breakpointsUsed >= 1)) {
        for (int index = entry; index <= _breakpointsUsed - 1; index++) {
            _breakpointData[index] = _breakpointData[index + 1];
        }
    }

    return Justina_interpreter::result_execOK;
}


// ---------------------------------
// *   print the breakpoint list   *
// ---------------------------------

void Breakpoints::printBreakpoints() {

    char s[110];////

    for (int i = 0; i < _breakpointsUsed; i++) {
        sprintf(s, "%5ld%4s%4s", _breakpointData[i].sourceLine, _breakpointData[i].BPenabled == 0b1 ? "x" : ".", _breakpointData[i].BPwithHitCount == 0b1 ? "x" : ".");
        _pJustina->print(s);
        sprintf(s, "%100s", _breakpointData[i].pView); _pJustina->print(s);
        if (_breakpointData[i].BPwithHitCount == 0b1) { sprintf(s, "%10lu", _breakpointData[i].hitCount); _pJustina->println(s); }
        else { sprintf(s, "%100s  ", _breakpointData[i].pTrigger); _pJustina->println(s); }
    }
}


