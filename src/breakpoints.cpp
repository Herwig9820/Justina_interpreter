/***********************************************************************************************************
*   Justina interpreter library                                                                            *
*                                                                                                          *
*   Copyright 2024, 2025 Herwig Taveirne                                                                   *
*                                                                                                          *
*   This file is part of the Justina Interpreter library.                                                  *
*   The Justina interpreter library is free software: you can redistribute it and/or modify it under       *
*   the terms of the GNU General Public License as published by the Free Software Foundation, either       *
*   version 3 of the License, or (at your option) any later version.                                       *
*                                                                                                          *
*   This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;              *
*   without even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.             *
*   See the GNU General Public License for more details.                                                   *
*                                                                                                          *
*   You should have received a copy of the GNU General Public License along with this program. If not,     *
*   see https://www.gnu.org/licenses.                                                                      *
*                                                                                                          *
*   The library is intended to work with 32 bit boards using the SAMD architecture ,                       *
*   the Arduino nano RP2040 and Arduino nano ESP32 boards.                                                 *
*                                                                                                          *
*   See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                          *
***********************************************************************************************************/


#include "Justina.h"

#define PRINT_DEBUG_INFO  0

// *****************************************************************
// ***            class Breakpoints - implementation             ***
// *****************************************************************


// -------------------
// *   constructor   *
// -------------------
Breakpoints::Breakpoints(Justina* pJustina, long lineRanges_memorySize, long maxBreakpointCount) :_pJustina(pJustina), _BPLineRangeMemorySize(lineRanges_memorySize), _maxBreakpointCount(maxBreakpointCount) {
    // NOTE: objects created / deleted in constructors / destructors are not counted
    _BPlineRangeStorage = new char[_BPLineRangeMemorySize];
    _pBreakpointData = new BreakpointData[_maxBreakpointCount];         // store active breakpoint

    _BPlineRangeStorageUsed = 0;                                        // in bytes
    _breakpointsUsed = 0;                                               // no breakpoints set
}


// ------------------
// *   destructor   *
// ------------------

Breakpoints::~Breakpoints() {
    // NOTE: objects created / deleted in constructors / destructors are not counted
    delete[] _pBreakpointData;
    delete[] _BPlineRangeStorage;
}


// -------------
// *   reset   *
// -------------

void Breakpoints::resetBreakpointsState() {
    for (int i = 0; i < _breakpointsUsed; i++) {
        _pJustina->setNewSystemExpression(_pBreakpointData[i].pView, nullptr);        // remove any heap objects created for non-empty view or trigger strings    
        _pJustina->setNewSystemExpression(_pBreakpointData[i].pTrigger, nullptr);
    }

    _BPlineRangeStorageUsed = 0;
    _breakpointsUsed = 0;
    _breakpointsAreOn = true;
    _breakpointsStatusDraft = false;
}


// ******************************
// *** during program parsing ***
// ******************************

/*
    A user must be able to set a breakpoint for each source line having at least one statement STARTING at that source line.
    To accomplish that, during program parsing, the line numbers of these source lines must be 'remembered' and linked to parsed statements.

    To do this with a minimal use of memory AND with NO impact on program memory allocated to parsed program statements, Justina stores pairs of line range sizes in a separate table, as follows:
    each pair consists of 
    - the 'gap' size (number of lines) between the end of a previous range of valid source lines (or the beginning of the source file) and the beginning of a next range of 'valid' source lines
    - the size of the next range of 'valid' source lines (lines where a breakpoint can be set)
    
    This information is stored in a table WITHOUT any impact on the memory required for parsed statements (program memory). 
    - gap size is less than 8 source lines and valid range size is less than 16 source lines: 1 byte is required in range pair table
    - gap size is less than 128 source lines and valid range size is less than 128 source lines: 2 bytes are required in range pair table
    - gap size is less than 2048 source lines and valid range size is less than 2048 source lines: 3 bytes are required in range pair table
    Larger line range sizes will produce an error.

    At the same time, during program parsing, statements for which setting a breakpoint is allowed, are marked by altering the statement separator token preceding
    the parsed statement. This does NOT consume any extra memory (it's merely a flag that is set).

    For each source line included in the range pair table, exactly one parsed statement is marked as 'breakpoint allowed' (1-to-1).
    When a user sets, clears, ... a breakpoint later during debugging the program, the line sequence number of the source line in the range pair table
    is established first (a number from 1 to the number of source lines where a breakpoint can be set). 
    Then the parsed program is scanned, counting only statements marked as 'breakpoint allowed', until the parsed statement matching
    the source line sequence number is found. The breakpoint flag for that statement is then adapted in program memory.

    Example: 'gap range-valid range' pairs 3,5,7,2 show that the source file consists at least of 3+5+7+2 = 17 lines; lines 4->8 and lines 16->17 are valid source lines.
    Each valid source line contains exactly one statement where a breakpoint can be set (the first statement starting on that source line).
    If the user wants to set a breakpoint for line 16 for example, as this is the 6th valid source line, Justina will find the 6th parsed statement and alter the
    preceding statement separator token, to indicate that the breakpoint is now set. Additional breakpoint attributes will be maintained in a separate table.
*/

// -----------------------------------------------------------------------------------------------------------------
// *   breakpoints: store info during program parsing to allow setting breakpoints during execution (debug mode)   *
// -----------------------------------------------------------------------------------------------------------------

Justina::parsingResult_type Breakpoints::collectSourceLineRangePairs(const char semiColonBPallowed_token, bool& parsedStatementStartsOnNewLine, bool& parsedStatementStartLinesAdjacent,
    long statementStartsAtLine, long& parsedStatementAllowingBPstartsAtLine, long& BPstartLine, long& BPendLine, long& BPpreviousEndLine) {

    static long gapLineRange{ 0 };

    // if the statement yet to parse starts on the same line as the previous statement, it doesn't start on a new line (obviously)
    parsedStatementStartsOnNewLine = (statementStartsAtLine != parsedStatementAllowingBPstartsAtLine);

    if (!(_pJustina->_programMode && (parsedStatementStartsOnNewLine))) { return Justina::parsingResult_type::result_parsing_OK; }  // nothing to do

    // 1. for each 'first' statement starting at a specific source line: alter the 'end of statement' token of the...
    //    ...previously parsed (preceding) statement to indicate 'setting breakpoint allowed' for the statement being parsed.
    // ----------------------------------------------------------------------------------------------------------------------

    if (_pJustina->_programCounter - 1 >= _pJustina->_programStorage) {
        *(_pJustina->_programCounter - 1) = semiColonBPallowed_token;
    }

    // 2. maintain source line ranges having at least one statement starting on that line
    // ----------------------------------------------------------------------------------

    // start of previous source line also contained start of a parsed statement ? Also rectify for beginning of file
    parsedStatementStartLinesAdjacent = (parsedStatementAllowingBPstartsAtLine == statementStartsAtLine - 1) && (statementStartsAtLine != 1);

    parsedStatementAllowingBPstartsAtLine = statementStartsAtLine;

    // still in a source line range with a new statement starting at the beginning of each line (disregarding white space) 
    if (parsedStatementStartLinesAdjacent) {
        BPendLine = parsedStatementAllowingBPstartsAtLine;
    }

    // first line of an 'adjacent source line range': time to calculate length of last gap range and previous 'adjacent' source line range
    else {
        long adjacentLineRange = (BPendLine == 0) ? 0 : BPendLine - BPstartLine + 1;        // PREVIOUS adjacent range

        // if valid, store in _BPlineRangeStorage array
        if (adjacentLineRange != 0) {            // skip first (invalid) gap/adjacent line range pair (produced at beginning of file)
            Justina::parsingResult_type result = addOneSourceLineRangePair(gapLineRange, adjacentLineRange);
            if (result != Justina::result_parsing_OK) { return result; }
        }
        BPpreviousEndLine = BPendLine;

        gapLineRange = parsedStatementAllowingBPstartsAtLine - BPendLine - 1;               // GAP range between previous and this start of new gap range 
        BPstartLine = parsedStatementAllowingBPstartsAtLine; BPendLine = BPstartLine;
    }

    return Justina::result_parsing_OK;
}


// ---------------------------------------------------------------------------------------------------------------------------------
// *   store a pair of program file line range lengths: gap line range length and adjacent line range length                     ***
// *   - gap source line range length: number of source file lines between previous and this adjacent line range                 ***
// *   - 'adjacent' source line range length: number of adjacent source file lines with a new statement starting at that line    *** 
// *   purpose: keep track of source lines where setting a breakpoint (debug mode) is allowed, in a relatively dense format      *** 
// ---------------------------------------------------------------------------------------------------------------------------------

Justina::parsingResult_type Breakpoints::addOneSourceLineRangePair(long gapLineRange, long adjacentLineRange) {

    // 'gap' source line range length requires 3 bits or less, 'adjacent' source line range length requires 4 bits or less: use 1 byte (1 flag bit + 3 + 4 bits = 8 bits)
    // byte 0 -> bit 0 = 0 (flag), bits 3..1: 'gap' source line range length (value range 0..7), bits 7..4: 'adjacent' source line range length (value range 0-15)
    if ((gapLineRange < 0x08) && (adjacentLineRange < 0x10)) {
        if (_BPlineRangeStorageUsed >= _BPLineRangeMemorySize - 1) { return Justina::result_BP_lineTableMemoryFull; } // memory full
        _BPlineRangeStorage[_BPlineRangeStorageUsed++] = (adjacentLineRange << 4) | (gapLineRange << 1);        // flag bit 0 = 0: 1 byte used
    }

    // 'gap' source line range and 'adjacent' source line range lengths both require 7 bits (value range 0 - 127): use 2 bytes (2 flag bits + 2 x 7 bits = 16 bits)
    // - byte 0-> bits 1..0 = 0b01 (flag),  bits b7..2: 6 LSB's 'gap' source line range length
    // - byte 1-> bit 0: MSB 'gap' source line range length, bits 7..1: 7 bits 'adjacent' source line range length
    else if ((gapLineRange < 0x80) && (adjacentLineRange < 0x80)) {
        if (_BPlineRangeStorageUsed >= _BPLineRangeMemorySize - 2) { return Justina::result_BP_lineTableMemoryFull; } // memory full
        _BPlineRangeStorage[_BPlineRangeStorageUsed++] = (gapLineRange << 2) | 0x01;                            // flag bits 1..0 = 0b01: 2 bytes used
        _BPlineRangeStorage[_BPlineRangeStorageUsed++] = (adjacentLineRange << 1) | (char)(gapLineRange >= 0x40);
    }

    // 'gap' source line range and 'adjacent' source line range lengths both require 11 bits or less (value range 0 - 2047): use 3 bytes (2 flag bits + 2 x 11 bits = 24 bits)
    // - byte 0-> bits 1..0 = 0b11 (flag),  bits b7..2: 6 LSB's 'gap' source line range length
    // - byte 1-> bits 4..0: 5 MSB's 'gap' source line range length, bits 7..5: 3 LSB's 'adjacent' source line range length
    // - byte 2-> bits 7..0: 7 MSB's 'adjacent' source line range length
    else if ((gapLineRange < 0x800) && (adjacentLineRange < 0x800)) {
        if (_BPlineRangeStorageUsed >= _BPLineRangeMemorySize - 3) { return Justina::result_BP_lineTableMemoryFull; } // memory full
        uint32_t temp = ((gapLineRange << 2) & ~((0b01 << 2) - 1)) | ((adjacentLineRange << 13) & ~((0b01 << 13) - 1)) | 0x03;
        memcpy(_BPlineRangeStorage + _BPlineRangeStorageUsed, &temp, 3);
        _BPlineRangeStorageUsed += 3;
    }
    else { return Justina::result_BP_lineRangeTooLong; } // gap range or adjacent source line range length too long

    return Justina::result_parsing_OK;
}



// ************************************************************************************************************************************************
// *** handle user commands to set or clear breakpoints, to change breakpoint settings and to print out a list of all breakpoints currently set ***
// ************************************************************************************************************************************************

// --------------------------------------------
// *   adapt a breakpoint for a source line   *
// --------------------------------------------

Justina::execResult_type Breakpoints::maintainBP(long breakpointLine, char actionCmdCode, int extraAttribCount, const char* viewString, long hitCount, const char* triggerString) {

    Justina::execResult_type execResult{ Justina::result_execOK };

    char* pProgramStep{ nullptr };
    bool doSet = (actionCmdCode == Justina::cmdcod_setBP);
    bool doClear = (actionCmdCode == Justina::cmdcod_clearBP);
    bool doEnable = (actionCmdCode == Justina::cmdcod_enableBP);
    bool doDisable = (actionCmdCode == Justina::cmdcod_disableBP);

    if (!_breakpointsStatusDraft) {   // skip 1. and 2. if breakpoint table has status 'DRAFT'

        // 1. find source line sequence number for line number (base 0) 
        // ------------------------------------------------------------

        // note: line sequence number = line index in the set of source lines having a statement STARTING AT THE START of the source line (discarding spaces)
        long lineSequenceNum = BPsourceLineFromToBPlineSequence(breakpointLine, true);
        if (lineSequenceNum == -1) { return  Justina::result_BP_notAllowedForSourceLine; }      // not a valid source line (doesn't start with a Justina statement)

        // 2. find parsed program statement and current breakpoint state ('set' or 'allowed but not set') for source line; if setBP or clearBP, adapt in program memory
        // ------------------------------------------------------------------------------------------------------------------------------------------------------------

        // parsed statement for the first statement STARTING on a source line: 
        // the semicolon step (statement separator) preceding the parsed statement is altered to indicate that a breakpoint is either set or allowed for this statement
        // note: the FIRST statement STARTING on that source line must be an executable statement. Otherwise that source line does not accept breakpoints

        execResult = progMem_getSetClearBP(lineSequenceNum, pProgramStep, doSet, doClear);
        if (execResult != Justina::result_execOK) { return execResult; }
    }

    // 3. Maintain breakpoint settings in breakpoint data table, for all breakpoints currently set  
    // -------------------------------------------------------------------------------------------

    // the count of source lines with a corresponding line sequence number is identical to the count of parsed statements with an altered preceding semicolon token (altered to indicate...
    // ...that a breakpoint is either set or allowed for the parsed statement).
    // in other words, for each source line with a valid line sequence number, there is exactly one parsed statement where a breakpoint is either set or allowed, and vice versa (1-to-1). 

    // NOTE: if the breakpoint table currently has status 'DRAFT', there is no link with 'real' source lines (any source line is valid), nor with parsed program statements. ...
    //       ...(a program may even not exist). 
    //       compatibility between action requested and BP existence is not tested: test in 'maintainBreakpointTable()'
    execResult = maintainBreakpointTable(breakpointLine, pProgramStep, doSet, doClear, doEnable, doDisable, extraAttribCount, viewString, hitCount, triggerString);

    return execResult;
}


// ------------------------------------------------------------------------------------------------------------------
// * find the address of the parsed program step corresponding to the first statement starting on a a source line   *
// ------------------------------------------------------------------------------------------------------------------

Justina::execResult_type Breakpoints::findParsedStatementForSourceLine(long sourceLine, char*& pProgramStep) {

    // 1. find source line sequence number for line number (base 0) 
    // ------------------------------------------------------------

    // note: line sequence number = line index in the set of source lines having a statement STARTING AT THE START of the source line (discarding spaces)
    long lineSequenceNum = BPsourceLineFromToBPlineSequence(sourceLine, true);
    if (lineSequenceNum == -1) { return  Justina::result_BP_notAllowedForSourceLine; }          // not a valid source line (not within source line range or doesn't start with a Justina statement)

    // 2. find parsed program statement
    // --------------------------------

    // parsed statement for the first statement STARTING on a source line: 
    // the semicolon step (statement separator) preceding the parsed statement is altered to indicate that a breakpoint is either set or allowed for this statement
    // the (set or allowed) breakpoint setting is only used to check that this source line can be set as next line to execute (given next constraints are fulfilled) 
    Justina::execResult_type execResult = progMem_getSetClearBP(lineSequenceNum, pProgramStep);
    return execResult;
}


// --------------------------------------------------------------------------------------------------------------------------------------
// *  find program step and current breakpoint state (set or 'allowed') for source line; if setBP or clearBP, adapt in program memory   *
// --------------------------------------------------------------------------------------------------------------------------------------

Justina::execResult_type Breakpoints::progMem_getSetClearBP(long lineSequenceNum, char*& pProgramStep, bool doSet, bool doClear) {

    // 1. find parsed statement corresponding to source line sequence number
    // ---------------------------------------------------------------------

    // NOTE: there is a 1-TO-1 relationship between each source line with a valid line sequence number AND a parsed statements

    pProgramStep = _pJustina->_programStorage;
    long i{ 0 };
    int matchedCriteriumNumber{};
    int matchedSemiColonTokenIndex{ 0 };

    if (lineSequenceNum == 0) { return Justina::result_BP_statementIsNonExecutable; }                   // first statement is not preceded by a semicolon (statement separator)
    while (i < lineSequenceNum) {                       // parsed statement corresponding to line sequence number (and source line statement) has been found (always a matching entry): exit          
        // find next semicolon token. It flags whether a breakpoint is allowed for the NEXT statement (pending further tests)   
        _pJustina->findTokenStep(pProgramStep, true, Justina::tok_isTerminalGroup1, Justina::termcod_semicolon,
            Justina::termcod_semicolon_BPset, Justina::termcod_semicolon_BPallowed, &matchedCriteriumNumber, &matchedSemiColonTokenIndex);    // always match
        if ((matchedCriteriumNumber >= 2)) { i++; }     // if parsed statement can receive a breakpoint (or breakpoint is set already), increase loop counter
    }

    // 2. in program memory, get / [set / clear] breakpoint for parsed statement by altering preceding statement separator      
    // -------------------------------------------------------------------------------------------------------------------
                                                                                                        // breakpoint allowed or already set based on statement position in source file 
    int statementTokenType = _pJustina->jumpTokens(1, pProgramStep);                                    // first character of first token of next statement (following the 'flagged' semicolon token)
    // if a command, check that setting a breakpoint is allowed by the command attributes
    if ((statementTokenType & 0x0F) == Justina::tok_isInternCommand) {                                  // statement is an internal command ? check whether it's executable
        int commandIndex = ((Justina::Token_internalCommand*)pProgramStep)->tokenIndex;
        if (Justina::_internCommands[commandIndex].usageRestrictions & Justina::cmd_skipDuringExec) { return Justina::result_BP_statementIsNonExecutable; }     // because not executable
    }
    // next line does not work if breakpoint table status is DRAFT (no link with program, if any) -> do this test later by checking existence of entry in breakpoint table 
    //BPwasSet = (((Justina::Token_terminal*)(pProgramStep - 1))->tokenTypeAndIndex == _pJustina->_semicolonBPset_token);        // BEFORE changing tokenTypeAndIndex (next line)

    // set preceding separator token in program memory to indicate 'breakpoint set' or 'breakpoint allowed'
    if (doSet || doClear) { ((Justina::Token_terminal*)(pProgramStep - 1))->tokenTypeAndIndex = doSet ? _pJustina->_semicolonBPset_token : _pJustina->_semicolonBPallowed_token; }                                 // flag 'breakpoint set'

    return Justina::result_execOK;
}


// ----------------------------------------------------------------------
// *   Maintain breakpoint settings for all breakpoints currently set   *
// ----------------------------------------------------------------------

Justina::execResult_type Breakpoints::maintainBreakpointTable(long sourceLine, char* pProgramStep, bool doSet, bool doClear, bool doEnable, bool doDisable,
    int extraArribCount, const char* viewString, long hitCount, const char* triggerString) {

    int entry{ 0 };
    bool BPwasSet{ false }, doInsertNewBP{ false };

    for (entry = 0; entry < _breakpointsUsed; entry++) {
        if (_pBreakpointData[entry].sourceLine == sourceLine) { BPwasSet = true; break; }
    }

    // action is set BP:              continue (even if BP was already set: attributes could change)
    //           clear BP:            if BP is currently set, continue. Otherwise, return (all done) 
    //           enable / disable BP: if BP is currently set, continue. Otherwise, return error (cannot enable/disable nonexistent BP)
    if (!BPwasSet && !doSet) { return doClear ? Justina::result_execOK : Justina::result_BP_wasNotSet; }

    if (BPwasSet) {                                                                     // if BP was set, all actions are allowed
        // clear, enable, disable BP (if BP was not set, then control returned to caller already)
        if (doSet) {}                                                                   // do nothing
        else if (doClear) { _breakpointsUsed--; }                                       // (entry will be moved to end of defined breakpoints when corresponding objects have been deleted)   
        else if (doEnable) { _pBreakpointData[entry].BPenabled = 0b1; }
        else if (doDisable) { _pBreakpointData[entry].BPenabled = 0b0; }
    }

    else {      // BP was not yet set: action is 'set BP'. Create breakpoint entry in breakpoint data array
        if (_breakpointsUsed == _maxBreakpointCount) { return Justina::result_BP_maxBPentriesReached; }
        entry = _breakpointsUsed;                                                       // first free entry 
        _pBreakpointData[entry].BPenabled = 0b1;
        _pBreakpointData[entry].sourceLine = sourceLine;
        _pBreakpointData[entry].pProgramStep = pProgramStep;                            // if status DRAFT, then pProgramStep is dummy
        _pBreakpointData[entry].pView = nullptr;
        _pBreakpointData[entry].pTrigger = nullptr;
        _breakpointsUsed++;
        doInsertNewBP = true;
    }

    if (doSet or doClear) {     // adapt attributes
        // keep view, hit count or trigger if setting a breakpoint that was already set, AND those attributes are not supplied
        bool keepView = (BPwasSet && doSet && (extraArribCount == 0));          // 'set' for an existing breakpoint: only change view string if supplied (0: no attributes supplied, 1: view string supplied)
        bool keepCondition = (BPwasSet && doSet && (extraArribCount <= 1));     // 'set' for an existing breakpoint: only change hit count or trigger string if supplied (2: condition supplied)

        // save view string and hit count value / trigger string
        if (!keepView) {
            _pBreakpointData[entry].BPwithViewExpr = (viewString == nullptr) ? 0b0 : 0b1;
            _pJustina->setNewSystemExpression(_pBreakpointData[entry].pView, viewString);
        }
        if (!keepCondition) {
            _pBreakpointData[entry].BPwithHitCount = (hitCount > 0) ? 0b1 : 0b0;
            _pBreakpointData[entry].BPwithTriggerExpr = (triggerString == nullptr) ? 0b0 : 0b1;

            _pBreakpointData[entry].hitCount = hitCount;
            _pBreakpointData[entry].hitCounter = 0;
            _pJustina->setNewSystemExpression(_pBreakpointData[entry].pTrigger, triggerString);
        }
    }

    // newly added entry  ? insert it in the set of ordered entries to keep the entries sorted by source line number
    if (doInsertNewBP && (_breakpointsUsed >= 2)) {                             // pre-existing breakpoints are sorted; just insert the new breakpoint in the right place in order to keep a sorted set
        BreakpointData breakpointBuffer = _pBreakpointData[entry];              // new entry
        int index{};
        for (index = entry - 1; index >= 0; index--) {                          // start with last sorted entry (preceding new entry), end with first
            if (breakpointBuffer.sourceLine > _pBreakpointData[index].sourceLine) { break; }
            else { _pBreakpointData[index + 1] = _pBreakpointData[index]; }
        }
        _pBreakpointData[index + 1] = breakpointBuffer;                         // store new entry 
    }

    // remove cleared entry ? move it to the end to keep the entries sorted by source line number
    else if (doClear && (_breakpointsUsed >= 1)) {
        for (int index = entry; index <= _breakpointsUsed - 1; index++) {
            _pBreakpointData[index] = _pBreakpointData[index + 1];
        }
    }

    return Justina::result_execOK;
}


// ---------------------------------
// *   print the breakpoint list   *
// ---------------------------------

void Breakpoints::printBreakpoints() {

    // print table header lines

    _pJustina->print("Breakpoints are currently "); _pJustina->println(_breakpointsAreOn ? "ON\r\n" : "OFF\r\n");
    _pJustina->println("source   enabled   view &\r\n  line             trigger\r\n------   -------   -------");

    char line[50];     // sufficient length for all line elements in first sprintf

    int BPprinted = 0;
    for (int i = 0; i < _breakpointsUsed; i++) {

        // print breakpoint settings line 1
        sprintf(line, "%6ld%7c      view : ", _pBreakpointData[i].sourceLine, _pBreakpointData[i].BPenabled == 0b1 ? 'x' : ' ');
        _pJustina->print(line);
        _pJustina->println(_pBreakpointData[i].BPwithViewExpr == 0b1 ? _pBreakpointData[i].pView : "-");

        // print breakpoint settings line 2
        sprintf(line, "%19s%0s", "", _pBreakpointData[i].BPwithHitCount == 0b1 ? "hit count: " : _pBreakpointData[i].BPwithTriggerExpr == 0b1 ? "trigger: " : "always trigger\r\n");
        _pJustina->print(line);

        if (_pBreakpointData[i].BPwithTriggerExpr == 0b1) { _pJustina->println(_pBreakpointData[i].pTrigger); }
        else if (_pBreakpointData[i].BPwithHitCount == 0b1) {
            sprintf(line, "%1ld (current is %0ld)", _pBreakpointData[i].hitCount, _pBreakpointData[i].hitCounter);
            _pJustina->println(line);
        }
        BPprinted++;
    }

    if (BPprinted == 0) { _pJustina->println("(none)"); }       // useful if 'print enabled BP' only

    if (_breakpointsStatusDraft) {
        _pJustina->println(); for (int i = 1; i <= 35; i++) { _pJustina->print('*'); }
        _pJustina->println("\r\n** Breakpoints have status DRAFT **");
        for (int i = 1; i <= 35; i++) { _pJustina->print('*'); } _pJustina->println();
    }
}



// ********************************
// *** during program execution ***
// ********************************

// ------------------------------------------------------------------------
// *   find breakpoint table entry for a parsed statement start address   *
// ------------------------------------------------------------------------

Breakpoints::BreakpointData* Breakpoints::findBPtableRow(char* pParsedStatement, int& row) {
    for (int i = 0; i < _breakpointsUsed; i++) {
        if (_pBreakpointData[i].pProgramStep == pParsedStatement) { row = i;  return _pBreakpointData + i; }
    }
    row = -1;               // no match
    return nullptr;
};


// --------------------------------------------------------------------
// *   find source line number for a parsed statement start address   *
// --------------------------------------------------------------------

long Breakpoints::findLineNumberForBPstatement(char* pProgramStepToFind) {

    // !!! note that this can be slow if program consists of a large number of statements

    long lineSequenceNumber{ 0 };
    int matchedCriteriumNumber{};
    int matchedSemiColonTokenIndex{ 0 };

    // 1. scan the parsed program, counting all statements preceded by a semicolon, semicolon 'BP set' or semicolon 'BP allowed' token.
    //    the very first valid statement, although not preceded by a semicolon, receives 'line sequence number' 0 

    char* pProgramStep = _pJustina->_programStorage;

    // do until parsed statement corresponding to line sequence number (and source line statement) has been found (always a matching entry)
    while (pProgramStep != pProgramStepToFind - 1) {
        // find next semicolon token. It flags whether a breakpoint is allowed for the NEXT statement (pending further tests)   
        _pJustina->findTokenStep(pProgramStep, true, Justina::tok_isTerminalGroup1, Justina::termcod_semicolon,
            Justina::termcod_semicolon_BPset, Justina::termcod_semicolon_BPallowed, &matchedCriteriumNumber, &matchedSemiColonTokenIndex);    // always match
        // if parsed statement can receive a breakpoint (or breakpoint is set already), increase loop counter
        if ((matchedCriteriumNumber >= 2)) { lineSequenceNumber++; }
    };

    // 2. find the source line number (base 1) corresponding to a line sequence number (base 0)  
    long sourceLineNumber = BPsourceLineFromToBPlineSequence(lineSequenceNumber, false);
    return sourceLineNumber;
}



// ***********************
// ***    utilities    ***
// ***********************

// ------------------------------------------------------------------------------------------------------------------
// *   return the sequence number of a given source line OR the source line for a given sequence number.            *
// *   the sequence number of a source line is the index (base 0) attributed to the source line, only counting...   *
// *   ...source lines with at least one statement STARTING on that line.                                           *
// ------------------------------------------------------------------------------------------------------------------

long Breakpoints::BPsourceLineFromToBPlineSequence(long BPlineOrIndex, bool toIndex) {

    // resp. value in and out; OR value out and in
    long BPsourceLine{  }, BPsourceLineIndex{  };

    if (toIndex) { BPsourceLine = BPlineOrIndex; if (BPsourceLine < 1) { return -1; } }         // not valid
    else { BPsourceLineIndex = BPlineOrIndex; if (BPsourceLineIndex < 0) { return -1; } }       // not valid

    int i = 0;
    long BPpreviousEndLine{ 0 };
    long BPpreviousEndLineIndex{ -1 };

    while (i < _BPlineRangeStorageUsed) {
        long gapLineRange{}, adjacentLineRange{};

        if (!(_BPlineRangeStorage[i] & 0b1)) {                  // LSB = 0b0: gap and adjacent source line ranges stored in one byte
            gapLineRange = (((uint32_t)_BPlineRangeStorage[i]) >> 1) & 0x7;                     // 3 bits long
            adjacentLineRange = (((uint32_t)_BPlineRangeStorage[i]) >> 4) & 0xF;                // 4 bits long
            i++;
        }

        else if ((_BPlineRangeStorage[i] & 0b11) == 0b01) {     // LSB's = 0b01: gap and adjacent source line ranges stored in two bytes
            uint32_t temp{};
            memcpy(&temp, _BPlineRangeStorage + i, 2);
            gapLineRange = (temp >> 2) & 0x7F;                                                  // each 7 bits long
            adjacentLineRange = (temp >> 9) & 0x7F;
            i += 2;
        }

        else if ((_BPlineRangeStorage[i] & 0b11) == 0b11) {     // LSB's = 0b11: gap and adjacent source line ranges stored in three bytes
            uint32_t temp{};
            memcpy(&temp, _BPlineRangeStorage + i, 3);
            gapLineRange = (temp >> 2) & 0x7FF;                                                 // each 11 bits long
            adjacentLineRange = (temp >> 13) & 0x7FF;
            i += 3;
        }

        if (toIndex) {
            long BPstartLine = BPpreviousEndLine + gapLineRange + 1;
            long BPendLine = BPstartLine + adjacentLineRange - 1;
            BPpreviousEndLine = BPendLine;

            if ((BPsourceLine >= BPstartLine) && (BPsourceLine <= BPendLine)) { return BPsourceLineIndex += (BPsourceLine - BPstartLine); }
            BPsourceLineIndex += BPendLine - BPstartLine + 1;
        }
        else {                                                                                  // to source line
            long BPstartLineIndex = BPpreviousEndLineIndex + 1;
            long BPendLineIndex = BPstartLineIndex + adjacentLineRange - 1;
            BPpreviousEndLineIndex = BPendLineIndex;

            if ((BPsourceLineIndex >= BPstartLineIndex) && (BPsourceLineIndex <= BPendLineIndex)) { return BPsourceLine += (BPsourceLineIndex - BPstartLineIndex) + gapLineRange + 1; }
            BPsourceLine += BPendLineIndex - BPstartLineIndex + gapLineRange + 1;
        }
    }

    return -1;             // signal range error
}


#if PRINT_DEBUG_INFO  
// ----------------------------------------------------------------------------------------
// *   print ranges of source lines with at least one statement starting on these lines   *
// ----------------------------------------------------------------------------------------

void Breakpoints::printLineRangesToDebugOut(Stream* output) {
    
    // note that these ranges contain lines with the start of non-executable statements as well (var, ...)
    
    int i = 0;
    long BPpreviousEndLine{ 0 };                        // introduce offset 1 here
    int bytes{ 0 };
    char s[80];

    output->println("Ranges of source lines containing the start of a statement\r\n");

    while (i < _BPlineRangeStorageUsed) {
        long gapLineRange{}, adjacentLineRange{};

        if ((_BPlineRangeStorage[i] & 0b1) == 0b0) {       // gap and adjacent source line ranges stored in one byte
            gapLineRange = (((uint32_t)_BPlineRangeStorage[i]) >> 1) & 0x7;                 // 3 bits long
            adjacentLineRange = (((uint32_t)_BPlineRangeStorage[i]) >> 4) & 0xF;            // 4 bits long
            bytes = 1;
        }

        else if ((_BPlineRangeStorage[i] & 0b11) == 0b01) {       // gap and adjacent source line ranges stored in two bytes
            uint32_t temp{};
            memcpy(&temp, _BPlineRangeStorage + i, 2);
            gapLineRange = (temp >> 2) & 0x7F;                                              // 7 bits long
            adjacentLineRange = (temp >> 9) & 0x7F;
            bytes = 2;
        }

        else if ((_BPlineRangeStorage[i] & 0b11) == 0b11) {       // gap and adjacent source line ranges stored in three bytes
            uint32_t temp{};
            memcpy(&temp, _BPlineRangeStorage + i, 3);
            gapLineRange = (temp >> 2) & 0x7FF;                                             // 11 bits long
            adjacentLineRange = (temp >> 13) & 0x7FF;
            bytes = 3;
        }
        
        i += bytes;
        
        long BPstartLine = BPpreviousEndLine + gapLineRange + 1;
        long BPendLine = BPstartLine + adjacentLineRange - 1;
        BPpreviousEndLine = BPendLine;

        sprintf(s, "%d bytes: gap range %ld, valid range %ld lines (valid from/to %ld-%ld)", bytes,gapLineRange, adjacentLineRange, BPstartLine, BPendLine);
        output->println(s);
    }

    sprintf(s, "\r\n%d bytes used", _BPlineRangeStorageUsed);
    output->println(s);

    return;
}
#endif