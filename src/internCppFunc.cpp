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

#define PRINT_HEAP_OBJ_CREA_DEL 0


// *****************************************************
// ***        class Justina - implementation         ***
// *****************************************************


// -------------------------------------
// *   execute internal cpp function   *
// -------------------------------------

// structure of an internal cpp function: function name (expression, expression, ...) ;
// during parsing, preliminary checks have been done already: minimum, maximum number of arguments allowed for the function and, for each argument supplied, whether a single value or an array is expected as argument
// the expression list as a whole is put between parentheses (in contrast to command arguments)

Justina::execResult_type Justina::execInternalCppFunction(LE_evalStack*& pFunctionStackLvl, LE_evalStack*& pFirstArgStackLvl, int suppliedArgCount, bool& forcedAbortRequest) {

    // this procedure is called when the closing parenthesis of an internal Justina function is encountered.
    // all internal Justina functions use the same standard mechanism (with exception of the eval() function):
    // -> all variables are passed by reference; parsed constants, intermediate constants (intermediate calculation results) are passed by value (for a string, this refers to the string pointer).
    // right now, any function arguments (parsed constants, variable references, intermediate calculation results) have been pushed on the evaluation stack already.
    // first thing to do is to copy these arguments (longs, floats, pointers to strings) to a fixed 'arguments' array, as well as a few attributes.
    // - variable references are not copied, instead the actual value of the variable is stored (long, float, string pointer OR array pointer if the variable is an array)
    // - in case the function needs to change the variable value, the variable reference is still available on the stack.
    //   => if it is not certain that this particular stack element does not contain a variable reference, check this first.
    // next, control is passed to the specific Justina function (switch statement below).

    // when the Justina function terminates, arguments are removed from the evaluation stack and the function result is pushed on the stack (at the end of the current procedure)...
    // ...as an intermediate constant (long, float, pointer to string).
    // if the result is a non-empty string, a new string is created on the heap (Justina convention: empty strings are represented by a null pointer to conserve memory).

    // IMPORTANT: at any time, when an error occurs, a RETURN <error code> statement can be called, BUT FIRST all 'intermediate character strings' which are NOT referenced 
    // within the evaluation stack MUST BE  DELETED (if referenced, they will be deleted automatically by error handling)


    // remember token address of internal cpp function token (address from where the internal cpp function is called), in case an error occurs (while passing arguments etc.)   
    _activeFunctionData.errorProgramCounter = pFunctionStackLvl->function.tokenAddress;

    int functionIndex = pFunctionStackLvl->function.index;
    char functionCode = _internCppFunctions[functionIndex].functionCode;

    char fcnResultValueType{ value_isLong };  // init
    Val fcnResult;
    fcnResult.longConst = 0;

    char argValueType[16];                                                                                                  // room for 16 arguments
    Val args[16];

    bool requestPrintTab{ false }, requestGotoPrintColumn{ false };

    long argIsVarBits{ 0 }, argIsConstantVarBits{ 0 }, argIsLongBits{ 0 }, argIsFloatBits{ 0 }, argIsStringBits{ 0 };


    // preprocess: retrieve argument(s) info: variable or constant, value type
    // -----------------------------------------------------------------------

    if (suppliedArgCount > 0) {
        LE_evalStack* pStackLvl = pFirstArgStackLvl;                                                                        // pointing to first argument on stack

        int bitNmask{ 0x01 };                                                                                               // lsb
        for (int i = 0; i < suppliedArgCount; i++) {

            // value type of args
            if (pStackLvl->varOrConst.tokenType == tok_isVariable) { argIsVarBits |= bitNmask; }
            if (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isConstantVar) { argIsConstantVarBits |= bitNmask; }

            argValueType[i] = (argIsVarBits & (1 << i)) ? (*pStackLvl->varOrConst.varTypeAddress & value_typeMask) : pStackLvl->varOrConst.valueType;
            args[i].floatConst = (argIsVarBits & (0x1 << i)) ? (*pStackLvl->varOrConst.value.pFloatConst) : pStackLvl->varOrConst.value.floatConst;// fetch args: line is valid for all value types

            if (((uint8_t)argValueType[i] == value_isLong)) { argIsLongBits |= bitNmask; }
            if (((uint8_t)argValueType[i] == value_isFloat)) { argIsFloatBits |= bitNmask; }
            if (((uint8_t)argValueType[i] == value_isStringPointer)) { argIsStringBits |= bitNmask; }

            bitNmask <<= 1;
            pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);                                             // value fetched: go to next argument
        }
    }

    // execute a specific function
    // ---------------------------

    switch (functionCode) {

        // ------------------
        // SD card: open file
        // ------------------

        case fnccod_open:
        {
            int newFileNumber{};
            // file path must be string
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }                                      // file full name including path

            // access mode
            long mode = READ_FILE;      // default: open for reading
            if (suppliedArgCount == 2) {
                if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
                mode = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : args[1].floatConst;
            }

            // open file and retrieve file number
            execResult_type execResult = SD_open(newFileNumber, args[0].pStringConst, mode);
            if (execResult != result_exec_OK) { return execResult; }

            // save file number as result
            fcnResultValueType = value_isLong;
            fcnResult.longConst = newFileNumber;                                                                            // 0: could not open file
        }
        break;


        // ---------------------------------------------------------------------
        // SD card: test if file exists, create or remove directory, remove file
        // ---------------------------------------------------------------------

        case fnccod_exists:                                                                                                 // does file or directory exist ?
        case fnccod_mkdir:                                                                                                  // create directory
        case fnccod_rmdir:                                                                                                  // remove directory
        case fnccod_remove:                                                                                                 // remove file
        case fnccod_fileNumber:                                                                                             // return file number for given filename; return 0 if not open
        {
            // checks
            if ((_justinaStartupOptions & SD_mask) == SD_notAllowed) { return result_SD_noCardOrNotAllowed; }
            if (!_SDinitOK) { return result_SD_noCardOrCardError; }
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }                                      // file path
            char* filePath = args[0].pStringConst;
            execResult_type execResult = pathValid(filePath);
            if (execResult != result_exec_OK) { return execResult; }                                           // is not a complete check, but it remedies a few flaws in SD library

            fcnResultValueType = value_isLong;                                                                              // init

            // first, check whether the file exists
            // ESP32 requires that the path starts with a slash
            int len = strlen(filePath);
            char* filePathWithSlash = filePath;                                                                             // init
            if (filePath[0] != '/') {                                                                                       // starting '/' missing)
                _systemStringObjectCount++;
                filePathWithSlash = new char[1 + len + 1];                                                                  // include space for starting '/' and ending '\0'
                filePathWithSlash[0] = '/';
                strcpy(filePathWithSlash + 1, filePath);                                                                    // copy original string
            }

            bool fileExists = (long)SD.exists(filePathWithSlash);

            // file exists check only: return 0 or 1;
            if (functionCode == fnccod_exists) {
                fcnResult.longConst = fileExists;
                if (filePathWithSlash != filePath) {
                    delete[] filePathWithSlash;           // compare pointers (if not equal, then one char* is new and must be deleted)
                    _systemStringObjectCount--;
                }
                break;
            }

            // make directory ? check that the file does not exist yet  
            if (functionCode == fnccod_mkdir) {
                fcnResult.longConst = fileExists ? 0 : (long)SD.mkdir(filePath);
                break;
            }

            // check whether the file is open 
            bool fileIsOpen{ false };
            int i{ 0 };
            if (_openFileCount > 0) {
                for (i = 0; i < MAX_OPEN_SD_FILES; ++i) {
                    if (openFiles[i].fileNumberInUse) {
                        // skip starting slash (always present) in stored file path if file path argument doesn't start with a slash
                        if (strcasecmp(openFiles[i].filePath, filePathWithSlash) == 0) {                                    // 8.3 file format: NOT case sensitive  
                            fileIsOpen = true;
                            break;                                                                                          // break inner loop only 
                        }
                    }
                }
            }

            // check for open file ? return 1 if open, 0 if not
            if (functionCode == fnccod_fileNumber) { fcnResult.longConst = fileIsOpen ? (i + 1) : 0; }
            // remove directory or file ? return 1 if success, 0 if not
            // the SD library function itself will test for correct file type (lib or file)  
            else if (functionCode == fnccod_rmdir) { fcnResult.longConst = fileIsOpen ? 0 : (long)SD.rmdir(filePathWithSlash); }
            else if (functionCode == fnccod_remove) { fcnResult.longConst = fileIsOpen ? 0 : (long)SD.remove(filePathWithSlash); }

            if (filePathWithSlash != filePath) {
                delete[] filePathWithSlash;             // compare pointers (if not equal, then one char* is new and must be deleted)
                _systemStringObjectCount--;
            }
        }
        break;


        // ----------------------------
        // SD card: directory functions
        // ----------------------------

        case fnccod_isDirectory:
        case fnccod_rewindDirectory:
        case fnccod_openNextFile:
        {
            // check directory file number (also perform related file and SD card object checks)
            File* pFile{};
            int allowedFileTypes = (functionCode == fnccod_isDirectory) ? 0 : 2;                                            // 0: all file types allowed, 1: files only, 2: directories only
            bool allowSystemFiles = (functionCode == fnccod_isDirectory);                                                   // allow system files only for isDirectory() function
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile, allowedFileTypes, allowSystemFiles);
            if (execResult != result_exec_OK) { return execResult; }

            // access mode (openNextFile only)
            long mode = READ_FILE;                                                                                          // init: open for reading
            if (suppliedArgCount == 2) {
                if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
                mode = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : args[1].floatConst;
            }

            // execute function
            fcnResult.longConst = 0;                                                                                        // init
            fcnResultValueType = value_isLong;

            if (functionCode == fnccod_isDirectory) {
                fcnResult.longConst = (long)(pFile->isDirectory());
            }
            else if (functionCode == fnccod_rewindDirectory) {                                                              // rewind directory
                pFile->rewindDirectory();
            }

            else {          // open next file in directory
                // open file and retrieve file number
                int dirFileNumber = (argIsLongBits & (0x1 << 0)) ? (args[0].longConst) : (args[0].floatConst);              // cast directory file number to integer
                int newFileNumber{ 0 };
                execResult_type execResult = SD_openNext(dirFileNumber, newFileNumber, pFile, mode);                        // file could be open already: to be safe, open in read only mode here
                if (execResult != result_exec_OK) { return execResult; }
                fcnResult.longConst = newFileNumber;
            }
        }
        break;


        // -------------------------------------
        // SD card: close or flush file / stream
        // -------------------------------------

        case fnccod_close:                                                                                                  // close a file
        case fnccod_flush:                                                                                                  // empty output buffer
        {
            // flush(): stream is output stream; not for directories

            Stream* pStream{};
            int streamNumber{};
            // perform checks and return stream (file)
            execResult_type execResult = returnStreamRef(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber, true, (functionCode == fnccod_close) ? 0 : 1, false);
            if (execResult != result_exec_OK) { return execResult; }
            if (functionCode == fnccod_close) {
                if (streamNumber <= 0) { return result_SD_invalidFileNumber; }
                SD_closeFile(streamNumber);
            }
            else { pStream->flush(); }                                                                                      // empty output buffer (write to stream)

            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
        }
        break;


        // ----------------------------------------------------
        // SD card: close or flush file (only NON-SYSTEM files)
        // ----------------------------------------------------

        case fnccod_closeAll:
        {
            SD_closeAllFiles(false);                                                                                        // do NOT close system files
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
        }
        break;


        // ----------------------------------------------------------------------------------
        // SD: check if a file for a given file number is open and whether it's a system file
        // ----------------------------------------------------------------------------------

        case fnccod_slotHasOpenFile:
        {
            File* pFile{};
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile, 0);                // file number (all file types, system files)
            // do not produce error if file is not open; all other errors result in error being reported
            if ((execResult != result_exec_OK) && (execResult != result_SD_fileIsNotOpen) && (execResult != result_SD_isOpenSystemFile)) { return execResult; }  // error

            // save result
            fcnResultValueType = value_isLong;
            fcnResult.longConst = (execResult == result_SD_fileIsNotOpen) ? 0 : (execResult == result_exec_OK) ? 1 : 2;     // 0 (file not open), 1 (user file open) or 2 (system file open)
        }
        break;


        // ------------------------------------------------------------------
        // SD: return file position, size or available characters for reading
        // ------------------------------------------------------------------

        case fnccod_position:
        case fnccod_size:
        case fnccod_available:
        {
            // perform checks and set pointer to IO stream or file
            Stream* pStream{ _pConsoleIn };
            int streamNumber{ 0 };

            if ((functionCode != fnccod_available) || (suppliedArgCount > 0)) {
                // perform checks and set pointer to IO stream or file
                // perform checks and return stream for input (required for available() function)
                execResult_type execResult = returnStreamRef(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber, false, 1, true);
                if (execResult != result_exec_OK) { return execResult; }
                if ((streamNumber <= 0) && (functionCode != fnccod_available)) { return result_SD_invalidFileNumber; }      // because a file number expected here
            }

            // retrieve and return value
            long val{};
            if (functionCode == fnccod_position) { val = (static_cast<File*>(pStream))->position(); }                       // SD file only
            else if (functionCode == fnccod_size) { val = (static_cast<File*>(pStream))->size(); }                          // SD file only
            else { val = pStream->available(); }                                                                            // can be I/O stream as well

            fcnResultValueType = value_isLong;
            fcnResult.longConst = val;
        }
        break;


        // ---------------------------------------------------------------------
        // set or get time to wait for incoming characters for a specific stream
        // ---------------------------------------------------------------------

        case fnccod_setTimeout:
        case fnccod_getTimeout:
        {
            // setting a timeout value only works for established connections, and only as long as the connection is maintained (cf. TCP)
            // if stream does not point to an established connection, an error is only produced for SD streams. For other I/O streams, no warning is given (nothing happens)


            Stream* pStream{ _pConsoleIn };
            int streamNumber{ 0 };

            fcnResultValueType = value_isLong;
            execResult_type execResult = returnStreamRef(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber);     // perform checks and return stream for input 
            if (execResult != result_exec_OK) { return execResult; }

            // check second argument: timeout in milliseconds (set time out only)
            if (functionCode == fnccod_setTimeout) {
                if ((!(argIsLongBits & (0x1 << 1))) && (!(argIsFloatBits & (0x1 << 1)))) { return result_arg_numberExpected; }  // number of bytes to read
                long arg2 = (argIsLongBits & (0x1 << 1)) ? (args[1].longConst) : (args[1].floatConst);

                pStream->setTimeout((arg2 > 0) ? arg2 : 0);

                fcnResult.longConst = 0;
            }

            else { fcnResult.longConst = pStream->getTimeout(); }                                                           // get time out
        }
        break;


        // ---------------------
        // SD: set file position
        // ---------------------

        case fnccod_seek:
        {
            // check file number (also perform related file and SD card object checks)
            File* pFile{};
            // perform file checks and return stream (file) - do not allow file type 'directory'
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile);
            if (execResult != result_exec_OK) { return execResult; }

            // check second argument: position in file to seek 
            if ((!(argIsLongBits & (0x1 << 1))) && (!(argIsFloatBits & (0x1 << 1)))) { return result_arg_numberExpected; }  // number of bytes to read
            long arg2 = (argIsLongBits & (0x1 << 1)) ? (args[1].longConst) : (args[1].floatConst);

            // NOTE: with nano ESP32 board, when file is opened for WRITE, size() does not follow actual (growing) file size while writing (although position() returns correct position).

            long size = pFile->size();
            if ((arg2 > size) || (arg2 < -1)) { return result_SD_fileSeekError; }
            if (arg2 == -1) { arg2 = size; }            // EOF

            if (!pFile->seek(arg2)) { return result_SD_fileSeekError; }                                                     // library seek error

            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
        }
        break;


        // --------------------
        // SD: return file name
        // --------------------

        case fnccod_name:
        case fnccod_fullName:
        {
            // check file number (also perform related file and SD card object checks)
            File* pFile{};
            execResult_type execResult = SD_fileChecks(argIsLongBits, argIsFloatBits, args[0], 0, pFile, 0, true);          // perform file checks and return stream (file) - allow system files
            if (execResult != result_exec_OK) { return execResult; }

            int fileNumber = (argIsLongBits & (0x1 << 0)) ? (args[0].longConst) : (args[0].floatConst);

            // retrieve file name or full name and save
            fcnResultValueType = value_isStringPointer;

            int len = strlen((functionCode == fnccod_name) ? pFile->name() : openFiles[fileNumber - 1].filePath);           // always longer than 0 characters
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len + 1];                                                                     // will be pushed to evaluation stack
            strcpy(fcnResult.pStringConst, (functionCode == fnccod_name) ? pFile->name() : openFiles[fileNumber - 1].filePath);
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("          file name ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif            
            // note: only ESP32 SD library has a method 'path()': keep track of full name within Justina
        }
        break;


        // ------------------------------
        // get write error (file or IO)
        // clear write error (file or IO)
        // ------------------------------

        case fnccod_getWriteError:
        case fnccod_clearWriteError:
        case fnccod_availableForWrite:
        {
            // getWriteError(stream number) returns 0 if no error, otherwise it returns an error code
            // clearWriteError(stream number) clears the write error
            // availableForWrite(stream number) returns the number of bytes available for write
            Stream* pStream{  };
            int streamNumber{  };
            execResult_type execResult = returnStreamRef(argIsLongBits, argIsFloatBits, args[0], 0, pStream, streamNumber, true);   // perform checks and return output stream
            if (execResult != result_exec_OK) { return execResult; }

            fcnResultValueType = value_isLong;
            if (functionCode == fnccod_getWriteError) { fcnResult.longConst = pStream->getWriteError(); }
            else if (functionCode == fnccod_clearWriteError) { pStream->clearWriteError(); fcnResult.longConst = 0; }
            else { fcnResult.longConst = pStream->availableForWrite(); }
        }
        break;

        // ------------------------------------------------
        // peek (a) character(s) from a stream (file or IO)
        // read characters from a stream (file or IO)
        // ------------------------------------------------

        case fnccod_cin:
        case fnccod_read:
        case fnccod_peek:
        {
            // peek([stream number])    NOTE: stream number defaults to console in
            // cin()                    NOTE: form 2: cin([terminator character, ] length) ==> see next
            // read(stream number)      NOTE: form 2: read(stream number, [terminator character, ] length) => see next 

            // cin(), peek( [stream number] ),  read(stream number) functions return an ASCII code (0xff indicates no character has been received) and do NOT wait for time out
            // cin() is equivalent to read(CONSOLE)

            bool stayHere = (functionCode == fnccod_peek) ? true : (suppliedArgCount < ((functionCode == fnccod_cin) ? 1 : 2));
            if (stayHere) {
                Stream* pStream{ _pConsoleIn };                                                 // init
                int streamNumber{ 0 };

                // note: commands cin and read use read() method, which uses internal stream variables _pStreamIn and _pStreamOut set by setActiveStreamTo(). 
                //       peek() and available() methods below use local variable pStream, which is returned by returnStreamRef() as well as setActiveStreamTo().
                execResult_type execResult = (functionCode == fnccod_cin) ? setActiveStreamTo(streamNumber, pStream) :
                    setActiveStreamTo(argIsLongBits, argIsFloatBits, args[0], 0, streamNumber, pStream);     // perform checks and set input stream
                if (execResult != result_exec_OK) { return execResult; }

                // read character from stream now 
                char c{ 0xff };                                                                 // init: no character read
                if (functionCode == fnccod_peek) { c = pStream->peek(); }
                else if (pStream->available()) { c = read(); }                                  // set global variables before reading 

                // save result
                fcnResultValueType = value_isLong;
                fcnResult.longConst = (long)c;

                break;
            }
        }

        // NO break here: execution continues

        case fnccod_cinLine:
        case fnccod_readLine:
        {
            // cin([terminator character, ] length) :
            // read(stream number, [terminator character, ] length) :
            // -> read characters from stream (file or IO) until the number of characters to read is reached or optional terminator character is found
            // -> return a string containing the characters read

            // cinLine()
            // readLine(stream number)
            // -> read characters from stream (file or IO) until the internal buffer is full or the a '\n' (new line) character is read 
            // cinLine() is equivalent to readLine(CONSOLE)

            // terminator character: first character of 'terminator' string (if empty string: error)
            // if the 'length' argument is a variable, it returns the count of bytes read (read() function only)
            // functions return a character string variable or a nullptr (empty string)

            // NOTE: external I/O (only): functions will time out (see SetTimeout() function) if no (more) characters are available


            // perform checks and set pointer to IO stream or file
            int streamNumber{ 0 };
            execResult_type execResult{ result_exec_OK };

            bool streamArgPresent = ((functionCode == fnccod_read) || (functionCode == fnccod_readLine));
            bool isLineForm = ((functionCode == fnccod_cinLine) || (functionCode == fnccod_readLine));
            bool terminatorArgPresent = (!isLineForm) && (suppliedArgCount == ((functionCode == fnccod_cin) ? 2 : 3));

            execResult = streamArgPresent ? setActiveStreamTo(argIsLongBits, argIsFloatBits, args[0], 0, streamNumber) :
                setActiveStreamTo(streamNumber);                                                                            // perform checks and set input stream 
            if (execResult != result_exec_OK) { return execResult; }

            // check terminator character: first character in char * 
            char terminator{ 0xff };                                                                                        // init: no terminator
            if (isLineForm) { terminator = '\n'; }
            else if (terminatorArgPresent) {                                                                                // terminator argument supplied ?
                int termArgIndex = streamArgPresent ? 1 : 0;
                if (!(argIsStringBits & (0x1 << (termArgIndex)))) { return result_arg_stringExpected; }                     // cin(...): first argument, read(...): second argument
                if (args[termArgIndex].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
                terminator = args[termArgIndex].pStringConst[0];
            }

            // limit string length, because object for it is created on the heap
            int maxLineLength = MAX_ALPHA_CONST_LEN - 1;        // init: input line -> length is not specified, line terminator '('\n') will be added at the end
            // input & read: check length
            if (!isLineForm) {     // last argument specifies maximum length
                int lengthArgIndex = suppliedArgCount - 1;            // base 0
                if ((!(argIsLongBits & (0x1 << lengthArgIndex))) && (!(argIsFloatBits & (0x1 << lengthArgIndex)))) { return result_arg_numberExpected; }   // number of bytes to read
                maxLineLength = (argIsLongBits & (0x1 << lengthArgIndex)) ? (args[lengthArgIndex].longConst) : (args[lengthArgIndex].floatConst);
                if ((maxLineLength < 1) || (maxLineLength > MAX_ALPHA_CONST_LEN)) { return result_arg_outsideRange; }
            }

            // prepare to read characters.
            // buffer, long enough to receive maximum line length and (input line only) line terminator ('\n')
            _intermediateStringObjectCount++;
            char* buffer = new char[isLineForm ? MAX_ALPHA_CONST_LEN + 1 : maxLineLength + 1];                              // including '\0' terminator
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)buffer, HEX);
            _pDebugOut->println("      read line (1-buffer still empty) ");
        #endif

            // read characters now
            // NOTE: next line of code is NOT used because while waiting for a time out, call backs would not occur
            // --->  int charsInBuffer = (isLineForm || terminatorArgPresent) ? pStream->readBytesUntil(terminator, buffer, maxLineLength) : pStream->readBytes(buffer, maxLineLength);

            int charsRead{ 0 };                                                                                             // init
            if ((streamNumber > 0) && (terminator == 0xff)) {                                                               // reading from file and NOT searching for a terminator: read all bytes at once
                charsRead = read(buffer, maxLineLength);                                                                    // if fewer bytes available, end reading WITHOUT time out; read() uses stream set by 'setActiveStreamTo()'
            }
            else {                                                                                                          // external input OR (all streams) search for terminator 
                bool kill{ false }, doAbort{ false }, stdConsDummy{ false };
                for (int i = 0; i < maxLineLength; i++) {
                    // get a character if available and perform a regular housekeeping callback as well
                    bool charFetched{ false };
                    char c = getCharacter(charFetched, kill, doAbort, stdConsDummy, (streamNumber <= 0));                   // time out only required if external IO
                    if (kill) {                                                                                             // kill request from caller ? 
                        _intermediateStringObjectCount--;
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)buffer, HEX);
                        _pDebugOut->print("      read line (2) ");   _pDebugOut->println(buffer);
                    #endif
                        delete[] buffer;
                        return EVENT_kill;
                    }
                    if (doAbort) { forcedAbortRequest = true; break; }                                                      // stop a running Justina program (buffer is now flushed until nex line character) 

                    if ((!charFetched) || ((c == terminator) && !isLineForm)) { break; }                                    // no more characters or [not for readLine()] terminator found ? break (terminator is not stored in buffer)
                    buffer[charsRead++] = c;
                    if ((c == terminator) && isLineForm) { break; }                                                         // readLine(): if terminator found (and added to string), break
                }
            }

            buffer[charsRead] = '\0';

            // return number of characters read into last argument, if it's not a constant

            if (!isLineForm) {     // last argument is number of characters to read, so it's numeric
                bool isConstant = (!(argIsVarBits & (0x1 << (suppliedArgCount - 1))) || (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));    // constant ?
                if (!isConstant) { // if last argument is constant: skip saving value in last argument WITHOUT error  

                    // last argument is a NUMERIC variable: replace current value (number of characters to read) with number of characters read. Keep the variable's value type.
                    bool varIsLong = (argIsLongBits & (0x1 << (suppliedArgCount - 1)));
                    if (varIsLong) { *_pEvalStackTop->varOrConst.value.pLongConst = (long)charsRead; }
                    else { *_pEvalStackTop->varOrConst.value.pFloatConst = (float)charsRead; }
                }
            }

            // save result
            // -----------
            fcnResultValueType = value_isStringPointer;

            // no characters read ? simply delete buffer
            if (charsRead == 0) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)buffer, HEX);
                _pDebugOut->print("      read line (3) ");   _pDebugOut->println(buffer);
            #endif
                _intermediateStringObjectCount--;
                delete[] buffer;
                fcnResult.pStringConst = nullptr;
            }

            // less characters read than maximum ? move string to a smaller character array to save space
            else if (charsRead < maxLineLength) {
                _intermediateStringObjectCount++;
                char* smallerBuffer = new char[charsRead + 1];                                                              // including space for terminating '\0'
                strcpy(smallerBuffer, buffer);
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)smallerBuffer, HEX);
                _pDebugOut->print("      read line (4) ");   _pDebugOut->println(smallerBuffer);

                _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)buffer, HEX);
                _pDebugOut->print("      read line (5) ");   _pDebugOut->println(buffer);
            #endif
                _intermediateStringObjectCount--;
                delete[] buffer;
                fcnResult.pStringConst = smallerBuffer;
            }
            else { fcnResult.pStringConst = buffer; }
        }
        break;


        // -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        // reads characters from a stream or from a string, parses what is read to long, float and string values and stores these values in variables provided in the argument list.   
        // a character stream previously printed with statements coutList, printList and vprintList (see: commands) can be read in safely with the following functions
        // -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

        case fnccod_cinParseList:
        case fnccod_parseList:
        case fnccod_parseListFromVar:
        {
            // cinList   (variable, variable, ...)    
            // readList  (stream number, variable, variable, ...)
            // vreadList (string value, variable, variable, variable, ...)

            // when reading from a stream, read until a newline character is encountered or a timeout occurs

            char* buffer{};
            execResult_type execResult{ result_exec_OK };
            int valuesSaved{ 0 };
            bool sourceArgPresent = ((functionCode == fnccod_parseList) || (functionCode == fnccod_parseListFromVar));      // stream or variable
            bool parseListFromStream = ((functionCode == fnccod_cinParseList) || (functionCode == fnccod_parseList));
            int firstArgIndex = sourceArgPresent ? 1 : 0;

            // check receiving arguments: must be variables
            for (int argIndex = firstArgIndex; argIndex < suppliedArgCount; ++argIndex) { if (!(argIsVarBits & (1 << argIndex))) { return result_arg_variableExpected; } }

            if (parseListFromStream) {
                int streamNumber{ 0 };

                // perform checks and set pointer to IO stream or file
                execResult = sourceArgPresent ? setActiveStreamTo(argIsLongBits, argIsFloatBits, args[0], 0, streamNumber) :
                    setActiveStreamTo(streamNumber);                                                                         // perform checks and set input stream 
                if (execResult != result_exec_OK) { return execResult; }

                // prepare to read characters
                _intermediateStringObjectCount++;
                // limit buffer length, because it's created on the heap (long enough to receive maximum line length + null; create AFTER last error check)
                buffer = new char[MAX_ALPHA_CONST_LEN + 1];
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)buffer, HEX);
                _pDebugOut->println("     parse list (1-buffer still empty) ");
            #endif

                // read line from stream
                bool kill{ false }, doAbort{ false }, stdConsDummy{ false };
                int charsRead{ 0 };

                for (int i = 0; i < MAX_ALPHA_CONST_LEN; i++) {
                    // get a character if available and perform a regular housekeeping callback as well
                    bool charFetched{ false };
                    char c = getCharacter(charFetched, kill, doAbort, stdConsDummy, (streamNumber <= 0));                   // time out only required if external IO
                    if (kill) {                            // kill request from caller ? 
                        if (kill) {                                                                                         // kill request from caller ? 
                            _intermediateStringObjectCount--;
                        #if PRINT_HEAP_OBJ_CREA_DEL
                            _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)buffer, HEX);
                            _pDebugOut->print("     parse list (2) ");   _pDebugOut->println(buffer);
                        #endif
                            delete[] buffer;
                            return EVENT_kill;
                        }
                    }
                    if (doAbort) { forcedAbortRequest = true; break; }                                                      // stop a running Justina program  

                    if (!charFetched) { break; }                                                                            // no more characters ? break
                    if (c == '\n') { break; }                                                                               // line end found ? break ('terminator'\n' is not stored in buffer)
                    buffer[charsRead++] = c;
                }

                buffer[charsRead] = '\0';                                                                                   // add terminating '\0'
                if (forcedAbortRequest) { break; }                                                                          // also end outer loop
            }
            else {      // parse from string
                if (!(argIsStringBits & (1 << 0))) { return result_arg_stringExpected; };
                buffer = (argIsVarBits & (1 << 0)) ? *pFirstArgStackLvl->varOrConst.value.ppStringConst : pFirstArgStackLvl->varOrConst.value.pStringConst;
            }

            // parse constants in buffer
            parsingResult_type parsingResult{ result_parsing_OK };

            char* pNext = buffer;   // init
            int commaLength = strlen(term_comma);
            bool stringObjectCreated{ false };
            Val value; char valueType{};
            LE_evalStack* pStackLvl = pFirstArgStackLvl;                                                                    // now points to first receiving variable (or, if present: stream number or string to parse)
            if (sourceArgPresent) { pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl); }                   // now points to first receiving variable


            // iterate through all value-receiving variables and separators between them
            // -------------------------------------------------------------------------

            // second argument, ... last argument
            for (int argIndex = firstArgIndex; argIndex < suppliedArgCount; ++argIndex, stringObjectCreated = false) {      //initialize 'stringObjectCreated' to false after each loop

                // move to the first non-space character of next token 
                while (pNext[0] == ' ') { pNext++; }                                                                        // skip leading spaces
                if (isSpace(pNext[0])) { break; }                                                                           // white space, including CR, LF, ...: end of instruction: prepare to quit parsing  
                if (pNext[0] == '\0') { break; }                                                                            // string terminator: idem

                char* pch = pNext;
                int predefinedConstIndex{};

                do {    // one loop only
                    if (argIndex > firstArgIndex) {      // first look for a separator
                        // do not look for trailing space, to use strncmp() with number of non-space characters found, because a space is not required after an operator
                        bool isComma = (strncmp(term_comma, pch, commaLength) == 0);                                        // token corresponds to terminal name ? Then exit loop    
                        if (!isComma) { parsingResult = result_separatorExpected;  break; }
                        pNext += commaLength;                                                                               // move to next character
                        while (pNext[0] == ' ') { pNext++; }                                                                // skip leading spaces
                        if ((isSpace(pNext[0])) || (pNext[0] == '\0')) { parsingResult = result_parseList_stringNotComplete; break; }   // white space, including CR, LF, ... or string terminator
                    }

                    // parsing functions below return...
                    //  - true: no parsing error. parsingResult determines whether token recognized (result_parsing_OK) or not (result_tokenNotFound) - in which case it can still be another token type
                    //  - false: parsing error. parsingResult indicates which error.

                    // float or integer ?
                    _initVarOrParWithUnaryOp = 0;       // needs to be zero before calling parseIntFloat()
                    if (!parseIntFloat(pNext, pch, value, valueType, predefinedConstIndex, parsingResult)) { break; }       // break with error
                    if (parsingResult == result_parsing_OK) { break; }                                                      // is this token type: look no further
                    // string ? if string and not empty, a string object is created by routine parseString() - except if predefined symbolic string constant
                    if (!parseString(pNext, pch, value.pStringConst, valueType, predefinedConstIndex, parsingResult, true)) { break; }            // break with error
                    if (parsingResult == result_parsing_OK) { break; }                                                      // is this token type: look no further
                    parsingResult = result_parseList_valueToParseExpected;
                } while (false);        // one loop only

                if (parsingResult != result_parsing_OK) { execResult = result_list_parsingError;   break; }                 // exit loop if token error (syntax, ...)

                // if a valid token was parsed and it's a non-empty string: if it's a predefined string, then we still need to create a copy (a ref. to the original predefined string was returned)...
                // ...because we will store it in a variable. If it's NOT a predefined string, then a string object was created on the heap already

                if ((valueType == value_isStringPointer) && (value.pStringConst != nullptr)) {
                    if (predefinedConstIndex >= 0) {                    // predefined string: copy on the heap is not yet made
                        _intermediateStringObjectCount++;
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)value.pStringConst, HEX);
                        _pDebugOut->print("     parse list (3) ");   _pDebugOut->println(value.pStringConst);
                    #endif            
                        char* strCopy = new char[strlen(value.pStringConst) + 1];
                        strcpy(strCopy, value.pStringConst);
                        value.pStringConst = strCopy;                   // copy pointer
                    }

                    stringObjectCreated = true;
                }


                // parsing OK: assign value to receiving variable
                // ----------------------------------------------

                // if variable is an array element, it's variable type is fixed. Compatible with provided value ?
                bool returnArgIsArray = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isArray);
                bool oldArgIsLong = (argIsLongBits & (0x1 << argIndex));
                bool oldArgIsFloat = (argIsFloatBits & (0x1 << argIndex));
                bool oldArgIsString = (argIsStringBits & (0x1 << argIndex));
                char oldArgValueType = (oldArgIsLong ? value_isLong : oldArgIsFloat ? value_isFloat : value_isStringPointer);

                // if receiving variable is array, both of the old and new values must be string OR both must be numeric (array value type is fixed)
                if (returnArgIsArray && (oldArgIsString != (valueType == value_isStringPointer))) { execResult = result_array_valueTypeIsFixed; break; }

                // if currently the variable contains a string object, delete it (if not empty)
                if (oldArgIsString) { execResult = deleteVarStringObject(pStackLvl); if (execResult != result_exec_OK) { break; } }

                // save new value and value type
                if (!returnArgIsArray || (oldArgValueType == valueType)) {
                    *pStackLvl->varOrConst.value.pLongConst = value.longConst;                                              // valid for all value types
                    *pStackLvl->varOrConst.varTypeAddress = (*pStackLvl->varOrConst.varTypeAddress & ~value_typeMask) | valueType;
                }
                else {      // is array and new and old value have different numeric types: convert to array value type
                    if (oldArgValueType == value_isLong) { *pStackLvl->varOrConst.value.pLongConst = value.floatConst; }
                    else { *pStackLvl->varOrConst.value.pFloatConst = value.longConst; }
                }

                ++valuesSaved;                                                                                              // number of values saved will be returned

                // if the new value is a (non-empty) temporary string, simply reference it in the Justina variable 
                if (stringObjectCreated) {
                    stringObjectCreated = false;                                                                            // it's becoming a Justina variable value now
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)value.pStringConst, HEX);
                    _pDebugOut->print("     parse list (4) ");   _pDebugOut->println(value.pStringConst);
                #endif
                    _intermediateStringObjectCount--;
                    // do NOT delete the object: it became a variable string

                    char varScope = (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_scopeMask);
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print((varScope == var_isUser) ? "\r\n+++++ (usr var str) " : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? "\r\n+++++ (var string ) " : "\r\n+++++ (loc var str) ");
                    _pDebugOut->println((uint32_t)*pStackLvl->varOrConst.value.ppStringConst, HEX);
                    _pDebugOut->print("     parse list (5) ");   _pDebugOut->println(*pStackLvl->varOrConst.value.pStringConst);
                #endif
                    (varScope == var_isUser) ? _userVarStringObjectCount++ : ((varScope == var_isGlobal) || (varScope == var_isStaticInFunc)) ? _globalStaticVarStringObjectCount++ : _localVarStringObjectCount++;
                }

                // retrieve stack level for next receiving variable (or nullptr if last was reached before)
                pStackLvl = (LE_evalStack*)evalStack.getNextListElement(pStackLvl);
                // reached last variable and input buffer not completely parsed ? break with error
                if (pStackLvl == nullptr) { break; }                                                                        // no more variables to save values into: quit parsing remainder of string / stream
            }

            // delete input temporary buffer
            if (parseListFromStream) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)buffer, HEX);
                _pDebugOut->print("     parse list (6) ");   _pDebugOut->println(buffer);
            #endif
                _intermediateStringObjectCount--;
                delete[] buffer;
            }

            // if an error occured while processing an argument, then an intermediate string object might still exist on the heap
            if (stringObjectCreated) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)value.pStringConst, HEX);
                _pDebugOut->print("     parse list (7) ");   _pDebugOut->println(value.pStringConst);
            #endif
                _intermediateStringObjectCount--;
                delete[] value.pStringConst;

            }


            // execution error ?
            if (execResult != result_exec_OK) {
                _evalParsingError = parsingResult;                                                                          // only relevant in case a list parsing error occurred
                return execResult;
            }

            // save result: number of values that were actually saved 
            fcnResultValueType = value_isLong;
            fcnResult.longConst = valuesSaved;
        }
        break;


        // ----------------------------------------------------------------------------------------------------------------------------------------------------
        // Find a target character sequence in characters read from a stream. Two forms:
        // - find a target string in a stream. Return 1 if found. Return 0 if a time out occurs (target not found)
        // - find a target string in a stream. Return 1 if found. Return 0 if a terminator string is encountered first, or a time out occurs (target not found)
        // Note: if the stream is an SD file, a time out is not applicable
        // ----------------------------------------------------------------------------------------------------------------------------------------------------

        case fnccod_find:
        case fnccod_findUntil:
        {
            // find(stream number, target string)
            // findUntil(stream number, target string, terminator string)

            int streamNumber{ 0 };
            execResult_type execResult = setActiveStreamTo(argIsLongBits, argIsFloatBits, args[0], 0, streamNumber);        // perform checks and set input stream
            if (execResult != result_exec_OK) { return execResult; }

            // check target string 
            if (!(argIsStringBits & (0x1 << 1))) { return result_arg_stringExpected; }
            if (args[1].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
            char* target = args[1].pStringConst;
            int targetLen = strlen(target);

            // check terminator string
            char* terminator{};
            int terminatorLen{ 0 };
            if (functionCode == fnccod_findUntil) {
                if (!(argIsStringBits & (0x1 << 2))) { return result_arg_stringExpected; }
                if (args[2].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
                terminator = args[2].pStringConst;
                terminatorLen = strlen(terminator);
            }

            // read characters ONE BY ONE now and check for target [and terminator] - check for events continuously
            // NOTE: next line of code is NOT used because while waiting for a time out, call backs do not happen
            // --->  bool targetFound = (functionCode == fnccod_findUntil) ? pStream->findUntil(target, terminator) : pStream->find(target);

            int targetCharsMatched{ 0 }, terminatorCharsMatched{ 0 };
            bool targetFound{ false };
            bool kill{ false }, doAbort{ false }, stdConsDummy{ false };

            while (true) {
                // get a character if available and perform a regular housekeeping callback as well
                bool charFetched{ false };
                char c = getCharacter(charFetched, kill, doAbort, stdConsDummy, (streamNumber <= 0));                       // time out only required if external IO
                if (kill) { return EVENT_kill; }                                                                            // kill request from caller ? 
                if (doAbort) { forcedAbortRequest = true; break; }                                                          // stop a running Justina program 
                if (!charFetched) { targetFound = false; break; }                                                           // target was not found

                if (c == target[targetCharsMatched]) {
                    if (++targetCharsMatched == targetLen) { targetFound = true; break; }
                }
                else { targetCharsMatched = 0; }                                                                            // last character does not match: start all over 

                if (functionCode == fnccod_findUntil) {
                    if (c == terminator[terminatorCharsMatched]) {
                        if (++terminatorCharsMatched == terminatorLen) { targetFound = false; break; }
                    }
                }
            }

            fcnResult.longConst = (long)targetFound;
            fcnResultValueType = value_isLong;
        }
        break;


        // -----------------------------------------------------------------------------
        //  go to a tab or a specific column while printing to a stream or to a variable
        // -----------------------------------------------------------------------------

        case fnccod_tab:
        case fnccod_gotoColumn:
        {
            // these functions are meaningful if used as DIRECT arguments of print statements EXCEPT PRINT LIST statements, to advance the print column to a 
            // desired tab position or directly to a desired print column
            // tab function takes an optional argument: desired number of tab positions to advance (default is 1)
            // goto column function takes one argument: print column to go to

            // NOTE: when executing a print command, all arguments have been evaluated already and stored on the evaluation stack. The print command pops these values from the stack and prints them, one by one.
            // This means that, when a tab() or col() function is evaluated, none of the previous arguments of the print command have been printed yet and so, it can not yet be calculated how many columns to skip.
            // The magic needs to happen while printing. To establish that, a flag is saved on the evaluation stack, together with the tab() or col() function result (the number of columns to skip resp. the column to go to).
            // When the printing routine finally reaches this value (popping it from the stack), the flag will indicate that it must not be printed but should be used to calculate the number of spaces to print.
            // The flag is lost if the col() or tab() function is itself used in a expression

            int value = 1;        // init

            if (suppliedArgCount == 1) {                                                                                    // tab() function only
                if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
                value = ((argIsLongBits & (0x1 << 0))) ? args[0].longConst : (long)args[0].floatConst;                      // convert to long if needed
                if (value <= 0) { return result_arg_outsideRange; }
                if (((functionCode == fnccod_tab) ? value * _tabSize : value) > MAX_ALPHA_CONST_LEN) { return result_arg_outsideRange; }
            }

            // function return: tabs x tabsize, column to go to, current print column after last printcommand
            fcnResultValueType = value_isLong;          // must be long (not float) value
            fcnResult.longConst = (functionCode == fnccod_tab) ? value * _tabSize : value;

            if (_activeFunctionData.activeCmd_isInternal == 1) {
                switch (_activeFunctionData.activeCmd_commandCode) {
                    case cmdcod_dbout:
                    case cmdcod_dboutLine:
                    case cmdcod_cout:
                    case cmdcod_coutLine:
                    case cmdcod_print:
                    case cmdcod_printLine:
                    case cmdcod_printToVar:
                    case cmdcod_printLineToVar:
                    case cmdcod_coutList:
                    case cmdcod_printList:
                    case cmdcod_printListToVar:
                    {
                        // this will only have effect if the function result is a direct argument of a print command (cout, print, etc...)
                        ((functionCode == fnccod_tab) ? requestPrintTab : requestGotoPrintColumn) = true;
                        fcnResult.longConst = value;
                    }
                    break;

                    // other commands, or no command: do nothing
                    default: {}
                }
            }
        }
        break;


        // ---------------------------------------
        // return zero if this is not a cold start
        // ---------------------------------------

        case fnccod_isColdStart:
        {
            fcnResultValueType = value_isLong;                              // must be long (not float) value
            fcnResult.longConst = (long)_coldStart;
        }
        break;



        // ----------------------------------------------------------------------------------------------------------------------
        // if in a print command, return last column that was printed for this stream before this print command started executing
        // if not within a print command, last column that was printed for the stream where last print command was executed
        // ----------------------------------------------------------------------------------------------------------------------

        case fnccod_getColumnPos:
        {
            fcnResultValueType = value_isLong;                              // must be long (not float) value
            fcnResult.longConst = long(*_pLastPrintColumn) + 1;
        }
        break;


        // ---------------------------------------------
        // evaluate expression contained within quotes
        // NOTE: eval() is the exact opposite of quote()
        // ---------------------------------------------

        case fnccod_eval:
        {
            // only one argument possible (eval() string)
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            char resultValueType;
            execResult_type execResult = launchEval(pFunctionStackLvl, args[0].pStringConst);
            if (execResult != result_exec_OK) { return execResult; }
            // a dummy 'Justina function' (executing the parsed eval() expressions) has just been 'launched' (and will start after current (right parenthesis) token is processed)
            // because eval function name token and single argument will be removed from stack now (see below, at end of this procedure, after the switch() block), adapt CALLER evaluation stack levels
            _activeFunctionData.callerEvalStackLevels -= 2;
        }
        break;


        // ------------------------------------------------------------------------------------
        // if the argument is a number, converts it to a string
        // if the argument is a string, add surrounding double quotes as part of the string,...
        // ...expand  \ to  \\  substrings and  "  to  \" substrings (2 to 4 characters)
        // see also routine 'quoteAndExpandEscSeq()'
        // NOTE: quote() does the exact opposite of eval()
        // ------------------------------------------------------------------------------------

        // !!! to enter a backslash in a constant literal string, remember to type in 2 backslashes. to enter a double quote in a string, type backslash double quote !!!

        case fnccod_quote:
        {
            fcnResultValueType = value_isStringPointer;                                                                     // init
            fcnResult.pStringConst = nullptr;

            if ((argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0))) {
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[30];                                                                      // provide sufficient length to store a number
                (argIsLongBits & (0x1 << 0)) ? sprintf(fcnResult.pStringConst, "%ld", args[0].longConst) : sprintf(fcnResult.pStringConst, "%G", args[0].floatConst);
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                _pDebugOut->print("              quote ");   _pDebugOut->println(fcnResult.pStringConst);
            #endif            
            }

            else if (argIsStringBits & (0x1 << 0)) {
                fcnResult.pStringConst = args[0].pStringConst;
                quoteAndExpandEscSeq(fcnResult.pStringConst);                                                               // returns a new intermediate string on the heap (never a null pointer)
            }
        }
        break;


        // -------------------------
        // switch and ifte functions
        // -------------------------

        case fnccod_switch:
        case fnccod_ifte:
        {
            // switch() arguments: switch expression, test expression 1 , result 1 [, ... [, test expression 7 , result 7]] [, default result expression]
            // ifte() arguments  : test expression 1, true part, false part 1 (simple if, then, else form)
            //               or  : test expression 1, true part 1, test expression 2, true part 2 [, test expression 3 , true part 3 ... [, test expression 7 , true part 7]]...] [, false part]
            // no preliminary restriction on type of arguments

            // set default value
            bool isSwitch = (functionCode == fnccod_switch);
            fcnResultValueType = (suppliedArgCount % 2 == (isSwitch ? 0 : 1)) ? argValueType[suppliedArgCount - 1] : value_isLong;      // init
            fcnResult.longConst = 0; if (suppliedArgCount % 2 == (isSwitch ? 0 : 1)) { fcnResult = args[suppliedArgCount - 1]; }        // OK if default value is not a string or an empty string

            bool testValueIsNumber = (argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0));                                     // (for switch function only)
            bool match{ false };
            int matchIndex{ 0 };
            int matchResultPairs = (suppliedArgCount - (isSwitch ? 1 : 0)) / 2;
            for (int pair = (isSwitch ? 1 : 0); pair <= matchResultPairs - (isSwitch ? 0 : 1); ++pair) {
                matchIndex = (pair << 1) - (isSwitch ? 1 : 0);                                                                          // index in argument array
                match = false;      // init

                if (isSwitch) {
                    if ((argIsStringBits & (0x1 << 0)) && (argIsStringBits & (0x1 << matchIndex))) {                                    // test value and match value are both strings
                        if ((args[0].pStringConst == nullptr) || (args[matchIndex].pStringConst == nullptr)) {
                            match = ((args[0].pStringConst == nullptr) && (args[matchIndex].pStringConst == nullptr));                  // equal
                        }
                        else { match = (strcmp(args[0].pStringConst, args[matchIndex].pStringConst) == 0); }                            // case sensitive comparison
                    }
                    else if (testValueIsNumber && (((argIsLongBits & (0x1 << matchIndex))) || ((argIsFloatBits & (0x1 << matchIndex))))) {              // test value and match value are both numeric
                        if ((argIsLongBits & (0x1 << 0)) && (argIsLongBits & (0x1 << matchIndex))) { match = ((args[0].longConst == args[matchIndex].longConst)); }
                        else { match = ((argIsFloatBits & (0x1 << 0)) ? args[0].floatConst : (float)args[0].longConst) == ((argIsFloatBits & (0x1 << matchIndex)) ? args[matchIndex].floatConst : (float)args[matchIndex].longConst); }
                    }
                }
                else {
                    if (!(argIsLongBits & (0x1 << matchIndex)) && !(argIsFloatBits & (0x1 << matchIndex))) { return result_testexpr_numberExpected; }   // test value and match value are both strings
                    match = ((argIsFloatBits & (0x1 << matchIndex)) ? (args[matchIndex].floatConst != 0.) : (args[matchIndex].longConst != 0));
                }

                if (match) {
                    fcnResultValueType = argValueType[matchIndex + 1];
                    fcnResult = args[matchIndex + 1];                                                                                   // OK if not string or empty string
                    break;
                }
            }

            // result is a non-empty string ? an object still has to be created on the heap
            if ((fcnResultValueType == value_isStringPointer) && (fcnResult.pStringConst != nullptr)) {
                int resultIndex = match ? matchIndex + 1 : suppliedArgCount - 1;
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[strlen(args[resultIndex].pStringConst) + 1];
                strcpy(fcnResult.pStringConst, args[resultIndex].pStringConst);
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                _pDebugOut->print("       switch, ifte ");   _pDebugOut->println(fcnResult.pStringConst);
            #endif            
            }
        }
        break;


        // ---------------
        // choose function
        // ---------------

        case fnccod_choose:
        {
            // arguments: expression, test expression 1, test expression 2 [... [, test expression 15]]
            // no preliminary restriction on type of arguments

            // the first expression is an index into the test expressions: return  
            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int index = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : args[0].floatConst;
            if ((index <= 0) || (index >= suppliedArgCount)) { return result_arg_outsideRange; }
            fcnResultValueType = argValueType[index];
            fcnResult = args[index];                                                                                                    // OK if not string or empty string

            // result is a non-empty string ? an object still has to be created on the heap
            if ((fcnResultValueType == value_isStringPointer) && (fcnResult.pStringConst != nullptr)) {
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[strlen(args[index].pStringConst) + 1];
                strcpy(fcnResult.pStringConst, args[index].pStringConst);
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                _pDebugOut->print("             choose ");   _pDebugOut->println(fcnResult.pStringConst);
            #endif            
            }
        }
        break;


        // ---------------
        // index function
        // --------------

        case fnccod_index:
        {
            // arguments : expression, test expression 1, test expression 2 [... [, test expression 15]]
            // no preliminary restriction on type of arguments

            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;                                                                                        // init: not found

            bool testValueIsNumber = (argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0));                         // (for switch function only)
            bool match{ false };
            for (int i = 1; i <= suppliedArgCount - 1; ++i) {
                if ((argIsStringBits & (0x1 << 0)) && (argIsStringBits & (0x1 << i))) {                                     // test value and mmtch value are both strings
                    if ((args[0].pStringConst == nullptr) || (args[i].pStringConst == nullptr)) {
                        match = ((args[0].pStringConst == nullptr) && (args[i].pStringConst == nullptr));                   // equal
                    }
                    else { match = (strcmp(args[0].pStringConst, args[i].pStringConst) == 0); }                             // case sensitive comparison
                }
                else if (testValueIsNumber && (((argIsLongBits & (0x1 << i))) || ((argIsFloatBits & (0x1 << i))))) {        // test value and match value are both numeric
                    if ((argIsLongBits & (0x1 << 0)) && (argIsLongBits & (0x1 << i))) { match = ((args[0].longConst == args[i].longConst)); }
                    else { match = ((argIsFloatBits & (0x1 << 0)) ? args[0].floatConst : (float)args[0].longConst) == ((argIsFloatBits & (0x1 << i)) ? args[i].floatConst : (float)args[i].longConst); }
                }

                if (match) {
                    fcnResult.longConst = i;
                    break;
                }
            }
        }
        break;


        // ---------------------------
        // dimension count of an array
        // ---------------------------

        case fnccod_dims:
        {
            void* pArray = *pFirstArgStackLvl->varOrConst.value.ppArray;

            fcnResultValueType = value_isLong;
            fcnResult.longConst = ((char*)pArray)[3];
        }
        break;


        // -----------------
        // array upper bound
        // -----------------

        case fnccod_ubound:
        {
            if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
            void* pArray = *pFirstArgStackLvl->varOrConst.value.ppArray;
            int arrayDimCount = ((char*)pArray)[3];
            int dimNo = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : int(args[1].floatConst);
            if ((argIsFloatBits & (0x1 << 1))) { if (args[1].floatConst != dimNo) { return result_arg_integerDimExpected; } }   // if float, fractional part should be zero
            if ((dimNo < 1) || (dimNo > arrayDimCount)) { return result_arg_dimNumberInvalid; }

            fcnResultValueType = value_isLong;
            fcnResult.longConst = ((char*)pArray)[--dimNo];
        }
        break;


        // -------------------
        // variable value type
        // -------------------

        case fnccod_valueType:
        {
            // note: to obtain the value type of an array, check the value type of one of its elements
            fcnResultValueType = value_isLong;
            fcnResult.longConst = argValueType[0];
        }
        break;


        // --------------------------------------------
        // retrieve one of the last calculation results
        // --------------------------------------------

        case fnccod_last:
        {
            int FiFoElement = 1;                                                                                            // init: newest FiFo element
            if (suppliedArgCount == 1) {                                                                                    // FiFo element specified
                if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
                FiFoElement = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);
                if ((FiFoElement < 1) || (FiFoElement > MAX_LAST_RESULT_DEPTH)) { return result_arg_outsideRange; }
            }
            if (FiFoElement > _lastValuesCount) { return result_arg_invalid; }
            --FiFoElement;

            fcnResultValueType = lastResultTypeFiFo[FiFoElement];
            fcnResult = lastResultValueFiFo[FiFoElement];

            if ((fcnResultValueType == value_isStringPointer) && (fcnResult.pStringConst != nullptr)) {
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[strlen(lastResultValueFiFo[FiFoElement].pStringConst) + 1];
                strcpy(fcnResult.pStringConst, lastResultValueFiFo[FiFoElement].pStringConst);
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                _pDebugOut->print("               last ");   _pDebugOut->println(fcnResult.pStringConst);
            #endif            
            }

        }
        break;


        // -----------------------------
        // retrieve batch file parameter
        // -----------------------------

        case fnccod_batchFilePar:
        {
            // this function has one argument: the batch file parameter to retrieve. 0 will return the full name of the open batch file,...
            // ... 1..n will return the value of the (optional) arguments given when the batch file was called.

            int blockType = _activeFunctionData.blockType;
            if (blockType != block_batchFile) { return result_IO_onlyAllowedInBatchFile; }

            int streamNumber = _activeFunctionData.statementInputStream;

            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int argNumber = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);
            int argCount = openFiles[streamNumber - 1].argCount;
            if ((argNumber < 0) || (argNumber > argCount - 1)) { return result_arg_outsideRange; }      // first argument is batch file name

            fcnResultValueType = openFiles[streamNumber - 1].pValueType[argNumber];
            if (argNumber == 0) { fcnResult.pStringConst = openFiles[streamNumber - 1].filePath; }
            else { fcnResult = openFiles[streamNumber - 1].pArgs[argNumber]; }    // OK for all value types

            // result is a non-empty string ? an object still has to be created on the heap
            if ((fcnResultValueType == value_isStringPointer) && (fcnResult.pStringConst != nullptr)) {
                _intermediateStringObjectCount++;
                char* temp = (argNumber == 0) ? openFiles[streamNumber - 1].filePath : openFiles[streamNumber - 1].pArgs[argNumber].pStringConst;

                fcnResult.pStringConst = new char[strlen(temp) + 1];
                strcpy(fcnResult.pStringConst, temp);
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                _pDebugOut->print("     batch file arg ");   _pDebugOut->println(fcnResult.pStringConst);
            #endif            
            }
        }
        break;



        // ---------------------------------------------------
        // format a number or a string into a formatted string
        // ---------------------------------------------------

        case fnccod_format:
        {
            // fmt (expression [, width [, precision [, specifier]  [, flags  [, character count] ] ] ]

            // fmt (expression                        , specifier   [, flags  [, character count] ] 
            // fmt (expression             precision  , specifier   [, flags  [, character count] ] 

            // mandatory argument 1: value to be formatted (any type)
            // optional arguments 2-5: width, precision, specifier, flags, characters printed (return value)
            // note that specifier argument can be left out, flags argument taking its place
            // behaviour corresponds to c++ printf, sprintf, ...

            // precision
            // (1) if expression is numeric (any type)
            //     - 'f', 'e' or 'E' specifier: number of digits printed after the decimal point 
            //     - 'g' or 'G' specifier: MAXIMUM number of significant digits to be printed 
            //     - 'd', 'x' and 'X': MINIMUM number of digits to be written, if the integer is shorter, it will be padded with leading zeros (non-integer values will be converted to integers for printing)
            // (2) if expression is a string: precision is interpreted as the maximum number of characters to print

            // specifier (optional parameter): specifies the format of the value printed
            // (1) if expression is numeric (any type): specifiers 'f', 'e', 'E', 'g', 'G', 'd', 'x' and 'X' are allowed
            //     =>  'f': fixed point, 'e' or 'E': scientific, 'g' ot 'G': shortest notation (fixed or scientific). 'E' or 'G': exponent character printed in capitals    
            //     =>  'd' signed integer, 'x' or 'X': unsigned hexadecimal integer. 'X': hex number is printed in capitals
            // (2) if expression is a string, if not omitted, the specifier needs to be 's' (string)

            // depending on the specifier, a numeric value to be printed will first be converted to the correct type (integer or float)

            // flags (optional parameter): 
            // value 0x1 = left justify within print field, 0x2 = force sign, 0x4 = insert a space if no sign, 0x8: (1) floating point numbers: ALWAYS add a decimal point, even if no digits follow...
            // ...(2) integers:  precede non-zero numbers with '0x' or '0X' if printed in hexadecimal format, value 0x10 = pad with zeros within print field

            // width, precision, specifier and flags are used as defaults for next calls to this function, if they are not provided again


            bool valueToFormatIsString = (argValueType[0] == value_isStringPointer);

            // make a local copy of current settings until all tests done
            // ----------------------------------------------------------
            int width = _fmt_width;
            int precision = valueToFormatIsString ? _fmt_strCharsToPrint : _fmt_numPrecision;
            char specifier{ valueToFormatIsString ? _fmt_stringSpecifier[0] : _fmt_numSpecifier[0] };
            int flags = valueToFormatIsString ? _fmt_stringFmtFlags : _fmt_numFmtFlags;
            int argCount = suppliedArgCount;                                                                                // init

            // test arguments and ADAPT print width, precision, specifier, flags
            // -----------------------------------------------------------------
            if (argCount > 1) {
                if (argValueType[1] == value_isString) {                                                                    // second argument is a string ?
                    // assume expression to format is directly followed by specifier: shift array elements 2 places (value and value type) and insert current width & precision
                    for (int i = 3; i >= 1; i--) { args[i + 2] = args[i]; argValueType[i + 2] = argValueType[i]; }
                    args[1].longConst = width;
                    args[2].longConst = precision;
                    argValueType[1] = argValueType[2] = value_isLong;
                    argCount += 2;
                }
                else if (argValueType[2] == value_isString) {                                                               // third argument is a string ?
                    // assume expression to format is directly followed by precision and specifier: shift array elements 1 place (value and value type) and insert current width
                    for (int i = 4; i >= 1; i--) { args[i + 1] = args[i]; argValueType[i + 1] = argValueType[i]; }
                    args[1].longConst = width;
                    argValueType[1] = value_isLong;
                    argCount++;
                }

                // args[] and argValueType[] arrays are now 'normalized': second element is always width, third is always precision (default if omitted in call)

                // test and limit width argument
                if ((argValueType[1] != value_isLong) && (argValueType[1] != value_isFloat)) { return result_arg_numberExpected; }              // numeric ?
                if ((argValueType[1] == value_isLong) ? args[1].longConst < 0 : args[1].floatConst < 0.) { return result_arg_outsideRange; }    // positive ?
                width = (argValueType[1] == value_isLong) ? args[1].longConst : (long)args[1].floatConst;
                width = min(width, MAX_PRINT_WIDTH);                                                                                            // limit width to MAX_PRINT_WIDTH
            }

            // check other arguments
            if (argCount > 2) {
                execResult_type execResult = checkFmtSpecifiers(false, argCount - 2, argValueType + 2, args + 2, specifier, precision, flags);
                if (execResult != result_exec_OK) { return execResult; }
            }

            // is specifier acceptable for data type ?
            if (valueToFormatIsString != (specifier == 's')) { return result_arg_wrongSpecifierForDataType; }               // if more string specifiers defined, add them here with 'or' operator

            // prepare format string and format
            // --------------------------------

            int charsPrinted{ 0 };
            char fmtString[20]{};                                                                                           // long enough to contain all format specifier parts
            bool isIntFmt = (specifier == 'X') || (specifier == 'x') || (specifier == 'd');                                 // for ALL numeric types

            // if formatting STRING with explicit change of width and without precision argument: init 'precision' (max. no of characters to print) to width.
            if (valueToFormatIsString) { if (argCount == 2) { precision = width; } }

            // limit precision (is stored separately for numbers and strings)
            precision = min(precision, valueToFormatIsString ? MAX_STRCHAR_TO_PRINT : isIntFmt ? MAX_INT_PRECISION : MAX_FLOAT_PRECISION);

            makeFormatString(flags, isIntFmt, &specifier, fmtString);
            printToString(width, precision, valueToFormatIsString, isIntFmt, argValueType, args, fmtString, fcnResult, charsPrinted);
            fcnResultValueType = value_isStringPointer;

            _fmt_width = width;
            (valueToFormatIsString ? _fmt_strCharsToPrint : _fmt_numPrecision) = precision;
            (valueToFormatIsString ? _fmt_stringSpecifier[0] : _fmt_numSpecifier[0]) = specifier;
            (valueToFormatIsString ? _fmt_stringFmtFlags : _fmt_numFmtFlags) = flags;

            // return number of characters printed into (variable) argument if it was supplied
            // -------------------------------------------------------------------------------

            bool hasSpecifierArg = false; // init
            if (argCount > 3) { hasSpecifierArg = (!(argIsLongBits & (0x1 << 3)) && !(argIsFloatBits & (0x1 << 3))); }      // third argument is either a specifier (string) or set of flags (number)

            if (argCount == (hasSpecifierArg ? 6 : 5)) {      // optional argument returning #chars that were printed is present
                bool isConstant = (!(argIsVarBits & (0x1 << (argCount - 1))) || (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));
                if (!isConstant) { // if last argument is constant: skip saving value in last argument WITHOUT error  
                    // don't return anything if variable's current type is not numeric (if array, type can not be changed anyway)
                    if ((argIsLongBits & (0x1 << (argCount - 1))) || (argIsFloatBits & (0x1 << (argCount - 1)))) {

                        // last arguments is a NUMERIC variable (tested above): replace current value with number of characters printed. Keep the variable's value type.
                        bool varIsLong = (argIsLongBits & (0x1 << (argCount - 1)));
                        if (varIsLong) { *_pEvalStackTop->varOrConst.value.pLongConst = (long)charsPrinted; }
                        else { *_pEvalStackTop->varOrConst.value.pFloatConst = (float)charsPrinted; }
                    }
                }
            }
        }
        break;


        // -------------------------
        // type conversion functions
        // -------------------------

        case fnccod_cint:                                                                                                   // convert to integer
        {
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;
            if ((argIsLongBits & (0x1 << 0))) { fcnResult.longConst = args[0].longConst; }
            else if ((argIsFloatBits & (0x1 << 0))) { fcnResult.longConst = (long)args[0].floatConst; }
            else if ((argIsStringBits & (0x1 << 0))) { fcnResult.longConst = strtol(args[0].pStringConst, nullptr, 0); }
        }
        break;

        case fnccod_cfloat:                                                                                                 // convert to float
        {
            fcnResultValueType = value_isFloat;
            fcnResult.floatConst = 0.;
            if ((argIsLongBits & (0x1 << 0))) { fcnResult.floatConst = (float)args[0].longConst; }
            else if ((argIsFloatBits & (0x1 << 0))) { fcnResult.floatConst = args[0].floatConst; }
            else if ((argIsStringBits & (0x1 << 0))) { fcnResult.floatConst = strtof(args[0].pStringConst, nullptr); }
        }
        break;

        case fnccod_cstr:
        {
            fcnResultValueType = value_isStringPointer;                                                                     // convert to string
            fcnResult.pStringConst = nullptr;
            if ((argIsLongBits & (0x1 << 0)) || (argIsFloatBits & (0x1 << 0))) {                                            // argument is long or float ?
                _intermediateStringObjectCount++;
                fcnResult.pStringConst = new char[30];                                                                      // provide sufficient length to store a number
                (argIsLongBits & (0x1 << 0)) ? sprintf(fcnResult.pStringConst, "%ld", args[0].longConst) : sprintf(fcnResult.pStringConst, "%#G", args[0].floatConst);

            }
            else if ((argIsStringBits & (0x1 << 0))) {                                                                      // argument is string ?
                if (args[0].pStringConst != nullptr) {
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[strlen(args[0].pStringConst) + 1];
                    strcpy(fcnResult.pStringConst, args[0].pStringConst);                                                   // just copy the string provided as argument
                }
            }
            if (fcnResult.pStringConst != nullptr) {
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                _pDebugOut->print("               cstr ");   _pDebugOut->println(fcnResult.pStringConst);
            #endif
            }
        }
        break;


        // --------------
        // math functions 
        // --------------

        case fnccod_sqrt:
        case fnccod_sin:
        case fnccod_cos:
        case fnccod_tan:
        case fnccod_asin:
        case fnccod_acos:
        case fnccod_atan:
        case fnccod_ln:
        case fnccod_log10:
        case fnccod_exp:
        case fnccod_expm1:
        case fnccod_lnp1:
        case fnccod_round:
        case fnccod_ceil:
        case fnccod_floor:
        case fnccod_trunc:
        case fnccod_abs:
        case fnccod_sign:
        case fnccod_min:
        case fnccod_max:
        case fnccod_fmod:
        {
            for (int i = 0; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
            }
            float arg1float = (argIsLongBits & (0x1 << 0)) ? (float)args[0].longConst : args[0].floatConst;                 // keep original value in args[0]

            fcnResultValueType = value_isFloat;                                                                             // init: return a float
            fcnResult.floatConst = 0.;                                                                                      // init: return 0. if the Arduino function doesn't return anything

            // test the arguments
            // ------------------
            if (functionCode == fnccod_sqrt) { if (arg1float < 0.) { return result_arg_outsideRange; } }
            else if ((functionCode == fnccod_asin) || (functionCode == fnccod_acos)) { if ((arg1float < -1.) || (arg1float > 1.)) { return result_arg_outsideRange; } }
            else if ((functionCode == fnccod_ln) || (functionCode == fnccod_log10)) { if (arg1float <= 0.) { return result_arg_outsideRange; } }
            else if (functionCode == fnccod_lnp1) { if (arg1float <= -1.) { return result_arg_outsideRange; } }

            // calculate
            // ---------
            // functions always returning a float
            if (functionCode == fnccod_sqrt) { fcnResult.floatConst = sqrt(arg1float); }
            else if (functionCode == fnccod_sin) { if (_angleMode == 1) { arg1float *= DEG_TO_RAD; } fcnResult.floatConst = sin(arg1float); }
            else if (functionCode == fnccod_cos) { if (_angleMode == 1) { arg1float *= DEG_TO_RAD; }fcnResult.floatConst = cos(arg1float); }
            else if (functionCode == fnccod_tan) { if (_angleMode == 1) { arg1float *= DEG_TO_RAD; }fcnResult.floatConst = tan(arg1float); }
            else if (functionCode == fnccod_asin) { fcnResult.floatConst = asin(arg1float); if (_angleMode == 1) { fcnResult.floatConst *= RAD_TO_DEG; } }
            else if (functionCode == fnccod_acos) { fcnResult.floatConst = acos(arg1float); if (_angleMode == 1) { fcnResult.floatConst *= RAD_TO_DEG; } }
            else if (functionCode == fnccod_atan) { fcnResult.floatConst = atan(arg1float); if (_angleMode == 1) { fcnResult.floatConst *= RAD_TO_DEG; } }
            else if (functionCode == fnccod_ln) { fcnResult.floatConst = log(arg1float); }
            else if (functionCode == fnccod_lnp1) { fcnResult.floatConst = log1p(arg1float); }
            else if (functionCode == fnccod_exp) { fcnResult.floatConst = exp(arg1float); }
            else if (functionCode == fnccod_expm1) { fcnResult.floatConst = expm1(arg1float); }
            else if (functionCode == fnccod_log10) { fcnResult.floatConst = log10(arg1float); }
            else if (functionCode == fnccod_round) { fcnResult.floatConst = round(arg1float); }
            else if (functionCode == fnccod_trunc) { fcnResult.floatConst = trunc(arg1float); }
            else if (functionCode == fnccod_floor) { fcnResult.floatConst = floor(arg1float); }
            else if (functionCode == fnccod_ceil) { fcnResult.floatConst = ceil(arg1float); }
            else if (functionCode == fnccod_fmod) { fcnResult.floatConst = fmod(arg1float, (argIsLongBits & (0x1 << 1)) ? args[1].longConst : args[1].floatConst); }

            // function always returning an integer
            else if (functionCode == fnccod_sign) { fcnResultValueType = value_isLong; fcnResult.longConst = (argIsLongBits & (0x1 << 0)) ? (args[0].longConst < 0 ? 1 : 0) : signbit(arg1float); }


            // functions returning a float if the argument / at least one of the arguments is a floating-point number. Otherwise returning an integer.
            else if ((functionCode == fnccod_min) || (functionCode == fnccod_max)) {
                // Arduino min(...) or max(long 0, something greater than 0) returns a double very close to zero, but not zero (same for max()). Avoid this.
                if ((argIsLongBits & (0x1 << 0)) && (argIsLongBits & (0x1 << 1))) {
                    fcnResultValueType = value_isLong;
                    fcnResult.longConst = (functionCode == fnccod_min) ? min(args[0].longConst, args[1].longConst) : max(args[0].longConst, args[1].longConst);
                }
                else {
                    float arg2float = (argIsLongBits & (0x1 << 1)) ? (float)args[1].longConst : args[1].floatConst;
                    fcnResult.floatConst = ((arg1float <= arg2float) == (functionCode == fnccod_min)) ? arg1float : arg2float;
                }
            }
            else if (functionCode == fnccod_abs) {
                // avoid -0. as Arduino abs() result: use fabs() if result = float value
                if ((argIsLongBits & (0x1 << 0))) { fcnResultValueType = value_isLong; };
                (argIsLongBits & (0x1 << 0)) ? fcnResult.longConst = abs(args[0].longConst) : fcnResult.floatConst = fabs(args[0].floatConst);
            }

            // test the result (do not test for subnormal numbers here)
            // --------------------------------------------------------
            if (fcnResultValueType == value_isFloat) {
                if (isnan(fcnResult.floatConst)) { return result_undefined; }
                if (!isfinite(fcnResult.floatConst)) { return result_overflow; }
            }
        }
        break;


        // -----------------------------------
        // bit and byte manipulation functions
        // -----------------------------------

        // Arduino bit manipulation functions
        // arguments and return values: same as the corresponding Arduino functions
        // all arguments need to be long integers; if a value is returned, it's always a long integer
        // except for the 'read' functions (and the 'bit number to value' function), if the first argument is a variable; it's value is adapted upon return as well

        case fnccod_bit:                // bit number -> value 

        case fnccod_bitRead:            // 2 arguments: long value, bit (0 to 31) to read. Returned: 0 or 1
        case fnccod_bitClear:           // 2 arguments: long value, bit (0 to 31) to clear. New value is returned
        case fnccod_bitSet:             // 2 arguments: long value, bit (0 to 31) to set. New value is returned 
        case fnccod_bitWrite:           // 3 arguments: long value, bit (0 to 31), new bit value (0 or 1). New value is returned 

        // extra Justina byte manipulation functons. Byte argument indicates which byte to read or write
        case fnccod_byteRead:           // 2 arguments: long, byte to read (0 to 3). Value returned is between 0x00 and 0xFF.     
        case fnccod_byteWrite:          // 3 arguments: long, byte to write (0 to 3), value to write (lowest 8 bits of argument). New value is returned    

        // extra Justina bit manipulation functons. Mask argument indicates which bits to read, set, clear or write
        case fnccod_wordMaskedRead:     // 2 arguments: long value, mask. Returns masked value 
        case fnccod_wordMaskedClear:    // 2 arguments: long value, mask = bits to clear: bits indicated by mask are cleared. New value is returned
        case fnccod_wordMaskedSet:      // 2 arguments: long value, mask = bits to set: bits indicated by mask are set. New value is returned 
        case fnccod_wordMaskedWrite:    // 3 arguments: long value, mask, bits to write: value bits indicated by mask are changed. New value is returned
        {
            // all arguments of these functions must have long value type, EXCEPT bitWrite function: last argument can be floating point number too (just as Arduino's bitWrite function).
            for (int i = 0; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i))) {
                    if ((i == suppliedArgCount - 1) && (functionCode == fnccod_bitWrite)) {                                                         // float also acceptable
                        if (!(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                        args[i].longConst = (long)args[i].floatConst;
                    }
                    else { return result_arg_integerTypeExpected; }
                }
            }

            // range checks
            if (functionCode == fnccod_bit && ((args[0].longConst < 0) || (args[0].longConst > 31))) { return result_arg_outsideRange; }
            if (((functionCode == fnccod_bitRead) || (functionCode == fnccod_bitClear) || (functionCode == fnccod_bitSet) || (functionCode == fnccod_bitWrite)) && ((args[1].longConst < 0) || (args[1].longConst > 31))) {
                return result_arg_outsideRange;
            }
            if (((functionCode == fnccod_byteRead) || (functionCode == fnccod_byteWrite)) && ((args[1].longConst < 0) || (args[1].longConst > 3))) { return result_arg_outsideRange; }

            fcnResultValueType = value_isLong;                                                                                                      // init: return a long
            fcnResult.longConst = 0;                                                                                                                // init: return 0 if the Arduino function doesn't return anything
            uint8_t* pBytes = (uint8_t*)&(args[0].longConst);                                                                                       // access individual bytes of a value

            if (functionCode == fnccod_bit) { fcnResult.longConst = 1 << args[0].longConst; }                                                       // requires no variable

            else if (functionCode == fnccod_bitRead) { fcnResult.longConst = (args[0].longConst & (1 << args[1].longConst)) != 0; }                 // requires no variable
            else if (functionCode == fnccod_bitClear) { fcnResult.longConst = args[0].longConst & ~(1 << args[1].longConst); }
            else if (functionCode == fnccod_bitSet) { fcnResult.longConst = args[0].longConst | (1 << args[1].longConst); }
            else if (functionCode == fnccod_bitWrite) { fcnResult.longConst = (args[2].longConst == 0) ? args[0].longConst & ~(1 << args[1].longConst) : args[0].longConst | (1 << args[1].longConst); }

            else if (functionCode == fnccod_wordMaskedRead) { fcnResult.longConst = (args[0].longConst & args[1].longConst); }                      // requires no variable; second argument is considered mask
            else if (functionCode == fnccod_wordMaskedClear) { fcnResult.longConst = args[0].longConst & ~args[1].longConst; }
            else if (functionCode == fnccod_wordMaskedSet) { fcnResult.longConst = args[0].longConst | args[1].longConst; }
            else if (functionCode == fnccod_wordMaskedWrite) { fcnResult.longConst = args[0].longConst & (~args[1].longConst | args[2].longConst) | (args[1].longConst & args[2].longConst); }

            else if (functionCode == fnccod_byteRead) { fcnResult.longConst = pBytes[args[1].longConst]; }                                          // requires variable; contents of 1 byte is returned
            else if (functionCode == fnccod_byteWrite) { pBytes[args[1].longConst] = args[2].longConst; fcnResult.longConst = args[0].longConst; }  // new variable value is returned
        }


        // function modifies variable (first argument) ?
        if ((functionCode == fnccod_bitClear) || (functionCode == fnccod_bitSet) || (functionCode == fnccod_bitWrite) ||
            (functionCode == fnccod_wordMaskedClear) || (functionCode == fnccod_wordMaskedSet) || (functionCode == fnccod_wordMaskedWrite) ||
            (functionCode == fnccod_byteWrite)) {

            bool isConstant = (!(argIsVarBits & (0x1 << 0)) || (_pEvalStackMinus2->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));         // first argument is variable ? then store result

            if (!isConstant) {
                LE_evalStack* pStackLvl = ((functionCode == fnccod_bitWrite) || (functionCode == fnccod_byteWrite) ||
                    (functionCode == fnccod_wordMaskedWrite)) ? _pEvalStackMinus2 : _pEvalStackMinus1;
                *pStackLvl->varOrConst.value.pLongConst = fcnResult.longConst;                                                                      // stack level for variable
            }
        }
        break;


        // -------------------------------------------------------
        // hardware memory address 8 bit and 32 bit read and write
        // -------------------------------------------------------

        // intended to directly read and write to memory locations, e.g. mapped to peripheral registers (I/O, counters, ...)
        // !!! dangerous if you don't know what you're doing 

        case    fnccod_mem32Read:
        case    fnccod_mem32Write:
        case    fnccod_mem8Read:
        case    fnccod_mem8Write:
        {
            // all arguments of these functions must have long value type
            for (int i = 0; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i))) { return result_arg_integerTypeExpected; }
            }

            // range checks
            if (((functionCode == fnccod_mem8Read) || (functionCode == fnccod_mem8Write)) && ((args[1].longConst < 0) || (args[1].longConst > 3))) { return result_arg_outsideRange; }

            fcnResultValueType = value_isLong;                                                                                                      // init: return a long
            fcnResult.longConst = 0;                                                                                                                // init: return 0 if the Arduino function doesn't return anything

            args[0].longConst &= ~0x3;                                                                                                              // align with word size

            // write functions: the memory / peripheral register location is not read afterwards (because this could trigger a specific hardware action),
            // so write functions return zero)  
            if (functionCode == fnccod_mem32Read) { fcnResult.longConst = *(volatile uint32_t*)args[0].longConst; }                                 // 32 bit register value is returned
            else if (functionCode == fnccod_mem8Read) { fcnResult.longConst = ((volatile uint8_t*)(args[0].longConst))[args[1].longConst]; }        // 8 bit register value is returned
            else if (functionCode == fnccod_mem32Write) { *(volatile uint32_t*)args[0].longConst = args[1].longConst; }
            else if (functionCode == fnccod_mem8Write) { ((volatile uint8_t*)(args[0].longConst))[args[1].longConst] = args[2].longConst; }
        }
        break;


        // ----------------------------------------
        // Arduino timing and digital I/O functions 
        // ----------------------------------------

        // all arguments can be long or float; if a value is returned, it's always a long integer
        // Note that, as Justina 'integer' constants and variables are internally represented by (signed) long values, large values returned by certain functions 
        // may show up as negative values (if greater then or equal to 2^31)
        // arguments and return values: same as the corresponding Arduino functions

        case fnccod_millis:
        case fnccod_micros:
        case fnccod_delay:
        case fnccod_digitalRead:
        case fnccod_digitalWrite:
        case fnccod_pinMode:
        case fnccod_analogRead:
        case fnccod_analogReference:
        case fnccod_analogWrite:
        case fnccod_analogReadResolution:
        case fnccod_analogWriteResolution:
        case fnccod_noTone:
        case fnccod_pulseIn:
        case fnccod_shiftIn:
        case fnccod_shiftOut:
        case fnccod_tone:
        case fnccod_random:
        case fnccod_randomSeed:
        {
            // for all arguments provided: check they are Justina integers or floats
            // no additional checks are done (e.g. floats with fractions)
            for (int i = 0; i < suppliedArgCount; ++i) {
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = int(args[i].floatConst); }                         // all these functions need integer values
            }
            fcnResultValueType = value_isLong;                                                                              // init: return a long
            fcnResult.longConst = 0;                                                                                        // init: return 0 if the Arduino function doesn't return anything

            if (functionCode == fnccod_millis) { fcnResult.longConst = millis(); }
            else if (functionCode == fnccod_micros) { fcnResult.longConst = micros(); }
            else if (functionCode == fnccod_delay) {                                                                        // args: milliseconds    
                unsigned long startTime = millis();
                while (startTime + (unsigned long)args[0].longConst > millis()) {
                    bool kill, doAbort{};
                    execPeriodicHousekeeping(&kill, &doAbort);
                    if (kill) { return EVENT_kill; }                                                                        // kill Justina interpreter ? (buffer is now flushed until next line character)
                    if (doAbort) { forcedAbortRequest = true; break; }                                                      // stop a running Justina program 
                }
            }
            else if (functionCode == fnccod_pinMode) {
                pinMode(args[0].longConst, args[1].longConst);                                                              // args: pin, pin mode
                fcnResult.longConst = args[1].longConst;                                                                    // return pin mode (could be the result of an expression)
            }
            else if (functionCode == fnccod_digitalRead) { fcnResult.longConst = digitalRead(args[0].longConst); }          // arg: pin
            else if (functionCode == fnccod_digitalWrite) {
                digitalWrite(args[0].longConst, args[1].longConst);                                                         // args: pin, value
                fcnResult.longConst = args[1].longConst;                                                                    // return value written to pin (could be the result of an expression)
            }
            else if (functionCode == fnccod_analogRead) { fcnResult.longConst = analogRead(args[0].longConst); }            // arg: pin
        #if defined(ARDUINO_ARCH_SAMD) || defined( ARDUINO_ARCH_NRF52840)                    
            else if (functionCode == fnccod_analogReference) { analogReference(args[0].longConst); }                        // arg: reference type (0 to 5: see Arduino doc - 2 is external reference)                                     
        #endif
            else if (functionCode == fnccod_analogWrite) { analogWrite(args[0].longConst, args[1].longConst); }             // args: pin, value
            else if (functionCode == fnccod_analogReadResolution) { analogReadResolution(args[0].longConst); }              // arg: bits
            else if (functionCode == fnccod_analogWriteResolution) { analogWriteResolution(args[0].longConst); }            // arg: bits
            else if (functionCode == fnccod_noTone) { noTone(args[0].longConst); }                                          // arg: pin
            else if (functionCode == fnccod_pulseIn) {                                                                      // args: pin, value, (optional) time out
                fcnResult.longConst = (suppliedArgCount == 2) ? pulseIn(args[0].longConst, args[1].bytes[0]) :
                    pulseIn(args[0].longConst, args[1].bytes[0], (uint32_t)args[2].longConst);
            }
            else if (functionCode == fnccod_shiftIn) {                                                                      // args: data pin, clock pin, bit order
            #if defined ARDUINO_ARCH_ESP32 
                fcnResult.longConst = shiftIn(args[0].longConst, args[1].longConst, args[2].longConst);
            #else
                fcnResult.longConst = shiftIn(args[0].longConst, args[1].longConst, (BitOrder)args[2].longConst);
            #endif
            }
            else if (functionCode == fnccod_shiftOut) {                                                                     // args: data pin, clock pin, bit order, value
            #if defined ARDUINO_ARCH_ESP32 
                shiftOut(args[0].longConst, args[1].longConst, args[2].longConst, args[3].longConst);
            #else    
                shiftOut(args[0].longConst, args[1].longConst, (BitOrder)args[2].longConst, args[3].longConst);
            #endif            
            }
            else if (functionCode == fnccod_tone) {                                                                         // args: pin, frequency, (optional) duration
                (suppliedArgCount == 2) ? tone(args[0].longConst, args[1].longConst) : tone(args[0].longConst, args[1].longConst, args[2].longConst);
            }
            else if (functionCode == fnccod_random) {                                                                       //args: boundaries
                fcnResult.longConst = (suppliedArgCount == 1) ? random(args[0].longConst) : random(args[0].longConst, args[1].longConst);
            }
            else if (functionCode == fnccod_randomSeed) { randomSeed(args[0].longConst); }                                  // arg: seed
        }
        break;


        // --------------------------------
        // 'character' and string functions
        // --------------------------------

        // first argument must be a non-empty string; optional argument must point to a character in the string (1 to string length)
        // if a value is returned, it's always a long integer (if boolean: 0 (false) or not 0 (true)) 

        case fnccod_isAlpha:
        case fnccod_isAlphaNumeric:
        case fnccod_isAscii:
        case fnccod_isControl:
        case fnccod_isDigit:
        case fnccod_isGraph:
        case fnccod_isHexadecimalDigit:
        case fnccod_isLowerCase:
        case fnccod_isPrintable:
        case fnccod_isPunct:
        case fnccod_isUpperCase:
        case fnccod_isWhitespace:
        case fnccod_asc:

        {
            // check that non-empty string is provided; if second argument is given, check it's within range
            // no additional checks are done (e.g. floats with fractions)
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
            int length = strlen(args[0].pStringConst);
            int charPos = 1;                                                                                                // first character in string
            if (suppliedArgCount == 2) {
                if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
                charPos = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : int(args[1].floatConst);
                if ((charPos < 1) || (charPos > length)) { return result_arg_outsideRange; }
            }
            fcnResultValueType = value_isLong;                                                                              // init: return a long
            fcnResult.longConst = 0;                                                                                        // init

            if (functionCode == fnccod_isAlpha) { fcnResult.longConst = isalpha(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isAlphaNumeric) { fcnResult.longConst = isAlphaNumeric(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isAscii) { fcnResult.longConst = isAscii(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isControl) { fcnResult.longConst = isControl(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isDigit) { fcnResult.longConst = isDigit(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isGraph) { fcnResult.longConst = isGraph(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isHexadecimalDigit) { fcnResult.longConst = isHexadecimalDigit(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isLowerCase) { fcnResult.longConst = isLowerCase(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isPrintable) { fcnResult.longConst = isPrintable(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isPunct) { fcnResult.longConst = isPunct(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isUpperCase) { fcnResult.longConst = isUpperCase(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_isWhitespace) { fcnResult.longConst = isWhitespace(args[0].pStringConst[--charPos]); }
            else if (functionCode == fnccod_asc) { fcnResult.longConst = args[0].pStringConst[--charPos]; }
        }
        break;


         //  convert two characters of a string, both containing the ASCII code of a hex. digit, to the ASCII code composed of these two hex. digits
        // examples: "61" -> 61 (ASCII code for 'a') ,  "35" -> 35 (ASCII code for '5')
        // ----------------------------------------------------------------------------------------------------------------------------------------

        case fnccod_hexStringToAsc:
        {
            fcnResultValueType = value_isLong;                                                          // init
            fcnResult.longConst = 0;

            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }

            int length = strlen(args[0].pStringConst);
            int charPos = 1;                                                                            // first character in string
            if (suppliedArgCount == 2) {
                if (!(argIsLongBits & (0x1 << 1)) && !(argIsFloatBits & (0x1 << 1))) { return result_arg_numberExpected; }
                charPos = (argIsLongBits & (0x1 << 1)) ? args[1].longConst : int(args[1].floatConst);
                if (args[1].longConst < 1) { return result_arg_outsideRange; }
            }

            if (strlen(args[0].pStringConst) < charPos + 1) { return result_arg_stringTooShort; }       // two characters needed, starting at given position

            const char asc_zero = 0x30, asc_a = 0x61, asc_A = 0x41;
            char*& hexDigitString = args[0].pStringConst;

            for (int i = charPos - 1; i <= charPos; i++) {                                              // base 0
                fcnResult.longConst <<= 4;
                if ((hexDigitString[i] >= asc_zero) && (hexDigitString[i] <= asc_zero + 9)) {			// ASCII code of a hex number
                    fcnResult.longConst |= (hexDigitString[i] - asc_zero);
                }
                else if ((hexDigitString[i] >= asc_a) && (hexDigitString[i] <= asc_a + 5)) {
                    fcnResult.longConst |= (hexDigitString[i] - asc_a + 10);
                }
                else if ((hexDigitString[i] >= asc_A) && (hexDigitString[i] <= asc_A + 5)) {
                    fcnResult.longConst |= (hexDigitString[i] - asc_A + 10);
                }
                else { fcnResult.longConst = -1; break; }                                               // break for block: not the ASCII code of a hex number
            }
        }
        break;


       // convert ASCII code (argument) to 1-character string
        // ---------------------------------------------------

        case fnccod_char:
        {
            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int asciiCode = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);
            if ((asciiCode < 0) || (asciiCode > 0xFF)) { return result_arg_outsideRange; }

            // result is string
            fcnResultValueType = value_isStringPointer;
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[2];
            fcnResult.pStringConst[0] = asciiCode;
            fcnResult.pStringConst[1] = '\0';                                                           // terminating \0
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("               char ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif
        }
        break;


        // print an ASCII code to a 2-character string, representing the two hex. digits of the ASCII code
        // examples: 0x61 (ASCII code for 'a') -> "61" ,  0x35 (ASCII code for '5') -> "35"
        // -----------------------------------------------------------------------------------------------

        case fnccod_ascToHexString:
        {
            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int asciiCode = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);
            if ((asciiCode < 0) || (asciiCode > 0xFF)) { return result_arg_outsideRange; }

            // result is string
            fcnResultValueType = value_isStringPointer;
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[3];
            sprintf(fcnResult.pStringConst, "%.2lx", args[0].longConst);
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("             strhex ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif    
        }
        break;


        // return length of a string
        // -------------------------

        case fnccod_len:
        {
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;                                                                                        // init
            if (args[0].pStringConst != nullptr) { fcnResult.longConst = strlen(args[0].pStringConst); }
        }
        break;


        // return CR and LF character string
        // ---------------------------------

        case fnccod_nl:
        {
            // result is string
            fcnResultValueType = value_isStringPointer;
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[3];
            fcnResult.pStringConst[0] = '\r';
            fcnResult.pStringConst[1] = '\n';
            fcnResult.pStringConst[2] = '\0';                                                                               // terminating \0
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("           new line ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif
        }
        break;


        // replace one character in a string: replaceChar(string, ASCII code [character position] )
        // ---------------------------------

        // this function replaces character position n in a string with a given ASCII code. No new string is created, 
        // so the changed string will be lost if it's a Justina constant (not a variable) and the value is not immediately used

        case fnccod_replaceChar:
        {
            LE_evalStack* pStackLvl{ (suppliedArgCount == 3) ? _pEvalStackMinus2 : _pEvalStackMinus1 };                     // stack level for first argument (string variable)
            bool isConstant = (!(argIsVarBits & (0x1 << 0)) || (pStackLvl->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));
            if (isConstant) { return result_arg_variableExpected; }                                                         // first argument must be a variable string
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (!(argIsLongBits & (0x1 << 1))) { return result_arg_integerTypeExpected; }
            int charPos = 1;                                                                                                // first character in string
            if (suppliedArgCount == 3) {
                if (!(argIsLongBits & (0x1 << 2)) && !(argIsFloatBits & (0x1 << 2))) { return result_arg_numberExpected; }
                charPos = (argIsLongBits & (0x1 << 2)) ? args[2].longConst : int(args[2].floatConst);
                if ((args[2].longConst < 1) || (args[2].longConst > strlen(args[0].pStringConst))) { return result_arg_outsideRange; }
            }

            args[0].pStringConst[--charPos] = args[1].longConst;                                                            // ASCII code

            fcnResultValueType = value_isLong;                                                                              // init: return a long
            fcnResult.longConst = 0;                                                                                        // init: return 0 if the Arduino function doesn't return anything
        }
        break;


        // create a string with n spaces
        // create a string with n times the first character in the argument string
        // -----------------------------------------------------------------------

        case fnccod_space:
        case fnccod_repeatchar:
        {
            fcnResultValueType = value_isStringPointer;                                                                     // init
            fcnResult.pStringConst = nullptr;

            char c{ ' ' };                                                                                                  // init
            if (functionCode == fnccod_repeatchar) {
                if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
                if (args[0].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }
                c = args[0].pStringConst[0];                                                                                // only first character in string will be repeated
            }

            int lengthArg = (functionCode == fnccod_repeatchar) ? 1 : 0;                                                    // index for argument containing desired length of result string
            if (!(argIsLongBits & (0x1 << lengthArg)) && !(argIsFloatBits & (0x1 << lengthArg))) { return result_arg_numberExpected; }
            int len = ((argIsLongBits & (0x1 << lengthArg))) ? args[lengthArg].longConst : (long)args[lengthArg].floatConst;    // convert to long if needed
            if ((len <= 0) || (len > MAX_ALPHA_CONST_LEN)) { return result_arg_outsideRange; }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len + 1];                                                                     // space for terminating '0'
            for (int i = 0; i <= len - 1; ++i) { fcnResult.pStringConst[i] = c; }
            fcnResult.pStringConst[len] = '\0';
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("     space, repChar ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif            
        }
        break;


        // case sensitive or non-case sensitive comparison
        // -----------------------------------------------

        case fnccod_strcmp:                                                                                                 // case sensitive
        case fnccod_strcasecmp:                                                                                             // NOT case sensitive
        {
            // arguments: string, substring to search for
            fcnResultValueType = value_isLong;
            fcnResult.longConst = 0;                                                                                        // init: nothing found

            if (!(argIsStringBits & (0x1 << 0)) || !(argIsStringBits & (0x1 << 1))) { return result_arg_stringExpected; }
            if ((args[0].pStringConst == nullptr) || (args[1].pStringConst == nullptr)) {
                if ((args[0].pStringConst == nullptr) && (args[1].pStringConst == nullptr)) { break; }                      // equal
                else { fcnResult.longConst = (args[0].pStringConst == nullptr) ? -1 : 1; break; }
            }

            fcnResult.longConst = (functionCode == fnccod_strcmp) ? strcmp(args[0].pStringConst, args[1].pStringConst) : strcasecmp(args[0].pStringConst, args[1].pStringConst);                                                   // none of the strings is an empty string
            if (fcnResult.longConst < 0) { fcnResult.longConst = -1; }
            else if (fcnResult.longConst > 0) { fcnResult.longConst = 1; }
        }
        break;


        // find   : arguments: string, substring to search for [, start]. Returns position (base 1) of first occurrence
        // replace: arguments: string, substring to search for, replacement string [, start]. Returns modified string
        //          if start is a variable, sets it to first character after changed part of string (or 0, if substring not found)
        // -----------------------------------------------------------------------------------------------------------------------

        case fnccod_findsubstr:
        case fnccod_replacesubstr:
        {
            bool isFind = (functionCode == fnccod_findsubstr), isReplace = !isFind;
            fcnResultValueType = isReplace ? value_isStringPointer : value_isLong;                                          // return value type

            if (isReplace) { (fcnResult.pStringConst = nullptr); }                                                          // init: resulting string is empty (string itself replaced by empty string)
            else { (fcnResult.longConst = 0); }                                                                             // init: no match found                    

            if (!(argIsStringBits & (0x1 << 0)) || !(argIsStringBits & (0x1 << 1))) { return result_arg_stringExpected; }   // string to change or substring to search
            if (isReplace) { if (!(argIsStringBits & (0x1 << 2))) { return result_arg_stringExpected; } }                   // replacement string

            char*& originalString = args[0].pStringConst, *& findString = args[1].pStringConst;
            if ((originalString == nullptr) || (findString == nullptr)) { break; }                                          // string or string to find is empty: return 0 (find) or "" (replace)

            char* replaceString{ nullptr };                                                                                 // cannot be a reference
            if (isReplace) { replaceString = args[2].pStringConst; }

            char* startSearchAt = originalString;                                                                           // init: search for a match from start of string

            // original string is not empty. replace only: findString can be empty 
            int originalStrLen = strlen(originalString);                                                                    // not an empty string
            int findStrLen = strlen(findString);                                                                            // not an empty string
            int replaceStrLen = isReplace ? ((replaceString == nullptr) ? 0 : strlen(replaceString)) : 0;

            if (suppliedArgCount == (isReplace ? 4 : 3)) {                                                                  // start position specified ? (base 1)
                int startArgIndex = (isReplace ? 3 : 2);                                                                    // c++: base 0
                if (!(argIsLongBits & (0x1 << startArgIndex)) && !(argIsFloatBits & (0x1 << startArgIndex))) { return result_arg_numberExpected; }
                int startSearchPos = ((argIsLongBits & (0x1 << startArgIndex)) ? args[startArgIndex].longConst : (long)args[startArgIndex].floatConst) - 1;
                if ((startSearchPos < 0) || (startSearchPos >= originalStrLen)) { return result_arg_outsideRange; }
                startSearchAt += startSearchPos;                                                                            // first character in string to start search (base 0)
            }

            // look for the substring
            char* foundSubstringStart{ nullptr };
            if (findString != nullptr) { foundSubstringStart = strstr(startSearchAt, findString); }

            // if foundSubstringStart is a null pointer, the string to look for is not found => the resulting string is the original string
            int foundStartPos = (foundSubstringStart == nullptr) ? -1 : foundSubstringStart - originalString;               // base 0
            if (isFind) { fcnResult.longConst = foundStartPos + 1; break; }                                                 // 0 if not found


            // replace only (all positions base 0 except for values returned to Justina)
            // -------------------------------------------------------------------------

            // always create a new string (even if new string equals old string)
            _intermediateStringObjectCount++;

            int newStringLen = originalStrLen;                                                                              // init
            if (foundStartPos != -1) { newStringLen += replaceStrLen - findStrLen; }
            fcnResult.pStringConst = new char[newStringLen + 1];

            if (foundStartPos == -1) {
                strcpy(fcnResult.pStringConst, originalString);                                                             // replace: return copy of original string
            }
            else {

                // start with terminator at the end   
                fcnResult.pStringConst[newStringLen] = '\0';

                // copy front porch
                int frontLength = foundSubstringStart - originalString;
                if (frontLength > 0) { memcpy(fcnResult.pStringConst, originalString, frontLength); }

                // copy new middle part: replaced characters
                if (replaceStrLen > 0) { memcpy(fcnResult.pStringConst + frontLength, replaceString, replaceStrLen); }

                // copy back porch
                int backPorchLength = originalStrLen - frontLength - findStrLen;
                if (backPorchLength > 0) { memcpy(fcnResult.pStringConst + frontLength + replaceStrLen, originalString + frontLength + findStrLen, backPorchLength); }
                foundStartPos = frontLength + replaceStrLen;                                                                // position after changed part of string (could be past end of new string)
            }

        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print(" find, repl. string ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif            


            // start position specified in a variable ? store first character position after changed part of string (possibly past end of new string)
            if (suppliedArgCount == 4) {                                                                                    // start position was specified
                bool isConstant = (!(argIsVarBits & (0x1 << (suppliedArgCount - 1))) || (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isConstantVar));
                if (!isConstant) {                                                                                          // it's a variable, and it's numeric (important if array, which is fixed type)

                    // last arguments is a NUMERIC variable (start position): replace current value with number of characters printed. Keep the variable's value type.
                    bool varIsLong = (argIsLongBits & (0x1 << (suppliedArgCount - 1)));
                    if (varIsLong) { *_pEvalStackTop->varOrConst.value.pLongConst = (long)(foundStartPos + 1); }            // base 1 now (return value to Justina)
                    else { *_pEvalStackTop->varOrConst.value.pFloatConst = (float)(foundStartPos + 1); }
                }
            }
        }
        break;


        // convert (part of) a string to upper or to lower case
        // ----------------------------------------------------

        case fnccod_toupper:
        case fnccod_tolower:
        {
            // arguments: string [, start [, end]])
            // if string only as argument, start = first character, end = last character
            // if start is specified, and end is not, then end = start
            fcnResultValueType = value_isStringPointer;                                                                                 // init
            fcnResult.pStringConst = nullptr;

            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { if (suppliedArgCount > 1) { return  result_arg_outsideRange; } else { break; } }     // result is empty string, but only one argument accepted

            int len = strlen(args[0].pStringConst);
            int first = 0, last = len - 1;                                                                                              // init: complete string

            for (int i = 1; i < suppliedArgCount; ++i) {                                                                                // skip if only one argument (string)
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = int(args[i].floatConst); }                                     // all these functions need integer values
                if (i == 1) { first = args[i].longConst - 1; last = first; }
                else { last = args[i].longConst - 1; }
            }
            if ((first > last) || (first < 0) || (last >= len)) { return result_arg_outsideRange; }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len + 1];                                                                                 // same length as original, space for terminating 
            strcpy(fcnResult.pStringConst, args[0].pStringConst);   // copy original string
            for (int i = first; i <= last; i++) { fcnResult.pStringConst[i] = ((functionCode == fnccod_toupper) ? toupper(fcnResult.pStringConst[i]) : tolower(fcnResult.pStringConst[i])); }
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("    to upper, lower ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif            
        }
        break;


        // return left, mid or right part of a string
        // ------------------------------------------

        case fnccod_left:       // arguments: string, number of characters, starting from left, to return
        case fnccod_right:      // arguments: string, number of characters, starting from right, to return
        case fnccod_mid:        // arguments: string, first character to return (starting from left), number of characters to return
        {

            fcnResultValueType = value_isStringPointer;                                                                     // init
            fcnResult.pStringConst = nullptr;

            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { return result_arg_nonEmptyStringExpected; }

            for (int i = 1; i < suppliedArgCount; ++i) {                                                                    // skip first argument (string)
                if (!(argIsLongBits & (0x1 << i)) && !(argIsFloatBits & (0x1 << i))) { return result_arg_numberExpected; }
                if ((argIsFloatBits & (0x1 << i))) { args[i].longConst = int(args[i].floatConst); }                         // all these functions need integer values
            }
            int len = strlen(args[0].pStringConst);

            int first = (functionCode == fnccod_left) ? 0 : (functionCode == fnccod_mid) ? args[1].longConst - 1 : len - args[1].longConst;
            int last = (functionCode == fnccod_left) ? args[1].longConst - 1 : (functionCode == fnccod_mid) ? first + args[2].longConst - 1 : len - 1;

            if ((first > last) || (first < 0) || (last >= len)) { return result_arg_outsideRange; }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[last - first + 2];                                                            // space for terminating '0'
            memcpy(fcnResult.pStringConst, args[0].pStringConst + first, last - first + 1);
            fcnResult.pStringConst[last - first + 1] = '\0';
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("   left, mid, right ");   _pDebugOut->println(fcnResult.pStringConst);

        #endif            
        }
        break;


        // left trim, right trim, left & right trim
        // ----------------------------------------

        case fnccod_ltrim:
        case fnccod_rtrim:
        case fnccod_trim:
        {
            fcnResultValueType = value_isStringPointer;                                                                     // init
            fcnResult.pStringConst = nullptr;

            int spaceCnt{ 0 };                                                                                              // init
            if (!(argIsStringBits & (0x1 << 0))) { return result_arg_stringExpected; }
            if (args[0].pStringConst == nullptr) { break; }                                                                 // original string is empty: return with empty result string

            int len = strlen(args[0].pStringConst);
            char* p = args[0].pStringConst;

            // trim leading spaces ?
            if ((functionCode == fnccod_ltrim) || (functionCode == fnccod_trim)) {                                          // trim leading spaces ?
                while (*p == ' ') { p++; };
                spaceCnt = p - args[0].pStringConst;                                                                        // subtraction of two pointers
            }
            if (spaceCnt == len) { break; }                                                                                 // trimmed string is empty: return with empty result string

            // trim trailing spaces ? (string does not only contain spaces)
            char* q = args[0].pStringConst + len - 1;                                                                       // last character
            if ((functionCode == fnccod_rtrim) || (functionCode == fnccod_trim)) {
                while (*q == ' ') { q--; };
                spaceCnt += (args[0].pStringConst + len - 1 - q);
            }

            // create new string
            _intermediateStringObjectCount++;
            fcnResult.pStringConst = new char[len - spaceCnt + 1];                                                          // space for terminating '0'
            memcpy(fcnResult.pStringConst, p, len - spaceCnt);
            fcnResult.pStringConst[len - spaceCnt] = '\0';
        #if PRINT_HEAP_OBJ_CREA_DEL
            _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
            _pDebugOut->print("              trim ");   _pDebugOut->println(fcnResult.pStringConst);
        #endif            
        }
        break;


        // get the current execution error
        // -------------------------------

        case fnccod_getTrappedErr:
        {
            if (suppliedArgCount == 1) {                                                        // return a trapped eval parsing error ?                                    
                if (!(argIsVarBits & (0x1 << 0))) { return result_arg_variableExpected; }

                // if an array element, it's value should be long
                bool returnArgIsArray = (_pEvalStackTop->varOrConst.sourceVarScopeAndFlags & var_isArray);
                bool isLong = (argIsLongBits & (0x1 << 0));
                bool isString = (argIsStringBits & (0x1 << 0));
                if (returnArgIsArray && !isLong) { return result_array_valueTypeIsFixed; }

                // if currently the variable contains a string object, delete it (if not empty)
                if (isString) { execResult_type execResult = deleteVarStringObject(_pEvalStackTop); if (execResult != result_exec_OK) { break; } }

                // save evaluation parsing error (or result_parsing_OK, if not an evaluation parsing error)
                *_pEvalStackTop->varOrConst.value.pLongConst = _trappedEvalParsingError;
                *_pEvalStackTop->varOrConst.varTypeAddress = (*_pEvalStackTop->varOrConst.varTypeAddress & ~value_typeMask) | value_isLong;
            }

            fcnResult.longConst = _trappedExecError;
            fcnResultValueType = value_isLong;
        }
        break;


        // -----------------------------------
        // retrieve a system value (read only)
        // -----------------------------------

        case fnccod_sysVal:
        {
            if (!(argIsLongBits & (0x1 << 0)) && !(argIsFloatBits & (0x1 << 0))) { return result_arg_numberExpected; }
            int sysVal = (argIsLongBits & (0x1 << 0)) ? args[0].longConst : int(args[0].floatConst);
            fcnResultValueType = value_isLong;                                                  // default for most system values

            switch (sysVal) {
                // display settings
                // ----------------
                case 0: fcnResult.longConst = _dispWidth; break;                                // display width                                                           
                case 1: fcnResult.longConst = _dispFloatPrecision; break;                       // floating point precision and formatting flags
                case 2: fcnResult.longConst = _dispFloatFmtFlags; break;

                case 4: fcnResult.longConst = _dispIntegerPrecision; break;                     // integers: precision and formatting flags
                case 5: fcnResult.longConst = _dispIntegerFmtFlags; break;

                case 7: fcnResult.longConst = _promptAndEcho; break;
                case 8: fcnResult.longConst = _printLastResult; break;
                case 9: fcnResult.longConst = _angleMode; break;

                // formatting function fmt()
                // -------------------------
                case 10: fcnResult.longConst = _fmt_width; break;                               // print field width
                case 11: fcnResult.longConst = _fmt_numPrecision; break;                        // numeric values: precision and formatting flags
                case 12: fcnResult.longConst = _fmt_numFmtFlags; break;

                case 14: fcnResult.longConst = _fmt_strCharsToPrint; break;                     // strings: number of characters to print
                case 15: fcnResult.longConst = _tabSize; break;                                 // tab size

                // display settings (continued)
                // ----------------------------
                case 3:                                                                         // floating point values: specifier
                case 6:                                                                         // integer values: specifier character

                // formatting function fmt() (continued)
                // -------------------------------------
                case 13:                                                                        // numeric values: specifier character
                {
                    fcnResultValueType = value_isStringPointer;
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[2];
                    strcpy(fcnResult.pStringConst, (sysVal == 3) ? _dispFloatSpecifier : (sysVal == 6) ? _dispIntegerSpecifier : _fmt_numSpecifier);
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                    _pDebugOut->print("         sysval(13) ");   _pDebugOut->println(fcnResult.pStringConst);
                #endif
                }
                break;

                // other settings
                // --------------
                case 16: fcnResult.longConst = _lastValuesCount; break;                         // current depth of last values FiF0


                case 17: fcnResult.longConst = _openFileCount; break;                           // open file count
                case 18: fcnResult.longConst = _externIOstreamCount; break;                     // number of external streams defined

                case 19:                                                                        // program name (if program loaded)
                {
                    fcnResultValueType = value_isStringPointer;
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[MAX_IDENT_NAME_LEN + 1];
                    strcpy(fcnResult.pStringConst, _programName);
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                    _pDebugOut->print("         sysval(19) ");   _pDebugOut->println(fcnResult.pStringConst);
                #endif
                }
                break;

                case 20:                                                                        // watch string
                {
                    fcnResultValueType = value_isStringPointer;
                    fcnResult.pStringConst = nullptr;                                           // init (empty string)
                    if (_pwatchString != nullptr) {
                        _intermediateStringObjectCount++;
                        fcnResult.pStringConst = new char[strlen(_pwatchString) + 1];
                        strcpy(fcnResult.pStringConst, _pwatchString);
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                        _pDebugOut->print("         sysval(20) ");   _pDebugOut->println(fcnResult.pStringConst);
                    #endif
                    }
                }
                break;

                case 21: fcnResult.longConst = _printWatchValueOnly; break;                     // view watched expression results without expression texts ?

                case 22: fcnResult.longConst = _pBreakpoints->_breakpointsStatusDraft ? 0 : 1; break;       // breakpoints are active ?

                // product info
                // ------------
                case 31:                                                                        // product name
                case 32:                                                                        // legal copy right
                case 33:                                                                        // product version 
                case 34:                                                                        // build date
                {
                    fcnResultValueType = value_isStringPointer;
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[((sysVal == 31) ? strlen(J_productName) : (sysVal == 32) ? strlen(J_legalCopyright) : (sysVal == 33) ? strlen(J_version) : strlen(J_buildDate)) + 1];
                    strcpy(fcnResult.pStringConst, (sysVal == 31) ? J_productName : (sysVal == 32) ? J_legalCopyright : (sysVal == 33) ? J_version : J_buildDate);
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                    _pDebugOut->print("      sysval(31-34) ");   _pDebugOut->println(fcnResult.pStringConst);
                #endif
                }
                break;

                // technical (normally less relevant for the user)
                // -----------------------------------------------
                // note:parsing stack element count is always zero during evaluation: no entry provided here
                case 36:fcnResult.longConst = evalStack.getElementCount(); break;               // evaluation stack element count
                case 37:fcnResult.longConst = flowCtrlStack.getElementCount(); break;           // flow control stack element count (call stack depth + stack levels used by open blocks)
                case 38:fcnResult.longConst = _callStackDepth; break;                           // call stack depth; this excludes stack levels used by blocks (while, if, ...)
                case 39:fcnResult.longConst = _openDebugLevels; break;                          // number of stopped programs
                case 40:fcnResult.longConst = parsedStatementLineStack.getElementCount(); break;  // immediate mode parsed programs stack element count: stopped program count + open eval() strings (being executed)

                case 41: fcnResult.longConst = evalStack.getCreatedObjectCount(); break;        // created list object count (cumulative and across linked lists: count is static)

                case 42:                                                                        // current active object count
                case 43:                                                                        // current accumulated object count errors since cold start
                {
                    fcnResultValueType = value_isStringPointer;
                    _intermediateStringObjectCount++;
                    fcnResult.pStringConst = new char[13 * 5 + 1];                              // includes place for 13 times 5 characters (3 digits max. for each number, max. 2 extra in between) and terminating \0
                    if (sysVal == 42) {     // print heap object counts
                        // (1)  program variable and function NAMES-(2)user variable NAMES-(3)parsed string constants-(4)last value strings-
                        // (5)  global and static variable strings-(6)global and static array storage areas-(7)user variable strings-(8)user array storage areas-
                        // (9)  local variable strings-(10)local array storage areas-(11)local variable base value areas-(12)intermediate strings-
                        // (13) system expression strings
                        sprintf(fcnResult.pStringConst, "%0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d",
                            min(999, _identifierNameStringObjectCount), min(999, _userVarNameStringObjectCount), min(999, _parsedStringConstObjectCount), min(999, _lastValuesStringObjectCount),
                            min(999, _globalStaticVarStringObjectCount), min(999, _globalStaticArrayObjectCount), min(999, _userVarStringObjectCount), min(999, _userArrayObjectCount),
                            min(999, _localVarStringObjectCount), min(999, _localArrayObjectCount), min(999, _localVarValueAreaCount), min(999, _intermediateStringObjectCount),
                            min(999, _systemStringObjectCount));
                    }
                    else {     // print heap object create/delete errors
                        sprintf(fcnResult.pStringConst, "%0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d:%0d:%0d:%0d / %0d",
                            min(999, _identifierNameStringObjectErrors), min(999, _userVarNameStringObjectErrors), min(999, _parsedStringConstObjectErrors), min(999, _lastValuesStringObjectErrors),
                            min(999, _globalStaticVarStringObjectErrors), min(999, _globalStaticArrayObjectErrors), min(999, _userVarStringObjectErrors), min(999, _userArrayObjectErrors),
                            min(999, _localVarStringObjectErrors), min(999, _localArrayObjectErrors), min(999, _localVarValueAreaErrors), min(999, _intermediateStringObjectErrors),
                            min(999, _systemStringObjectErrors));
                    }
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
                    _pDebugOut->print("         sysval(43) ");   _pDebugOut->println(fcnResult.pStringConst);
                #endif
                }
                break;

                case 44:                                                                        // processor board or architecture
                {
                #if  defined(ARDUINO_ARCH_SAMD)             // nano 33 IoT
                    fcnResult.longConst = 1; break;
                #elif defined(ARDUINO_ARCH_RP2040) 
                    fcnResult.longConst = 2; break;
                #elif defined(ARDUINO_ARCH_ESP32)   
                    fcnResult.longConst = 3; break;
                #elif defined(ARDUINO_ARCH_NRF52840)        // nano 33 BLE
                    fcnResult.longConst = 4; break;
                #else 
                    fcnResult.longConst = 0; break;     // none of these
                #endif
                }
                break;

                default: return result_arg_invalid; break;
            }                                                                                   // switch (sysVal)
        }
        break;

    }                                                                                           // end switch


    // post-process: delete function name token and arguments from evaluation stack, create stack entry for function result 
    // -------------------------------------------------------------------------------------------------------------------

    clearEvalStackLevels(suppliedArgCount + 1);

    if (functionCode != fnccod_eval) {    // Note: function eval() (only) does not yet have a function result: the eval() string has been parsed but execution is yet to start 

        // push result to stack
        // --------------------

        _pEvalStackTop = (LE_evalStack*)evalStack.appendListElement(sizeof(VarOrConstLvl));
        _pEvalStackMinus1 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackTop);
        _pEvalStackMinus2 = (LE_evalStack*)evalStack.getPrevListElement(_pEvalStackMinus1);

        _pEvalStackTop->varOrConst.value = fcnResult;                                                                       // long, float or pointer to string
        _pEvalStackTop->varOrConst.valueType = fcnResultValueType;                                                          // value type of second operand  
        _pEvalStackTop->varOrConst.tokenType = tok_isConstant;                                                              // use generic constant type
        _pEvalStackTop->varOrConst.sourceVarScopeAndFlags = 0x00;                                                           // not an array, not an array element (it's a constant) 
        _pEvalStackTop->varOrConst.valueAttributes = constIsIntermediate | (requestPrintTab ? isPrintTabRequest : 0)
            | (requestGotoPrintColumn ? isPrintColumnRequest : 0);                                                          // set tab() or col() function flag if requested
    }

    return result_exec_OK;
}


