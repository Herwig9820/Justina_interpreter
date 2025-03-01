/***********************************************************************************************************
*   Justina interpreter library                                                                            *
*                                                                                                          *
*   Copyright 2024, 2025 Herwig Taveirne                                                                        *
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

#define PRINT_HEAP_OBJ_CREA_DEL 0
#define PRINT_PARSED_TOKENS 0
#define PRINT_DEBUG_INFO 0


// *****************************************************************
// ***        class Justina - implementation         ***
// *****************************************************************

// --------------------------------------------------------------------------------------------------------------------
// *   parse ONE statement in a character string, ended by an optional ';' character and a '\0' mandatory character   *
// --------------------------------------------------------------------------------------------------------------------

Justina::parsingResult_type Justina::parseStatement(char*& pInputStart, char*& pNextParseStatement, int& clearIndicator, bool isNewSourceLine, long sourceLine) {
    _lastTokenType_hold = tok_no_token;
    _lastTokenType = tok_no_token;                                                      // no token yet

    _lastTokenIsString = false;
    _lastTokenIsTerminal = false;
    _lastTokenIsPrefixOp = false;
    _lastTokenIsPostfixOp = false;
    _lastTokenIsPrefixIncrDecr = false;

    // expression syntax check
    _thisLvl_lastIsVariable = false;                                                    // init
    _thislvl_lastIsConstVar = false;
    _thisLvl_assignmentStillPossible = true;                                            // assume for now
    _thisLvl_lastOpIsIncrDecr = false;                                                  // assume for now

    // command argument constraints check: reset before starting to parse an instruction
    _lvl0_withinExpression = false;
    _lvl0_isPurePrefixIncrDecr = false;
    _lvl0_isPureVariable = false;
    _lvl0_isVarWithAssignment = false;

    // initializer unary operators
    _initVarOrParWithUnaryOp = 0;   // 0=no prefix, 1=plus, 2=minus

    _parenthesisLevel = 0;                                                              // current number of open parentheses (during parsing)

    _isCommand = false;

    int commandIndex{};
    bool isInternalCommand{};

    *_programCounter = tok_no_token;                                                    // in case first token produces error
    parsingResult_type result = result_parsing_OK;                                      // possible error will be determined during parsing 
    tokenType_type& t = _lastTokenType;
    char* pNext = pInputStart;                                                          // set to first character in instruction
    char* pNext_hold = pNext;

#if PRINT_PARSED_TOKENS
    _pDebugOut->println("\r\n** START parsing 1 statement");
#endif

    do {                                                                                // parse ONE token in an instruction
        bool isLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;
        bool isRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;
        bool isComma = _lastTokenIsTerminal ? (_lastTermCode == termcod_comma) : false;
        bool isSemicolon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
        bool isOperator = _lastTokenIsTerminal ? (_lastTermCode <= termcod_opRangeEnd) : false;

        if ((_lastTokenType == tok_no_token) || isSemicolon) {
            _isProgramCmd = false;
            _isJustinaFunctionCmd = false; _isVoidJustinaFunctionCmd = false; _isGlobalOrUserVarCmd = false; _isLocalVarCmd = false; _isStaticVarCmd = false; _isAnyVarCmd = false, _isConstVarCmd = false;;
            _isForCommand = false;
            _isDeleteVarCmd = false;
            _isClearProgCmd = false;
            _isClearAllCmd = false;

            _userVarUnderConstruction = false;
        }
        // determine token group of last token parsed (bits b4 to b0): this defines which tokens are allowed as next token
        _lastTokenGroup_sequenceCheck_bit = isOperator ? lastTokenGroup_0 :
            isComma ? lastTokenGroup_1 :
            ((t == tok_no_token) || isSemicolon || (t == tok_isInternCommand) || (t == tok_isExternCommand)) ? lastTokenGroup_2 :
            ((t == tok_isConstant) || (t == tok_isSymbolicConstant) || isRightPar) ? lastTokenGroup_3 :
            ((t == tok_isInternCppFunction) || (t == tok_isExternCppFunction) || (t == tok_isJustinaFunction)) ? lastTokenGroup_4 :
            isLeftPar ? lastTokenGroup_5 : lastTokenGroup_6;                                                        // token group 6: scalar or array variable name

        // a space may be required between last token and next token (not yet known), if one of them is a keyword
        // and the other token is either a keyword, an alphanumeric constant or a parenthesis
        // space check result is OK if a check is not required or if a space is present anyway
        _leadingSpaceCheck = ((t == tok_isInternCommand) || (t == tok_isExternCommand) || _lastTokenIsString || isRightPar) && (pNext[0] != ' ');

        // move to the first non-space character of next token 
        while (pNext[0] == ' ') { pNext++; }                                                                        // skip leading spaces
        if (pNext[0] == '\0') { pNextParseStatement = pNext; break; }                                               // end of statement: prepare to quit parsing  

        // trace, BP view or BP trigger string ? parse one statement at a time, then execute it first (note: within BP trigger strings, only the first expression will be parsed and executed)
        if ((_parsingExecutingTraceString || _parsingExecutingTriggerString) && isSemicolon) { pNextParseStatement = pNext;  break; }


        _lastTokenType_hold = _lastTokenType;                                                                       // remember the last parsed token during parsing of a next token
        _lastTermCode_hold = _lastTermCode;                                                                         // only relevant for certain tokens
        _lastTokenIsTerminal_hold = _lastTokenIsTerminal;

        pNext_hold = pNext;

        // try to parse a token
        // --------------------
        do {                                                                                                        // one loop only
            // parsing routines below try to parse characters as a token of a specific type
            // if a function returns true, then either proceed OR skip reminder of loop ('continue') if 'result' indicates a token has been found
            // if a function returns false, then break with 'result' containing the error

            // check that there is still room to store the longest possible token + 2 extra ('tok_isEvalEnd' and ending '\0')
            char* lastProgramByte = _programStorage + _PROGRAM_MEMORY_SIZE + (_programMode ? 0 : IMM_MEM_SIZE) - 1;
            if ((_programCounter + sizeof(Token_symbolicConstant) + 2) > lastProgramByte) { result = result_progMemoryFull; break; };

            if (!parseAsInternCommand(pNext, result)) { break; } if (result == result_parsing_OK) { break; }        // check first  
            if (!parseAsExternCommand(pNext, result)) { break; } if (result == result_parsing_OK) { break; }
            if (!parseTerminalToken(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }         // check before checking for number
            if (!parseAsNumber(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }
            if (!parseAsStringConstant(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }
            if (!parseAsInternCPPfunction(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }   // check before checking for Justina function / variable 
            if (!parseAsExternCPPfunction(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }
            if (!parseAsJustinaFunction(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }     // check before checking for variable
            if (!parseAsVariable(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }
            if (!parseAsIdentifierName(pNext, result)) { break; }  if (result == result_parsing_OK) { break; }      // at the end
            result = (pNext[0] == '#') ? result_prefixCharNotAllowedHere : result = result_token_not_recognised;
        } while (false);
        // one token parsed (or error)
        if (result != result_parsing_OK) { break; }                                                                 // exit loop if token error (syntax, ...). Checked before checking command syntax


        // command ? Perform additional syntax checks 
        // ------------------------------------------

        bool isStatementStart = (_lastTokenType_hold == tok_no_token) || (_lastTokenIsTerminal_hold ? (_lastTermCode_hold == termcod_semicolon) : false);
        bool isCommandStart = false;
        if (isStatementStart) {
            isCommandStart = (_lastTokenType == tok_isInternCommand) || (_lastTokenType == tok_isExternCommand);    // keyword at start of statement ? is start of a command 
            _isCommand = isCommandStart;                                                                            // is start of a command ? then within a command now. Otherwise, it's an 'expression only' statement
            if (_isCommand) {
                isInternalCommand = (_lastTokenType == tok_isInternCommand);                                        // remember during parsing of this statement
                commandIndex = _tokenIndex;
                if (!checkCommandKeyword(result, commandIndex, isInternalCommand)) { ; pNext = pNext_hold; break; }
            }
        }

        bool isCommandArgToken = (!isCommandStart && _isCommand);
        if (isCommandArgToken) { if (!checkCommandArgToken(result, clearIndicator, commandIndex, isInternalCommand)) { pNext = pNext_hold; break; } }

    } while (true);

    // one statement parsed (or error: no token found OR command syntax error OR semicolon encountered)

    pInputStart = pNext;                                                                                            // set to next character (if error: indicates error position)

    if (_userVarUnderConstruction) { deleteUserVariable(); }

    return result;
}


// ----------------------------------------------------------------------
// *   Start of a command only: apply additional command syntax rules   *
// ----------------------------------------------------------------------

bool Justina::checkCommandKeyword(parsingResult_type& result, int commandIndex, bool commandIsInternal) {           // command syntax checks

#if PRINT_PARSED_TOKENS
    _pDebugOut->println("   checking command keyword");
#endif
    int key_allowedParTypes{};
    int index{ 0 };
    if (!commandIsInternal) {
        key_allowedParTypes = ((CppCommand*)_pExternCommands)[commandIndex].argTypeRestrictions;                    // external command ?

        int records = sizeof(cmdArgSeq_records) / sizeof(cmdArgSeq_records[0]);                                     // check validity argument sequence key 
        for (index = 0; index < records; index++) {
            if (cmdArgSeq_records[index].key == key_allowedParTypes) { break; }
        }

        if (index == records) { result = result_cmd_argTypeRestrictionNotValid; return false; }
    }

    _pCmdAllowedParTypes = commandIsInternal ? _internCommands[commandIndex].pCmdAllowedParTypes : cmdArgSeq_records[index].record;   // remember allowed parameter types
    _cmdParSpecColumn = 0;                                                                                          // reset actual command parameter counter
    _cmdArgNo = 0;

    constexpr Justina::CmdBlockDef externCmdBlockNone{ block_none, block_na, block_na, block_na };
    CmdBlockDef cmdBlockDef{ commandIsInternal ? _internCommands[commandIndex].cmdBlockDef : externCmdBlockNone };

    bool hasTokenStep = commandIsInternal ? (_internCommands[commandIndex].cmdBlockDef.blockType != block_none) : false;

    _isJustinaFunctionCmd = _isVoidJustinaFunctionCmd = _isReturnCommand = _isProgramCmd = _isGlobalOrUserVarCmd = _isLocalVarCmd = _isStaticVarCmd
        = _isConstVarCmd = _isForCommand = _isDeleteVarCmd = _isClearProgCmd = _isClearAllCmd = _isAnyVarCmd = false;   // init: assume command is external (user cpp)

    if (commandIsInternal) {
        _isJustinaFunctionCmd = ((_internCommands[commandIndex].commandCode == cmdcod_function) || (_internCommands[commandIndex].commandCode == cmdcod_voidFunction));
        _isVoidJustinaFunctionCmd = (_internCommands[commandIndex].commandCode == cmdcod_voidFunction);
        _isReturnCommand = (_internCommands[commandIndex].commandCode == cmdcod_return);
        _isProgramCmd = _internCommands[commandIndex].commandCode == cmdcod_program;
        _isGlobalOrUserVarCmd = ((_internCommands[commandIndex].commandCode == cmdcod_var) || (_internCommands[commandIndex].commandCode == cmdcod_constVar)) && !_justinaFunctionBlockOpen;
        _isLocalVarCmd = ((_internCommands[commandIndex].commandCode == cmdcod_var) || (_internCommands[commandIndex].commandCode == cmdcod_constVar)) && _justinaFunctionBlockOpen;
        _isStaticVarCmd = _internCommands[commandIndex].commandCode == cmdcod_static;
        _isConstVarCmd = (_internCommands[commandIndex].commandCode == cmdcod_constVar);
        _isForCommand = _internCommands[commandIndex].commandCode == cmdcod_for;
        _isDeleteVarCmd = _internCommands[commandIndex].commandCode == cmdcod_deleteVar;
        _isClearProgCmd = _internCommands[commandIndex].commandCode == cmdcod_clearProg;
        _isClearAllCmd = _internCommands[commandIndex].commandCode == cmdcod_clearAll;

        _isAnyVarCmd = _isGlobalOrUserVarCmd || _isLocalVarCmd || _isStaticVarCmd;                                  //  var, local, static
    }

    // is this command allowed here ? Check restrictions
    // -------------------------------------------------

    // retrieve restrictions
    char cmdRestriction = commandIsInternal ? _internCommands[commandIndex].usageRestrictions & cmd_usageRestrictionMask : ((CppCommand*)_pExternCommands)[commandIndex].usageRestrictions;

    // external (user cpp) commands: check if specified usage restriction is valid
    // note: external commands are always executable (not 'parsing only')
    if (!commandIsInternal && (cmdRestriction != cmd_noRestrictions) && (cmdRestriction != cmd_onlyInFunctionBlock)
        && (cmdRestriction != cmd_onlyImmediate) && (cmdRestriction != cmd_onlyImmOrInsideFuncBlock)) {
        result = result_cmd_usageRestrictionNotValid; return false;
    }

    if ((cmdRestriction == cmd_onlyProgramTop) && (_lastTokenStep != 0)) { result = result_cmd_onlyProgramStart; return false; }        // not a 'program' command
    if ((cmdRestriction != cmd_onlyProgramTop) && (_lastTokenStep == 0)) { result = result_cmd_programCmdMissing; return false; }
    if ((cmdRestriction == cmd_onlyImmModeTop) && (_programCounter != _programStorage + _PROGRAM_MEMORY_SIZE + sizeof(Token_internalCommand) - (hasTokenStep ? 0 : 2))) { result = result_cmd_onlyImmModeFirstStatement; return false; }
    if (_programMode && (cmdRestriction == cmd_onlyImmediate)) { result = result_cmd_onlyImmediateMode; return false; }
    if (!_programMode && (cmdRestriction == cmd_onlyInProgram)) { result = result_cmd_onlyInsideProgram; return false; }
    if (!_justinaFunctionBlockOpen && (cmdRestriction == cmd_onlyInFunctionBlock)) { result = result_cmd_onlyInsideFunction; return false; }
    if (_justinaFunctionBlockOpen && (cmdRestriction == cmd_onlyOutsideFunctionBlock)) { result = result_cmd_onlyOutsideFunction; return false; }
    if (((!_programMode) || _justinaFunctionBlockOpen) && (cmdRestriction == cmd_onlyInProgOutsideFunc)) { result = result_cmd_onlyInProgOutsideFunction; return false; };
    if ((_programMode && !_justinaFunctionBlockOpen) && (cmdRestriction == cmd_onlyImmOrInsideFuncBlock)) { result = result_cmd_onlyImmediateOrInFunction; return false; };
    if ((_programMode || (_blockLevel > 0)) && (cmdRestriction == cmd_onlyImmediateNotWithinBlock)) { result = result_cmd_onlyImmediateNotWithinBlock; return false; }
    if (_justinaFunctionBlockOpen && _isJustinaFunctionCmd) { result = result_function_defsCannotBeNested; return false; }       // separate message to indicate 'no nesting'

   // not a block command: nothing more to do here 
    if (cmdBlockDef.blockType == block_none) { return true; }                                                       // (always for external commands) 


    // perform specific checks related to block commands
    // -------------------------------------------------

    if (cmdBlockDef.blockPosOrAction == block_startPos) {                                                           // is a block start command ?                          
        _blockLevel++;                                                                                              // increment stack counter and create corresponding list element
        _pParsingStack = (LE_parsingStack*)parsingStack.appendListElement(sizeof(LE_parsingStack));
        _pParsingStack->openBlock.cmdBlockDef = cmdBlockDef;                                                        // store in stack: block type, block position ('start'), n/a, n/a

        memcpy(_pParsingStack->openBlock.toTokenStep, &_lastTokenStep, sizeof(char[2]));                            // store in stack: pointer to block start command token of open block
        _blockStartCmdTokenStep = _lastTokenStep;                                                                   // remember pointer to block start command token of open block
        _blockCmdTokenStep = _lastTokenStep;                                                                        // remember pointer to last block command token of open block
        _justinaFunctionBlockOpen = _justinaFunctionBlockOpen || _isJustinaFunctionCmd;                             // Justina function is open until block closing END command     
        _JustinaVoidFunctionBlockOpen = _JustinaVoidFunctionBlockOpen || _isVoidJustinaFunctionCmd;                 // idem; but only for void Justina function 
        return true;                                                                                                // nothing more to do
    }

    if (_blockLevel == 0) { result = result_block_noOpenBlock; return false; }                                      // not a block start and no open block: error

    if ((cmdBlockDef.blockType == block_alterFlow) && (_blockLevel > 0)) {
        // check for a compatible open block (e.g. a BREAK command can only occur if at least one open loop block exists)
        // parenthesis level is zero, because this is a block start command -> all stack levels are block levels
        LE_parsingStack* pStackLvl = _pParsingStack;                                                                // start with current open block level
        while (pStackLvl != nullptr) {
            if ((pStackLvl->openBlock.cmdBlockDef.blockType == block_JustinaFunction) &&                            // an open Justina function block has been found (call or definition)
                (cmdBlockDef.blockPosOrAction == block_inOpenFunctionBlock)) {                                      // and current flow altering command is allowed in open function block
                // store pointer from 'alter flow' token (command) to block start command token of compatible open block (from RETURN to FUNCTION token)
                memcpy(((Token_internalCommand*)(_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.toTokenStep, sizeof(char[2]));
                break;                                                                                              // -> applicable open block level found
            }
            if (((pStackLvl->openBlock.cmdBlockDef.blockType == block_for) ||
                (pStackLvl->openBlock.cmdBlockDef.blockType == block_while)) &&                                     // an open loop block has been found (e.g. FOR ... END block)
                (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock)) {                                          // and current flow altering command is allowed in open loop block
                // store pointer from 'alter flow' token (command) to block start command token of compatible open block (e.g. from BREAK to FOR token)
                memcpy(((Token_internalCommand*)(_programStorage + _lastTokenStep))->toTokenStep, pStackLvl->openBlock.toTokenStep, sizeof(char[2]));
                break;                                                                                              // -> applicable open block level found
            }
            pStackLvl = (LE_parsingStack*)parsingStack.getPrevListElement(pStackLvl);
        }
        if (pStackLvl == nullptr) { result = (cmdBlockDef.blockPosOrAction == block_inOpenLoopBlock) ? result_block_noOpenLoop : result_block_noOpenFunction; }
        return (pStackLvl != nullptr);
    }

    if ((cmdBlockDef.blockType != _pParsingStack->openBlock.cmdBlockDef.blockType) &&                               // same block type as open block (or block type is generic block end) ?
        (cmdBlockDef.blockType != block_genericEnd)) {
        result = result_block_notAllowedInThisOpenBlock; return false;                                              // wrong block type: error
    }

    bool withinRange = (_pParsingStack->openBlock.cmdBlockDef.blockPosOrAction >= cmdBlockDef.blockMinPredecessor) &&   // sequence of block commands OK ?
        (_pParsingStack->openBlock.cmdBlockDef.blockPosOrAction <= cmdBlockDef.blockMaxPredecessor);
    if (!withinRange) { result = result_block_wrongBlockSequence; return false; }                                   // sequence of block commands (for current stack level) is not OK: error

    // pointer from previous open block token to this open block token (e.g. pointer from IF token to ELSEIF or ELSE token)
    memcpy(((Token_internalCommand*)(_programStorage + _blockCmdTokenStep))->toTokenStep, &_lastTokenStep, sizeof(char[2]));
    _blockCmdTokenStep = _lastTokenStep;                                                                            // remember pointer to last block command token of open block


    if (cmdBlockDef.blockPosOrAction == block_endPos) {                                                             // is this a block END command token ? 
        if (_pParsingStack->openBlock.cmdBlockDef.blockType == block_JustinaFunction) { _justinaFunctionBlockOpen = _JustinaVoidFunctionBlockOpen = false; }  // FUNCTON definition blocks cannot be nested
        memcpy(((Token_internalCommand*)(_programStorage + _lastTokenStep))->toTokenStep, &_blockStartCmdTokenStep, sizeof(char[2]));
        parsingStack.deleteListElement(nullptr);                                                                    // decrement stack counter and delete corresponding list element
        _blockLevel--;                                                                                              // also set pointer to currently last element in stack (if it exists)

        if (_blockLevel + _parenthesisLevel > 0) { _pParsingStack = (LE_parsingStack*)parsingStack.getLastListElement(); }
        if (_blockLevel > 0) {
            // retrieve pointer to block start command token and last block command token of open block
            memcpy(&_blockStartCmdTokenStep, _pParsingStack->openBlock.toTokenStep, sizeof(char[2]));               // pointer to block start command token of open block       
            uint16_t tokenStep = _blockStartCmdTokenStep;                                                           // init pointer to last block command token of open block
            uint16_t tokenStepPointedTo;
            memcpy(&tokenStepPointedTo, ((Token_internalCommand*)(_programStorage + tokenStep))->toTokenStep, sizeof(char[2]));
            while (tokenStepPointedTo != 0xFFFF)
            {
                tokenStep = tokenStepPointedTo;
                memcpy(&tokenStepPointedTo, ((Token_internalCommand*)(_programStorage + tokenStep))->toTokenStep, sizeof(char[2]));
            }

            _blockCmdTokenStep = tokenStep;                                                                         // pointer to last block command token of open block                       
        }
    }
    else { _pParsingStack->openBlock.cmdBlockDef = cmdBlockDef; }                                                   // overwrite (block type (same or generic end), position, min & max predecessor)

    return true;
}


// ------------------------------------------------------------------------------
// *   Check a command argument token (apply additional command syntax rules)   *
// ------------------------------------------------------------------------------

bool Justina::checkCommandArgToken(parsingResult_type& result, int& clearIndicator, int commandIndex, bool commandIsInternal) {

    // NOTE: 'commandIndex' and 'commandIsInternal' refer to the command being parsed, NOT to the current token (of that command) that is being parsed


    // init and adapt variables
    // ------------------------
#if PRINT_PARSED_TOKENS
    _pDebugOut->println("   checking command argument");
#endif

    static uint8_t allowedParType = cmdArg_none;                                                                    // init

    bool isSpareTokenType = false;                                                                                  // spare
    bool isGenIdent = (_lastTokenType == tok_isGenericName);
    bool isSemiColonSep = _lastTokenIsTerminal ? (_terminals[_tokenIndex].terminalCode == termcod_semicolon) : false;
    bool isLeftPar = _lastTokenIsTerminal ? (_terminals[_tokenIndex].terminalCode == termcod_leftPar) : false;
    bool isCommaSep = _lastTokenIsTerminal ? (_terminals[_tokenIndex].terminalCode == termcod_comma) : false;
    bool isLvl0CommaSep = isCommaSep && (_parenthesisLevel == 0);
    bool isAssignmentOp = _lastTokenIsTerminal ? ((_terminals[_tokenIndex].terminalCode == termcod_assign)
        || (_terminals[_tokenIndex].terminalCode == termcod_plusAssign) || (_terminals[_tokenIndex].terminalCode == termcod_minusAssign)
        || (_terminals[_tokenIndex].terminalCode == termcod_multAssign) || (_terminals[_tokenIndex].terminalCode == termcod_divAssign)
        || (_terminals[_tokenIndex].terminalCode == termcod_modAssign) || (_terminals[_tokenIndex].terminalCode == termcod_bitAndAssign)
        || (_terminals[_tokenIndex].terminalCode == termcod_bitOrAssign) || (_terminals[_tokenIndex].terminalCode == termcod_bitXorAssign)
        || (_terminals[_tokenIndex].terminalCode == termcod_bitShLeftAssign) || (_terminals[_tokenIndex].terminalCode == termcod_bitShRightAssign)) : false;

    // is this token part of an expression ? 
    _lvl0_withinExpression = !(isSpareTokenType || isGenIdent || isLvl0CommaSep || isSemiColonSep);

    // start of expression: if within expression, AND the preceding token was a level 0 comma separator, keyword or generic name
    bool previousTokenWasCmdArgSep = false;
    previousTokenWasCmdArgSep = (_lastTokenIsTerminal_hold ? (_lastTermCode_hold == termcod_comma) : false) && (_parenthesisLevel == isLeftPar ? 1 : 0);
    bool isExpressionFirstToken = _lvl0_withinExpression &&
        ((_lastTokenType_hold == tok_isInternCommand) || (_lastTokenType_hold == tok_isExternCommand) || (_lastTokenType_hold == tok_isGenericName) || previousTokenWasCmdArgSep);

    // keep track of argument index within command
    // -------------------------------------------
    if (isSpareTokenType || isGenIdent || isExpressionFirstToken) {
        _cmdArgNo++;
        if (_cmdArgNo > (commandIsInternal ? c_internalFncOrCmdMaxArgs : c_ExternalFncOrCmdMaxArgs)) { result = result_function_maxArgsExceeded; return false; }
        if (_cmdArgNo > (commandIsInternal ? _internCommands[commandIndex].maxArgs : ((CppCommand*)_pExternCommands)[commandIndex].maxArgCount)) { result = result_cmd_tooManyArguments; return false; }
        // return command (internal) with return value in void function ?
        if (_JustinaVoidFunctionBlockOpen && _isReturnCommand) { result = result_function_void_returnValueNotAllowed; return false; }
    }

    else if (isSemiColonSep) {
        if (_cmdArgNo < (commandIsInternal ? _internCommands[commandIndex].minArgs : ((CppCommand*)_pExternCommands)[commandIndex].minArgCount)) { result = result_cmd_argumentMissing; return false; }
        // return command (internal) without return value in non-void function ?
        if ((_justinaFunctionBlockOpen && !_JustinaVoidFunctionBlockOpen) && _isReturnCommand and (_cmdArgNo == 0)) { result = result_function_returnValueExpected; return false; }
    }


// if first token of a command parameter or a semicolon: check allowed argument types with respect to command definition (expression, identifier, ...) 
// ---------------------------------------------------------------------------------------------------------------------------------------------------
    bool multipleParameter = false, optionalParameter = false;
    if (isSpareTokenType || isGenIdent || isExpressionFirstToken || isSemiColonSep) {
        allowedParType = (_cmdParSpecColumn == sizeof(cmdArgSeq_100)) ? cmdArg_none : (uint8_t)(_pCmdAllowedParTypes[_cmdParSpecColumn]);   //sizeof array: OK, because elements are char type (1 byte each) 
        multipleParameter = (allowedParType & cmdArg_multipleFlag);
        optionalParameter = (allowedParType & cmdArg_optionalFlag);
        if (!multipleParameter) { _cmdParSpecColumn++; }                                                            // increase parameter count, unless multiple parameters of this type are accepted  
        allowedParType = allowedParType & ~cmdArg_flagMask;
    }

    if (isSemiColonSep) {                                                                                           // semicolon: end of command                                                    
        // NOTE: clear program / memory command will be executed when normal execution ends (before entering idle mode, waiting for input)
        if (_isClearProgCmd) { clearIndicator = 1; }                                                                // clear program: set flag 
        else if (_isClearAllCmd) { clearIndicator = 2; }                                                            // clear all: set flag
        return true;                                                                                                // nothing more to do for this command
    }


    // check command argument validity
    // -------------------------------
    // check each token, but skip tokens within open parenthesis (whatever is in there has no relevance for argument checking) ...
    // ... and skip commas separating arguments (because these commas have just reset variables used for command argument constraints checking, preparing for next command argument (if any))

    if ((_parenthesisLevel == 0) && (!isLvl0CommaSep)) {                                                                        // a comma resets variables used for command argument constraint checks
        if (allowedParType == cmdArg_none) { result = result_cmd_tooManyArguments; return false; }                              // irrespective of defined max. arg. count for command
        if (allowedParType == cmdArg_spare && !isSpareTokenType) { result = result_cmd_spareExpectedAsPar; return false; }      // does not occur, but keep for completeness
        if (allowedParType == cmdArg_ident && !isGenIdent) { result = _isDeleteVarCmd ? result_cmd_variableNameExpectedAsPar : result_cmd_identExpectedAsPar; return false; }
        if ((allowedParType == cmdArg_expression) && !_lvl0_withinExpression) { result = result_cmd_expressionExpectedAsPar; return false; }    // does not occur, but keep for completeness

        if ((allowedParType == cmdArg_varOptAssignment) && !_lvl0_isPurePrefixIncrDecr && !_lvl0_isPureVariable && !_isConstVarCmd && !_lvl0_isVarWithAssignment) {
            result = (parsingResult_type)result_cmd_varWithOptionalAssignmentExpectedAsPar; return false;
        }

        if ((allowedParType == cmdArg_varNoAssignment) && !_lvl0_isPureVariable) {
            result = isAssignmentOp ? (parsingResult_type)result_cmd_varWithoutAssignmentExpectedAsPar : (parsingResult_type)result_cmd_variableExpectedAsPar; return false;
        }
    }

    return true;
}


// ------------------------------------------------------------------------------------------
// *   try to parse next characters as an internal command (start of a command statement)   *
// ------------------------------------------------------------------------------------------

bool Justina::parseAsInternCommand(char*& pNext, parsingResult_type& result) {
    result = result_tokenNotFound;                                                                              // init: flag 'no token found'
    char* pch = pNext;                                                                                          // pointer to first character to parse (any spaces have been skipped already)
    int commandIndex;

    if (!isalpha(pNext[0])) { return true; }                                                                    // first character is not a letter ? Then it's not a keyword (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                                                 // do until first character after alphanumeric token (can be anything, including '\0')

    for (commandIndex = _internCommandCount - 1; commandIndex >= 0; commandIndex--) {                           // for all defined keywords: check against alphanumeric token (NOT ending by '\0')
        if (strlen(_internCommands[commandIndex]._commandName) != pNext - pch) { continue; }                    // token has correct length ? If not, skip remainder of loop ('continue')                            
        if (strncmp(_internCommands[commandIndex]._commandName, pch, pNext - pch) != 0) { continue; }           // token corresponds to keyword ? If not, skip remainder of loop ('continue') 

        // commands (starting with a keyword) are not allowed within trace, BP view, BP trigger strings and in eval() strings.
        // if not allowed, reset pointer to first character to parse, indicate error and return
        if (_parsingExecutingTraceString || _parsingExecutingTriggerString || _parsingEvalString) { pNext = pch; result = result_trace_eval_commandNotAllowed; return false; }

        if (_parenthesisLevel > 0) { pNext = pch; result = result_commandNotAllowedHere; return false; }

        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_2_0)) { pNext = pch; result = result_commandNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_commandNotAllowedHere; return false; }

        if (!_isCommand) {                                                                                      // already within a command: do not test here
            bool lastIsSemiColon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
            if (!lastIsSemiColon && (_lastTokenType != tok_no_token)) {
                pNext = pch; result = result_commandNotAllowedHere; return false;                               // keyword only at start of a statement (not within an expression)
            }
        }

        if (_leadingSpaceCheck) { pNext = pch; result = result_spaceMissing; return false; }
        _tokenIndex = commandIndex;                                                                             // needed in case it's the start of a command (to determine parameters)

        // token is an internal command name, and it's allowed here

        // expression syntax check 
        _thisLvl_lastIsVariable = false;
        _thislvl_lastIsConstVar = false;
        _thisLvl_assignmentStillPossible = true;                                                                // reset (expression may follow)                          

        // command argument constraints check: reset for next command parameter
        _lvl0_withinExpression = false;
        _lvl0_isPurePrefixIncrDecr = false;
        _lvl0_isPureVariable = false;
        _lvl0_isVarWithAssignment = false;

        // if NOT a block command, bytes for token step are not needed 
        bool hasTokenStep = (_internCommands[commandIndex].cmdBlockDef.blockType != block_none);

        Token_internalCommand* pToken = (Token_internalCommand*)_programCounter;
        pToken->tokenType = tok_isInternCommand | ((sizeof(Token_internalCommand) - (hasTokenStep ? 0 : 2)) << 4);
        pToken->tokenIndex = commandIndex;
        if (hasTokenStep) { pToken->toTokenStep[0] = 0xFF; pToken->toTokenStep[1] = 0xFF; }                     // -1: no token ref. uint16_t not necessarily aligned with word size: store as two separate bytes                            

        _lastTokenStep = _programCounter - _programStorage;
        _lastTokenType = tok_isInternCommand;
        _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

    #if PRINT_PARSED_TOKENS
        _pDebugOut->print("   parsed keyword: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); _pDebugOut->print(_internCommands[commandIndex]._commandName);  _pDebugOut->println("]");
        _pDebugOut->print("   token (res.word) index = "); _pDebugOut->println(commandIndex);
    #endif

        _programCounter += sizeof(Token_internalCommand) - (hasTokenStep ? 0 : 2);
        *_programCounter = tok_no_token;                                                                        // indicates end of program
        result = result_parsing_OK;                                                                             // flag 'valid token found'
        return true;
    }

    pNext = pch;                                                                                                // reset pointer to first character to parse (because no token was found)
    return true;                                                                                                // token is not a keyword (but can still be something else)
}


// -----------------------------------------------------------------------------------------------------
// *   try to parse next characters as an external (user cpp) command (start of a command statement)   *
// -----------------------------------------------------------------------------------------------------

bool Justina::parseAsExternCommand(char*& pNext, parsingResult_type& result) {
    result = result_tokenNotFound;                                                                              // init: flag 'no token found'
    char* pch = pNext;                                                                                          // pointer to first character to parse (any spaces have been skipped already)
    int commandIndex;

    if (!isalpha(pNext[0])) { return true; }                                                                    // first character is not a letter ? Then it's not a keyword (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                                                 // do until first character after alphanumeric token (can be anything, including '\0')

    for (commandIndex = _externCommandCount - 1; commandIndex >= 0; commandIndex--) {                           // for all defined keywords: check against alphanumeric token (NOT ending by '\0')
        const char* funcName = ((CppCommand*)_pExternCommands)[commandIndex].cppCommandName;

        if (strlen(((CppCommand*)_pExternCommands)[commandIndex].cppCommandName) != pNext - pch) { continue; }  // token has correct length ? If not, skip remainder of loop ('continue')                            
        if (strncmp(funcName, pch, pNext - pch) != 0) { continue; }                                             // token corresponds to keyword ? If not, skip remainder of loop ('continue') 

        if (pNext - pch > MAX_IDENT_NAME_LEN) { pNext = pch; result = result_identifierTooLong;  return false; }   // function name is too long

        // commands are not allowed within trace, BP view, BP trigger strings and in eval() strings.
        // if not allowed, reset pointer to first character to parse, indicate error and return
        if (_parsingExecutingTraceString || _parsingExecutingTriggerString || _parsingEvalString) { pNext = pch; result = result_trace_eval_commandNotAllowed; return false; }

        if (_parenthesisLevel > 0) { pNext = pch; result = result_commandNotAllowedHere; return false; }

        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_2_0)) { pNext = pch; result = result_commandNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_commandNotAllowedHere; return false; }

        if (!_isCommand) {                                                                                      // already within a command: do not test here
            bool lastIsSemiColon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
            if (!lastIsSemiColon && (_lastTokenType != tok_no_token)) {
                pNext = pch; result = result_commandNotAllowedHere; return false;                               // keyword only at start of a statement (not within an expression)
            }
        }

        if (_leadingSpaceCheck) { pNext = pch; result = result_spaceMissing; return false; }
        _tokenIndex = commandIndex;                                                                             // needed in case it's the start of a command (to determine parameters)

        // token is an external command name, and it's allowed here

        // expression syntax check 
        _thisLvl_lastIsVariable = false;
        _thislvl_lastIsConstVar = false;
        _thisLvl_assignmentStillPossible = true;                                                                // reset (expression may follow)                          

        // command argument constraints check: reset for next command parameter
        _lvl0_withinExpression = false;
        _lvl0_isPurePrefixIncrDecr = false;
        _lvl0_isPureVariable = false;
        _lvl0_isVarWithAssignment = false;

        Token_externalCommand* pToken = (Token_externalCommand*)_programCounter;
        pToken->tokenType = tok_isExternCommand | (sizeof(Token_externalCommand) << 4);
        pToken->tokenIndex = commandIndex;

        _lastTokenStep = _programCounter - _programStorage;
        _lastTokenType = tok_isExternCommand;
        _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

    #if PRINT_PARSED_TOKENS
        _pDebugOut->print("   parsed keyword: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); _pDebugOut->print(_internCommands[commandIndex]._commandName);  _pDebugOut->println("]");
        _pDebugOut->print("   token (res.word) index = "); _pDebugOut->println(commandIndex);
    #endif

        _programCounter += sizeof(Token_externalCommand);
        *_programCounter = tok_no_token;                                                                        // indicates end of program
        result = result_parsing_OK;                                                                             // flag 'valid token found'
        return true;
    }

    pNext = pch;                                                                                                // reset pointer to first character to parse (because no token was found)
    return true;                                                                                                // token is not a keyword (but can still be something else)
}


// ------------------------------------------------
// *   try to parse next characters as a number   *
// ------------------------------------------------

bool Justina::parseAsNumber(char*& pNext, parsingResult_type& result) {
    result = result_tokenNotFound;                                                                              // init: flag 'no token found'
    char* pch = pNext;                                                                                          // pointer to first character to parse (any spaces have been skipped already)

    // try to parse as number (int or float)
    Val value; char valueType{};
    int predefinedConstIndex{};

    if (!parseIntFloat(pNext, pch, value, valueType, predefinedConstIndex, result)) { return false; }           // if returning with error, 'result' contains error number
    if (result != result_parsing_OK) { return true; }                                                           // is not a number, but can still be another valid token

    float flt{ 0 }; long lng{ 0 };
    if (valueType == value_isLong) { lng = value.longConst; }
    else { flt = value.floatConst; }

    // is valid number: continue processing

    if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }  // program mode and no PROGRAM command
    // token is a number constant, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_numConstNotAllowedHere; return false; }
    if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_numConstNotAllowedHere; return false; }

    // overflow ? (underflow is not detected with strtof() ) 
    if (valueType == value_isFloat) { if (!isfinite(flt)) { pNext = pch; result = result_parse_overflow; return false; } }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
    if (!tokenAllowed) { pNext = pch; result = result_numConstNotAllowedHere; return false; ; }

    // is a variable required instead of a constant ?
    bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
    varRequired = varRequired || (_isConstVarCmd && ((_lastTokenIsTerminal) ? (_lastTermCode == termcod_comma) : (_lastTokenType == tok_isInternCommand))); // tok_isInternCommand: the CONST keyword itself
    if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }

    // Function command: check that constant can only appear after an equal sign
    // (in a variable declaration statement (VAR,...), this is handled by the keyword 'allowed command parameter' key)
    // Note: in a (variable or parameter) declaration statement, operators other than assignment operators are not allowed, which is detected in terminal token parsing
    bool lastIsPureAssignmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;             // not a compound assignment
    if (_isJustinaFunctionCmd && !lastIsPureAssignmentOp) { pNext = pch; result = result_numConstNotAllowedHere; return false; }

    // array declaration: dimensions must be number constants (global, static, local arrays)
    bool isArrayDimSpec = _isAnyVarCmd && (_parenthesisLevel > 0);
    if (isArrayDimSpec) {
        if (valueType == value_isLong && (lng < 1)) { pNext = pch; result = result_arrayDef_dimNotValid; return false; }
        else if ((valueType == value_isFloat) && ((flt != int(flt)) || (flt < 1))) { pNext = pch; result = result_arrayDef_dimNotValid; return false; }
    }

    // token is a number, and it's allowed here

    // expression syntax check 
    _thisLvl_lastIsVariable = false;
    _thislvl_lastIsConstVar = false;

    // command argument constraints check
    _lvl0_withinExpression = true;

    Token_symbolicConstant* pToken = (Token_symbolicConstant*)_programCounter;                                  // OK for literal constants as well
    pToken->tokenType = ((predefinedConstIndex == -1) ? tok_isConstant : tok_isSymbolicConstant) | (valueType << 4);
    if (valueType == value_isLong) { memcpy(pToken->cstValue.longConst, &lng, sizeof(lng)); }
    else { memcpy(pToken->cstValue.floatConst, &flt, sizeof(flt)); }                                            // float not necessarily aligned with word size: copy memory instead
    if (predefinedConstIndex >= 0) { pToken->nameIndex = predefinedConstIndex; }
    _lastTokenStep = _programCounter - _programStorage;                                                         // before referencing _lastTokenStep

    _lastTokenType = (predefinedConstIndex == -1) ? tok_isConstant : tok_isSymbolicConstant;
    _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && lastIsPureAssignmentOp);
    if (doNonLocalVarInit) { initVariable(_lastVariableTokenStep, _lastTokenStep); }                            // initialization of global / static variable ? (operator: is always assignment)

#if PRINT_PARSED_TOKENS
    _pDebugOut->print("   parsed number: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); if (valueType == value_isLong) { _pDebugOut->print(lng); }
    else { _pDebugOut->print(flt); }  _pDebugOut->println("]");
#endif

    _programCounter += (predefinedConstIndex == -1) ? sizeof(Token_constant) : sizeof(Token_symbolicConstant);
    *_programCounter = tok_no_token;                                                                            // indicates end of program
    result = result_parsing_OK;                                                                                 // flag 'valid token found'
    return true;
}


// ----------------------------------------------------------------
// *   try to parse next characters as an alphanumeric constant   *
// ----------------------------------------------------------------

bool Justina::parseAsStringConstant(char*& pNext, parsingResult_type& result) {
    result = result_tokenNotFound;                                                                              // init: flag 'no token found'
    char* pch = pNext;                                                                                          // pointer to first character to parse (any spaces have been skipped already)
    // no opening quote and not an alpha character ? Is not a string literal not a symbolic string constant (it can still be something else)
    if ((pNext[0] != '\"') && !isalpha(pNext[0])) { return true; }

    // try to parse as string now
    char* pStringCst = nullptr;                                                                                 // init: is empty string (prevent creating a string object to conserve memory)
    char valueType; //dummy
    int predefinedConstIndex{};

    bool isPureAssignmentOp{ false };

    // string is parsed here, because next error messages suppose it's an alphanumeric constant
    if (!parseString(pNext, pch, pStringCst, valueType, predefinedConstIndex, result, false)) { return false; } // error (before a parsed string is created)
    if (result != result_parsing_OK) { return true; }              // is a symbolic constant but NOT a symbolic string constant: continue parsing (no error)

    // if not a symbolic constant, then a parsed string (object) has been created: in case of an error (in next lines) it must be deleted again
    result = result_tokenNotFound;                                                                              // init: flag 'no token found'
    do {
        if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; break; }  // program mode and no PROGRAM command

        // token is an alphanumeric constant, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_alphaConstNotAllowedHere; break; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_alphaConstNotAllowedHere; break; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        // can only happen with initializer, if constant string is preceded by unary plus or minus operator
        if (_initVarOrParWithUnaryOp != 0) { pNext = pch; result = result_alphaConstNotAllowedHere; break; }
        bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
        if (!tokenAllowed) { pNext = pch; result = result_alphaConstNotAllowedHere; break; }

        // is a variable required instead of a constant ?
        bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
        varRequired = varRequired || (_isConstVarCmd && ((_lastTokenIsTerminal) ? (_lastTermCode == termcod_comma) : (_lastTokenType == tok_isInternCommand))); // tok_isInternCommand: the CONST keyword itself
        if (varRequired) { pNext = pch; result = result_variableNameExpected; break; }

        // Function command: check that constant can only appear after an equal sign
        // (in a variable declaration statement (VAR,...), this is handled by the keyword 'allowed command parameter' key)
        // Note: in a (variable or parameter) declaration statement, operators other than assignment operators are not allowed, which is detected in terminal token parsing
        isPureAssignmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;                  // not a compound assignment
        if (_isJustinaFunctionCmd && !isPureAssignmentOp) { pNext = pch; result = result_alphaConstNotAllowedHere; break; }

        // array declaration: dimensions must be number constants (global, static, local arrays)
        bool isArrayDimSpec = _isAnyVarCmd && (_parenthesisLevel > 0);
        if (isArrayDimSpec) { pNext = pch; result = result_arrayDef_dimNotValid; break; }

        if (_leadingSpaceCheck) { pNext = pch; result = result_spaceMissing; break; }
    } while (false);

    if (result != result_tokenNotFound) {
        if (predefinedConstIndex == -1) {                       // an ordinary (not a symbolic) parsed string constant ? Delete the string object
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n----- (parsed str ) ");   _pDebugOut->println((uint32_t)pStringCst, HEX);
            _pDebugOut->print("  parse str cst (1) ");   _pDebugOut->println(pStringCst);
        #endif
            _parsedStringConstObjectCount--;
            delete[] pStringCst;
        }
        pNext = pch;  return false;
    }

    // expression syntax check 
    _thisLvl_lastIsVariable = false;
    _thislvl_lastIsConstVar = false;

    // command argument constraints check
    _lvl0_withinExpression = true;

    Token_symbolicConstant* pToken = (Token_symbolicConstant*)_programCounter;                                  // OK for literal constants as well
    pToken->tokenType = ((predefinedConstIndex == -1) ? tok_isConstant : tok_isSymbolicConstant) | (valueType << 4);
    memcpy(pToken->cstValue.pStringConst, &pStringCst, sizeof(pStringCst));                                     // pointer not necessarily aligned with word size: copy pointer instead
    if (predefinedConstIndex >= 0) { pToken->nameIndex = predefinedConstIndex; }
    _lastTokenStep = _programCounter - _programStorage;

    _lastTokenType = (predefinedConstIndex == -1) ? tok_isConstant : tok_isSymbolicConstant;
    _lastTokenIsString = true, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

    bool isLocalVarInitCheck = (_isLocalVarCmd && isPureAssignmentOp);
    bool doNonLocalVarInit = ((_isGlobalOrUserVarCmd || _isStaticVarCmd) && isPureAssignmentOp);                // (operator: is always assignment)
    bool isArrayVar = ((Token_variable*)(_programStorage + _lastVariableTokenStep))->identInfo & var_isArray;

    if (isLocalVarInitCheck && isArrayVar && (pStringCst != nullptr)) {
        result = result_arrayDef_emptyInitStringExpected;                                                       // only check (init when function is called)
    }
    if (doNonLocalVarInit) {                                                                                    // initialization of global / static variable ? 
        if (!initVariable(_lastVariableTokenStep, _lastTokenStep)) {
            result = result_arrayDef_emptyInitStringExpected;
        };
    }

    if (result == result_arrayDef_emptyInitStringExpected) {
        if (predefinedConstIndex == -1) {                       // an ordinary (not a symbolic) parsed string constant ? Delete the string object
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n----- (parsed str ) ");   _pDebugOut->println((uint32_t)pStringCst, HEX);
            _pDebugOut->print("  parse str cst (2) ");   _pDebugOut->println(pStringCst);
        #endif
            _parsedStringConstObjectCount--;
            delete[] pStringCst;
        }
        pToken->tokenType = tok_no_token;       // because already set
        pNext = pch;  return false;
    }

#if PRINT_PARSED_TOKENS
    _pDebugOut->print("   parsed string: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ['"); _pDebugOut->print(pStringCst);  _pDebugOut->println("']");
#endif

    _programCounter += (predefinedConstIndex == -1) ? sizeof(Token_constant) : sizeof(Token_symbolicConstant);
    *_programCounter = tok_no_token;                                                                            // indicates end of program
    result = result_parsing_OK;                                                                                 // flag 'valid token found'

    return true;
}


// ---------------------------------------------------------------------------------
// *   try to parse next characters as a terminal token (one- or two-characters)   *
// ---------------------------------------------------------------------------------

bool Justina::parseTerminalToken(char*& pNext, parsingResult_type& result) {

    // Justina function definition statement parsing: count number of mandatory and optional arguments in function definition for storage
    static int justinaFunctionDef_minArgCounter{ 0 };
    static int justinaFunctionDef_maxArgCounter{ 0 };

    // array definition statement parsing: record dimensions (if 1 dimension only: set dim 2 to zero) 
    static int array_dimCounter{ 0 };
    static int arrayDef_dims[MAX_ARRAY_DIMS]{ 0 };

    result = result_tokenNotFound;                                                                              // init: flag 'no token found'
    char* pch = pNext;                                                                                          // pointer to first character to parse (any spaces have been skipped already)
    int termIndex{};

    // for all defined terminal symbolic names: check against alphanumeric token. Search, starting at the end (because of the ordering of names in the _terminals array)
    for (termIndex = _termTokenCount - 1; termIndex >= 0; termIndex--) {
        int len = strlen(_terminals[termIndex].terminalName);                                                   // token has correct length ? If not, skip remainder of loop ('continue')                            
        // do not look for trailing space, to use strncmp() with number of non-space characters found, because a space is not required after an operator
        if (strncmp(_terminals[termIndex].terminalName, pch, len) == 0) { break; }                              // token corresponds to terminal name ? Then exit loop    
    }
    if (termIndex < 0) { return true; }                                                                         // token is not a one-character token (and it's not a two-char token, because these start with same character)
    pNext += strlen(_terminals[termIndex].terminalName);                                                        // move to first character after terminal name

    int nextTermIndex;  // peek: is next token a terminal ? nextTermIndex will be -1 if not
    char* peek = pNext;     // first character of next token (or '\0')
    while (peek[0] == ' ') { peek++; }
    for (nextTermIndex = _termTokenCount - 1; nextTermIndex >= 0; nextTermIndex--) {                            // for all defined terminal symbolic names: check against alphanumeric token (NOT ending by '\0')
        int len = strlen(_terminals[nextTermIndex].terminalName);                                               // token has correct length ? If not, skip remainder of loop ('continue')                            
        // do not look for trailing space, to use strncmp() with number of non-space characters found, because a space is not required after an operator
        if (strncmp(_terminals[nextTermIndex].terminalName, peek, len) == 0) { break; }                         // token corresponds to terminal name ? Then exit loop   
    }


    tokenType_type tokenType;
    uint8_t flags{ B0 }, flags2{ B0 };

    switch (_terminals[termIndex].terminalCode) {

        case termcod_leftPar:
        {
            // -------------------------------------
            // Case 1: is token a left parenthesis ?
            // -------------------------------------

            if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }   // program mode and no PROGRAM command

            // token is left parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
            if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_5_4_2_1_0)) { pNext = pch;  result = result_parenthesisNotAllowedHere; return false; }
            if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

            // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
            bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
            if (!tokenAllowed) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }

            if (_isAnyVarCmd && (_parenthesisLevel > 0)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }  // no parenthesis nesting in array declarations
            // parenthesis nesting in function definitions, only to declare an array parameter AND only if followed by a closing parenthesis 
            if ((_isJustinaFunctionCmd) && (_parenthesisLevel > 0) && (_lastTokenType != tok_isVariable)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }
            if (_isProgramCmd || _isDeleteVarCmd) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

            bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
            if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }

            if (_leadingSpaceCheck) { pNext = pch; result = result_spaceMissing; return false; }

            // token is a left parenthesis, and it's allowed here

            // store specific flags in stack, because if nesting functions or parentheses, values will be overwritten
            flags = (_lastTokenType == tok_isJustinaFunction) ? JustinaFunctionBit :
                (_lastTokenType == tok_isInternCppFunction) ? internCppFunctionBit :
                (_lastTokenType == tok_isExternCppFunction) ? externCppFunctionBit :
                (_lastTokenType == tok_isVariable) ? arrayBit : openParenthesisBit;                             // is it following an internal cpp, external cpp or Justina function name ?

            // Justina function (call or definition) opening parenthesis
            if (_lastTokenType == tok_isJustinaFunction) {
                if (justinaFunctionData[_functionIndex].pJustinaFunctionStartToken != nullptr) { flags = flags | JustinaFunctionPrevDefinedBit; }
            }

            // expression syntax check 
            _thisLvl_lastIsVariable = false;       // currently open block

            if (_thislvl_lastIsConstVar) { flags2 |= varIsConstantBit; }
            _thislvl_lastIsConstVar = false;

            if (_thisLvl_assignmentStillPossible) { flags = flags | varAssignmentAllowedBit; }                  // remember if array element can be assigned to (after closing parenthesis)
            _thisLvl_assignmentStillPossible = true;                                                            // array subscripts: reset assignment allowed flag (init)

            if (_thisLvl_lastOpIsIncrDecr) { flags = flags | varHasPrefixIncrDecrBit; }                         // remember if array element has a prefix incr/decr operator (before opening parenthesis) 
            _thisLvl_lastOpIsIncrDecr = false;                                                                  // array subscripts: reset assignment allowed flag 

            // command argument constraints check
            _lvl0_withinExpression = true;

            // if function DEFINITION: initialize variables for counting of allowed mandatory and optional arguments (not an array parameter, would be parenthesis level 1)
            if (_isJustinaFunctionCmd && (_parenthesisLevel == 0)) {                                            // not an array parameter (would be parenthesis level 1)
                justinaFunctionDef_minArgCounter = 0;
                justinaFunctionDef_maxArgCounter = 0;                                                           // init count; range from 0 to a hard coded maximum 
            }

            if (_isJustinaFunctionCmd && (_parenthesisLevel == 1)) {                                            // array parameter (would be parenthesis level 1)
                if (peek[0] != term_rightPar[0]) { pNext = pch; result = result_function_arrayParamMustHaveEmptyDims; return false; }
            }

            // if LOCAL, STATIC or GLOBAL array DEFINITION or USE (NOT: parameter array): initialize variables for reading dimensions 
            if (flags & arrayBit) {                                                                             // always count, also if not first definition (could happen for global variables)
                if (_varIsConstant) { pNext = pch; result = result_var_constantArrayNotAllowed; return false; }
                array_dimCounter = 0;
                for (int i = 0; i < MAX_ARRAY_DIMS; i++) { arrayDef_dims[i] = 0; }                              // init dimensions (dimension count will result from dimensions being non-zero
            }

            // left parenthesis only ? (not a function or array opening parenthesis): min & max allowed argument count not yet initialized
            if (_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_5) {
                _minFunctionArgs = 1;                                                                           // initialize min & max allowed argument count to 1
                _maxFunctionArgs = 1;
            }

            // min & max argument count: either allowed range (if function previously defined), current range of actual args counts (if previous calls only), or not initialized
            _parenthesisLevel++;                                                                                // increment stack counter and create corresponding list element
            _pParsingStack = (LE_parsingStack*)parsingStack.appendListElement(sizeof(LE_parsingStack));
            _pParsingStack->openPar.minArgs = _minFunctionArgs;
            _pParsingStack->openPar.maxArgs = _maxFunctionArgs;
            _pParsingStack->openPar.actualArgsOrDims = 0;
            // dimensions of previously defined array. If zero, then this array did not yet exist, or it's a scalar variable
            _pParsingStack->openPar.arrayDimCount = _arrayDimCount;
            _pParsingStack->openPar.flags = flags;

            // NOTE: _functionIndex is not needed for external cpp functions (for internal functions, it's used to check the arguments array pattern)
            _pParsingStack->openPar.identifierIndex = (_lastTokenType == tok_isInternCppFunction) ? _functionIndex :
                (_lastTokenType == tok_isJustinaFunction) ? _functionIndex :
                (_lastTokenType == tok_isVariable) ? _variableNameIndex : 0;
            _pParsingStack->openPar.variableScope = _variableScope;

            _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;
        }
        break;


        case termcod_rightPar:
        {
            // --------------------------------------
            // Case 2: is token a right parenthesis ?
            // --------------------------------------

            if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }   // program mode and no PROGRAM command

            // token is right parenthesis, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
            if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_5_3_0)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }
            if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; }

            // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
            bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
            if (!tokenAllowed) { pNext = pch; result = result_parenthesisNotAllowedHere; return false; ; }
            if (_parenthesisLevel == 0) { pNext = pch; result = result_missingLeftParenthesis; return false; }

            flags = _pParsingStack->openPar.flags;

            // expression syntax check 
            _thisLvl_lastIsVariable = (flags & arrayBit);                                                                   // note: parameter array (empty parenthesis): array bit not set
            _thislvl_lastIsConstVar = (flags2 & varIsConstantBit);
            _thisLvl_assignmentStillPossible = (flags & varAssignmentAllowedBit);                                           // array subscripts: retrieve assignment allowed flag 
            _thisLvl_lastOpIsIncrDecr = (flags & varHasPrefixIncrDecrBit);


            // 2.1 Justina function definition (not a call), OR array parameter definition closing parenthesis ?
            // -------------------------------------------------------------------------------------------------

            if (_isJustinaFunctionCmd) {
                if (_parenthesisLevel == 1) {                                                                               // function definition closing parenthesis
                    // stack level will not change until closing parenthesis (because within definition, no nesting of parenthesis is possible)
                    // stack min & max values: current range of args counts that occurred in previous calls (not initialized if no earlier calls occurred)

                    // if empty function parameter list, then do not increment parameter count (function taking no parameters)

                    bool emptyParamList = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;                // OK because no nesting allowed
                    _pParsingStack->openPar.actualArgsOrDims += (emptyParamList ? 0 : 1);

                    // check order of mandatory and optional arguments, check if max. n° not exceeded
                    if (!emptyParamList) { if (!checkJustinaFunctionArguments(result, justinaFunctionDef_minArgCounter, justinaFunctionDef_maxArgCounter, true)) { pNext = pch; return false; }; }

                    int funcIndex = _pParsingStack->openPar.identifierIndex;                                                // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
                    // if previous calls, check if range of actual argument counts that occurred in previous calls corresponds to mandatory and optional arguments defined now
                    bool previousCalls = (JustinaFunctionNames[funcIndex][MAX_IDENT_NAME_LEN + 1]) != c_JustinaFunctionFirstOccurFlag;
                    if (previousCalls) {                                                                                    // stack contains current range of actual args occurred in previous calls
                        if (((int)_pParsingStack->openPar.minArgs < justinaFunctionDef_minArgCounter) ||
                            (int)_pParsingStack->openPar.maxArgs > justinaFunctionDef_maxArgCounter) {
                            pNext = pch; result = result_function_prevCallsWrongArgCount; return false;                     // argument count in previous calls to this function does not correspond 
                        }
                    }

                    // store min required & max allowed n� of arguments in identifier storage
                    // this replaces the range of actual argument counts that occurred in previous calls (if any)
                    JustinaFunctionNames[funcIndex][MAX_IDENT_NAME_LEN + 1] = (justinaFunctionDef_minArgCounter << 4) | (justinaFunctionDef_maxArgCounter);

                    // check that order of arrays and scalar variables is consistent with previous calls and function definition
                    if (!checkJustinaFuncArgArrayPattern(result, true)) { pNext = pch; return false; };                     // verify that the order of scalar and array parameters is consistent with arguments
                }
            }


            // 2.2 Array definition closing parenthesis ?
            // ------------------------------------------

            else if (_isAnyVarCmd) {                                                                                        // note: parenthesis level is 1 (because no inner parenthesis allowed)
                if (!checkArrayDimCountAndSize(result, arrayDef_dims, array_dimCounter)) { pNext = pch; return false; }

                int varNameIndex = _pParsingStack->openPar.identifierIndex;
                uint8_t varQualifier = _pParsingStack->openPar.variableScope;

                bool isUserVar = (varQualifier == var_isUser);
                bool isGlobalVar = (varQualifier == var_isGlobal);
                bool isStaticVar = (varQualifier == var_isStaticInFunc);
                bool isLocalVar = (varQualifier == var_isLocalInFunc);                                                      // but not function parameter definitions

                float* pArray;
                int arrayElements = 1;              // init
                int valueIndex = (isUserVar || isGlobalVar) ? varNameIndex : programVarValueIndex[varNameIndex];

                // user, global and static arrays: create array on the heap. Array dimensions will be stored in array element 0

                bool arrayWithAssignmentOp = (nextTermIndex < 0) ? false : _terminals[nextTermIndex].terminalCode == termcod_assign;
                bool arrayWithoutInitializer = (nextTermIndex < 0) ? false : ((_terminals[nextTermIndex].terminalCode == termcod_comma) ||
                    (_terminals[nextTermIndex].terminalCode == termcod_semicolon));
                if (!arrayWithAssignmentOp && !arrayWithoutInitializer) {
                    if (isUserVar) {
                        // consider user variable not created (relevant for user variables only, because program variables are destroyed anyway if parsing fails)
                        _userVarCount--;
                        _userVarUnderConstruction = false;
                    }
                    pNext = pch; result = result_assignmentOrSeparatorExpected; return false;
                }

                if (isUserVar || isGlobalVar || isStaticVar) {
                    for (int dimCnt = 0; dimCnt < array_dimCounter; dimCnt++) { arrayElements *= arrayDef_dims[dimCnt]; }
                    isUserVar ? _userArrayObjectCount++ : _globalStaticArrayObjectCount++;
                    pArray = new float[arrayElements + 1];
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print(isUserVar ? "\r\n+++++ (usr ar stor) " : "\r\n+++++ (array stor ) "); _pDebugOut->println((uint32_t)pArray, HEX);
                #endif

                    if (!arrayWithAssignmentOp) {                                                                           // no explicit initializer: initialize now (as real) 
                        for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) { ((float*)pArray)[arrayElem] = 0.; }
                    }

                    // only now, the array flag can be set, because only now the object exists
                    if (isUserVar) {
                        userVarValues[valueIndex].pArray = pArray;
                        userVarType[varNameIndex] |= var_isArray;                                                           // set array bit
                    }
                    else if (isGlobalVar) {
                        globalVarValues[valueIndex].pArray = pArray;
                        globalVarType[varNameIndex] |= var_isArray;                                                         // set array bit
                    }
                    else if (isStaticVar) {
                        staticVarValues[valueIndex].pArray = pArray;
                        staticVarType[_staticVarCount - 1] |= var_isArray;                                                  // set array bit
                    }
                }

                // local arrays (note: NOT for function parameter arrays): set pointer to dimension storage 
                // the array flag has been set when local variable was created (including function parameters, which are also local variables)
                // dimensions are not stored in array value array (because created at runtime) but are temporarily stored here during function parsing  
                else if (isLocalVar) {
                    pArray = (float*)localVarDims[_localVarCountInFunction - 1];
                }

                // global, static and local arrays: store array dimensions (local arrays: temporary storage during parsing only)
                // store dimensions in element 0: char 0 to 2 is dimensions; char 3 = dimension count 
                for (int i = 0; i < MAX_ARRAY_DIMS; i++) {
                    ((char*)pArray)[i] = arrayDef_dims[i];
                }
                ((char*)pArray)[3] = array_dimCounter;  // (note: for param arrays, set to max dimension count during parsing)
            }


            // 2.3 Internal cpp, external cpp or Justina function call, or parenthesis pair, closing parenthesis ?
            // ---------------------------------------------------------------------------------------------------

            else if (flags & (internCppFunctionBit | externCppFunctionBit | JustinaFunctionBit | openParenthesisBit)) {

                // if empty function call argument list, then do not increment argument count (function call without arguments)
                bool emptyArgList = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;                      // OK because no nesting allowed
                _pParsingStack->openPar.actualArgsOrDims += (emptyArgList ? 0 : 1);
                int actualArgs = (int)_pParsingStack->openPar.actualArgsOrDims;
                bool callToNotYetDefinedFunc = ((flags & (JustinaFunctionBit | JustinaFunctionPrevDefinedBit)) == JustinaFunctionBit);

                // call to not yet defined Justina function ? update minimum and maximum argument count across all these calls (there might be previous calls)
                if (callToNotYetDefinedFunc) {
                    // if at least one previous call (maybe a nested call) is completely parsed, retrieve current range of actual args that occurred in these previous calls
                    // and update this range with the argument count of the current Justina function call that is at its closing parenthesis
                    int funcIndex = _pParsingStack->openPar.identifierIndex;                                                // of current function call: stored in stack for current PARENTHESIS level
                    bool prevJusinaFuncCompletelyParsed = (JustinaFunctionNames[funcIndex][MAX_IDENT_NAME_LEN + 1]) != c_JustinaFunctionFirstOccurFlag;
                    if (prevJusinaFuncCompletelyParsed) {
                        _pParsingStack->openPar.minArgs = ((JustinaFunctionNames[funcIndex][MAX_IDENT_NAME_LEN + 1]) >> 4) & 0x0F;
                        _pParsingStack->openPar.maxArgs = (JustinaFunctionNames[funcIndex][MAX_IDENT_NAME_LEN + 1]) & 0x0F;
                        if ((int)_pParsingStack->openPar.minArgs > actualArgs) { _pParsingStack->openPar.minArgs = actualArgs; }
                        if ((int)_pParsingStack->openPar.maxArgs < actualArgs) { _pParsingStack->openPar.maxArgs = actualArgs; }
                    }
                    // no previous call: simply set this range to the argument count of the current Justina function call that is at its closing parenthesis
                    else { _pParsingStack->openPar.minArgs = actualArgs; _pParsingStack->openPar.maxArgs = actualArgs; }

                    // store the up to date range of actual argument counts in identifier storage
                    JustinaFunctionNames[funcIndex][MAX_IDENT_NAME_LEN + 1] = (_pParsingStack->openPar.minArgs << 4) | (_pParsingStack->openPar.maxArgs);
                }

                // if call to previously defined Justina function, to an internal or external cpp function, or if open parenthesis, then check argument count 
                else {
                    bool isOpenParenthesis = (flags & openParenthesisBit);
                    if (isOpenParenthesis) { _pParsingStack->openPar.minArgs = 1; _pParsingStack->openPar.maxArgs = 1; }
                    bool argCountTooLow = ((actualArgs < (int)_pParsingStack->openPar.minArgs));
                    bool argCountTooHigh = ((actualArgs > (int)_pParsingStack->openPar.maxArgs));   // possible if no arguments allowed and one argument provided
                    if (argCountTooLow && isOpenParenthesis) { pNext = pch; result = result_missingExpression; return false; }
                    else if (argCountTooLow || argCountTooHigh) { pNext = pch; result = result_function_wrongArgCount; return false; }
                }

                // if this is a function call to a Justina 'procedure' (not returning a value) defined previously: check that it is not the start of an expression
                if (flags & JustinaFunctionBit) {
                    int index = _pParsingStack->openPar.identifierIndex;
                    // semicolon expected as next token
                    bool thisCallIsPartOfExpr = ((nextTermIndex < 0) ? true : (_terminals[nextTermIndex].terminalCode != termcod_semicolon));
                    if (thisCallIsPartOfExpr) {
                        justinaFunctionData[index].callsAsPartOfExpression = 1;
                        // is this a call to a PROCEDURE defined PRIOR to this call ? Then this call is part of an expression (this is case B: the expression starts with the call)
                        if (justinaFunctionData[index].isVoidFunctionDef == 1) { pNext = pch; result = result_function_cannotUseProceduresInExpression; return false; }
                    }
                }

                // check that order of arrays and scalar variables is consistent with function definition and (Justina functions only: with previous calls) 
                if (flags & internCppFunctionBit) { if (!checkInternCppFuncArgArrayPattern(result)) { pNext = pch; return false; }; }
                if (flags & externCppFunctionBit) { if (!checkExternCppFuncArgIsScalar(result)) { pNext = pch; return false; }; }
                else if (flags & JustinaFunctionBit) { if (!checkJustinaFuncArgArrayPattern(result, true)) { pNext = pch; return false; }; }
            }


            // 2.4 Array subscripts closing parenthesis ?
            // ------------------------------------------

            else if (flags & arrayBit) {
                // check if array dimension count corresponds (individual dimension adherence can only be checked at runtime)
                // for function parameters, array dimension count can only be checked at runtime as well
                // if previous token is left parenthesis (' () '), then do not increment argument count
                bool lastWasLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;        // OK because no nesting allowed
                if (!lastWasLeftPar) { _pParsingStack->openPar.actualArgsOrDims++; }

                int varNameIndex = _pParsingStack->openPar.identifierIndex;
                uint8_t varScope = _pParsingStack->openPar.variableScope;
                bool isParam = (varScope == var_isParamInFunc);
                int actualDimCount = _pParsingStack->openPar.actualArgsOrDims;

                if (actualDimCount == 0) { pNext = pch; result = result_arrayUse_noDims; return false; }        // dim count too high: already handled when preceding comma was parsed
                if (!isParam) {
                    if (actualDimCount != (int)_pParsingStack->openPar.arrayDimCount) { pNext = pch; result = result_arrayUse_wrongDimCount; return false; }
                }
            }

            else {}                                                                                             // for documentation only: all cases handled


            // token is a right parenthesis, and it's allowed here

            parsingStack.deleteListElement(nullptr);                                                            // decrement open parenthesis stack counter and delete corresponding list element
            _parenthesisLevel--;

            // set pointer to currently last element in stack
            if (_blockLevel + _parenthesisLevel > 0) { _pParsingStack = (LE_parsingStack*)parsingStack.getLastListElement(); }

            _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;
        }
        break;


        case termcod_comma:
        {
            // ------------------------------------
            // Case 3: is token a comma separator ?
            // ------------------------------------

            if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }   // program mode and no PROGRAM command

            // token is comma separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
            if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_0)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }
            if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

            // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
            bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
            if (!tokenAllowed) { pNext = pch; result = result_separatorNotAllowedHere; return false; ; }

            // if no open parenthesis, a comma can only occur to separate command parameters
            if ((_parenthesisLevel == 0) && !_isCommand) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

            flags = (_parenthesisLevel > 0) ? _pParsingStack->openPar.flags : 0;

            // expression syntax check 
            _thisLvl_lastIsVariable = false;                                                                    // currently open block, new expression
            _thislvl_lastIsConstVar = false;
            _thisLvl_assignmentStillPossible = true;                                                            // init (start of (sub-)expression)
            _thisLvl_lastOpIsIncrDecr = false;

            // command argument constraints check: reset for next command argument (if within a command)
            if (_parenthesisLevel == 0) {
                _lvl0_withinExpression = false;
                _lvl0_isPurePrefixIncrDecr = false;
                _lvl0_isPureVariable = false;
                _lvl0_isVarWithAssignment = false;
            }

            _initVarOrParWithUnaryOp = 0;   // reset (needed for Function definitions with multiple parameters) 


            // 3.1 Justina function definition (not a call) parameter separator ?
            // ------------------------------------------------------------------

            if (_isJustinaFunctionCmd) {
                if (_parenthesisLevel == 1) {                                                                   // not an array parameter (would be parenthesis level 2)
                    _pParsingStack->openPar.actualArgsOrDims++;
                    // check order of mandatory and optional arguments, check if max. n� not exceeded
                    if (!checkJustinaFunctionArguments(result, justinaFunctionDef_minArgCounter, justinaFunctionDef_maxArgCounter, false)) { pNext = pch; return false; };

                    // Check order of mandatory and optional arguments (function: parenthesis levels > 0)
                    if (!checkJustinaFuncArgArrayPattern(result, false)) { pNext = pch; return false; };        // verify that the order of scalar and array parameters is consistent with arguments
                }
            }


            // 3.2 Array definition dimension separator ? 
            // ------------------------------------------

            else if (_isAnyVarCmd) {
                // parenthesis level 1: separator between array dimensions (level 0: sep. between variables)
                if (_parenthesisLevel == 1) {
                    // Check dimension count and array size 
                    if (!checkArrayDimCountAndSize(result, arrayDef_dims, array_dimCounter)) { pNext = pch; return false; }
                }
                else if ((_lastTokenType == tok_isVariable) && _varIsConstant) { result = result_var_constantVarNeedsAssignment; pNext = pch; return false; }
            }


            // 3.3 Internal cpp, external cpp or Justina function call argument separator ?
            // -------------------------------------------------------------------------------

            else if (flags & (internCppFunctionBit | externCppFunctionBit | JustinaFunctionBit | openParenthesisBit)) {

                _pParsingStack->openPar.actualArgsOrDims++;                                                     // include argument before the comma in argument count     
                int actualArgs = (int)_pParsingStack->openPar.actualArgsOrDims;
                bool callToNotYetDefinedFunc = ((_pParsingStack->openPar.flags & (JustinaFunctionBit | JustinaFunctionPrevDefinedBit)) == JustinaFunctionBit);

                // check that argument count does not exceed absolute maximum defined in header file for each function type
                if (!(flags & openParenthesisBit)) {                                                            // checked further down; check here would be redundant
                    if (actualArgs >= ((flags & JustinaFunctionBit) ? c_JustinaFunctionMaxArgs : (flags & internCppFunctionBit) ? c_internalFncOrCmdMaxArgs : c_ExternalFncOrCmdMaxArgs)) {
                        pNext = pch; result = result_function_maxArgsExceeded; return false;
                    }
                }

                // call to NOT YET  defined Justina function ? (and there might be previous calls as well)
                if (callToNotYetDefinedFunc) {}     // nothing to do yet (we have no information yet about minimum and maximum number of arguments that will be defined in the function definition)

                // if value between parentheses, OR if call to previously defined Justina function, to an internal or external cpp function: check argument count versus function definition parameter count
                else {
                    bool isOpenParenthesis = (flags & openParenthesisBit);
                    if (isOpenParenthesis) { _pParsingStack->openPar.minArgs = 1; _pParsingStack->openPar.maxArgs = 1; }
                    bool argCountTooHigh = (actualArgs >= (int)_pParsingStack->openPar.maxArgs);                // check against allowed maximum number of arguments for this function
                    if (argCountTooHigh) { pNext = pch; result = isOpenParenthesis ? result_missingRightParenthesis : result_function_wrongArgCount; return false; }
                }

                // check that order of arrays and scalar variables is consistent with function definition and (Justina functions only: with previous calls) 
                if (flags & internCppFunctionBit) { if (!checkInternCppFuncArgArrayPattern(result)) { pNext = pch; return false; }; }
                if (flags & externCppFunctionBit) { if (!checkExternCppFuncArgIsScalar(result)) { pNext = pch; return false; }; }
                else if (flags & JustinaFunctionBit) { if (!checkJustinaFuncArgArrayPattern(result, false)) { pNext = pch; return false; }; }
            }


            // 3.4 Array subscript separator ?
            // ----------------------------------

            else if (flags & arrayBit) {
                // check if array dimension count corresponds (individual boundary adherence can only be checked at runtime)
                _pParsingStack->openPar.actualArgsOrDims++;
                if ((int)_pParsingStack->openPar.actualArgsOrDims == (int)_pParsingStack->openPar.arrayDimCount) { pNext = pch; result = result_arrayUse_wrongDimCount; return false; }
            }

            else {}                                                                                             // for documentation only: all cases handled

            // token is a comma separator, and it's allowed here
            _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

            if (_parenthesisLevel == 0) { _userVarUnderConstruction = false; }                                  // if a var was under construction, it has been created now without errors

        }
        break;


        case termcod_semicolon:
        {
            // ----------------------------------------
            // Case 4: is token a semicolon separator ?
            // ----------------------------------------

            if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }   // program mode and no PROGRAM command

            // token is semicolon separator, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
            if (_parenthesisLevel > 0) { pNext = pch; result = result_missingRightParenthesis; return false; }
            if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_2_0)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }
            if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && !(_lastTokenIsPostfixOp)) { pNext = pch; result = result_separatorNotAllowedHere; return false; }

            if (_isAnyVarCmd && (_lastTokenType == tok_isVariable) && _varIsConstant) { result = result_var_constantVarNeedsAssignment; pNext = pch; return false; }

            // token is a semicolon separator, and it's allowed here

            _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

            // expression syntax check 
            _thisLvl_lastIsVariable = false;                                                                    // currently open block
            _thislvl_lastIsConstVar = false;
            _thisLvl_assignmentStillPossible = true;
            _thisLvl_lastOpIsIncrDecr = false;

            // command argument constraints check: reset for next command argument
            _lvl0_withinExpression = false;
            _lvl0_isPurePrefixIncrDecr = false;
            _lvl0_isPureVariable = false;
            _lvl0_isVarWithAssignment = false;

            _userVarUnderConstruction = false;                                                                  // if a var was under construction, it has been created now without errors
        }
        break;


        default:
        {
            // ----------------------------
            // Case 5: token is an operator
            // ----------------------------

            // 1. Is an operator allowed here ? 
            // --------------------------------

            if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }   // program mode and no PROGRAM command

            // does last token type allow an operator as current token ?
            if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_5_3_2_1_0)) { pNext = pch; result = result_operatorNotAllowedHere; return false; }

            // allow token (pending further tests) if within most commands, if in immediate mode and inside a function   
            bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
            if (!tokenAllowed) { pNext = pch; result = result_operatorNotAllowedHere; return false; ; }

            // 1.a Check additional constraints within specific commands
            // ---------------------------------------------------------

            bool lastWasAssignment = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;

            if (_isAnyVarCmd) {
                if (_parenthesisLevel > 0) { pNext = pch; result = result_operatorNotAllowedHere; return false; }           // no operators in array dimensions (must be constants)
                // prefix increment operators before variable to be declared are not detected in command argument checking: test here
                else if (!_lvl0_withinExpression) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
                // initializer is constant only: not followed by any operators
                else if ((_lastTokenType == tok_isConstant) || (_lastTokenType == tok_isSymbolicConstant)) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
            }

            if (_isJustinaFunctionCmd) {
                // only a scalar variable or an assignment can be followed by an operator (an assignment and a prefix plus or minus, respectively)
                if ((_lastTokenType != tok_isVariable) && !lastWasAssignment) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
                // initializer is a scalar variable: can not be followed by any other operator than assignment
                if ((_lastTokenType == tok_isVariable) && (_terminals[termIndex].terminalCode != termcod_assign)) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
            }

            // numeric initializer with plus or minus prefix
            if ((_isJustinaFunctionCmd && (_parenthesisLevel == 1)) || (_isAnyVarCmd && (_parenthesisLevel == 0))) {
                bool isPrefixPlusOrMinus = ((_terminals[termIndex].terminalCode == termcod_plus) || (_terminals[termIndex].terminalCode == termcod_minus));
                if (isPrefixPlusOrMinus) {
                    if (_initVarOrParWithUnaryOp != 0) { pNext = pch; result = result_operatorNotAllowedHere; return false; }   // already a prefix operator found (only one allowed in initializer)
                    else {
                        _initVarOrParWithUnaryOp = (_terminals[termIndex].terminalCode == termcod_minus) ? -1 : 1;          // -1 if minus, 1 if plus prefix operator
                        while (pNext[0] == ' ') { pNext++; }                                                                // find start of next token
                        if (pNext[0] == '\0') { break; }                                                                    // safety: instruction was not ended by a semicolon (should never happen) 
                        result = result_parsing_OK;
                        // consider unary plus or minus operator as processed, but do not create token; remember which of the two was found
                        return true;
                    }
                }
            }

            if (_isProgramCmd || _isDeleteVarCmd) { pNext = pch; result = result_operatorNotAllowedHere; return false; }


            // 1.b Find out if the provided operator type (prefix, infix or postfix) is allowed 
            // --------------------------------------------------------------------------------

            // does last token type limit allowable operators to infix and postfix ?
            bool tokenIsPrefixOp{ false }, tokenIsPostfixOp{ false };
            if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3) ||
                ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp)) {
                // infix and postfix operators are allowed: test that current operator is infix or postfix
                if ((_terminals[termIndex].infix_priority == 0) && (_terminals[termIndex].postfix_priority == 0)) { pNext = pch; result = result_prefixOperatorNotAllowedhere; return false; }
                tokenIsPrefixOp = false; tokenIsPostfixOp = (_terminals[termIndex].postfix_priority != 0);                  // token is either infix or postfix

            }
            else {                                                                                                          // prefix operators only are allowed
                if (_terminals[termIndex].prefix_priority == 0) { pNext = pch; result = result_invalidOperator; return false; }
                tokenIsPrefixOp = true; tokenIsPostfixOp = false;
            }

            bool isPrefixIncrDecr = (tokenIsPrefixOp && ((_terminals[termIndex].terminalCode == termcod_incr) || (_terminals[termIndex].terminalCode == termcod_decr)));
            bool isPostfixIncrDecr = (tokenIsPostfixOp && ((_terminals[termIndex].terminalCode == termcod_incr) || (_terminals[termIndex].terminalCode == termcod_decr)));

            if (isPostfixIncrDecr) {
                if (!_thisLvl_lastIsVariable) { pNext = pch; result = result_operatorNotAllowedHere; return false; }        // not a variable or array element
                if (_thislvl_lastIsConstVar) { pNext = pch; result = result_cannotChangeConstantValue; return false; }      // variable is declared constant
                if (_thisLvl_lastOpIsIncrDecr) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
            }

            if (tokenIsPrefixOp && !isPrefixIncrDecr) {
                if (_thisLvl_lastOpIsIncrDecr) { pNext = pch; result = result_operatorNotAllowedHere; return false; }
            }
            // remember, because not allowed with postfix increment / decrement (has higher priority and does not return a variable reference)
            _thisLvl_lastOpIsIncrDecr = (isPrefixIncrDecr || isPostfixIncrDecr);


            // 1.c If current token is an assignment operator, check whether it's allowed here
            // -------------------------------------------------------------------------------

            bool operatorContainsAssignment = ((_terminals[termIndex].terminalCode == termcod_assign)
                || (_terminals[termIndex].terminalCode == termcod_plusAssign) || (_terminals[termIndex].terminalCode == termcod_minusAssign)
                || (_terminals[termIndex].terminalCode == termcod_multAssign) || (_terminals[termIndex].terminalCode == termcod_divAssign)
                || (_terminals[termIndex].terminalCode == termcod_modAssign) || (_terminals[termIndex].terminalCode == termcod_bitAndAssign)
                || (_terminals[termIndex].terminalCode == termcod_bitOrAssign) || (_terminals[termIndex].terminalCode == termcod_bitXorAssign)
                || (_terminals[termIndex].terminalCode == termcod_bitShLeftAssign) || (_terminals[termIndex].terminalCode == termcod_bitShRightAssign));


            if (operatorContainsAssignment) {
                if (!_thisLvl_lastIsVariable) { pNext = pch; result = result_assignmNotAllowedHere; return false; }         // not a variable or array element
                if (_thislvl_lastIsConstVar && !_isConstVarCmd) { pNext = pch; result = result_cannotChangeConstantValue; return false; }   // variable is declared constant
                if (!_thisLvl_assignmentStillPossible) { pNext = pch; result = result_assignmNotAllowedHere; return false; }
            }

            if (!(operatorContainsAssignment || isPrefixIncrDecr)) { _thisLvl_assignmentStillPossible = false; }            // further assignments at this expression level not possible any more


            // 1.d Command argument constraints check
            // --------------------------------------

            if (_parenthesisLevel == 0) {
                if (!_lvl0_withinExpression || _lvl0_isPurePrefixIncrDecr) { _lvl0_isPurePrefixIncrDecr = (isPrefixIncrDecr || isPostfixIncrDecr); }
                if (_lvl0_isPureVariable) { _lvl0_isVarWithAssignment = operatorContainsAssignment; }
                _lvl0_isPureVariable = false;
                _lvl0_withinExpression = true;
            }

            // 1.e Token is an operator, and it's allowed here
            // -----------------------------------------------

            _lastTokenIsPrefixOp = tokenIsPrefixOp;
            _lastTokenIsPostfixOp = tokenIsPostfixOp;
            _lastTokenIsPrefixIncrDecr = isPrefixIncrDecr;
        }
    }

    // create token
    // ------------

    // too many terminals for 1 terminal group: provide multiple groups
    tokenType = (termIndex <= 0x0F) ? tok_isTerminalGroup1 : (termIndex <= 0x1F) ? tok_isTerminalGroup2 : tok_isTerminalGroup3;
    _tokenIndex = termIndex;

    Token_terminal* pToken = (Token_terminal*)_programCounter;
    pToken->tokenTypeAndIndex = tokenType | ((termIndex & 0x0F) << 4);                                  // terminal tokens only: token type character includes token index too 
    _lastTokenStep = _programCounter - _programStorage;

    _lastTokenType = tokenType;
    _lastTokenIsString = false, _lastTokenIsTerminal = true;
    _lastTermCode = (termin_code)_terminals[termIndex].terminalCode;

#if PRINT_PARSED_TOKENS
    _pDebugOut->print("   parsed terminal: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" [ "); _pDebugOut->print(_terminals[termIndex].terminalName);  _pDebugOut->println(" ]");
#endif

    _programCounter += sizeof(Token_terminal);
    *_programCounter = tok_no_token;                                                                    // indicates end of program
    result = result_parsing_OK;                                                                         // flag 'valid token found'
    return true;
}


// --------------------------------------------------------------------------------
// *   try to parse next characters as an internal cpp (built in) function name   *
// --------------------------------------------------------------------------------

bool Justina::parseAsInternCPPfunction(char*& pNext, parsingResult_type& result) {
    result = result_tokenNotFound;                                                                      // init: flag 'no token found'
    char* pch = pNext;                                                                                  // pointer to first character to parse (any spaces have been skipped already)
    int funcIndex;

    if (!isalpha(pNext[0])) { return true; }                                                            // first character is not a letter ? Then it's not a function name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                                         // do until first character after alphanumeric token (can be anything, including '\0')

    for (funcIndex = _internCppFunctionCount - 1; funcIndex >= 0; funcIndex--) {                        // for all defined function names: check against alphanumeric token (NOT ending by '\0')
        if (strlen(_internCppFunctions[funcIndex].funcName) != pNext - pch) { continue; }               // token has correct length ? If not, skip remainder of loop ('continue')                            
        if (strncmp(_internCppFunctions[funcIndex].funcName, pch, pNext - pch) != 0) { continue; }      // token corresponds to function name ? If not, skip remainder of loop ('continue')    

        // token is a function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
        if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }   // program mode and no PROGRAM command

        if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_functionNotAllowedHere; return false; }
        if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_functionNotAllowedHere; return false; }

        // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
        bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
        if (!tokenAllowed) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

        bool varExpected = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
        if (varExpected) { pNext = pch; result = result_variableNameExpected; return false; }

        if (_isJustinaFunctionCmd) { pNext = pch; result = result_function_redefiningNotAllowed; return false; }
        if (_isAnyVarCmd) { pNext = pch; result = result_variableNameExpected; return false; }          // is a variable declaration: cpp function name not allowed
        if (_isDeleteVarCmd) { pNext = pch; result = result_variableNameExpected; return false; }

        // eval() function can not occur within a trace, BP view or BP trigger string (all other internal functions can)
        // note: an eval() functions are allowed in other eval() function
        if (_parsingExecutingTraceString || _parsingExecutingTriggerString) {
            if (_internCppFunctions[funcIndex].functionCode == fnccod_eval) { pNext = pch; result = result_trace_evalFunctonNotAllowed; return false; }
        }

        // token is an internal cpp function, and it's allowed here

        _minFunctionArgs = _internCppFunctions[funcIndex].minArgs;                                      // set min & max for allowed argument count (note: minimum is 0)
        _maxFunctionArgs = _internCppFunctions[funcIndex].maxArgs;
        _functionIndex = funcIndex;

        // expression syntax check 
        _thisLvl_lastIsVariable = false;
        _thislvl_lastIsConstVar = false;

        // command argument constraints check
        _lvl0_withinExpression = true;

        Token_internCppFunction* pToken = (Token_internCppFunction*)_programCounter;
        pToken->tokenType = tok_isInternCppFunction | (sizeof(Token_internCppFunction) << 4);
        pToken->tokenIndex = _functionIndex;

        _lastTokenStep = _programCounter - _programStorage;
        _lastTokenType = tok_isInternCppFunction;
        _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

    #if PRINT_PARSED_TOKENS
        _pDebugOut->print("   parsed internal function: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); _pDebugOut->print(_internCppFunctions[funcIndex].funcName);  _pDebugOut->println("]");
    #endif

        _programCounter += sizeof(Token_internCppFunction);
        *_programCounter = tok_no_token;                                                            // indicates end of program
        result = result_parsing_OK;                                                                 // flag 'valid token found'
        return true;
    }

    pNext = pch;                                                                                    // reset pointer to first character to parse (because no token was found)
    return true;                                                                                    // token is not a function name (but can still be something else)
}


// ----------------------------------------------------------------------------
// *   try to parse next characters as an external cpp (user) function name   *
// ----------------------------------------------------------------------------

bool Justina::parseAsExternCPPfunction(char*& pNext, parsingResult_type& result) {
    result = result_tokenNotFound;                                                                  // init: flag 'no token found'
    char* pch = pNext;                                                                              // pointer to first character to parse (any spaces have been skipped already)

    if (!isalpha(pNext[0])) { return true; }                                                        // first character is not a letter ? Then it's not a function name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                                     // do until first character after alphanumeric token (can be anything, including '\0')

    int extFunctionReturnType{ 0 }, extFuncIndexInType{ 0 };
    for (extFunctionReturnType = 0; extFunctionReturnType < (sizeof(_ExtCppFunctionCounts) / sizeof(_ExtCppFunctionCounts[0])); extFunctionReturnType++) {

        // entry point for specific return type was not initialized (no callback functions for this return type) ? skip this return type category
        if ((CppDummyVoidFunction*)_pExtCppFunctions[extFunctionReturnType] == nullptr) { continue; }

        for (extFuncIndexInType = 0; extFuncIndexInType < _ExtCppFunctionCounts[extFunctionReturnType]; extFuncIndexInType++) {
            const char* funcName = ((CppDummyVoidFunction*)_pExtCppFunctions[extFunctionReturnType])[extFuncIndexInType].cppFunctionName;

            // note: strncmp() is used to compare strings instead of strcmp(), because one of the strings to compare does not have a terminating '\0' 
            if (strlen(funcName) != pNext - pch) { continue; }                                      // token has same length as a stored name ? If not, skip remainder of loop ('continue')                            
            if (strncmp(funcName, pch, pNext - pch) != 0) { continue; }                             // token has same characters as stored name ? If not, skip remainder of loop ('continue')    

            if (pNext - pch > MAX_IDENT_NAME_LEN) { pNext = pch; result = result_identifierTooLong;  return false; }    // function name is too long

            // token is a function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
            if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }   // program mode and no PROGRAM command

            if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_functionNotAllowedHere; return false; }
            if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_functionNotAllowedHere; return false; }

            // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
            bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
            if (!tokenAllowed) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

            bool varExpected = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
            if (varExpected) { pNext = pch; result = result_variableNameExpected; return false; }

            if (_isJustinaFunctionCmd) { pNext = pch; result = result_function_redefiningNotAllowed; return false; }
            if (_isAnyVarCmd) { pNext = pch; result = result_variableNameExpected; return false; }  // is a variable declaration: cpp function name not allowed
            if (_isDeleteVarCmd) { pNext = pch; result = result_variableNameExpected; return false; }

            // token is an external cpp function, and it's allowed here

            _minFunctionArgs = ((CppDummyVoidFunction*)_pExtCppFunctions[extFunctionReturnType])[extFuncIndexInType].minArgCount;   // set min & max for allowed argument count (note: minimum is 0)
            _maxFunctionArgs = ((CppDummyVoidFunction*)_pExtCppFunctions[extFunctionReturnType])[extFuncIndexInType].maxArgCount;

            // expression syntax check 
            _thisLvl_lastIsVariable = false;
            _thislvl_lastIsConstVar = false;

            // command argument constraints check
            _lvl0_withinExpression = true;

            Token_externCppFunction* pToken = (Token_externCppFunction*)_programCounter;
            pToken->tokenType = tok_isExternCppFunction | (sizeof(Token_externCppFunction) << 4);
            pToken->returnValueType = extFunctionReturnType;
            pToken->funcIndexInType = extFuncIndexInType;

            _lastTokenStep = _programCounter - _programStorage;
            _lastTokenType = tok_isExternCppFunction;
            _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

        #if PRINT_PARSED_TOKENS
            _pDebugOut->print("   parsed user CPP function: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); _pDebugOut->print(funcName);  _pDebugOut->println("]");
        #endif

            _programCounter += sizeof(Token_externCppFunction);
            *_programCounter = tok_no_token;                                                        // indicates end of program
            result = result_parsing_OK;                                                             // flag 'valid token found'
            return true;
        }
    }

    pNext = pch;                                                                                    // reset pointer to first character to parse (because no token was found)
    return true;                                                                                    // token is not a function name (but can still be something else)
}


// ---------------------------------------------------------------
// *   try to parse next characters as a Justina function name   *
// ---------------------------------------------------------------

bool Justina::parseAsJustinaFunction(char*& pNext, parsingResult_type& result) {

    if (_isProgramCmd) { return true; }                                                             // looking for an UNQUALIFIED identifier name; prevent it's mistaken for a variable name (same format)

    // 1. Is this token a function name ?
    // ----------------------------------

    result = result_tokenNotFound;                                                                  // init: flag 'no token found'
    char* pch = pNext;                                                                              // pointer to first character to parse (any spaces have been skipped already)

    if (!isalpha(pNext[0])) { return true; }                                                        // first character is not a letter ? Then it's not an identifier name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                                     // do until first character after alphanumeric token (can be anything, including '\0')

    // name already in use as global or user variable name ? Then it's not a Justina function
    bool createNewName = false;
    int index = getIdentifier(programVarNames, _programVarNameCount, MAX_PROGVARNAMES, pch, pNext - pch, createNewName);
    if (index != -1) { pNext = pch; return true; }                                                                      // is a program variable
    index = getIdentifier(userVarNames, _userVarCount, MAX_USERVARNAMES, pch, pNext - pch, createNewName, true);
    if (index != -1) { pNext = pch; return true; }                                                                      // is a user variable

    // user functions cannot occur within a trace, BP view or BP trigger string (they are allowed in eval() strings, however)
    if (_parsingExecutingTraceString || _parsingExecutingTriggerString) { pNext = pch; result = result_trace_userFunctonNotAllowed; return false; }

    if ((_isJustinaFunctionCmd) && (_parenthesisLevel > 0)) { pNext = pch; return true; }                               // only array parameter allowed now


    // 2. Is a function name allowed here ? 
    // ------------------------------------

    if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; }       // program mode and no PROGRAM command

    // token is a Justina function, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_functionNotAllowedHere; return false; }
    if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_functionNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
    if (!tokenAllowed) { pNext = pch; result = result_functionNotAllowedHere; return false; ; }

    // if function name is too long, reset pointer to first character to parse, indicate error and return
    if (pNext - pch > MAX_IDENT_NAME_LEN) { pNext = pch; result = result_identifierTooLong;  return false; }

    createNewName = false;                                                                                              // only check if function is defined, do NOT YET create storage for it
    index = getIdentifier(JustinaFunctionNames, _justinaFunctionCount, MAX_JUSTINA_FUNCTIONS, pch, pNext - pch, createNewName);

    if (_isAnyVarCmd) {     // is a variable declaration
        if (index == -1) { pNext = pch; return true; }                                                                  // it's not a defined Justina function: move on
        else { pNext = pch; result = result_variableNameExpected; return false; }                                       // it's a Justina function: not allowed here
    }
    if (_isDeleteVarCmd) { pNext = pch; result = result_variableNameExpected; return false; }

    // not a defined variable and not a variable definition: consider it as a function

    // next character is not a left parenthesis ? do not wait until next token to produce (more specific) error
    char* peek1 = pNext; while (peek1[0] == ' ') { peek1++; }
    if (peek1[0] != term_leftPar[0]) {
        if (_isJustinaFunctionCmd) { pNext = pch; result = result_function_needsParentheses; return false; }            // function definition: left parenthesis required
        if (index != -1) { pNext = pch; result = result_function_needsParentheses; return false; }                      // defined function: left parenthesis required
        pNext = pch; return true;                                                                                       // it's not a defined Justina function: move on
    }

    else if (!_programMode) {                                                                                           // if in immediate mode: the function must be defined earlier (in a program)
        if (index == -1) { pNext = pch; result = result_function_undefinedFunctionOrArray; return false; }
    }

    bool varRequired = _lastTokenIsTerminal ? ((_lastTermCode == termcod_incr) || (_lastTermCode == termcod_decr)) : false;
    if (varRequired) { pNext = pch; result = result_variableNameExpected; return false; }

    // token is a Justina function (definition or call), and it's allowed here


    // 3. Has function attribute storage already been created for this function ? (because of a previous function definition or a previous function call)
    // --------------------------------------------------------------------------------------------------------------------------------------------------

    createNewName = true;                                                                                               // if new Justina function, create storage for it
    index = getIdentifier(JustinaFunctionNames, _justinaFunctionCount, MAX_JUSTINA_FUNCTIONS, pch, pNext - pch, createNewName);
    if (index == -1) { pNext = pch; result = result_maxJustinaFunctionsReached; return false; }
    char* funcName = JustinaFunctionNames[index];                                                                       // either new or existing function name

    // new function name (could be a function call or function definition)
    if (createNewName) {
        justinaFunctionData[index].isVoidFunctionDef = 0;                                                               // init flag (1 bit wide) to indicate a PROCEDURE (void function) definition
        justinaFunctionData[index].callsAsPartOfExpression = 0;                                                         // init flag (1 bit wide) to indicate that not all function calls occur at the start of a statement

        // init max (bits 7654) & min (bits 3210) allowed n� OR actual n� of arguments; store in last position (behind string terminating character)
        funcName[MAX_IDENT_NAME_LEN + 1] = c_JustinaFunctionFirstOccurFlag;                                             // max (bits 7654) < (bits 3210): indicates value is not yet updated by parsing previous calls closing parenthesis
        justinaFunctionData[index].pJustinaFunctionStartToken = nullptr;                                                // initialize. Pointer will be set when function definition is parsed (checked further down)
        justinaFunctionData[index].paramIsArrayPattern[1] = 0x80;                                                       // set flag (bit 15) to indicate a new function name is parsed (definition or call)
        justinaFunctionData[index].paramIsArrayPattern[0] = 0x00;                                                       // array pattern bits 7-0
    }

    // if function storage was created already: check for double function definition
    else if (_isJustinaFunctionCmd) {                                                                                   // this is a function definition (not a call)
        // pointer to function starting token already defined: this is a double definition
        if (justinaFunctionData[index].pJustinaFunctionStartToken != nullptr) { pNext = pch; result = result_function_redefiningNotAllowed; return false; }
    }

    // Is this a Justina function definition (not a function call) ?
    if (_isJustinaFunctionCmd) {

        // is this a PROCEDURE definition AND are any calls to it, made PRIOR to this definition, part of an expression ?
        justinaFunctionData[index].isVoidFunctionDef = (_isVoidJustinaFunctionCmd ? 1 : 0);                              // flag (1 bit wide) indicating that the function definition was found and it is a procedure (not returning a value))
        if ((justinaFunctionData[index].isVoidFunctionDef == 1) && (justinaFunctionData[index].callsAsPartOfExpression == 1)) { pNext = pch; result = result_function_cannotUseProceduresInExpression; return false; }

        justinaFunctionData[index].pJustinaFunctionStartToken = _programCounter;                                        // store pointer to function start token 

        // global program variable name usage array: reset in-function reference flags to be able to keep track of in-procedure variable value types used
        // KEEP all other settings
        for (int i = 0; i < _programVarNameCount; i++) { globalVarType[i] = (globalVarType[i] & ~var_scopeMask) | var_scopeToSpecify; }       // indicates 'variable with this name has not been referred to in current procedure'

        _paramOnlyCountInFunction = 0;                                                                                  // reset local and parameter variable count in function 
        _localVarCountInFunction = 0;                                                                                   // reset local and parameter variable count in function
        _staticVarCountInFunction = 0;                                                                                  // reset static variable count in function
        justinaFunctionData[index].paramOnlyCountInFunction = 0;
        justinaFunctionData[index].localVarCountInFunction = 0;
        justinaFunctionData[index].staticVarCountInFunction = 0;

        // if function will define static variables, then storage area will start right after storage area for previously defined user function's static variable area (this is needed while in debugging only)
        justinaFunctionData[index].staticVarStartIndex = _staticVarCount;

        // if function will define local variables, although storage area is dynamic, this is needed while in debugging (only)
        justinaFunctionData[index].localVarNameRefs_startIndex = _localVarCount;

        _pFunctionDefStack = _pParsingStack;                                                                            // stack level for FUNCTION definition block
        _pFunctionDefStack->openBlock.fcnBlock_functionIndex = index;                                                   // store in BLOCK stack level: only if function def
    }

    // is this a Justina function call (not a function definition) ?
    else {
        bool lastIsSemiColon = _lastTokenIsTerminal ? (_lastTermCode == termcod_semicolon) : false;
        if (!lastIsSemiColon && (_lastTokenType != tok_no_token)) {
            justinaFunctionData[index].callsAsPartOfExpression = 1;                                                     // function name is not start of statement: function is part of an expression
            // is this a call to a PROCEDURE defined PRIOR to this call ? Then this call is part of an expression (this is case A: part of the expression precedes the procedure call)
            if (justinaFunctionData[index].isVoidFunctionDef == 1) { pNext = pch; result = result_function_cannotUseProceduresInExpression; return false; }
        }
    }

    // if function was defined prior to this occurrence (which is then a call), retrieve min & max allowed arguments for checking actual argument count
    // if function not yet defined: retrieve current state of min & max of actual argument count found in COMPLETELY PARSED previous calls to same function 
    // if no previous occurrences at all: data is not yet initialized (which is OK)
    _minFunctionArgs = ((funcName[MAX_IDENT_NAME_LEN + 1]) >> 4) & 0x0F;                                                // use only for passing to parsing stack
    _maxFunctionArgs = (funcName[MAX_IDENT_NAME_LEN + 1]) & 0x0F;
    _functionIndex = index;

    // expression syntax check 
    _thisLvl_lastIsVariable = false;
    _thislvl_lastIsConstVar = false;

    // command argument constraints check
    _lvl0_withinExpression = true;


    // 4. Store token in program memory
    // --------------------------------

    Token_JustinaFunction* pToken = (Token_JustinaFunction*)_programCounter;
    pToken->tokenType = tok_isJustinaFunction | (sizeof(Token_JustinaFunction) << 4);
    pToken->identNameIndex = index;

    _lastTokenStep = _programCounter - _programStorage;
    _lastTokenType = tok_isJustinaFunction;
    _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if PRINT_PARSED_TOKENS
    _pDebugOut->print("   parsed Justina user function: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); _pDebugOut->print(JustinaFunctionNames[_functionIndex]);  _pDebugOut->println("]");
#endif

    _programCounter += sizeof(Token_JustinaFunction);
    *_programCounter = tok_no_token;                                                                             // indicates end of program
    result = result_parsing_OK;                                                                                  // flag 'valid token found'
    return true;
}


// --------------------------------------------------
// *   try to parse next characters as a variable   *
// --------------------------------------------------

bool Justina::parseAsVariable(char*& pNext, parsingResult_type& result) {

    // looking for an UNQUALIFIED identifier name; prevent it's mistaken for a variable name (same format)
    if (_isProgramCmd || _isDeleteVarCmd) { return true; }

    // 1. Is this token a variable name ?
    // ----------------------------------
    result = result_tokenNotFound;                                                                              // init: flag 'no token found'
    char* pch = pNext;                                                                                          // pointer to first character to parse (any spaces have been skipped already)
    bool debug_functionVarOnly{ false };

    if (!isalpha(pNext[0]) && (pNext[0] != '#')) { return true; }                                               // first character is not a letter ? Then it's not a variable name (it can still be something else)
    if (pNext[0] == '#') {
        if (_programMode) { pNext = pch; result = result_var_illegalInProgram; return false; }
        if (_isAnyVarCmd) { pNext = pch; result = result_var_illegalInDeclaration; return false; }
        else {
            debug_functionVarOnly = true; ++pNext;                                                              // record that a 'function variable only' prefix was found 
            if (!isalpha(pNext[0])) { pNext = pch; result = result_variableNameExpected; return false; }
        }
    }
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                                                 // do until first character after alphanumeric token (can be anything, including '\0')
    char* pName = pch + (debug_functionVarOnly ? 1 : 0);


    // 2. Is a variable name allowed here ?
    // ------------------------------------

    if (_programCounter == _programStorage) { pNext = pch; result = result_cmd_programCmdMissing; return false; } // program mode and no PROGRAM command

    // token is a variable, but is it allowed here ? If not, reset pointer to first character to parse, indicate error and return
    if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_5_2_1_0)) { pNext = pch; result = result_variableNotAllowedHere; return false; }
    if ((_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_0) && _lastTokenIsPostfixOp) { pNext = pch; result = result_variableNotAllowedHere; return false; }

    // allow token (pending further tests) if within a command, if in immediate mode and inside a function   
    bool tokenAllowed = (_isCommand || (!_programMode) || _justinaFunctionBlockOpen);
    if (!tokenAllowed) { pNext = pch; result = result_variableNotAllowedHere; return false; ; }

    // scalar or array variable ? (could still be function 'array' argument; this will be detected further below)
    char* peek1 = pNext; while (peek1[0] == ' ') { peek1++; }                                                   // peek next character: is it a left parenthesis ?
    char* peek2; if (peek1[0] == term_leftPar[0]) { peek2 = peek1 + 1; while (peek2[0] == ' ') { peek2++; } }   // also find the subsequent character
    bool isArray = (peek1[0] == term_leftPar[0]);

    // Function parameter definition: check for proper function name, proper array definition (empty parentheses) and proper initializer (must be constant) 
    // Variable definition: 

    // an initializer can only be a constant and not a variable: produce error now, before variable is created (if it doesn't exist yet)
    bool lastIsPureAssgnmentOp = _lastTokenIsTerminal ? (_lastTermCode == termcod_assign) : false;
    if ((_isJustinaFunctionCmd || _isAnyVarCmd) && lastIsPureAssgnmentOp) { pNext = pch; result = result_constantValueExpected; return false; }

    // array declaration: dimensions must be number constants (global, static, local arrays)
    if (_isAnyVarCmd && (_parenthesisLevel > 0)) { pNext = pch; result = result_variableNotAllowedHere; return false; }

    // if variable name is too long, indicate error and return
    if (pNext - pch > MAX_IDENT_NAME_LEN) { pNext = pch; result = result_identifierTooLong;  return false; }

    // name already in use as Justina function name ? 
    bool createNewName{ false };
    int varNameIndex = getIdentifier(JustinaFunctionNames, _justinaFunctionCount, MAX_JUSTINA_FUNCTIONS, pch, pNext - pch, createNewName);
    if (varNameIndex != -1) { pNext = pch; result = result_var_nameInUseForFunction; return false; }

    // token is a variable NAME, and a variable is allowed here


    // 3. Check whether this name exists already for variables, and create if needed
    // -----------------------------------------------------------------------------

    // note that multiple distinct program variables (global, static, local) and function parameters can all share the same name, which is only stored once 
    // user variable names are stored separately

    // set pointers to variable name storage areas for program variable names and user variable names, respectively
    char** pvarNames[2]; pvarNames[0] = programVarNames; pvarNames[1] = userVarNames;
    int* varNameCount[2]; varNameCount[0] = &_programVarNameCount; varNameCount[1] = &_userVarCount;
    int maxVarNames[2]; maxVarNames[0] = MAX_PROGVARNAMES; maxVarNames[1] = MAX_USERVARNAMES;
    char* varType[2]; varType[0] = globalVarType; varType[1] = userVarType;
    Val* varValues[2]; varValues[0] = globalVarValues; varValues[1] = userVarValues;

    // 0: program variable, 1: user variable
    int primaryNameRange = (_programMode || debug_functionVarOnly) ? 0 : 1;      // immediate mode while in debug only: if '#' prefix is found, force 'function variable'
    int secondaryNameRange = _programMode ? 1 : 0;
    int activeNameRange = primaryNameRange;

    // init: program parsing: assume program variable name for now; immediate mode parsing: assume user variable name
    bool isProgramVar = (_programMode || debug_functionVarOnly);

    // check if variable exists already (program mode OR '#' prefix found (debug imm. mode): as program variable; immediate mode: as user variable)
    // if a variable DEFINITION, then create variable name if it does not exist yet
    // note: for PROGRAM variables, this only concerns the NAME, not yet the actual variable (program variables: local, static, param and global variables can all share the same name)
    createNewName = _isJustinaFunctionCmd || _isAnyVarCmd;
    varNameIndex = getIdentifier(pvarNames[primaryNameRange], *varNameCount[primaryNameRange], maxVarNames[primaryNameRange], pName, pNext - pName, createNewName, !isProgramVar);

    if (_isJustinaFunctionCmd || _isAnyVarCmd) {                                                                        // variable or parameter DEFINITION: if name didn't exist, it should have been created now
        if (varNameIndex == -1) { pNext = pch; result = result_maxVariableNamesReached; return false; }                 // name still does not exist: error

        // user variables: detect immediately if a variable has been redeclared 
        if (!isProgramVar) {
            if (!createNewName) { pNext = pch; result = result_var_redeclared; return false; }
            _userVarUnderConstruction = true;                                                                           // it may have to be deleted again (an error can still occur
        }

        // name exists (newly created or pre-existing)
        // variable name is new: clear all variable value type flags and indicate 'qualifier not determined yet'
        // variable value type (array, float or string) will be set later
        if (createNewName) { varType[primaryNameRange][varNameIndex] = var_scopeToSpecify; } // new name was created now: reset scope (not yet done) - only for use while parsing functions
    }
    else { // not a variable definition, just a variable reference
        if (varNameIndex == -1) {
            // variable name does not exist in primary range (and no error produced, so it was not a variable declaration):
            // check if the name is defined in the secondary name range (except if only looking for function variable -> '#' prefix found)
            if (!debug_functionVarOnly) {
                isProgramVar = !_programMode;                                                                           // program parsing: is program variable; immediate mode: is user variable
                varNameIndex = getIdentifier(pvarNames[secondaryNameRange], *varNameCount[secondaryNameRange], maxVarNames[secondaryNameRange], pName, pNext - pName, createNewName, !isProgramVar);
                if (varNameIndex == -1) { pNext = pch; result = result_var_notDeclared; return false; }  // if the name doesn't exist, the variable doesn't
                activeNameRange = secondaryNameRange;
            }
            else { pNext = pch; result = result_var_notDeclared; return false; }                                        // if the name doesn't exist, the variable doesn't
        }

        // user variable referenced in program: set flag in user var types array (only; will not be copied in token info)
        if (_programMode && !isProgramVar) { varType[activeNameRange][varNameIndex] = varType[activeNameRange][varNameIndex] | var_userVarUsedByProgram; }
    }

    if (_isAnyVarCmd && !isArray) {  // scalar var declarations: check that variable is followed, either by an assignment operator or a comma or semicolon 
        bool scalarWithAssignmentOp = (peek1[0] == term_assign[0]);
        bool scalarWithoutInitializer = ((peek1[0] == term_comma[0]) || (peek1[0] == term_semicolon[0]));
        if (!scalarWithAssignmentOp && !scalarWithoutInitializer) {
            if (!isProgramVar) {
                // consider user variable not created (relevant for user variables only, because program variables are destroyed anyway if parsing fails)
                (*varNameCount[primaryNameRange])--;
                _userVarUnderConstruction = false;                                                                      // user variable has just been 'deleted'
            }
            pNext = pch; result = result_assignmentOrSeparatorExpected; return false;
        }
    }


    // 4. The variable NAME exists now, but we still need to check whether storage space for the variable itself has been created / allocated
    //    Notes: USER variable storage was created when the user variable name was created (just now)
    //           LOCAL variable storage is created at runtime when a function is called
    // --------------------------------------------------------------------------------------------------------------------------------------

    bool globalVarStorageMissingOrIsNotGlobal = false;                                                                  // init: assume global (program or user) var with defined storage location
    bool isOpenFunctionStaticVariable{ false }, isOpenFunctionLocalVariable{ false }, isOpenFunctionParam{ false };
    bool isOpenFunctionLocalArrayVariable{ false };
    int openFunctionVar_valueIndex{};
    int openFunctionArray_dimCount{};

    // 4.1 Currently parsing a FUNCTION...END block ? 
    // ----------------------------------------------

    if (_justinaFunctionBlockOpen) {                                                                                    // (only while parsing program instructions)

        // first use of a particular variable NAME in a function ?  (either a program variable or a user variable)
        bool isFirstVarNameRefInFnc = (((uint8_t)varType[activeNameRange][varNameIndex] & var_scopeMask) == var_scopeToSpecify);
        // first use of a particular PROGRAM variable NAME ? 
        // check if a user variable with same name exists AND it is (maybe) already used within current function
        if (isFirstVarNameRefInFnc && isProgramVar) {
            createNewName = false;
            bool userVarUsedInFunction{ false };
            int userVarIndex = getIdentifier(pvarNames[secondaryNameRange], *varNameCount[secondaryNameRange], maxVarNames[secondaryNameRange], pName, pNext - pName, createNewName, true);
            if (userVarIndex != -1) { userVarUsedInFunction = (((uint8_t)varType[secondaryNameRange][userVarIndex] & var_scopeMask) != var_scopeToSpecify); }
            // user variable name used already within function ? Then this is NOT the first use of this name within the function
            isFirstVarNameRefInFnc = !userVarUsedInFunction;
        }

        if (isFirstVarNameRefInFnc) {                                                                                   // variable not yet referenced within currently parsed procedure

            // determine variable qualifier
            // if a variable definition statement: set scope to parameter, local or static (global and user variable definition: not possible in a function) 
            // if a variable reference: we will determine the qualifier in a moment 

            uint8_t varScope = _isJustinaFunctionCmd ? var_isParamInFunc : _isLocalVarCmd ? var_isLocalInFunc : _isStaticVarCmd ? var_isStaticInFunc : var_scopeToSpecify;

            varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~var_scopeMask) | varScope;     //set scope bits (will be stored in token AND needed during current procedure)

            if (_isStaticVarCmd) {                                                                                      // definition of NEW static variable for function
                globalVarStorageMissingOrIsNotGlobal = true;                                                            // (but it isn't a global variable) 
                if (_staticVarCount == MAX_STATIC_VARIABLES) { pNext = pch; result = result_maxStaticVariablesReached; return false; }

                programVarValueIndex[varNameIndex] = _staticVarCount;
                if (!isArray) { staticVarValues[_staticVarCount].floatConst = 0.; }                                     // initialize variable (if initializer and/or array: will be overwritten)

                staticVarType[_staticVarCount] = value_isFloat;                                                         // init as float (for array or scalar - array flag will be added later) 

                // will only be used while in DEBUGGING mode: index of static variable name
                staticVarNameRef[_staticVarCount] = varNameIndex;

                _staticVarCountInFunction++;
                _staticVarCount++;

                // Justina function index: in parsing stack level for FUNCTION definition command
                int fcnIndex = _pFunctionDefStack->openBlock.fcnBlock_functionIndex;
                justinaFunctionData[fcnIndex].staticVarCountInFunction = _staticVarCountInFunction;
            }

            // definition of NEW parameter (in function definition) or NEW local variable for function
            // (but it isn't a global variable; relative position in a function's local variables area will be defined now) 
            else if (_isJustinaFunctionCmd || _isLocalVarCmd) {
                globalVarStorageMissingOrIsNotGlobal = true;
                if (_localVarCountInFunction == MAX_LOC_VARS_IN_FUNC) { pNext = pch; result = result_maxLocalVariablesReached; return false; }

                programVarValueIndex[varNameIndex] = _localVarCountInFunction;
                // param and local variables: array flag temporarily stored during function parsing       
                // storage space creation and initialization will occur when function is called during execution 
                localVarType[_localVarCountInFunction] = (localVarType[_localVarCountInFunction] & ~(var_isArray | var_isConstantVar)) |
                    (isArray ? var_isArray : 0) | (_isConstVarCmd ? var_isConstantVar : 0);                             // init (no storage needs to be created: set array flag here) 

                // will only be used while in DEBUGGING mode: index of local variable name
                localVarNameRef[_localVarCount] = varNameIndex;

                _localVarCountInFunction++;
                if (_isJustinaFunctionCmd) { _paramOnlyCountInFunction++; }
                _localVarCount++;

                // Justina function index: in stack level for FUNCTION definition command
                int fcnIndex = _pFunctionDefStack->openBlock.fcnBlock_functionIndex;
                justinaFunctionData[fcnIndex].localVarCountInFunction = _localVarCountInFunction;                       // after incrementing count
                if (_isJustinaFunctionCmd) { justinaFunctionData[fcnIndex].paramOnlyCountInFunction = _paramOnlyCountInFunction; }
            }

            else {
                // not a variable definition:  CAN BE an EXISTING global or user variable, within a function
                // it CANNOT be a local or static variable, because this is the first reference of this variable name in the function and it's not a variable definition
                // if the variable name refers to a user variable, the variable exists, so it's known then
                globalVarStorageMissingOrIsNotGlobal = isProgramVar ? (!(varType[activeNameRange][varNameIndex] & var_nameHasGlobalValue)) : false;
                // variable is NEW ? Variable has not been declared
                if (globalVarStorageMissingOrIsNotGlobal) {                                                             // undeclared global program variable                                                             
                    pNext = pch; result = result_var_notDeclared; return false;
                }
                // // IS the use of an EXISTING global or user variable, within a function
                varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~var_scopeMask) | (isProgramVar ? var_isGlobal : var_isUser);

            }
        }

        else {  // if variable name already referenced before in function (global / user variable use OR param, local, static declaration), then it has been defined already
            bool isLocalDeclaration = (_isJustinaFunctionCmd || _isLocalVarCmd || _isStaticVarCmd);                     // local variable declaration ? (parameter, local, static)
            if (isLocalDeclaration) { pNext = pch; result = result_var_redeclared; return false; }
        }
    }


    // 4.2 NOT parsing FUNCTION...END block 
     // ------------------------------------

     // parsing program instructions AND parsing instructions entered in immediate mode
    else {
        // concentrate on global program variables and user variables first (not yet on function variables)
        // if program variable: has a GLOBAL program variable with this name been declared already ? (if user variable, because the name exists, storage exists)
        globalVarStorageMissingOrIsNotGlobal = isProgramVar ? !(varType[activeNameRange][varNameIndex] & var_nameHasGlobalValue) : _isGlobalOrUserVarCmd;
        if (debug_functionVarOnly) { globalVarStorageMissingOrIsNotGlobal = true; }                                     // because it's not a global variable

        // qualifier 'var_isGlobal' (program variables): set it now, because could have been cleared by previously parsed function (will ultimately be stored in token)
        if (isProgramVar) { varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~var_scopeMask) | var_isGlobal; }

    #if PRINT_DEBUG_INFO
        _pDebugOut->print("   (4.2) var NAME index: "); _pDebugOut->println(varNameIndex);
    #endif

        // variable not yet declared as global or user variable ? (If in debug mode, it can still be a static or local variable of a stopped function)
        if (globalVarStorageMissingOrIsNotGlobal) {

        #if PRINT_DEBUG_INFO
            _pDebugOut->println("\r\n   (4.2) var not yet known");
        #endif

            // but this can still be a global or user variable declaration 
            if (_isGlobalOrUserVarCmd) {                                                                                // is it a declaration ?  define storage location now
            #if PRINT_DEBUG_INFO
                _pDebugOut->println("   (4.2) is global or user var");
            #endif

                // is a declaration of a new program global variable (in program mode), or a new user variable (in immediate mode) 
                // variable qualifier : don't care for now (global variables: reset at start of next Justina function parsing)
                if (!isArray) { varValues[activeNameRange][varNameIndex].floatConst = 0.; }                             // initialize variable (if initializer and/or array: will be overwritten)
                varType[activeNameRange][varNameIndex] = varType[activeNameRange][varNameIndex] | value_isFloat;        // init as float (for scalar and array)
                if (isProgramVar) { varType[activeNameRange][varNameIndex] = varType[activeNameRange][varNameIndex] | var_nameHasGlobalValue; }   // set 'has global value' or 'user var' bit
                varType[activeNameRange][varNameIndex] = (varType[activeNameRange][varNameIndex] & ~var_isArray);       // init (array flag may only be added when storage is created) 
                varType[activeNameRange][varNameIndex] |= (_isConstVarCmd ? var_isConstantVar : 0);
            }
            else {  // not a variable declaration, but a variable reference
            #if PRINT_DEBUG_INFO
                _pDebugOut->println("   (4.2) is not a global or user var");
            #endif

                // it's neither a global or user variable declaration, nor a global or user variable reference (because storage does not exist for it). But the variable name exists,
                // so local or static function variables using this name have been defined already. 
                // retrieve data about this function now.

                 // in debug mode (program stopped), the name could refer to a local or static variable within the currently stopped function (open function).
                 // if parsing a BP trigger string, debug mode is not active yet (function is not stopped yet- will depend of trigger string execution result)   

                if ((_openDebugLevels > 0) || _parsingExecutingTriggerString) {
                    void* pFlowCtrlStackLvl{};
                    if (_parsingExecutingTriggerString) {
                        pFlowCtrlStackLvl = &_activeFunctionData;                        // program not yet stopped: function data reside in _activeFunctionData
                    }
                    else {

                        // first locate the debug command level (either in active function data or down in the flow control stack)
                        // from there onwards, find the first flow control stack level containing a 'function' block type  
                        // The 'open function data' (function where the program was stopped) that we need to retrieve function variable data, is referenced in that flow control stack level
                        //
                        // note: levels in between debug command level and open function level may exist, containing open block data for the debug command level
                        // these levels can NOT refer to an eval() string execution level, because a program can not be stopped during the execution of an eval() string
                        // (although it can during a Justina function called from an eval() string)

                        int blockType = _activeFunctionData.blockType;
                        pFlowCtrlStackLvl = _pFlowCtrlStackTop;       // one level below _activeFunctionData
                        bool isDebugCmdLevel = (blockType == block_JustinaFunction) ? (_activeFunctionData.pNextStep >= (_programStorage + _PROGRAM_MEMORY_SIZE)) : false;
                        if (!isDebugCmdLevel) {                                                                         // find debug level in flow control stack instead
                            do {
                                blockType = ((openBlockGeneric*)pFlowCtrlStackLvl)->blockType;
                                isDebugCmdLevel = (blockType == block_JustinaFunction) ? (((OpenFunctionData*)pFlowCtrlStackLvl)->pNextStep >= (_programStorage + _PROGRAM_MEMORY_SIZE)) : false;
                                pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                            } while (!isDebugCmdLevel);                                                                 // stack level for open function found immediate below debug line found (always match)
                        }

                        blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
                        while (blockType != block_JustinaFunction) {
                            pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                            blockType = ((OpenFunctionData*)pFlowCtrlStackLvl)->blockType;
                        }
                    }

                    int openFunctionIndex = ((OpenFunctionData*)pFlowCtrlStackLvl)->functionIndex;                      // function index of stopped program's deepest function in call stack

                    // check whether this is a local or static function variable reference of the deepest open function in the call stack
                    int staticVarStartIndex = justinaFunctionData[openFunctionIndex].staticVarStartIndex;
                    int staticVarCountInFunction = justinaFunctionData[openFunctionIndex].staticVarCountInFunction;

                    //   is variable defined in this function as a static variable ?
                    int i{};

                    for (i = staticVarStartIndex; i <= staticVarStartIndex + staticVarCountInFunction - 1; ++i) {       // skip if count is zero
                        // is a static variable of function and its value index is known ?
                        if (staticVarNameRef[i] == varNameIndex) { isOpenFunctionStaticVariable = true; openFunctionVar_valueIndex = i; break; }
                    }

                    //  not a static variable of the open function: is it a local variable (including a function parameter) ?
                    // this is a little more complex, because local variable memory is dynamically allocated when functions are launched (during execution)
                    if (!isOpenFunctionStaticVariable) {
                        int localVarNameRefs_startIndex = justinaFunctionData[openFunctionIndex].localVarNameRefs_startIndex;
                        int localVarCountInFunction = justinaFunctionData[openFunctionIndex].localVarCountInFunction;
                        int paramOnlyCountInFunction = justinaFunctionData[openFunctionIndex].paramOnlyCountInFunction;

                        for (i = localVarNameRefs_startIndex; i <= localVarNameRefs_startIndex + localVarCountInFunction - 1; ++i) {    // skip if count is zero
                            if (localVarNameRef[i] == varNameIndex) {
                                openFunctionVar_valueIndex = i - localVarNameRefs_startIndex;                           // calculate value index within local storage
                                isOpenFunctionLocalVariable = (openFunctionVar_valueIndex >= paramOnlyCountInFunction);
                                isOpenFunctionParam = (openFunctionVar_valueIndex < paramOnlyCountInFunction);
                                break;                                                                                  // is a local variable of function and its value index is known
                            }

                        }
                        if (isOpenFunctionLocalVariable || isOpenFunctionParam) {
                            // supplied argument is a variable ? (scalar or array)
                            bool isSourceVarRef = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes[openFunctionVar_valueIndex] & value_isVarRef;
                        #if PRINT_DEBUG_INFO
                            _pDebugOut->print("   is open function 'var ref': "); _pDebugOut->println(isSourceVarRef);
                        #endif


                            // has this local variable been defined as a scalar or array ?
                            isOpenFunctionLocalArrayVariable = ((OpenFunctionData*)pFlowCtrlStackLvl)->pVariableAttributes[openFunctionVar_valueIndex] & var_isArray;
                        #if PRINT_DEBUG_INFO 
                            _pDebugOut->print("   is open function local array: "); _pDebugOut->println(isOpenFunctionLocalArrayVariable);
                        #endif

                            if (isOpenFunctionLocalArrayVariable) {
                                void* pArray = isSourceVarRef ? *(((OpenFunctionData*)pFlowCtrlStackLvl)->pLocalVarValues[openFunctionVar_valueIndex].ppArray) :
                                    ((OpenFunctionData*)pFlowCtrlStackLvl)->pLocalVarValues[openFunctionVar_valueIndex].pArray;
                                openFunctionArray_dimCount = ((char*)pArray)[3];
                            #if PRINT_DEBUG_INFO
                                _pDebugOut->print("   open function local var dim count: "); _pDebugOut->println(openFunctionArray_dimCount);
                            #endif

                            }
                        }
                    }
                    if (!isOpenFunctionStaticVariable && !isOpenFunctionLocalVariable && !isOpenFunctionParam) {
                        pNext = pch; result = result_var_notDeclared; return false;
                    }

                }

                else {
                    pNext = pch; result = result_var_notDeclared; return false;
                }
            }

        }

        else {
            // global PROGRAM variable exists already: check for double definition (USER variables: detected when NAME was declared a second time) 
        #if PRINT_DEBUG_INFO
            _pDebugOut->println("   (4.2) is existing global");
        #endif

            if (_isGlobalOrUserVarCmd) {
                _userVarUnderConstruction = false;          // variable was declared earlier: don't delete it 
                pNext = pch; result = result_var_redeclared; return false;
            }
        }
    }

    // 5. If NOT a new variable, check if it corresponds to the variable definition (scalar or array) and retrieve array dimension count (if array)
    //    If it is a FOR loop control variable, check that it is not in use by a FOR outer loop (in same function)
    // --------------------------------------------------------------------------------------------------------------------------------------------

#if PRINT_DEBUG_INFO
    _pDebugOut->println("   (5) further checks");
#endif

    uint8_t varScope = isOpenFunctionStaticVariable ? var_isStaticInFunc :
        isOpenFunctionLocalVariable ? var_isLocalInFunc :
        isOpenFunctionParam ? var_isParamInFunc :
        !isProgramVar ? var_isUser :
        varType[activeNameRange][varNameIndex] & var_scopeMask;         // may only contain variable scope info (parameter, local, static, global, user)

    bool isGlobalOrUserVar = (isOpenFunctionStaticVariable || isOpenFunctionLocalVariable || isOpenFunctionParam) ? false :
        // NOTE: inside a function, test against 'var_isGlobal', outside a function, test against 'var_nameHasGlobalValue'
        isProgramVar ? ((_justinaFunctionBlockOpen && (varScope == var_isGlobal)) || (!_justinaFunctionBlockOpen && (varType[activeNameRange][varNameIndex] & var_nameHasGlobalValue))) : true;
    bool isStaticVar = isOpenFunctionStaticVariable ? true : (_justinaFunctionBlockOpen && (varScope == var_isStaticInFunc));
    bool isLocalVar = isOpenFunctionLocalVariable ? true : (_justinaFunctionBlockOpen && (varScope == var_isLocalInFunc));
    bool isParam = isOpenFunctionParam ? true : (_justinaFunctionBlockOpen && (varScope == var_isParamInFunc));

    int valueIndex = (isOpenFunctionStaticVariable || isOpenFunctionLocalVariable || isOpenFunctionParam) ? openFunctionVar_valueIndex :
        isGlobalOrUserVar ? varNameIndex : programVarValueIndex[varNameIndex];

    bool varIsConstantVar = isGlobalOrUserVar ? (varType[activeNameRange][varNameIndex] & var_isConstantVar) :          // note: a STATIC or a PARAM variable is never a constant
        isLocalVar ? (localVarType[valueIndex] & var_isConstantVar) : false;

    if (_lastTokenIsPrefixIncrDecr && varIsConstantVar) { pNext = pch; result = result_cannotChangeConstantValue; return false; }

    if (!(_isJustinaFunctionCmd || _isAnyVarCmd)) {                                                                     // not a variable definition but a variable reference
        bool existingArray = false;
        _arrayDimCount = 0;                                                                                             // init: if new variable (or no array), then set dimension count to zero
        existingArray = (isOpenFunctionLocalVariable || isOpenFunctionParam) ? isOpenFunctionLocalArrayVariable :
            isGlobalOrUserVar ? (varType[activeNameRange][valueIndex] & var_isArray) :
            isStaticVar ? (staticVarType[valueIndex] & var_isArray) :
            (localVarType[valueIndex] & var_isArray);                                                                   // param or local
        // if not a function definition: array name does not have to be followed by a left parenthesis (passing the array and not an array element)
        // Is this variable part of a function call argument, without further nesting of parenthesis, and has it been defined as an array ? 
        bool isPartOfFuncCallArgument = (_parenthesisLevel > 0) ? (_pParsingStack->openPar.flags & (internCppFunctionBit | externCppFunctionBit | JustinaFunctionBit)) : false;
        if (isPartOfFuncCallArgument && existingArray) {
            // if NOT followed by an array element enclosed in parenthesis, it references the complete array
            // this is only allowed if not part of an expression: check

            bool isFuncCallArgument = _lastTokenIsTerminal ? ((_lastTermCode == termcod_leftPar) || (_lastTermCode == termcod_comma)) : false;
            isFuncCallArgument = isFuncCallArgument && ((peek1[0] == term_comma[0]) || (peek1[0] == term_rightPar[0]));
            if (isFuncCallArgument) { isArray = true; }
        }

        if (existingArray ^ isArray) { pNext = pch; result = isArray ? result_var_definedAsScalar : result_var_definedAsArray; return false; }


        // if existing array: retrieve dimension count against existing definition, for testing against definition afterwards
        if (existingArray) {
            void* pArray = nullptr;
            if (isStaticVar) { pArray = staticVarValues[valueIndex].pArray; }
            else if (isGlobalOrUserVar) { pArray = varValues[activeNameRange][valueIndex].pArray; }
            else if (isLocalVar) { pArray = (float*)localVarDims[valueIndex]; }                                         // dimensions and count are stored in a float

            // retrieve dimension count from array element 0, character 3 (char 0 to 2 contain the dimensions) 
            // parameters; set to maximum allowed for now (count only known during exec))
            // debug mode: retrieve from local array variable in stopped function
            _arrayDimCount = (isParam && !isOpenFunctionParam) ? MAX_ARRAY_DIMS : isOpenFunctionLocalArrayVariable ? openFunctionArray_dimCount : ((char*)pArray)[3];
        }


        // if FOR loop control variable, check it is not in use by a FOR outer loop of SAME function  
        // -----------------------------------------------------------------------------------------

        if (_isForCommand) {            // within a FOR statement
            if (varIsConstantVar && (_cmdArgNo == 0)) { pNext = pch; result = result_var_controlVarIsConstant; return false; }  // this check applies only to control variable (cmdArg increases right after)
            if (_blockLevel > 1) {     // minimum 1 other (outer) open block

                // check whether this is the control variable (following the 'for' keyword) => this means there are no parenthesis levels to skip, below
                char tokenType = *(_programStorage + _lastTokenStep);                                                   // step preceding the variable step (which is not yet stored in program memory)
                if ((tokenType & 0x0f) == tok_isInternCommand) {                                                        // preceding step can only be a 'for' keyword, because we are in a 'for' command

                    // check if control variable is in use by a FOR outer loop
                    LE_parsingStack* pStackLvl = (LE_parsingStack*)parsingStack.getLastListElement();                   // current open block level
                    do {
                        pStackLvl = (LE_parsingStack*)parsingStack.getPrevListElement(pStackLvl);                       // an outer block stack level
                        if (pStackLvl == nullptr) { break; }

                        if (pStackLvl->openBlock.cmdBlockDef.blockType == block_for) {                                  // outer block is FOR loop as well (could be while, if, ... block)
                            TokenPointer prgmCnt;
                            uint16_t tokenStep{ 0 };
                            memcpy(&tokenStep, pStackLvl->openBlock.toTokenStep, sizeof(char[2]));
                            prgmCnt.pTokenChars = _programStorage + tokenStep + sizeof(Token_internalCommand);          // program step for control variable
                            bool isSameControlVariable = ((uint8_t(prgmCnt.pVar->identInfo & var_scopeMask) == varScope)
                                && ((int)prgmCnt.pVar->identNameIndex == varNameIndex)
                                && ((int)prgmCnt.pVar->identValueIndex == valueIndex));
                            if (isSameControlVariable) { pNext = pch; result = result_var_ControlVarInUse; return false; }
                        }
                    } while (true);
                }
            }
        }
    }

    _variableNameIndex = varNameIndex;                                                                                  // will be pushed to parsing stack
    _variableScope = varScope;
    _varIsConstant = varIsConstantVar;

    // expression syntax check 
    _thisLvl_lastIsVariable = true;
    _thislvl_lastIsConstVar = varIsConstantVar;

    // command argument constraints check
    if (!_lvl0_withinExpression || _lvl0_isPurePrefixIncrDecr) { _lvl0_isPureVariable = !varIsConstantVar; _lvl0_isPurePrefixIncrDecr = false; }
    _lvl0_withinExpression = true;                                                                                      // reset for next command parameter


    // 6. Store token in program memory
    // --------------------------------

#if PRINT_DEBUG_INFO
    _pDebugOut->println("   (6) store token");
#endif

    Token_variable* pToken = (Token_variable*)_programCounter;
    pToken->tokenType = tok_isVariable | (sizeof(Token_variable) << 4);
    // identInfo only contains variable scope info (parameter, local, static, global), 'is array' flag, is constant var flag, and 'is forced function variable in debug mode' flag (for printing only) 
    pToken->identInfo = varScope | (isArray ? var_isArray : 0) | (varIsConstantVar ? var_isConstantVar : 0) |
        (debug_functionVarOnly ? var_isForcedFunctionVar : 0);                                     // qualifier, array flag ? (is fixed for a variable -> can be stored in token)  
    pToken->identNameIndex = varNameIndex;
    pToken->identValueIndex = valueIndex;                                                          // points to storage area element for the variable  


    _lastTokenStep = _programCounter - _programStorage;
    _lastVariableTokenStep = _lastTokenStep;
    _lastTokenType = tok_isVariable;
    _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if PRINT_PARSED_TOKENS
    _pDebugOut->print("   parsed var name: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); _pDebugOut->print(pvarNames[activeNameRange][varNameIndex]);  _pDebugOut->println("]");
#endif

    _programCounter += sizeof(Token_variable);
    *_programCounter = tok_no_token;                                                            // indicates end of program
    result = result_parsing_OK;                                                                 // flag 'valid token found'

    return true;
}


// -----------------------------------------------------------------
// *   try to parse next characters as a generic identifier name   *
// -----------------------------------------------------------------

bool Justina::parseAsIdentifierName(char*& pNext, parsingResult_type& result) {
    result = result_tokenNotFound;                                                              // init: flag 'no token found'
    char* pch = pNext;                                                                          // pointer to first character to parse (any spaces have been skipped already)

    bool stay = (_isProgramCmd || _isDeleteVarCmd || _isClearAllCmd || _isClearProgCmd);
    if (!stay) { return true; }

    if (!isalpha(pNext[0]) && (pNext[0] != '#')) { return true; }                               // first character is not a letter ? Then it's not an identifier name (it can still be something else)
    while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                                 // do until first character after alphanumeric token (can be anything, including '\0')

    // generic identifiers cannot occur within a trace, BP view or BP trigger string, and not within an eval() string
    // if not allowed here, reset pointer to first character to parse, indicate error and return
    if (_parsingExecutingTraceString || _parsingExecutingTriggerString || _parsingEvalString) { pNext = pch; result = result_trace_eval_genericNameNotAllowed; return false; }

    if (_parenthesisLevel > 0) { pNext = pch; result = result_identifierNotAllowedHere; return false; }
    if (_isDeleteVarCmd) {        // delete variable: previous token can only be a command ("delete") or a comma (token group one)
        if ((_lastTokenType != tok_isInternCommand) && !(_lastTokenGroup_sequenceCheck_bit & lastTokenGroup_1)) { pNext = pch; result = result_identifierNotAllowedHere; return false; }
    }
    else if (!(_lastTokenGroup_sequenceCheck_bit & lastTokenGroups_6_3_2_0)) { pNext = pch; result = result_identifierNotAllowedHere; return false; }

    // if name is too long, reset pointer to first character to parse, indicate error and return
    if (pNext - pch > MAX_IDENT_NAME_LEN) { pNext = pch; result = result_identifierTooLong;  return false; }

    // token is an identifier name, and it's allowed here
    _parsedStringConstObjectCount++;
    char* pIdentifierName = new char[pNext - pch + 1];                                          // create char array on the heap to store identifier name, including terminating '\0'
    strncpy(pIdentifierName, pch, pNext - pch);                                                 // store identifier name in newly created character array
    pIdentifierName[pNext - pch] = '\0';                                                        // string terminating '\0'
#if PRINT_HEAP_OBJ_CREA_DEL
    _pDebugOut->print("\r\n+++++ (parsed str ) "); _pDebugOut->println((uint32_t)pIdentifierName, HEX);
    _pDebugOut->print("   parse ident name "); _pDebugOut->println(pIdentifierName);
#endif

    // Declaring program name or aliases ? Store 
    if (_isProgramCmd) {
        strcpy(_programName, pIdentifierName);
    }

    else if (_isDeleteVarCmd) {
        // NOTE: deletion of user variables NEEDS to be done during parsing (before execution starts)  to keep system consistency, because variable creation also occurs during parsing

        char* p = pNext;
        while (p[0] == ' ') { p++; }                                                            // find first non-space character
        if ((p[0] != term_comma[0]) && (p[0] != term_semicolon[0]) && (p[0] != '\0')) { result = result_var_deleteSyntaxinvalid; }
        else { result = deleteUserVariable(pIdentifierName); }

        if (result != result_parsing_OK) {
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n----- (parsed str ) ");   _pDebugOut->println((uint32_t)pIdentifierName, HEX);
            _pDebugOut->print("   parse ident name ");   _pDebugOut->println(pIdentifierName);
        #endif
            _parsedStringConstObjectCount--;
            delete[] pIdentifierName;
            pNext = pch; return false;
        }
    }

    // expression syntax check 
    _thisLvl_lastIsVariable = false;
    _thislvl_lastIsConstVar = false;

    // command argument constraints check : reset for next command parameter
    _lvl0_withinExpression = false;
    _lvl0_isPurePrefixIncrDecr = false;
    _lvl0_isPureVariable = false;
    _lvl0_isVarWithAssignment = false;

    Token_constant* pToken = (Token_constant*)_programCounter;
    pToken->tokenType = tok_isGenericName | (sizeof(Token_constant) << 4);
    memcpy(pToken->cstValue.pStringConst, &pIdentifierName, sizeof(pIdentifierName));           // pointer not necessarily aligned with word size: copy memory instead

    _lastTokenStep = _programCounter - _programStorage;
    _lastTokenType = tok_isGenericName;
    _lastTokenIsString = false, _lastTokenIsTerminal = false; _lastTokenIsPrefixOp = false; _lastTokenIsPostfixOp = false, _lastTokenIsPrefixIncrDecr = false;

#if PRINT_PARSED_TOKENS
    _pDebugOut->print("   parsed identifier: step is "); _pDebugOut->print(_lastTokenStep); _pDebugOut->print(" ["); _pDebugOut->print(pIdentifierName);  _pDebugOut->println("]");
#endif

    _programCounter += sizeof(Token_constant);
    *_programCounter = tok_no_token;                                                            // indicates end of program
    result = result_parsing_OK;                                                                 // flag 'valid token found'
    return true;
}


// --------------------------------------------------------------------------------------------
// *   Array parsing: check that max dimension count and maximum array size is not exceeded   *
// --------------------------------------------------------------------------------------------

bool Justina::checkArrayDimCountAndSize(parsingResult_type& result, int* arrayDef_dims, int& dimCnt) {

    bool lastIsLeftPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_leftPar) : false;
    if (lastIsLeftPar) { result = result_arrayDef_noDims; return false; }

    dimCnt++;

    if (dimCnt > MAX_ARRAY_DIMS) { result = result_arrayDef_maxDimsExceeded; return false; }

    char valueType = (*(_programStorage + _lastTokenStep) >> 4) & value_typeMask;

    long l{};                                                                                   // last token is a number constant: dimension
    float f{ 0 };
    if (valueType == (value_isLong)) {     // float
        memcpy(&l, ((Token_constant*)(_programStorage + _lastTokenStep))->cstValue.longConst, sizeof(l));
    }
    else {
        memcpy(&f, ((Token_constant*)(_programStorage + _lastTokenStep))->cstValue.floatConst, sizeof(f));
        l = int(f);
    }

    if (l < 1) { result = result_arrayDef_negativeDim; return false; }
    if (l > 255) { result = result_arrayDef_dimTooLarge; return false; }
    arrayDef_dims[dimCnt - 1] = l;
    int arrayElements = 1;
    for (int cnt = 0; cnt < dimCnt; cnt++) { arrayElements *= arrayDef_dims[cnt]; }
    if (arrayElements > MAX_ARRAY_ELEM) { result = result_arrayDef_maxElementsExceeded; return false; }
    return true;
}


// -------------------------------------------------------------------------------------------------------------------------------------
// *   Justina function definition statement parsing: check order of mandatory and optional arguments, check if max. n° not exceeded   *
// -------------------------------------------------------------------------------------------------------------------------------------

bool Justina::checkJustinaFunctionArguments(parsingResult_type& result, int& minArgCnt, int& maxArgCnt, bool thisTokenIsRightParenthesis) {

    // if a parameter in the function definition is (1) a scalar variable without assignment to a constant, OR (2) an array variable, the corresponding argument in a function call is mandatory
    // -> if the last token processed (before the current token, which is either a comma or a right parenthesis) is a (scalar) variable or an array variable closing parenthesis,...
    // ...the corresponding argument is mandatory; otherwise it is optional
    bool argWasMandatory = ((_lastTokenType == tok_isVariable) || (_lastTokenIsTerminal && (_lastTermCode == termcod_rightPar)));
    bool alreadyOptArgs = (minArgCnt != maxArgCnt);

    if (argWasMandatory && alreadyOptArgs) { result = result_function_mandatoryArgFoundAfterOptionalArgs; return false; }
    if (argWasMandatory) { minArgCnt++; }
    maxArgCnt++;

    // if last token was a comma, check that max argument count will not be exceeded (number must fit in 4 bits)
    if ((!thisTokenIsRightParenthesis) && (maxArgCnt >= c_JustinaFunctionMaxArgs)) {
        result = result_function_maxArgsExceeded; return false;
    }
    return true;
}


// ---------------------------------------------------------------------------------------------------------------------
// *   internal cpp function: check that order of arrays and scalar variables is consistent with function definition   *
// ---------------------------------------------------------------------------------------------------------------------

bool Justina::checkInternCppFuncArgArrayPattern(parsingResult_type& result) {
    int funcIndex = _pParsingStack->openPar.identifierIndex;                                    // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
    char paramIsArrayPattern = _internCppFunctions[funcIndex].arrayPattern;
    int argNumber = _pParsingStack->openPar.actualArgsOrDims;

    if (argNumber > 0) {
        bool isArray = false;
        if (_lastTokenType == tok_isVariable) {                                                 // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((Token_variable*)(_programStorage + _lastTokenStep))->identInfo) & var_isArray;
        }

        bool arrayArgumentExpected = false;
        if (argNumber >= 8) {                                                                   // only the first 8 arguments can pass arrays
            if (isArray) { result = result_function_scalarArgExpected; return false; }
        }
        else {
            if (((paramIsArrayPattern >> (argNumber - 1)) & 0b1) != isArray) { result = isArray ? result_function_scalarArgExpected : result_function_arrayArgExpected; return false; }
        }
    }
    return true;
}


// ----------------------------------------------------------------------------------------------
// *   external cpp function: check that an argument is a scalar value (variable or constant)   *
// ----------------------------------------------------------------------------------------------

bool Justina::checkExternCppFuncArgIsScalar(parsingResult_type& result) {
    int argNumber = _pParsingStack->openPar.actualArgsOrDims;

    if (argNumber > 0) {
        bool isArray = false;
        if (_lastTokenType == tok_isVariable) {                                                 // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((Token_variable*)(_programStorage + _lastTokenStep))->identInfo) & var_isArray;
        }

        if (isArray) { result = result_function_scalarArgExpected; return false; }
    }
    return true;
}


// -----------------------------------------------------------------------------------------------------------------------------------
// *   Justina function: check that order of arrays and scalar variables is consistent with previous calls and function definition   *
// -----------------------------------------------------------------------------------------------------------------------------------

bool Justina::checkJustinaFuncArgArrayPattern(parsingResult_type& result, bool isFunctionClosingParenthesis) {

    int funcIndex = _pParsingStack->openPar.identifierIndex;                                    // note: also stored in stack for FUNCTION definition block level; here we can pick one of both
    int argNumber = _pParsingStack->openPar.actualArgsOrDims;
    uint16_t paramIsArrayPattern{ 0 };
    memcpy(&paramIsArrayPattern, justinaFunctionData[funcIndex].paramIsArrayPattern, sizeof(char[2]));
    if (argNumber > 0) {

        bool isArray = false;
        bool lastIsRightPar = _lastTokenIsTerminal ? (_lastTermCode == termcod_rightPar) : false;

        if (_isJustinaFunctionCmd) { isArray = lastIsRightPar; }                                // function definition: if variable name followed by empty parameter list ' () ': array parameter
        else if (_lastTokenType == tok_isVariable) {                                            // function call and last token is variable name ? Could be an array name                                                                                      // function call
            // check if variable is defined as array (then it will NOT be part of an expression )
            isArray = (((Token_variable*)(_programStorage + _lastTokenStep))->identInfo) & var_isArray;
        }

        uint16_t paramArrayMask = 1 << (argNumber - 1);
        if (paramIsArrayPattern & 0x8000) {                                                     // function not used yet (before it was defined now: no need to check, just set array bit)
            paramIsArrayPattern = paramIsArrayPattern | (isArray ? paramArrayMask : 0);
        }
        else {  // error message can not be more specific (scalar expected, array expected) because maybe function has not been defined yet
            if ((paramIsArrayPattern & paramArrayMask) != (isArray ? paramArrayMask : 0)) { result = result_function_scalarAndArrayArgOrderNotConsistent; return false; }
        }
    }

    if (isFunctionClosingParenthesis) { paramIsArrayPattern = paramIsArrayPattern & ~0x8000; }  // function name used now: order of scalar and array parameters is now fixed
    memcpy(justinaFunctionData[funcIndex].paramIsArrayPattern, &paramIsArrayPattern, sizeof(char[2]));
    return true;
}


// -----------------------------------------
// *   parse a number (integer or float)   *
// -----------------------------------------

// called while parsing expressions and while executing specifying print commands (e.g. 'readList')

bool Justina::parseIntFloat(char*& pNext, char*& pch, Val& value, char& valueType, int& predefinedConstIndex, parsingResult_type& result) {

    result = result_tokenNotFound;                                                              // init: flag 'no token found'
    pch = pNext;                                                                                // pointer to first character to parse (any spaces have been skipped already)
    predefinedConstIndex = -1;                                                                  // init: assume literal constant

    // first, check for symbolic number
    char* tokenStart = pNext;
    if (isalpha(pNext[0])) {                                                                    // first character is a letter ? could be symbolic constant
        while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                             // position as if symbolic constant was found, for now

        for (int index = _symbvalueCount - 1; index >= 0; index--) {                            // for all defined symbolic names: check against alphanumeric token (NOT ending by '\0')
            if (strlen(_symbNumConsts[index].symbolName) != pNext - pch) { continue; }          // token has correct length ? If not, skip remainder of loop ('continue')                            
            if (strncmp(_symbNumConsts[index].symbolName, pch, pNext - pch) != 0) { continue; } // token corresponds to symbolic name ? If not, skip remainder of loop ('continue')    

            bool isNumber = ((_symbNumConsts[index].valueType == value_isLong) || (_symbNumConsts[index].valueType == value_isFloat));
            if (isNumber) {
                if ((_symbNumConsts[index].valueType == value_isLong)) { value.longConst = strtol(_symbNumConsts[index].symbolValue, nullptr, 0); }
                else { value.floatConst = strtof(_symbNumConsts[index].symbolValue, nullptr); }
                valueType = _symbNumConsts[index].valueType;
                predefinedConstIndex = index;
                result = result_parsing_OK;
                return true;                               // is a symbolic NUMBER constant: return 
            }

            else { pNext = pch;  return true; }            // is a symbolic constant but NOT a symbolic NUMBER constant:  reset first input stream character and return
        }
        pNext = pch; return true;                          // is not a symbolic constant, nor a literal constant (but it can still be another token type): reset first input stream character and return
    }

    // is not a symbolic number: numeric literal ?

    // all numbers will be positive, because leading '-' or '+' characters are parsed separately as prefix operators
    // this is important if next infix operator (power) has higher priority then this prefix operator: -2^4 <==> -(2^4) <==> -16, AND NOT (-2)^4 <==> 16 
    // exception: variable declarations with initializers: prefix operators are not parsed separately (no expressions, only one literal constant)

    pNext = tokenStart;
    ;
    bool isLong{ false };
    int i{ 0 };

    int base = ((tokenStart[0] == '0') && ((tokenStart[1] == 'x') || (tokenStart[1] == 'X'))) ? 16 : ((tokenStart[0] == '0') && ((tokenStart[1] == 'b') || (tokenStart[1] == 'B'))) ? 2 : 10;

    if (base == 10) {      // base 10
        while (isDigit(tokenStart[++i]));
        isLong = ((i > 0) && (tokenStart[i] != '.') && (tokenStart[i] != 'E') && (tokenStart[i] != 'e'));   // no decimal point, no exponent and minimum one digit
    }

    else {       // binary or hexadecimal
        tokenStart += 2;      // skip "0b" or "0x" and start looking for digits at next position
        while ((base == 16) ? isxdigit(tokenStart[++i]) : ((tokenStart[i] == '0') || (tokenStart[i] == '1'))) { ++i; }
        isLong = (i > 0);        // minimum one digit
        if (!isLong) { pNext = pch; result = result_numberInvalidFormat; return false; }        // not a long constant, but not a float either 
    }

    if (isLong) {                                                                               // token can be parsed as long ?
        valueType = value_isLong;
        // string to UNSIGNED long before assigning to (signed) long -> 0xFFFFFFFF will be stored as -1, as it should (all bits set)
        value.longConst = strtoul(tokenStart, &pNext, base);
        if (_initVarOrParWithUnaryOp == -1) { value.longConst = -value.longConst; }
    }
    else {
        valueType = value_isFloat;
        value.floatConst = strtof(tokenStart, &pNext);
        if (_initVarOrParWithUnaryOp == -1) { value.floatConst = -value.floatConst; }
    }                                                                                           // token can be parsed as float ?

    bool isValidNumber = (tokenStart != pNext);                                                 // is a number if pointer pNext was not moved (is NO error - possibly it's another valid token type)
    if (isValidNumber) { result = result_parsing_OK; }
    return true;                                                                                // no error; result indicates whether valid token was found or search for valid token needs to be continued
}


// --------------------------------
// *   parse a character string   *
// --------------------------------

// called while parsing expressions and while executing specifying print commands (e.g. 'readList')

bool Justina::parseString(char*& pNext, char*& pch, char*& pStringCst, char& valueType, int& predefinedConstIndex, parsingResult_type& result, bool isIntermediateString) {

    result = result_tokenNotFound;                                                              // init: flag 'no token found'
    pch = pNext;                                                                                // pointer to first character to parse (any spaces have been skipped already)
    predefinedConstIndex = -1;                                                                  // init: assume literal constant

    // first, check for predefined constant name
    char* tokenStart = pNext;
    if (isalpha(pNext[0])) {                                                                    // first character is a letter ? could be predefined symbolic constant
        while (isalnum(pNext[0]) || (pNext[0] == '_')) { pNext++; }                             // position as if symbolic constant was found, for now
        for (int index = _symbvalueCount - 1; index >= 0; index--) {                            // for all defined symbolic names: check against alphanumeric token (NOT ending by '\0')
            if (strlen(_symbNumConsts[index].symbolName) != pNext - pch) { continue; }          // token has correct length ? If not, skip remainder of loop ('continue')                            
            if (strncmp(_symbNumConsts[index].symbolName, pch, pNext - pch) != 0) { continue; } // token corresponds to symbolic name ? If not, skip remainder of loop ('continue')    

            bool isString = (_symbNumConsts[index].valueType == value_isStringPointer);
            if (isString) {
                pStringCst = (char*)_symbNumConsts[index].symbolValue;                          // no copy of the string itself: point to the table value entry
                valueType = value_isStringPointer;
                predefinedConstIndex = index;                                                   // this flags parsed constant as predefined symbolic constant
                result = result_parsing_OK;
                return true;                                // is a symbolic STRING constant: return
            }

            else { pNext = pch;  return true; }             // is a symbolic constant but NOT a symbolic STRING constant: reset first input stream character and return
        }
        pNext = pch; return true;                           // is not a symbolic constant, nor a literal constant (but it can still be another token type): reset first input stream character and return
    }

    // is not a symbolic string constant: string literal ?

    if ((pNext[0] != '\"')) { return true; }                                                    // no opening quote ? Is not an alphanumeric cst (it can still be something else)

    pNext++;                                                                                    // skip opening quote
    int escChars = 0;

    while (pNext[0] != '\"') {                                                                  // do until closing quote, if any
        // if no closing quote found, an invalid escape sequence or a control character detected, reset pointer to first character to parse, indicate error and return
        if (pNext[0] == '\0') { pNext = pch; result = result_alphaClosingQuoteMissing; return false; }
        if (pNext[0] < ' ') { pNext = pch; result = result_alphaNoCtrlCharAllowed; return false; }
        if (pNext[0] == '\\') {
            // valid escape sequences: "\\" (add backslash), "\"" (add double quote), "\r" (add '\r' = cr) and "\n" (add '\n' = nl)
            if ((pNext[1] == '\\') || (pNext[1] == '\"') || (pNext[1] == 'r') || (pNext[1] == 'n')) { pNext++; escChars++; }
            else { pNext = pch; result = result_alphaConstInvalidEscSeq; return false; }
        }
        pNext++;
    };

    // if alphanumeric constant is too long, reset pointer to first character to parse, indicate error and return
    if (pNext - (pch + 1) - escChars > MAX_ALPHA_CONST_LEN) { pNext = pch; result = result_alphaConstTooLong; return false; }

    // token is an alphanumeric constant, and it's allowed here
    pStringCst = nullptr;                                                                       // init
    if (pNext - (pch + 1) - escChars > 0) {    // not an empty string: create string object 
        isIntermediateString ? _intermediateStringObjectCount++ : _parsedStringConstObjectCount++;
        pStringCst = new char[pNext - (pch + 1) - escChars + 1];                                // create char array on the heap to store alphanumeric constant, including terminating '\0'
        // store alphanumeric constant in newly created character array
        pStringCst[pNext - (pch + 1) - escChars] = '\0';                                        // store string terminating '\0' (pch + 1 points to character after opening quote, pNext points to closing quote)
        char* pSource = pch + 1, * pDestin = pStringCst;                                        // pSource points to character after opening quote
        bool isEscSeq{ false };
        while (pSource + escChars < pNext) {                                                    // store alphanumeric constant in newly created character array (terminating '\0' already added)
            if (isEscSeq = (pSource[0] == '\\')) { pSource++; escChars--; }                     // if escape sequences found: skip first escape sequence character (backslash)
            pDestin++[0] = pSource++[0];

            if (isEscSeq && ((pSource[-1] == 'r') || (pSource[-1] == 'n'))) {
                pDestin[-1] = (pSource[-1] == 'r') ? '\r' : '\n';
            }

        }
    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print(isIntermediateString ? "\r\n+++++ (Intermd str) " : "\r\n+++++ (parsed str ) "); _pDebugOut->println((uint32_t)pStringCst, HEX);
        _pDebugOut->print("       parse string "); _pDebugOut->println(pStringCst);
    #endif
    }
    pNext++;                                                                                    // skip closing quote

    valueType = value_isStringPointer;
    result = result_parsing_OK;
    return true;                                                                                // valid string
}


// -------------------------------------------------------------------------
// *   check if identifier storage exists already, optionally create new   *
// -------------------------------------------------------------------------

int Justina::getIdentifier(char** pIdentNameArray, int& identifiersInUse, int maxIdentifiers, char* pIdentNameToCheck, int identLength, bool& createNewName, bool isUserVar) {

    char* pIdentifierName;
    int index = 0;          // points to last variable in use
    while (index < identifiersInUse) {                       // points to variable in use
        pIdentifierName = pIdentNameArray[index];
        if (strlen(pIdentifierName) == identLength) {                                           // identifier with name with same length found
            if (strncmp(pIdentifierName, pIdentNameToCheck, identLength) == 0) { break; }       // storage for identifier name was created already 
        }
        index++;
    }
    if (index == identifiersInUse) { index = -1; }                                              // not found
    if (!createNewName) { return index; }                                                       // if check only: identNameIndex to identifier name or -1, createNewName = false

    createNewName = (index == -1);                                                              // create new ?

    // create new identifier if it does not exist yet ?
    // upon return, createNew indicates whether new identifier storage NEEDED to be created ...
    // and, if creating was successful, identifiersInUse will be set to the new identifier count

    if (createNewName) {
        if (identifiersInUse == maxIdentifiers) { return index; }                               // create identifier name failed: return -1 with createNewName = true
        isUserVar ? _userVarNameStringObjectCount++ : _identifierNameStringObjectCount++;
        pIdentifierName = new char[MAX_IDENT_NAME_LEN + 1 + 1];                                 // create standard length char array on the heap, including '\0' and an extra character 
        strncpy(pIdentifierName, pIdentNameToCheck, identLength);                               // store identifier name in newly created character array
        pIdentifierName[identLength] = '\0';                                                    // string terminating '\0'
    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print(isUserVar ? "\r\n+++++ (usrvar name) " : "\r\n+++++ (ident name ) "); _pDebugOut->println((uint32_t)pIdentifierName, HEX);
        _pDebugOut->print("     get identifier "); _pDebugOut->println(pIdentifierName);
    #endif
        pIdentNameArray[identifiersInUse] = pIdentifierName;
        identifiersInUse++;
        return identifiersInUse - 1;                                                            // identNameIndex to newly created identifier name
    }
    else { return index; }
}


// --------------------------------------------------------------
// *   initialize a variable or an array with (a) constant(s)   *
// --------------------------------------------------------------

bool Justina::initVariable(uint16_t varTokenStep, uint16_t constTokenStep) {
    long l{ 0 };
    float f{ 0. };        // last token is a number constant: dimension
    char* pString{ nullptr };

    // parsing: initialize variables and arrays with a constant number or (arrays: empty) string

    // fetch variable location and attributes
    bool isArrayVar = ((Token_variable*)(_programStorage + varTokenStep))->identInfo & var_isArray;
    bool isGlobalVar = (((Token_variable*)(_programStorage + varTokenStep))->identInfo & var_scopeMask) == var_isGlobal;
    bool isUserVar = (((Token_variable*)(_programStorage + varTokenStep))->identInfo & var_scopeMask) == var_isUser;
    int varValueIndex = ((Token_variable*)(_programStorage + varTokenStep))->identValueIndex;
    void* pVarStorage = isGlobalVar ? globalVarValues : isUserVar ? userVarValues : staticVarValues;
    char* pVarTypeStorage = isGlobalVar ? globalVarType : isUserVar ? userVarType : staticVarType;
    void* pArrayStorage;        // array storage (if array) 

    // fetch constant (numeric or alphanumeric) 
    char valueType = (((Token_constant*)(_programStorage + constTokenStep))->tokenType >> 4) & value_typeMask;
    bool isLongConst = (valueType == value_isLong);
    bool isFloatConst = (valueType == value_isFloat);
    bool isStringConst = (valueType == value_isStringPointer);

    if (isLongConst) { memcpy(&l, ((Token_constant*)(_programStorage + constTokenStep))->cstValue.longConst, sizeof(l)); }          // copy float
    else if (isFloatConst) { memcpy(&f, ((Token_constant*)(_programStorage + constTokenStep))->cstValue.floatConst, sizeof(f)); }   // copy float
    else { memcpy(&pString, ((Token_constant*)(_programStorage + constTokenStep))->cstValue.pStringConst, sizeof(pString)); }       // copy pointer to string (not the string itself)
    int length = (!isStringConst) ? 0 : (pString == nullptr) ? 0 : strlen(pString);                                                 // only relevant for strings

    if (isArrayVar) {
        pArrayStorage = ((void**)pVarStorage)[varValueIndex];                                                                       // void pointer to an array 
        int dimensions = (((char*)pArrayStorage)[3]);                                                                               // can range from 1 to MAX_ARRAY_DIMS
        int arrayElements = 1;                                                                                                      // determine array size
        for (int dimCnt = 0; dimCnt < dimensions; dimCnt++) { arrayElements *= (int)((((char*)pArrayStorage)[dimCnt])); }
        // fill up with numeric constants or (empty strings:) null pointers
        if (isLongConst) { for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) { ((long*)pArrayStorage)[arrayElem] = l; } }
        else if (isFloatConst) { for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) { ((float*)pArrayStorage)[arrayElem] = f; } }
        else {                                                                                                                      // alphanumeric constant
            if (length != 0) { return false; };                                                                                     // to limit memory usage, no mass initialization with non-empty strings
            for (int arrayElem = 1; arrayElem <= arrayElements; arrayElem++) {
                ((char**)pArrayStorage)[arrayElem] = nullptr;
            }
        }
    }

    else {                                                                                                                          // scalar
        if (isLongConst) { ((long*)pVarStorage)[varValueIndex] = l; }                                                               // store numeric constant
        else if (isFloatConst) { ((float*)pVarStorage)[varValueIndex] = f; }                                                        // store numeric constant
        else {                                                                                                                      // alphanumeric constant
            if (length == 0) {
                ((char**)pVarStorage)[varValueIndex] = nullptr;                                                                     // an empty string does not create a heap object
            }
            else { // create string object and store string
                isUserVar ? _userVarStringObjectCount++ : _globalStaticVarStringObjectCount++;
                // create char array on the heap to store alphanumeric constant, including terminating '\0'
                char* pVarAlphanumValue = new char[length + 1];
                // store alphanumeric constant in newly created character array
                strcpy(pVarAlphanumValue, pString);              // including terminating \0
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print(isUserVar ? "\r\n+++++ (usr var str) " : "\r\n+++++ (var string ) "); _pDebugOut->println((uint32_t)pVarAlphanumValue, HEX);
                _pDebugOut->print("      init variable "); _pDebugOut->println(pVarAlphanumValue);
            #endif
                ((char**)pVarStorage)[varValueIndex] = pVarAlphanumValue;                                                           // store pointer to string
            }
        }
    }


    pVarTypeStorage[varValueIndex] = (pVarTypeStorage[varValueIndex] & ~value_typeMask) |
        (isLongConst ? value_isLong : isFloatConst ? value_isFloat : value_isStringPointer);
    return true;
};


// ------------------------------
// *   delete a user variable   *
// ------------------------------

Justina::parsingResult_type Justina::deleteUserVariable(char* userVarName) {

    bool deleteLastVar = (userVarName == nullptr);

    bool varDeleted{ false };
    for (int index = (deleteLastVar ? _userVarCount - 1 : 0); index < _userVarCount; index++) {
        if (!deleteLastVar) {
            if (strcmp(userVarNames[index], userVarName) != 0) { continue; }                                // no match yet: continue looking for it (if it exists)
        }

        bool userVarUsedInProgram = (userVarType[index] & var_userVarUsedByProgram);
        if (userVarUsedInProgram) { return result_var_usedInProgram; }                                      // match, but cannot delete (variable used in program)

        int valueType = (userVarType[index] & value_typeMask);
        bool isLong = (valueType == value_isLong);
        bool isFloat = (valueType == value_isFloat);
        bool isString = (valueType == value_isStringPointer);
        bool isArray = (userVarType[index] & var_isArray);


        // 1. delete variable name object
        // ------------------------------
    #if PRINT_HEAP_OBJ_CREA_DEL
        _pDebugOut->print("\r\n----- (usrvar name) "); _pDebugOut->println((uint32_t) * (userVarNames + index), HEX);
        _pDebugOut->print("    delete user var "); _pDebugOut->println(*(userVarNames + index));
    #endif
        _userVarNameStringObjectCount--;
        delete[] * (userVarNames + index);

        // 2. if variable is an array of strings: delete all non-empty strings in array
        // ----------------------------------------------------------------------------
        if (isArray && isString) { deleteOneArrayVarStringObjects(userVarValues, index, true, false); }

        // 3. if variable is an array: delete the array storage
        // ----------------------------------------------------
        //    NOTE: do this before checking for strings (if both 'var_isArray' and 'value_isStringPointer' bits are set: array of strings, with strings already deleted)
        if (isArray) {       // variable is an array: delete array storage          
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n----- (usr ar stor)"); _pDebugOut->println((uint32_t)userVarValues[index].pArray, HEX);
        #endif
            delete[]  userVarValues[index].pArray;
            _userArrayObjectCount--;
        }

        // 4. if variable is a scalar string value: delete string
        // ------------------------------------------------------
        else if (isString) {                                                                                // variable is a scalar containing a string
            if (userVarValues[index].pStringConst != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n----- (usr var str) "); _pDebugOut->println((uint32_t)userVarValues[index].pStringConst, HEX);
                _pDebugOut->print("del user var string "); _pDebugOut->println(userVarValues[index].pStringConst);
            #endif
                _userVarStringObjectCount--;
                delete[]  userVarValues[index].pStringConst;
            }
        }

        // 5. move up next user variables one place
        //    if a user variable is used in currently loaded program: adapt index in program storage
        // -----------------------------------------------------------------------------------------
        for (int i = index; i < _userVarCount - 1; i++) {
            userVarNames[i] = userVarNames[i + 1];
            userVarValues[i] = userVarValues[i + 1];
            userVarType[i] = userVarType[i + 1];

            userVarUsedInProgram = (userVarType[i + 1] & var_userVarUsedByProgram);
            if (userVarUsedInProgram) {

                char* programStep = _programStorage;
                int tokenType{};
                do {
                    tokenType = findTokenStep(programStep, true, tok_isVariable, var_isUser, i + 1);
                    if (tokenType == '\0') { break; }
                    --((Token_variable*)programStep)->identValueIndex;
                } while (true);
            }
        }

        _userVarCount--;
        varDeleted = true;
    }

    if (!varDeleted) { return result_variableNameExpected; }

    return result_parsing_OK;
}
