/***********************************************************************************************************
*   Justina interpreter library                                                                            *
*                                                                                                          *
*   Copyright 2024, Herwig Taveirne                                                                        *
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


// *****************************************************************
// ***        class Justina - implementation         ***
// *****************************************************************


// --------------------------
// *   initialize SD card   *
// --------------------------

Justina::execResult_type Justina::startSD() {

    if (_SDinitOK) { return result_execOK; }                                                            // card is initialized: nothing to do

    if ((_justinaStartupOptions & SD_mask) == SD_notAllowed) { return result_SD_noCardOrNotAllowed; }
#if !defined ARDUINO_ARCH_ESP32
    _SDcard.init(SPI_FULL_SPEED, _SDcardChipSelectPin);         // needed for listFilesToSerial command (not for nano ESP32)
#endif
    if (!SD.begin(_SDcardChipSelectPin)) { return result_SD_noCardOrCardError; }

    _openFileCount = 0;
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) { openFiles[i].fileNumberInUse = false; }
    _SDinitOK = true;
    return result_execOK;
}


// -------------------------------------------------------
// *   ESP32 only: convert file access modes for ESP32   *
// -------------------------------------------------------

// nano ESP32 boards: READ cannot be combined with WRITE or APPEND (underlying library restriction) -> prioritize READ
//                    prioritize APPEND over WRITE, because on NON-nano ESP32 boards mode is 0x06 for APPEND
#if defined ARDUINO_ARCH_ESP32
char* Justina::SD_ESP32_convert_accessMode(int mode) {
    if (mode & 0x04) { return FILE_APPEND; }
    else if (mode & 0x02) { return FILE_WRITE; }
    else if (mode & 0x01) { return FILE_READ; }
    else { return FILE_READ; }                            // default
}
#endif


// -----------------------
// *   open an SD file   *
// -----------------------

Justina::execResult_type Justina::SD_open(int& fileNumber, char* filePath, int mode) {
    fileNumber = 0;                                                                                                         // init: no file number yet

    if ((_justinaStartupOptions & SD_mask) == SD_notAllowed) { return result_SD_noCardOrNotAllowed; }
    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if (_openFileCount == MAX_OPEN_SD_FILES) { return result_SD_maxOpenFilesReached; }                                      // max. open files reached

    // create temp string
    if (!pathValid(filePath)) { return result_SD_pathIsNotValid; }                                                          // is not a complete check, but it remedies a few flaws in SD library

    // create a new string, providing space for starting '/' if missing
    // always create the new char*, because if the file is closed later, Justina will assume that it must be deleted 
    int len = strlen(filePath);
    _systemStringObjectCount++;
    char* filePathInCapitals = new char[((filePath[0] == '/') ? 0 : 1) + len + 1];
    if (filePath[0] != '/') { filePathInCapitals[0] = '/'; }
    strcpy(filePathInCapitals + ((filePath[0] == '/') ? 0 : 1), filePath);                                                  // copy original string
    for (int i = 0; i < strlen(filePathInCapitals); i++) { filePathInCapitals[i] = toupper(filePathInCapitals[i]); }

#if defined ARDUINO_ARCH_ESP32
    // if only reading file: does file exist ? (this check allows for a more specific error code, instead of 'could not open file' error code)
    bool modeIsOnlyRead = (!(mode & (WRITE_FILE | APPEND_FILE)));
    bool fileExists = SD.exists(filePathInCapitals);

    // ESP32 only: ESP32 SD library does not test for user-imposed constraints about file (non-)existence: perform tests here
    bool fileMustExist = (mode & (CREATE_FILE | EXCL_FILE)) == 0;
    bool fileMustNotExist = (mode & (CREATE_FILE | EXCL_FILE)) == (CREATE_FILE | EXCL_FILE);

    if (((modeIsOnlyRead || fileMustExist) && !fileExists) || (fileMustNotExist && fileExists)) {
        delete[] filePathInCapitals;
        _systemStringObjectCount--;
        return result_SD_fileNotFound;
    }
#else
    // if only reading file: does file exist ? (this check allows for a more specific error code, instead of 'could not open file' error code)
    if (!(mode & (WRITE_FILE | APPEND_FILE))) {
        if (!SD.exists(filePathInCapitals)) {
            delete[] filePathInCapitals;
            _systemStringObjectCount--;
            return result_SD_fileNotFound;
        }
    }
#endif

// currently open files ? Check that the same file is not open already
    if (fileIsOpen(filePathInCapitals)) {
        delete[] filePathInCapitals;
        _systemStringObjectCount--;
        return result_SD_fileAlreadyOpen;
    }

    // find a free file number 
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (!openFiles[i].fileNumberInUse) {

        #if defined ARDUINO_ARCH_ESP32
            // last argument (create) = true: create directory path towards file if it doesn't exist yet
            openFiles[i].file = SD.open(filePathInCapitals, SD_ESP32_convert_accessMode(mode), true);
        #else
            openFiles[i].file = SD.open(filePathInCapitals, mode);
        #endif
            if (!openFiles[i].file) {
                delete[] filePathInCapitals;
                _systemStringObjectCount--;
                return result_SD_couldNotOpenFile;
            }                                                                                                               // could not open file (in case caller ignores this error, file number returned is 0)

            File* pFile = &openFiles[i].file;

            openFiles[i].file.setTimeout(DEFAULT_READ_TIMEOUT);
            openFiles[i].fileNumberInUse = true;
            // note: only ESP32 SD library has a method 'path()': keep track of full name within Justina
            openFiles[i].filePath = filePathInCapitals;                                                                     // delete when file is closed
            openFiles[i].currentPrintColumn = 0;
            fileNumber = i + 1;
            break;
        }
    }

    _openFileCount++;

    return result_execOK;
}


// ----------------------------------------
// *   open next SD file in a directory   *
// ----------------------------------------

Justina::execResult_type Justina::SD_openNext(int dirFileNumber, int& fileNumber, File* pDirectory, int mode) {
    fileNumber = 0;                                                                                                         // init: no next file opened

    if ((_justinaStartupOptions & SD_mask) == SD_notAllowed) { return result_SD_noCardOrNotAllowed; }
    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if (_openFileCount == MAX_OPEN_SD_FILES) { return result_SD_maxOpenFilesReached; }                                      // max. open files reached

    // it's not possible to check whether the next file is open already, because we don't know which file will be opened as next. We'll check afterwards

    // note: only ESP32 SD library has a method 'path()': keep track of full name within Justina
    char* dirPath = openFiles[dirFileNumber - 1].filePath;                                                                  // path for the directory
    int dirPathLength = strlen(dirPath);

    // find a free file number and assign it to this file
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (!openFiles[i].fileNumberInUse) {
        #if defined ARDUINO_ARCH_ESP32
            openFiles[i].file = pDirectory->openNextFile(SD_ESP32_convert_accessMode(mode));
        #else
            openFiles[i].file = pDirectory->openNextFile(mode);
        #endif
            // file evaluates to false: assume last file in directory is currently open (if any) and no more files are available. Do not return error, file number 0 indicates 'last file reached'
            if (!openFiles[i].file) { return result_execOK; }

            // room for full name, '/' between path and name, '\0' terminator
            openFiles[i].file.setTimeout(DEFAULT_READ_TIMEOUT);
            openFiles[i].fileNumberInUse = true;
            _systemStringObjectCount++;
            openFiles[i].filePath = new char[dirPathLength + 1 + strlen(openFiles[i].file.name()) + 1];
            strcpy(openFiles[i].filePath, dirPath);
            if (dirPathLength > 1) { strcat(openFiles[i].filePath, "/"); }              // if not root directory: add '/' between path and file name
            strcat(openFiles[i].filePath, openFiles[i].file.name());
            openFiles[i].currentPrintColumn = 0;
            fileNumber = i + 1;
            break;
        }
    }

    ++_openFileCount;

    // currently older open files ? Check that the same file is not opened twice now
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            if (i + 1 != fileNumber) {
                // compare the long filenames (including file path) for the newly opened file with the previously opened open file (comparing file refs doesn't work)
                if (strcasecmp(openFiles[i].filePath, openFiles[fileNumber - 1].filePath) == 0) {                           // 8.3 file format: NOT case sensitive      
                    openFiles[fileNumber - 1].file.close();                                                                 // close newer file ref again
                    _openFileCount--;
                    openFiles[fileNumber - 1].fileNumberInUse = false;
                    delete[] openFiles[fileNumber - 1].filePath;
                    _systemStringObjectCount--;
                    return result_SD_fileAlreadyOpen;                                                                       // return with error
                }
            }
        }
    }

    return result_execOK;
}



// ------------------------
// *   close an SD file   *
// ------------------------

void Justina::SD_closeFile(int fileNumber) {

    // checks must have been done before calling this function

    if (static_cast <Stream*>(&openFiles[fileNumber - 1].file) == static_cast <Stream*>(_pDebugOut)) { _pDebugOut = _pConsoleOut; }
    openFiles[fileNumber - 1].file.close();                                                                                 // does not return errors
    _openFileCount--;
    openFiles[fileNumber - 1].fileNumberInUse = false;
    delete[] openFiles[fileNumber - 1].filePath;
    _systemStringObjectCount--;
}


// -------------------------------
// *   close all open SD files   *
// -------------------------------

void  Justina::SD_closeAllFiles() {

    if (!_SDinitOK) { return; }          // card is NOT initialized: nothing to do

    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            if (static_cast <Stream*>(&openFiles[i].file) == static_cast <Stream*>(_pDebugOut)) {    // debug out could also be an external io stream: test for file (which is open; otherwise debug would not point to it)
                _pDebugOut = _pConsoleOut;
            }
            openFiles[i].fileNumberInUse = false;                                                                           // slot is open again
            delete[] openFiles[i].filePath;
            _systemStringObjectCount--;
            openFiles[i].file.close();                                                                                      // does not return errors
            _openFileCount--;
        }
    }
}


// ---------------------------------------------------------------------------------------------------
// *   list all files on SD card with date and size, to any output stream (even an SD file itself)   *
// ---------------------------------------------------------------------------------------------------

Justina::execResult_type Justina::SD_listFiles() {

    if ((_justinaStartupOptions & SD_mask) == SD_notAllowed) { return result_SD_noCardOrNotAllowed; }
    if (!_SDinitOK) { return result_SD_noCardOrCardError; }

    // print to console (default), or to any other defined I/O device, or to a file, but without date and time stamp (unfortunately this fixed in SD library)
    // before calling this function, output stream must be set by function 'setStream(...)'

    // while printing directories, 2 files will be open at the same time (a directory and a file or subdirectory): make sure max. open file count will not be exceeded 
    if (_openFileCount > MAX_OPEN_SD_FILES - 2) { return result_SD_maxOpenFilesReached; }

    // open files will not increase '_openFileCount' because we don't save file info in _openFiles array anyway 
    File SDroot = SD.open("/");
    println("SD card: files (name, size in bytes): ");
    printDirectory(SDroot, 0);
    SDroot.close();
    return result_execOK;
}


// -------------------------------------------------------------------------------------------------------------------------------------
// *   recursively list all files in all directories of an SD card with date and size, to any output stream (even an SD file itself)   *
// -------------------------------------------------------------------------------------------------------------------------------------

void Justina::printDirectory(File dir, int indentLevel) {
    constexpr int step{ 2 }, defaultSizeAttrColumn{ 30 }, minimumColumnSpacing{ 4 };

    // before calling this function, output stream must be set by function 'setStream(...)'

    while (true) {

        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        for (uint8_t i = 1; i <= indentLevel * step; i++) {
            print(" ");
        }
        print(entry.name());
        if (entry.isDirectory()) {
            println("/");
            printDirectory(entry, indentLevel + 1);
        }
        else {
            // files have sizes, directories do not
            int len = indentLevel * step + strlen(entry.name());
            if (len < defaultSizeAttrColumn - minimumColumnSpacing) {
                for (int i = len; i < defaultSizeAttrColumn; i++) { print(" "); }
            }
            else {
                for (int i = 0; i < minimumColumnSpacing; i++) { print(" "); }
            }
            println(entry.size());

        }
        entry.close();
    }
}


// -------------------------------------
// *   check validity of a file path   *
// -------------------------------------

bool Justina::pathValid(char* path) {

    // SD library allows to run into issues if path is not valid (hanging, invalid creation of directories / files)
    // this routine performs a few basic checks: 
    // - path should NOT start with a space
    // - path should NOT end with a '/' or a space
    // - never two '/' in a row

    if (path == nullptr) { return false; }  // empty path is not valid
    if (strlen(path) == 1) { return (path[0] != ' '); }
    if ((path[0] == ' ') || (path[strlen(path) - 1] == '/') || (path[strlen(path) - 1] == ' ')) { return false; }

    bool previousIsSlash{ true }, currentIsSlash{ false };
    char* p{};
    for (p = path + 1; p < path + strlen(path); p++) {                                                                      // skip first character in test 
        currentIsSlash = (p[0] == '/');
        if (previousIsSlash && currentIsSlash) { break; }
        previousIsSlash = currentIsSlash;
    }
    return (p == path + strlen(path));                                                                                      // if loop ended normally, then no issues found
}


// -----------------------------------------
// *   check if a file is currently open   *
// -----------------------------------------

bool Justina::fileIsOpen(char* path) {
    // currently open files ? Check that the same file is not open already
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            if (strcasecmp(openFiles[i].filePath, path) == 0) { return true; }                                              // 8.3 file format: NOT case sensitive
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------------
// *   check if a stream is valid for input or output and set it for future IO operations   *
// *   this set either _streamNumberIn, _pStreamIn or _streamNumberOut, _pStreamOut         * 
// ------------------------------------------------------------------------------------------

Justina::execResult_type Justina::setStream(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, int& streamNumber, bool forOutput) {
    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }    // file number
    streamNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;

    setStream(streamNumber, forOutput);
    return result_execOK;
}



Justina::execResult_type Justina::setStream(int streamNumber, bool forOutput) {

    Stream* pTemp;

    execResult_type execResult = determineStream(streamNumber, pTemp, forOutput);
    if (execResult != result_execOK) { return execResult; }
    (forOutput ? _streamNumberOut : _streamNumberIn) = streamNumber;
    if (forOutput) { _pStreamOut = static_cast<Print*> (pTemp); }
    else { _pStreamIn = pTemp; }
    return result_execOK;
}


// this overload also returns the pointer to the set stream (pStream), in addition to _streamNumberIn, _pStreamIn or _streamNumberOut, _pStreamOut

Justina::execResult_type Justina::setStream(int streamNumber, Stream*& pStream, bool forOutput) {

    execResult_type execResult = determineStream(streamNumber, pStream, forOutput);
    if (execResult != result_execOK) { return execResult; }
    (forOutput ? _streamNumberOut : _streamNumberIn) = streamNumber;
    if (forOutput) { _pStreamOut = static_cast<Print*> (pStream); }
    else { _pStreamIn = pStream; }

    return result_execOK;
}


// -------------------------------------------------------------------------------------------------------------------------------
// *   return pointer to a stream based on stream number (for console only, this will depend on value of argument 'forOutput')   *
// *   WITHOUT setting _streamNumberIn, _pStreamIn or _streamNumberOut, _pStreamOut                                              *
// -------------------------------------------------------------------------------------------------------------------------------

Justina::execResult_type Justina::determineStream(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex,
    Stream*& pStream, int& streamNumber, bool forOutput, int allowFileTypes) {

    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }    // stream number
    streamNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;

    return determineStream(streamNumber, pStream, forOutput, allowFileTypes);
}


Justina::execResult_type  Justina::determineStream(int streamNumber, Stream*& pStream, bool forOutput, int allowFileTypes) {

    if (streamNumber == 0) { pStream = forOutput ? static_cast<Stream*> (_pConsoleOut) : _pConsoleIn; }  // init: assume console
    else if ((-streamNumber) > _externIOstreamCount) { return result_IO_invalidStreamNumber; }
    else if (streamNumber < 0) {
        if ((forOutput ? _ppExternOutputStreams[(-streamNumber) - 1] : _ppExternInputStreams[(-streamNumber) - 1]) == nullptr) {
            return forOutput ? result_IO_noDeviceOrNotForOutput : result_IO_noDeviceOrNotForInput;
        }
        pStream = forOutput ? static_cast<Stream*>(_ppExternOutputStreams[(-streamNumber) - 1]) : _ppExternInputStreams[(-streamNumber) - 1];
    }    // external IO: stream number -1 => array index 0, etc.
    else {
        File* pFile{};
        execResult_type execResult = SD_fileChecks(pFile, streamNumber, allowFileTypes);                                    // operand: file number
        if (execResult != result_execOK) { return execResult; }
        pStream = static_cast<Stream*> (pFile);
    }
    return result_execOK;
}


// -------------------------------------------------------------------
// *   perform file checks prior to performing actions on the file   *
// -------------------------------------------------------------------

Justina::execResult_type Justina::SD_fileChecks(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, File*& pFile, int allowFileTypes)
{
    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }    // file number
    int fileNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;
    execResult_type execResult = SD_fileChecks(pFile, fileNumber, allowFileTypes);
    return execResult;
}

Justina::execResult_type Justina::SD_fileChecks(bool argIsLong, bool argIsFloat, Val arg, File*& pFile, int allowFileTypes)
{
    // check file number (also perform related file and SD card object checks)
    if ((!argIsLong) && (!argIsFloat)) { return result_numberExpected; }                                                        // file number
    int fileNumber = argIsLong ? arg.longConst : arg.floatConst;

    execResult_type execResult = SD_fileChecks(pFile, fileNumber, allowFileTypes);
    return execResult;
}

Justina::execResult_type Justina::SD_fileChecks(File*& pFile, int fileNumber, int allowFileTypes)
{
    // check that SD card is initialized, file is open and file type (directory, file) is OK
    if ((_justinaStartupOptions & SD_mask) == SD_notAllowed) { return result_SD_noCardOrNotAllowed; }
    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if ((fileNumber < 1) || (fileNumber > MAX_OPEN_SD_FILES)) { return result_SD_invalidFileNumber; }
    if (!openFiles[fileNumber - 1].fileNumberInUse) { return result_SD_fileIsNotOpen; }
    pFile = &(openFiles[fileNumber - 1].file);
    if (allowFileTypes > 0) {                   // 0: allow files and directories, 1: allow files, 2: allow directories
        if (pFile->isDirectory()) { if (allowFileTypes == 1) { return result_SD_directoryNotAllowed; } }
        else { if (allowFileTypes == 2) { return result_SD_directoryExpected; } }
    }
    return result_execOK;
}


// ---------------------------------------------------------------------------------------------
// *   stream read and write functions with stream number supplied as argument                 *
// *   this allows setting application flags before calling the respective Arduino functions   * 
// *   this does NOT set global variables _pStreamIn, _pStreamOut and _streamNumberOut         *
// ---------------------------------------------------------------------------------------------


// read functions
// --------------

int Justina::readFrom(int streamNumber) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream) != result_execOK) { return 0; }          // if error, zero characters written but error is not returned to caller
    char c = pStream->read();

    if (c != 0xFF) {
        int readingFromExtStreamNumber{ streamNumber };
        if (streamNumber == 0) { readingFromExtStreamNumber = _consoleIn_sourceStreamNumber; }          // replace by real external IO stream number
        if (readingFromExtStreamNumber < 0) {
            _appFlags |= appFlag_dataInOut;
        }
    }
    return c;
}

int Justina::readFrom(int streamNumber, char* buffer, int length) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream) != result_execOK) { return 0; }          // if error, zero characters written but error is not returned to caller
    // NOTE: stream MUST be a file (check before call) -> appFlag_dataInOut must not be set
    return static_cast<File*>(pStream)->read((uint8_t*)buffer, length);
}


// write functions
// ---------------

size_t Justina::writeTo(int streamNumber, char c) {                         // allow to write 0xff as well
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->write(c);
}

size_t Justina::writeTo(int streamNumber, char* s, int size) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->write(s);
}


// print functions
// ---------------

size_t Justina::printTo(int streamNumber, char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(c);
}

size_t Justina::printTo(int streamNumber, unsigned char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(c);
}

size_t Justina::printTo(int streamNumber, int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(i);
}

size_t Justina::printTo(int streamNumber, unsigned int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(i);
}

size_t Justina::printTo(int streamNumber, long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(l);
}

size_t Justina::printTo(int streamNumber, unsigned long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(l);
}

size_t Justina::printTo(int streamNumber, double d) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(d);
}

size_t Justina::printTo(int streamNumber, char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(s);
}

size_t Justina::printTo(int streamNumber, const char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(s);
}


// print line functions
// --------------------

size_t Justina::printlnTo(int streamNumber, char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(c);
}

size_t Justina::printlnTo(int streamNumber, unsigned char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(c);
}

size_t Justina::printlnTo(int streamNumber, int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(i);
}

size_t Justina::printlnTo(int streamNumber, unsigned int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(i);
}

size_t Justina::printlnTo(int streamNumber, long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(l);
}

size_t Justina::printlnTo(int streamNumber, unsigned long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(l);
}

size_t Justina::printlnTo(int streamNumber, double d) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(d);
}

size_t Justina::printlnTo(int streamNumber, char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(s);
}

size_t Justina::printlnTo(int streamNumber, const char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(s);
}


size_t Justina::printlnTo(int streamNumber) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }    // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println();
}


// --------------------------------------------------------------------------------------------------------
// *   stream read and write functions, to a PRESET stream (NOT supplied as argument)                     *
// *   this allows setting application flags before calling the respective Arduino functions              * 
// *   function setStream() should be called first (to set the desired stream as input or output stream   *
// --------------------------------------------------------------------------------------------------------


// read functions
// --------------

int Justina::read() {
    char c = _pStreamIn->read();
    if (c != 0xFF) {
        int readingFromExtStreamNumber{ _streamNumberIn };
        if (_streamNumberIn == 0) { readingFromExtStreamNumber = _consoleIn_sourceStreamNumber; }           // replace by real external IO stream number
        if (readingFromExtStreamNumber < 0) {
            _appFlags |= appFlag_dataInOut;
        }
    }

    return c;
}

int Justina::read(char* buffer, int length) {
    // NOTE: stream MUST be a file (check before call) -> appFlag_dataInOut must not be set
    return (static_cast <File*>(_pStreamIn))->read((uint8_t*)buffer, length);
}


// write functions
// ---------------

size_t Justina::write(char c) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->write(c);
}

size_t Justina::write(char* s, int size) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->write(s, size);
}


// print functions
// ---------------

size_t Justina::print(char c) {
    if (_streamNumberIn <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(c);
}

size_t Justina::print(unsigned char c) {
    if (_streamNumberIn <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(c);
}

size_t Justina::print(int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(i);
}

size_t Justina::print(unsigned int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(i);
}

size_t Justina::print(long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(l);
}

size_t Justina::print(unsigned long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(l);
}

size_t Justina::print(double d) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(d);
}

size_t Justina::print(char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(s);
}

size_t Justina::print(const char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(s);
}



// print line functions
// --------------------

size_t Justina::println(char c) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(c);
}

size_t Justina::println(unsigned char c) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(c);
}

size_t Justina::println(int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(i);
}

size_t Justina::println(unsigned int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(i);
}

size_t Justina::println(long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(l);
}

size_t Justina::println(unsigned long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(l);
}

size_t Justina::println(double d) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(d);
}

size_t Justina::println(char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(s);
}

size_t Justina::println(const char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(s);
}

size_t Justina::println() {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println();
}


// ------------------------------------------------------------------------------------------------
// *   read character, if available, from stream, and regularly perform a housekeeping callback   *
// ------------------------------------------------------------------------------------------------

// NOTE: the stream must be set beforehand by function setStream()

char Justina::getCharacter(bool& kill, bool& forcedStop, bool& forcedAbort, bool& setStdConsole, bool allowWaitTime, bool useLongTimeout) {     // default: no time out, input from console

    // enable time out = false: only check once for a character
    //                   true: allow a certain time for the character to arrive   

    char c = 0xFF;                                                      // init: no character read
    long startWaitForReadTime = millis();                               // note the time
    bool readCharWindowExpired{};
    long timeOutValue = _pStreamIn->getTimeout();                       // get timeout value for the stream
    bool stop{ false }, abort{ false }, stdCons{ false };
    do {
        execPeriodicHousekeeping(&kill, &stop, &abort, &stdCons);       // get housekeeping flags
        // get character (if available)
        if (_pStreamIn->available() > 0) { c = read(); }
        if (kill) { return c; }                                         // flag 'kill' (request from Justina caller): return immediately
        forcedAbort = forcedAbort || abort;                             // do not exit immediately, except if waiting for a 'first' character (with a long timeout)
        if (forcedAbort && useLongTimeout && (_streamNumberIn <= 0)) { break; }
        forcedStop = forcedStop || stop;                                // flag 'stop': continue looking for a character (do not exit immediately). Upon exit, signal 'stop' flag has been raised
        setStdConsole = setStdConsole || stdCons;
        if (c != 0xff) { break; }


        // try to read character only once or keep trying until timeout occurs ?
        readCharWindowExpired = true;      // init (for file input) 
        if (_streamNumberIn <= 0) readCharWindowExpired = (!allowWaitTime || (startWaitForReadTime + (useLongTimeout ? LONG_WAIT_FOR_CHAR_TIMEOUT : timeOutValue) < millis()));
    } while (!readCharWindowExpired);
    return c;

}

// ----------------------------
// *   flush console buffer   *
// ----------------------------

bool Justina::flushInputCharacters(bool& forcedStop, bool& forcedAbort) {
    // flush incoming characters from a stream. The stream must be set before calling this procedure. 
    char c{};
    unsigned long start = millis();
    bool messageGiven{ false };
    long charCounter{ 0 };
    bool kill{ false };
    do {                                                                        // process remainder of input file (flush)
        // NOTE: forcedStop and forcedAbort are dummy arguments here and will be ignored because already flushing input file after error, abort or kill
        bool stop{ false }, abort{ false }, stdConsDummy{ false };              // dummy arguments (not needed here)
        c = getCharacter(kill, stop, abort, stdConsDummy, true);
        if (kill) { break; }                                                    // return value true: kill Justina interpreter (buffer is now flushed until next line character)
        if (abort) { forcedAbort = true; }                                      // do NOT exit immediately, keep on flushing
        if (stop) { forcedStop = true; }
        if (((millis() - start) > 1000) && !messageGiven) { messageGiven = true; printlnTo(0, "Flushing incoming characters... Please wait"); }

        // after a set time, start showing progress by printing dots 
        if (messageGiven) {
            if ((++charCounter & 0x1fff) == 0) {                                // print a dot each 512 characters
                printTo(0, '.');
                if ((charCounter & 0xfffff) == 0) { printlnTo(0); }             // print a crlf each 64 dots
            }
        }
    } while (c != 0xFF);     // kill: exit immediately
    printlnTo(0);
    return kill;
}


// ---------------------------------------------------------
// *   read text from keyboard and store in c++ variable   *
// ---------------------------------------------------------

// read characters and store in 'input' variable. Return on terminator character (length is stored in 'length').
// return value 'true' indicates kill request from Justina caller

bool Justina::getConsoleCharacters(bool& forcedStop, bool& forcedAbort, bool& doCancel, bool& doDefault, char* input, int& length, char terminator) {
    bool backslashFound{ false }, quitNow{ false };

    // NOTE: all characters are stored as typed, including backslash and double quote characters.

    int maxLength = length;  // init
    length = 0;
    setStream(0);                                                                           // set _pStreamIn to console, for use by Justina methods
    do {                                                                                    // until new line character encountered
        // read a character, if available in buffer
        char c{ };                                                                          // init: no character available
        bool kill{ false }, stop{ false }, abort{ false }, stdConsDummy{ false };
        c = getCharacter(kill, stop, abort, stdConsDummy);                                  // get a key (character from console) if available and perform a regular housekeeping callback as well
        if (kill) { return true; }                                                          // return value true: kill Justina interpreter (buffer is now flushed until next line character)
        if (abort) { forcedAbort = true; return false; }                                    // exit immediately
        if (stop) { forcedStop = true; }

        if (c != 0xFF) {                                                                    // terminal character available for reading ?
            if (c == terminator) { break; }                                                 // read until terminator found (if terminator is 0xff (default): no search for a terminator 
            else if (c < ' ') { continue; }                                                 // skip control-chars except new line (ESC is skipped here as well - flag already set)

            // Check for Justina ESCAPE sequence (sent by terminal as individual characters) and cancel input, or use default value, if indicated
            if (c == '\\') { backslashFound = !backslashFound; }                            // backslash character found

            else if (tolower(c) == 'c') {                                                   // part of a Justina ESCAPE sequence ? Cancel if allowed 
                if (backslashFound) { backslashFound = false;  doCancel = true; }
            }
            else if (tolower(c) == 'd') {                                                   // part of a Justina ESCAPE sequence ? Use default value if provided
                if (backslashFound) { backslashFound = false; doDefault = true; }
            }

            else { backslashFound = false; }

            if (length >= maxLength) { continue; }                                          // max. input length exceeded: drop character
            input[length] = c; input[++length] = '\0';
        }
    } while (true);

    return false;
}


// ---------------------------------------------------------------------------------------
// *   print a list of global program variables, to any output stream (external or SD)   *
// *   variables are printed with name, type, qualifier, value                           *
// ---------------------------------------------------------------------------------------

void Justina::printVariables(bool userVars) {

    // user variables only: indicate whether they are used in the currently parsed program (if any)
    // arrays: indicate dimensions and number of elements

    // before calling this function, output stream must be set by function 'setStream(...)'

    // print table header
    char line[MAX_IDENT_NAME_LEN + 30];     // sufficient length for all line elements except the variable value itself
    sprintf(line, ("%-*s %-2c%-8s%-7svalue"), MAX_IDENT_NAME_LEN, (userVars ? "user variable       " : "global prog variable"), (userVars ? 'U' : ' '), "type", "qual");
    println(line);
    sprintf(line, "%-*s %-2c%-8s%-7s-----", MAX_IDENT_NAME_LEN, (userVars ? "-------------" : "--------------------"), (userVars ? '-' : ' '), "----", "----");
    println(line);

    // print table
    int varCount = userVars ? _userVarCount : _programVarNameCount;
    char** varName = userVars ? userVarNames : programVarNames;
    char* varType = userVars ? userVarType : globalVarType;
    Val* varValues = userVars ? userVarValues : globalVarValues;
    bool userVarUsedInProgram{};
    bool varNameHasGlobalValue{};
    bool linesPrinted{ false };

    for (int q = 0; q <= 1; q++) {
        bool lookForConst = q == 0;
        for (int i = 0; i < varCount; i++) {
            varNameHasGlobalValue = userVars ? true : varType[i] & var_nameHasGlobalValue;
            if (varNameHasGlobalValue) {
                bool isConst = (varType[i] & var_isConstantVar);
                if (lookForConst == isConst) {
                    int valueType = (varType[i] & value_typeMask);
                    userVarUsedInProgram = userVars ? (varType[i] & var_userVarUsedByProgram) : false;
                    bool isLong = (valueType == value_isLong);
                    bool isFloat = (valueType == value_isFloat);
                    bool isString = (valueType == value_isStringPointer);
                    bool isArray = (varType[i] & var_isArray);

                    char type[10];
                    strcpy(type, isLong ? "long" : isFloat ? "float" : isString ? "string" : "????");

                    sprintf(line, "%-*s %-2c%-8s%-7s", MAX_IDENT_NAME_LEN, *(varName + i), (userVarUsedInProgram ? 'x' : ' '), type, (isConst ? "const  " : "       "));
                    print(line);

                    if (isArray) {
                        uint8_t* dims = (uint8_t*)varValues[i].pArray;
                        int dimCount = dims[3];
                        char arrayText[40] = "";
                        sprintf(arrayText, "(array %d", dims[0]);
                        if (dimCount >= 2) { sprintf(arrayText, "%sx%d", arrayText, dims[1]); }
                        if (dimCount == 3) { sprintf(arrayText, "%sx%d", arrayText, dims[2]); }
                        if (dimCount >= 2) { sprintf(arrayText, "%s = %d", arrayText, int(dims[0]) * int(dims[1]) * int(dimCount == 3 ? dims[2] : 1)); }
                        strcat(arrayText, " elem)");
                        println(arrayText);
                    }

                    else if (isLong) { println(varValues[i].longConst); }
                    else if (isFloat) { println(varValues[i].floatConst); }
                    else if (isString) {
                        char* pString = varValues[i].pStringConst;
                        quoteAndExpandEscSeq(pString);        // creates new string
                        println(pString);
                    #if PRINT_HEAP_OBJ_CREA_DEL
                        _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)pString, HEX);
                        _pDebugOut->print("    print variables ");   _pDebugOut->println(pString);
                    #endif
                        _intermediateStringObjectCount--;
                        delete[] pString;
                    }
                    else { println("????"); }

                    linesPrinted = true;
                }
            }
        }
    }
    if (!linesPrinted) { println("    (none)"); }
}


// --------------------------------------------------------------------------------------------------------
// *   print the program call stack, even from a running program, to any output stream (external or SD)   *
// *   stopped programs (if there are) are included in the print                                          *
// --------------------------------------------------------------------------------------------------------

// before calling this function, output stream must be set by function 'setStream(...)'

void Justina::printCallStack() {
    if (_callStackDepth > 0) {      // including eval() stack levels but excluding open block (for, if, ...) stack levels
        int indent = 0;
        void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;
        int blockType = block_none;
        for (int i = 0; i < flowCtrlStack.getElementCount(); ++i) {
            char s[MAX_IDENT_NAME_LEN + 1] = "";
            blockType = ((openBlockGeneric*)pFlowCtrlStackLvl)->blockType;
            if (blockType == block_eval) {
                for (int space = 0; space < indent - 4; ++space) { print(" "); }
                if (indent > 0) { print("|__ "); }
                println("eval() string");
                indent += 4;
            }
            else if (blockType == block_JustinaFunction) {
                if (((OpenFunctionData*)pFlowCtrlStackLvl)->pNextStep < (_programStorage + _PROGRAM_MEMORY_SIZE)) {
                    for (int space = 0; space < indent - 4; ++space) { print(" "); }
                    if (indent > 0) { print("|__ "); }
                    int index = ((OpenFunctionData*)pFlowCtrlStackLvl)->functionIndex;                                  // print function name
                    sprintf(s, "%s()", JustinaFunctionNames[index]);
                    println(s);
                    indent += 4;
                }
                else {
                    for (int space = 0; space < indent - 4; ++space) { print(" "); }
                    if (indent > 0) { print("|__ "); }
                    println((i < flowCtrlStack.getElementCount() - 1) ? "debugging command line" : "command line");     // command line
                    indent = 0;
                }
            }
            pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
        }
    }
    else  println("(no program running)");
}

// --------------------------------------
// *   print execution error or event   *
// --------------------------------------

void Justina::printExecError(execResult_type execResult, bool  showStopmessage) {
    if (*_pConsolePrintColumn != 0) { printlnTo(0);  *_pConsolePrintColumn = 0; }

    bool isEvent = (execResult >= result_startOfEvents);                                                                // not an error but an event ?

    // plain error, or event ? 
    if (!isEvent) {
        int sourceErrorPos{ 0 };
        int functionNameLength{ 0 };
        long programCounterOffset{ 0 };

        // if an execution error occurs, normally the info needed to correctly identify and print the statement and the function (if not an imm. mode statement) where the error occurred...
        // will be found in structure _activeFunctionData.
        // But if the cause of the STATEMENT execution error is actually a PARSING or EXECUTION error in a (nested or not) eval() string, the info MAY be found in the flow ctrl stack 

        // [1] If a PARSING error occurs while parsing an UNNESTED eval() string, as in statement  a = 3 + eval("2+5*")   (the asterisk will produce a parsing error),
        // then the info pointing to the correct statement ('caller' of the eval() function) is still available in the active function data structure (block type 'block_JustinaFunction'),  
        // because the data has not yet been pushed to the flow ctrl stack

        // [2] If a PARSING error occurs while parsing a NESTED eval() string, or an EXECUTION error occurs while executing ANY parsed eval() string (nested or not),
        // the info pointing to the correct statement has been pushed to the flow ctrl stack already 

        char* errorStatementStartStep = _activeFunctionData.errorStatementStartStep;
        char* errorProgramCounter = _activeFunctionData.errorProgramCounter;
        int functionIndex = _activeFunctionData.functionIndex;          // init

        // info to identify and print the statement where the error occurred is on the flow ctrl stack ? find it there
        if (_activeFunctionData.blockType == block_eval) {
            void* pFlowCtrlStackLvl = _pFlowCtrlStackTop;                                                               // one level below _activeFunctionData
            char* pImmediateCmdStackLvl = _pParsedCommandLineStackTop;

            while (((OpenFunctionData*)pFlowCtrlStackLvl)->blockType == block_eval) {
                pFlowCtrlStackLvl = flowCtrlStack.getPrevListElement(pFlowCtrlStackLvl);
                pImmediateCmdStackLvl = parsedCommandLineStack.getPrevListElement(pImmediateCmdStackLvl);
            }

            // retrieve error statement pointers and function index (in case the 'function' block type is referring to immediate mode statements)
            errorStatementStartStep = ((OpenFunctionData*)pFlowCtrlStackLvl)->errorStatementStartStep;
            errorProgramCounter = ((OpenFunctionData*)pFlowCtrlStackLvl)->errorProgramCounter;
            functionIndex = ((OpenFunctionData*)pFlowCtrlStackLvl)->functionIndex;

            // if the error statement pointers refer to immediate mode code (not to a program), pretty print directly from the imm.mode parsed command stack: add an offset to the pointers 
            bool isImmMode = (errorStatementStartStep >= (_programStorage + _PROGRAM_MEMORY_SIZE));
            if (isImmMode) { programCounterOffset = pImmediateCmdStackLvl + sizeof(char*) - (_programStorage + _PROGRAM_MEMORY_SIZE); }
        }

        printTo(0, "\r\n  ");
        prettyPrintStatements(0, 1, errorStatementStartStep + programCounterOffset, errorProgramCounter + programCounterOffset, &sourceErrorPos);
        for (int i = 1; i <= sourceErrorPos; ++i) { printTo(0, " "); }

        char execInfo[50 + MAX_IDENT_NAME_LEN] = "";
        sprintf(execInfo, "  ^\r\n  Exec error %d", execResult);                                                        // in main program level 
        printTo(0, execInfo);

        // errorProgramCounter is never pointing to a token directly contained in a parsed() eval() string 
        if (errorProgramCounter >= (_programStorage + _PROGRAM_MEMORY_SIZE)) { sprintf(execInfo, ""); }
        else {
            long sourceLine = _pBreakpoints->findLineNumberForBPstatement(errorStatementStartStep);
            sprintf(execInfo, " in user function %s, source line %ld", JustinaFunctionNames[functionIndex], sourceLine);
        }
        printTo(0, execInfo);

        if (execResult == result_eval_parsingError) { sprintf(execInfo, " (eval() parsing error %ld)\r\n", _evalParseErrorCode); }
        else if (execResult == result_list_parsingError) { sprintf(execInfo, " (list input parsing error %ld)\r\n", _evalParseErrorCode); }
        else { sprintf(execInfo, "\r\n"); }
        printTo(0, execInfo);
    }

    else if (execResult == result_quit) {printTo(0, "\r\nExecuting 'quit' command\r\n"); }
    else if (execResult == result_kill) {}      // do nothing
    else if (execResult == result_abort) { printTo(0, "\r\n+++ Abort: code execution terminated +++\r\n"); }
    else if (execResult == result_stopForDebug) { if (showStopmessage) { printTo(0, "\r\n+++ Program stopped +++\r\n"); } }
    else if (execResult == result_initiateProgramLoad) {}                                                               // (nothing to do here for this event)

    _lastValueIsStored = false;                                                                                         // prevent printing last result (if any)
}

// -----------------------------------------
// *   pretty print a parsed instruction   *
// -----------------------------------------
void Justina::prettyPrintStatements(int outputStream, int instructionCount, char* startToken, char* errorProgCounter, int* sourceErrorPos) {

    // input: stored tokens
    TokenPointer progCnt;
    progCnt.pTokenChars = (startToken == nullptr) ? _programStorage + _PROGRAM_MEMORY_SIZE : startToken;
    int tokenType = *progCnt.pTokenChars & 0x0F;
    int lastTokenType = tok_no_token;
    bool lastHasTrailingSpace = true, testForPostfix = false, testForPrefix = false;
    bool lastWasPostfixOperator = false, lastWasInfixOperator = false;

    bool allInstructions = (instructionCount == 0);
    bool multipleInstructions = (instructionCount > 1);                                                                 // multiple, but not all, instructions
    bool isFirstInstruction = true;

    // output: printable token (text) - must be long enough to hold one token in text (e.g. a variable name)
    const int maxCharsPrettyToken{ MAX_ALPHA_CONST_LEN };                                                               // IT IS SUPPOSED THAT A STRING CAN BE LONGER THAN ANY OTHER TOKEN
    const int maxOutputLength{ 200 };
    int outputLength = 0;                                                                                               // init: first position

    char intFormatStr[10] = "%#l";                                                                                       // '#' flag: always precede hex values with 0x
    strcat(intFormatStr, _dispIntegerSpecifier);
    char floatFmtStr[10] = "%#.*";                                                                                      // '#' flag: always a decimal point
    strcat(floatFmtStr, _dispFloatSpecifier);

    while (tokenType != tok_no_token) {                                                                                 // for all tokens in token list
        int tokenLength = (tokenType >= tok_isTerminalGroup1) ? sizeof(TokenIsTerminal) : (tokenType == tok_isConstant) ? sizeof(TokenIsConstant) :
            (tokenType == tok_isSymbolicConstant) ? sizeof(TokenIsSymbolicConstant) : (*progCnt.pTokenChars >> 4) & 0x0F;
        TokenPointer nextProgCnt;
        nextProgCnt.pTokenChars = progCnt.pTokenChars + tokenLength;
        int nextTokenType = *nextProgCnt.pTokenChars & 0x0F;                                                            // next token type (look ahead)
        bool tokenHasLeadingSpace = false, testNextForPostfix = false, isPostfixOperator = false, isInfixOperator = false;
        bool hasTrailingSpace = false;
        bool isSemicolon = false;

        char prettyToken[maxCharsPrettyToken] = "";                                         // used for all tokens except string values; must be long enough for the longest token in text
        char* pPrettyToken{ prettyToken };                                                  // init: for all tokens except string values

        switch (tokenType)
        {
            case tok_isReservedWord:
            {
                TokenIsResWord* pToken = (TokenIsResWord*)progCnt.pTokenChars;
                bool nextIsTerminal = ((nextTokenType == tok_isTerminalGroup1) || (nextTokenType == tok_isTerminalGroup2) || (nextTokenType == tok_isTerminalGroup3));
                bool nextIsSemicolon = false;
                if (nextIsTerminal) {
                    int nextTokenIndex = ((nextProgCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F);
                    nextTokenIndex += ((nextTokenType == tok_isTerminalGroup2) ? 0x10 : (nextTokenType == tok_isTerminalGroup3) ? 0x20 : 0);
                    nextIsSemicolon = (_terminals[nextTokenIndex].terminalCode == termcod_semicolon) || (_terminals[nextTokenIndex].terminalCode == termcod_semicolon_BPset)
                        || (_terminals[nextTokenIndex].terminalCode == termcod_semicolon_BPallowed);
                }

                sprintf(prettyToken, nextIsSemicolon ? "%s" : "%s ", _resWords[progCnt.pResW->tokenIndex]._resWordName);
                hasTrailingSpace = true;
            }
            break;

            case tok_isInternCppFunction:
                strcpy(prettyToken, _internCppFunctions[progCnt.pInternCppFunc->tokenIndex].funcName);
                break;

            case tok_isExternCppFunction:
                strcpy(prettyToken, ((CppDummyVoidFunction*)_pExtCppFunctions[progCnt.pExternCppFunc->returnValueType])[progCnt.pExternCppFunc->funcIndexInType].cppFunctionName);
                break;

            case tok_isJustinaFunction:
            {
                int identNameIndex = (int)progCnt.pJustinaFunc->identNameIndex;             // Justina function list element
                char* identifierName = JustinaFunctionNames[identNameIndex];
                strcpy(prettyToken, identifierName);
            }
            break;

            case tok_isVariable:
            {
                int identNameIndex = (int)(progCnt.pVar->identNameIndex);
                bool isForcedFunctionVar = (progCnt.pVar->identInfo & var_isForcedFunctionVar) == var_isForcedFunctionVar;
                bool isUserVar = (progCnt.pVar->identInfo & var_scopeMask) == var_isUser;
                char* identifierName = isUserVar ? userVarNames[identNameIndex] : programVarNames[identNameIndex];
                sprintf(prettyToken, "%s%s", (isForcedFunctionVar ? "#" : ""), identifierName);
                testNextForPostfix = true;
            }
            break;

            case tok_isSymbolicConstant:
            {
                int nameIndex = progCnt.pSymbCstToken->nameIndex;
                sprintf(prettyToken, "%s", _symbNumConsts[nameIndex].symbolName);
                testNextForPostfix = true;
            }
            break;

            case tok_isConstant:
            {
                char valueType = (*progCnt.pTokenChars >> 4) & value_typeMask;
                bool isLongConst = (valueType == value_isLong);
                bool isFloatConst = (valueType == value_isFloat);
                bool isStringConst = (valueType == value_isStringPointer);

                if (isLongConst) {
                    long  l;
                    memcpy(&l, progCnt.pCstToken->cstValue.longConst, sizeof(l));           // pointer not necessarily aligned with word size: copy memory instead
                    sprintf(prettyToken, intFormatStr, l);                                  // integers always displayed without exponent
                    testNextForPostfix = true;
                    break;   // and quit switch
                }

                else if (isFloatConst) {
                    float f;
                    memcpy(&f, progCnt.pCstToken->cstValue.floatConst, sizeof(f));          // pointer not necessarily aligned with word size: copy memory instead
                    sprintf(prettyToken, floatFmtStr, _dispFloatPrecision, f);              // displayed with current floating point precision
                    testNextForPostfix = true;
                    break;   // and quit switch
                }

                else if (isStringConst) {
                    testNextForPostfix = true;                                              // no break here: fall into generic name handling
                }
            }
            // NO break here

            case tok_isGenericName:
            {
                char* pAnum{ nullptr };
                memcpy(&pAnum, progCnt.pCstToken->cstValue.pStringConst, sizeof(pAnum));    // copy pointer, not string (not necessarily aligned with word size: copy memory instead)

                if (testNextForPostfix) {                                                   // string constant and NOT a generic name ? expand '\' sequences and add string delimiters
                    quoteAndExpandEscSeq(pAnum);                                            // returns pointer to new (temporary) string created on the heap 
                    strcpy(prettyToken, pAnum);
                #if PRINT_HEAP_OBJ_CREA_DEL
                    _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)pAnum, HEX);
                    _pDebugOut->print("  pretty print name ");   _pDebugOut->println(pAnum);
                #endif
                    _intermediateStringObjectCount--;
                    delete[]pAnum;
                }
                else {
                    strcpy(prettyToken, pAnum); strcat(prettyToken, " ");                   // generic token: just add a space at the end
                }
                hasTrailingSpace = !testNextForPostfix;
            }
            break;

            default:  // terminal
            {
                int index = (progCnt.pTermTok->tokenTypeAndIndex >> 4) & 0x0F;
                index += ((tokenType == tok_isTerminalGroup2) ? 0x10 : (tokenType == tok_isTerminalGroup3) ? 0x20 : 0);
                char trailing[2] = "\0";                                                    // init: empty string

                if (_terminals[index].terminalCode <= termcod_opRangeEnd) {                 // operator 
                    isPostfixOperator = testForPostfix ? (_terminals[index].postfix_priority != 0) : false;

                    isInfixOperator = lastWasInfixOperator ? false : testForPostfix ? !isPostfixOperator : false;

                    if (lastWasPostfixOperator && isPostfixOperator) {                      // check if operator is postfix operator 
                        strcat(prettyToken, " ");                                           // leading space
                        tokenHasLeadingSpace = true;
                    }

                    if (!isPostfixOperator && !lastHasTrailingSpace) {                      // check if operator is postfix operator 
                        strcat(prettyToken, " ");                                           // leading space
                        tokenHasLeadingSpace = true;
                    }

                    if ((isInfixOperator)) {
                        trailing[0] = ' ';                                                  // single space (already terminated by '\0')
                        hasTrailingSpace = true;
                    }

                    testNextForPostfix = isPostfixOperator;
                }

                else if (_terminals[index].terminalCode == termcod_rightPar) {
                    testNextForPostfix = true;
                }

                else if (_terminals[index].terminalCode == termcod_leftPar) {
                    hasTrailingSpace = true;
                    testNextForPostfix = false;
                }

                else if ((_terminals[index].terminalCode == termcod_comma) || (_terminals[index].terminalCode == termcod_semicolon) || (_terminals[index].terminalCode == termcod_semicolon_BPset)
                    || (_terminals[index].terminalCode == termcod_semicolon_BPallowed)) {
                    testNextForPostfix = false;
                    trailing[0] = ' ';                                                      // single space (already terminated by '\0')
                    hasTrailingSpace = true;
                }

                strcat(prettyToken, _terminals[index].terminalName);                        // concatenate with empty string or single-space string
                strcat(prettyToken, trailing);
                isSemicolon = (_terminals[index].terminalCode == termcod_semicolon) || (_terminals[index].terminalCode == termcod_semicolon_BPset) ||
                    (_terminals[index].terminalCode == termcod_semicolon_BPallowed);
            }
            break;
        }


        // print pretty token
        // ------------------

        // if not printing all instructions, then limit output, but always print the first instruction in full
        if (!allInstructions && !isFirstInstruction && (outputLength > maxOutputLength)) { break; }

        int tokenSourceLength = strlen(pPrettyToken);
        if (isSemicolon) {
            if (multipleInstructions && isFirstInstruction) { pPrettyToken[1] = '\0'; }     // no space after semicolon
            if ((nextTokenType != tok_no_token) && (allInstructions || (instructionCount > 1))) { printTo(outputStream, pPrettyToken); }
            if (isFirstInstruction && multipleInstructions) { printTo(outputStream, "   ( ==>> "); }
        }

        else { printTo(outputStream, pPrettyToken); }                                       // not a semicolon

        // if printing a fixed number of instructions, return output error position based on token where execution error was produced
        if (!allInstructions) {
            if (errorProgCounter == progCnt.pTokenChars) {
                *sourceErrorPos = outputLength + (tokenHasLeadingSpace ? 1 : 0);
            }
            if (isSemicolon) {
                if (--instructionCount == 0) { break; }                                     // all statements printed
            }
            outputLength += tokenSourceLength;
        }


        // advance to next token
        // ---------------------

        progCnt.pTokenChars = nextProgCnt.pTokenChars;
        lastTokenType = tokenType;
        tokenType = nextTokenType;                                                          // next token type
        testForPostfix = testNextForPostfix;
        lastHasTrailingSpace = hasTrailingSpace;
        lastWasInfixOperator = isInfixOperator;
        lastWasPostfixOperator = isPostfixOperator;

        if (isSemicolon) { isFirstInstruction = false; }
    }

    // exit
    printTo(outputStream, multipleInstructions ? " ...)\r\n" : allInstructions ? "" : "\r\n"); _lastPrintedIsPrompt = false;
}


// ----------------------------
// *   print parsing result   *
// ----------------------------

void Justina::printParsingResult(parsingResult_type result, int funcNotDefIndex, char* const pInstruction, int lineCount, char* pErrorPos) {

    char parsingInfo[100 + MAX_IDENT_NAME_LEN] = "";                                        // provide sufficient room for longest possible message (int: no OK message in immediate mode)
    if (result == result_parsing_OK) {                                                      // prepare message with parsing result
        if (_programMode) {
            if (_lastProgramStep == _programStorage) { strcpy(parsingInfo, "\r\nNo program loaded\r\n"); }
            else {
                sprintf(parsingInfo, "\r\nProgram parsed without errors. %lu %% of program memory used (%lu of %lu bytes)\r\n",
                    (uint32_t)(((_lastProgramStep - _programStorage + 1) * 100) / _PROGRAM_MEMORY_SIZE), (uint32_t)(_lastProgramStep - _programStorage + 1), _PROGRAM_MEMORY_SIZE);
            }
        }
    }

    else  if ((result == result_function_undefinedFunctionOrArray) && _programMode) {       // in program mode only 
        // during Justina function call parsing, it is not always known whether the function exists (because function can be defined after a call) 
        // -> a line number can not be given, but the undefined function can
        sprintf(parsingInfo, "\r\n  Parsing error %d: function or array '%s' is not defined", result, JustinaFunctionNames[funcNotDefIndex]);
    }

    else {                                                                                  // parsing error
        // instruction not parsed (because of error): print source instruction where error is located (can not 'unparse' yet for printing instruction)
        if (result == result_statementTooLong) { pErrorPos = pInstruction; }

        printTo(0, "\r\n  "); printlnTo(0, pInstruction);
        char point[pErrorPos - pInstruction + 3];                                           // 2 extra positions for 2 leading spaces, 2 for '^' and '\0' characters
        memset(point, ' ', pErrorPos - pInstruction + 2);
        point[pErrorPos - pInstruction + 2] = '^';
        point[pErrorPos - pInstruction + 3] = '\0';
        printlnTo(0, point);

        if (_programMode) { sprintf(parsingInfo, "  Parsing error %d: statement ending at line %d", result, lineCount + 1); }
        else { sprintf(parsingInfo, "  Parsing error %d", result); }
    }

    if (strlen(parsingInfo) > 0) { printlnTo(0, parsingInfo); _lastPrintedIsPrompt = false; }
};


// -----------------------------------------------------------------------------------------
// *   add surrounding quotes AND expand backslash and double quote characters in string   *
// -----------------------------------------------------------------------------------------

void Justina::quoteAndExpandEscSeq(char*& stringValue) {

    // backslash characters expand to two successive backslash characters    ...\...  becomes  ...\\...
    // double quote characters expand to a backslash and a double quote      ..."...  becomes  ...\"...

    // examples (all quotes shown are part of the function result): 
    // so, number 123   -> string 123  (same as function cStr(123) )
    //     string 123   -> string "123"
    //     string \123\ -> string "\\123\\"
    //     string "123" -> string "\"123\""
    //     string abc\def"ghi -> string "abc\\def\"ghi" -> string "\"abc\\\\def\\\"ghi\"" -> ... 

    // this function is used by the Justina quote() function and certain Justina print functions

    // NOTE: this routine creates a character string on the heap; it must be deleted afterwards

    int occurences{ 0 };                   // count '\' and '"' characters within string
    char* pos = stringValue;

    if (stringValue != nullptr) { do { pos = strpbrk(pos, "\\\""); if (pos != nullptr) { ++occurences; } } while (pos++ != nullptr); }
    int oldLen = (stringValue == nullptr) ? 0 : strlen(stringValue);

    _intermediateStringObjectCount++;
    char* output = new char[oldLen + occurences + 2 + 1];                               // provide room for expanded \ and " characters, 2 string terminating " and terminator
    output[0] = '"';                                                                    // add starting string terminator

    if (oldLen != 0) {
        char* newPos = pos = stringValue;

        char* destinPos = output + 1;

        for (int i = 1; i <= occurences; ++i) {
            newPos = strpbrk(pos, "\\\"");      // always match
            int len = newPos - pos;             // if zero, first character is \ or "
            if (len > 0) { memcpy(destinPos, pos, len);  destinPos += len; }            // copy characters before found \ or " character 
            destinPos[0] = '\\'; destinPos[1] = newPos[0];  destinPos += 2;
            newPos++;
            pos = newPos;
        }
        strcpy(destinPos, pos);     // remainder of string
    }
    output[oldLen + occurences + 2 - 1] = '"';
    output[oldLen + occurences + 2] = '\0';
#if PRINT_HEAP_OBJ_CREA_DEL
    _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)output, HEX);
    _pDebugOut->print("     quote & expand ");   _pDebugOut->println(output);
#endif

    stringValue = output;
    
    return;
}


// -------------------------------
// *   check format specifiers   *
// -------------------------------

// possible callers: 
// -----------------
// 1. display and integer format command:
//    dispFmt precision [, specifier]  [, flags] ]
//    intFmt precision [, specifier]  [, flags] ]
//    
// 2. fmt() function:
//    fmt (expression [, width [, precision [, specifier]  [, flags  [, character count] ] ] ]
//        =>> !!! expression and width are NOT passed to this function !!! <<=

Justina::execResult_type Justina::checkFmtSpecifiers(bool isDispFmtCmd, int argCount, char* valueType, Val* operands, char& specifier, int& precision, int& flags) {

    // format a value: one-character specifier string included ?
    bool hasSpecifierArg{ false }; // init
    if (argCount > 1) { hasSpecifierArg = (valueType[1] == value_isStringPointer); }

    bool hasReturnParameter = isDispFmtCmd ? false : argCount == 3;                // no error if no variable, but nothing will be returned

    char spec = ' ';
    if (hasSpecifierArg) {
        if (operands[1].pStringConst == nullptr) { return result_arg_invalid; }
        if (strlen(operands[1].pStringConst) != 1) { return result_arg_invalid; }
        spec = operands[1].pStringConst[0];
        char* pChar(strchr("fGgEeXxds", spec));
        if (pChar == nullptr) { return result_arg_invalid; }
        specifier = spec;                   // valid specifier: return it

        // move next arguments, if supplied, down one position 
        for (int index = 1; index < argCount - 1; index++) {
            operands[index] = operands[index + 1];
            valueType[index] = valueType[index + 1];
        }
        argCount--;
    }

    // first and last index only refer to formatting fields with specifier field removed, not to value to format (if dispFmt command) and not 'chars printed' return value (if dispFmt command)
    int firstFmtArgIndex = 0;
    int lastFmtArgIndex = (isDispFmtCmd ? 2 : 3) - (hasSpecifierArg ? 0 : 1) - (hasReturnParameter ? 1 : 0);       // exclude return value
    if (argCount <= lastFmtArgIndex) { lastFmtArgIndex = argCount - 1; }

    int prec{}, fl{};
    for (int argIndex = firstFmtArgIndex; argIndex <= lastFmtArgIndex; argIndex++) {

        // Width, precision, flags ? Numeric arguments expected
        if ((valueType[argIndex] != value_isLong) && (valueType[argIndex] != value_isFloat)) { return result_arg_numberExpected; }    // numeric ?
        if ((valueType[argIndex] == value_isLong) ? operands[argIndex].longConst < 0 : operands[argIndex].floatConst < 0.) { return result_arg_outsideRange; }                                           // positive ?
        int argValue = (valueType[argIndex] == value_isLong) ? operands[argIndex].longConst : (long)operands[argIndex].floatConst;

        if (argIndex == firstFmtArgIndex) { precision = argValue; }            // precision
        else if (argIndex == firstFmtArgIndex + 1) { flags = argValue; }       // flags
    }

    flags &= 0b11111;       // apply mask

    return result_execOK;
}


// ------------------------------
// *   create a format string   *
// ------------------------------


void  Justina::makeFormatString(int flags, bool longPrefix, char* specifier, char* fmtString) {

    fmtString[0] = '%';
    int strPos = 1;
    for (int i = 1; i <= 5; i++, flags >>= 1) {
        if (flags & 0b1) { fmtString[strPos] = ((i == 1) ? '-' : (i == 2) ? '+' : (i == 3) ? ' ' : (i == 4) ? '#' : '0'); ++strPos; }
    }
    fmtString[strPos] = '*'; ++strPos; fmtString[strPos] = '.'; ++strPos; fmtString[strPos] = '*'; ++strPos;               // width and precision specified with additional arguments (*.*)
    if (longPrefix) { fmtString[strPos] = 'l'; ++strPos; fmtString[strPos] = specifier[0]; ++strPos; }                     // "ld", "lx": long integer in decimal or hex format
    else { fmtString[strPos] = specifier[0]; ++strPos; }
    fmtString[strPos] = '%'; ++strPos; fmtString[strPos] = 'n'; ++strPos; fmtString[strPos] = '\0'; ++strPos;              // %n specifier (return characters printed - for fmt() function only)

    return;
}


// -------------------------------------------------------------------------------
// *   format number or string according to format string (result is a string)   *
// -------------------------------------------------------------------------------

void  Justina::printToString(int width, int precision, bool inputIsString, bool isIntFmt, char* valueType, Val* value, char* fmtString,
    Val& fcnResult, int& charsPrinted, bool expandStrings) {
    int opStrLen{ 0 }, resultStrLen{ 0 };

    if (inputIsString) {
        if ((*value).pStringConst != nullptr) {
            opStrLen = strlen((*value).pStringConst);
            if (opStrLen > MAX_PRINT_WIDTH) { (*value).pStringConst[MAX_PRINT_WIDTH] = '\0'; opStrLen = MAX_PRINT_WIDTH; }  // clip input string without warning (won't need it any more)
        }
        resultStrLen = max(width + 10, opStrLen + 10);                                                                      // allow for a few extra formatting characters, if any
    }
    else {
        resultStrLen = max(width + 10, 30);                                                                                 // 30: ensure length is sufficient to print a formatted number
    }

    _intermediateStringObjectCount++;
    fcnResult.pStringConst = new char[resultStrLen + 1];

    if (inputIsString) {
        if (expandStrings) {
            if ((*value).pStringConst != nullptr) {
                char* pString = (*value).pStringConst;                                                                      // remember pointer to original string
                quoteAndExpandEscSeq((*value).pStringConst);                                                                // creates new string
            #if PRINT_HEAP_OBJ_CREA_DEL
                _pDebugOut->print("\r\n----- (Intermd str) ");   _pDebugOut->println((uint32_t)pString, HEX);
                _pDebugOut->print("  printToString (2) ");   _pDebugOut->println(pString);
            #endif
                _intermediateStringObjectCount--;
                delete[] pString;                                               // delete old string
            }
        }
        sprintf(fcnResult.pStringConst, fmtString, width, precision, ((*value).pStringConst == nullptr) ? (expandStrings ? "\"\"" : "") : (*value).pStringConst, &charsPrinted);
    }
    // note: hex output for floating point numbers is not provided (Arduino)
    else if (isIntFmt) {
        sprintf(fcnResult.pStringConst, fmtString, width, precision, (*valueType == value_isLong) ? (*value).longConst : (long)(*value).floatConst, &charsPrinted);
    }
    else {      // floating point
        sprintf(fcnResult.pStringConst, fmtString, width, precision, (*valueType == value_isLong) ? (float)(*value).longConst : (*value).floatConst, &charsPrinted);
    }

#if PRINT_HEAP_OBJ_CREA_DEL
    _pDebugOut->print("\r\n+++++ (Intermd str) ");   _pDebugOut->println((uint32_t)fcnResult.pStringConst, HEX);
    _pDebugOut->print("  printToString (1) ");   _pDebugOut->println(fcnResult.pStringConst);
#endif

    return;
}


