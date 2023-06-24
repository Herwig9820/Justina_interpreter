/***************************************************************************************
    Justina interpreter library for Arduino Nano 33 IoT and Arduino RP2040.

    Version:    v1.00 - xx/xx/2022
    Author:     Herwig Taveirne

    Justina is an interpreter which does NOT require you to use an IDE to write
    and compile programs. Programs are written on the PC using any text processor
    and transferred to the Arduino using any terminal capable of sending files.
    Justina can store and retrieve programs and other data on an SD card as well.

    See GitHub for more information and documentation: //// <links>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************************/


#include "Justina.h"

// *****************************************************************
// ***        class Justina_interpreter - implementation         ***
// *****************************************************************

// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type Justina_interpreter::startSD() {

    if (_SDinitOK) { return result_execOK; }          // card is initialised: nothing to do

    if ((_JustinaConstraints & 0b0011) == 0) { return result_SD_noCardOrCardError; }
    if (!_SDcard.init(SPI_HALF_SPEED, _SDcardChipSelectPin)) { return result_SD_noCardOrCardError; }
    if (!SD.begin(_SDcardChipSelectPin)) { return result_SD_noCardOrCardError; }

    _openFileCount = 0;
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) { openFiles[i].fileNumberInUse = false; }
    _SDinitOK = true;
    return result_execOK;
}


// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type Justina_interpreter::SD_open(int& fileNumber, char* filePath, int mode) {

    fileNumber = 0;                                                     // init: no file number yet

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if (_openFileCount == MAX_OPEN_SD_FILES) { return result_SD_maxOpenFilesReached; }                               // max. open files reached

    // create temp string
    if (!pathValid(filePath)) { return result_SD_pathIsNotValid; }        // is not a complete check, but it remedies a few plaws in SD library

    int len = strlen(filePath);
    // provide space for starting '/' if missing                 
    char* filePathInCapitals = new char[((filePath[0] == '/') ? 0 : 1) + len + 1];                 // not counted for memory leak testing
    if (filePath[0] != '/') { filePathInCapitals[0] = '/'; }
    strcpy(filePathInCapitals + ((filePath[0] == '/') ? 0 : 1), filePath);   // copy original string
    for (int i = 0; i < strlen(filePathInCapitals); i++) { filePathInCapitals[i] = toupper(filePathInCapitals[i]); }

    // currently open files ? Check that the same file is not open already
    if (fileIsOpen(filePathInCapitals)) {
        delete[] filePathInCapitals;                // not counted for memory leak testing
        return result_SD_fileAlreadyOpen;
    }

    // find a free file number 
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (!openFiles[i].fileNumberInUse) {
            openFiles[i].file = SD.open(filePathInCapitals, mode);
            if (!openFiles[i].file) {
                delete[] filePathInCapitals;                // not counted for memory leak testing
                return result_SD_couldNotOpenFile;
            }                                              // could not open file (in case caller ignores this error, file number returned is 0)

            File* pFile = &openFiles[i].file;

            openFiles[i].file.setTimeout(DEFAULT_READ_TIMEOUT);
            openFiles[i].fileNumberInUse = true;
            openFiles[i].filePath = filePathInCapitals;                          // delete when file is closed
            openFiles[i].currentPrintColumn = 0;
            fileNumber = i + 1;
            break;
        }
    }

    ++_openFileCount;

    return result_execOK;
}


// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type Justina_interpreter::SD_openNext(int dirFileNumber, int& fileNumber, File* pDirectory, int mode) {

    fileNumber = 0;                                                     // init: no next file opened

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if (_openFileCount == MAX_OPEN_SD_FILES) { return result_SD_maxOpenFilesReached; }                               // max. open files reached

    // it's not possible to check whether the next file is open already, because we don't know which file will be opened as next. We'll check afterwards

    char* dirPath = openFiles[dirFileNumber - 1].filePath;            // path for the directory
    int dirPathLength = strlen(dirPath);

    // find a free file number and assign it to this file
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (!openFiles[i].fileNumberInUse) {
            openFiles[i].file = pDirectory->openNextFile(mode);
            // file evaluates to false: assume last file in directory is currently open (if any) and no more files are available. Do not return error, file number 0 indicates 'last file reached'
            if (!openFiles[i].file) { return result_execOK; }

            // room for full name, '/' between path and name, '\0' terminator
            openFiles[i].file.setTimeout(DEFAULT_READ_TIMEOUT);
            openFiles[i].fileNumberInUse = true;
            openFiles[i].filePath = new char[dirPathLength + 1 + strlen(openFiles[i].file.name()) + 1];                  // not counted for memory leak testing     
            strcpy(openFiles[i].filePath, dirPath);
            strcat(openFiles[i].filePath, "/");
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
                // compare the long filenames (including filepath) for the newly opened file with the previously opened open file (comparing file refs doesn't work)
                if (strcasecmp(openFiles[i].filePath, openFiles[fileNumber - 1].filePath) == 0) {      // 8.3 file format: NOT case sensitive      
                    openFiles[fileNumber - 1].file.close();                                             // close newer file ref again
                    openFiles[fileNumber - 1].fileNumberInUse = false;
                    delete[] openFiles[fileNumber - 1].filePath;                // not counted for memory leak testing
                    --_openFileCount;
                    return result_SD_fileAlreadyOpen;                                                   // return with error
                }
            }
        }
    }

    return result_execOK;
}



// --------------------------------
//
// --------------------------------

void Justina_interpreter::SD_closeFile(int fileNumber) {

    // checks must have been done before calling this function

    if (static_cast <Stream*>(&openFiles[fileNumber - 1].file) == _pDebugOut) {_pDebugOut = _pConsoleOut; }
    openFiles[fileNumber - 1].file.close();                                             // does not return errors
    openFiles[fileNumber - 1].fileNumberInUse = false;
    delete[] openFiles[fileNumber - 1].filePath;                // not counted for memory leak testing
    --_openFileCount;
}


// --------------------------------
//
// --------------------------------

void  Justina_interpreter::SD_closeAllFiles() {

    if (!_SDinitOK) { return; }          // card is NOT initialised: nothing to do

    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            if (static_cast <Stream*>(&openFiles[i].file) == _pDebugOut) { _pDebugOut = _pConsoleOut; }     // debug out could also be an external io stream
            openFiles[i].fileNumberInUse = false;                                   // slot is open again
            delete openFiles[i].filePath;
            openFiles[i].file.close();                                              // does not return errors
        }
    }
    _openFileCount = 0;
}


// ---------------------------------------------
// list all files in the card with date and size
// ---------------------------------------------

void Justina_interpreter::printDirectory(File dir, int indentLevel) {
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
            if (len < defaultSizeAttrColumn - minimumColumnSpacing) {      // 
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

Justina_interpreter::execResult_type Justina_interpreter::SD_listFiles() {

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }

    // print to console (default), or to any other defined I/O device, or to a file, but without date and time stamp (unfortunately fixed in SD library)
    // before calling this function, output stream must be set by function 'setStream(...)'

    SDLib::File SDroot = SD.open("/");
    println("SD card: files (name, size in bytes): ");
    printDirectory(SDroot, 0);

    return result_execOK;
}


// ----------
//
// ----------

bool Justina_interpreter::pathValid(char* path) {

    // SD library allows to run into issues if path is not valid (hanging, nivalid creation of directories / files)
    // this routine performs a few basic checks: 
    // - path should NOT start with a space
    // - path should NOT end with a '/' or a space
    // - never two '/' in a row

    if (path == nullptr) { return false; }  // empty path is not valid
    if (strlen(path) == 1) { return (path[0] != ' '); }
    if ((path[0] == ' ') || (path[strlen(path) - 1] == '/') || (path[strlen(path) - 1] == ' ')) { return false; }

    bool previousIsSlash{ true }, currentIsSlash{ false };
    char* p{};
    for (p = path + 1; p < path + strlen(path); p++) {          // skip first character in test 
        currentIsSlash = (p[0] == '/');
        if (previousIsSlash && currentIsSlash) { break; }
        previousIsSlash = currentIsSlash;
    }
    return (p == path + strlen(path));      // if loop ended normally, then no issues found
}


// ----------
//
// ----------

bool Justina_interpreter::fileIsOpen(char* path) {
    // currently open files ? Check that the same file is not open already
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            if (strcasecmp(openFiles[i].filePath, path) == 0) { return true; }     // 8.3 file format: NOT case sensitive
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// set either _streamNumberIn, _pStreamIn or _streamNumberOut, _pStreamOut for future IO operations
// ------------------------------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::setStream(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, int& streamNumber, bool forOutput) {
    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }                      // file number
    streamNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;

    setStream(streamNumber, forOutput);
    return result_execOK;
}


Justina_interpreter::execResult_type Justina_interpreter::setStream(int streamNumber, bool forOutput) {

    Stream* pTemp;

    execResult_type execResult = determineStream(streamNumber, pTemp, forOutput); if (execResult != result_execOK) { return execResult; }
    (forOutput ? _streamNumberOut : _streamNumberIn) = streamNumber;
    (forOutput ? _pStreamOut : _pStreamIn) = pTemp;         // set global variables instead (forOutput argument doesn't play)

    return result_execOK;
}


// this overload also returns the pointer to the set stream (pStream), in addition to _streamNumberIn, _pStreamIn or _streamNumberOut, _pStreamOut

Justina_interpreter::execResult_type Justina_interpreter::setStream(int streamNumber, Stream*& pStream, bool forOutput) {

    execResult_type execResult = determineStream(streamNumber, pStream, forOutput); if (execResult != result_execOK) { return execResult; }
    (forOutput ? _streamNumberOut : _streamNumberIn) = streamNumber;
    (forOutput ? _pStreamOut : _pStreamIn) = pStream;         // set global variables instead (forOutput argument doesn't play)

    return result_execOK;
}


// -----------------------------------------------------------------------------------------------------------------------
// return pointer to a stream based on stream number (for console only, this will depend on value of argument 'forOutput') 
// NOTE: this does NOT set _streamNumberIn, _pStreamIn or _streamNumberOut, _pStreamOut
// -----------------------------------------------------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::determineStream(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, Stream*& pStream, int& streamNumber, bool forOutput) {
    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }                      // file number
    streamNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;

    return determineStream(streamNumber, pStream, forOutput);
}


Justina_interpreter::execResult_type  Justina_interpreter::determineStream(int streamNumber, Stream*& pStream, bool forOutput) {

    if (streamNumber == 0) { pStream = forOutput ? _pConsoleOut : _pConsoleIn; }  // init: assume console
    else if ((-streamNumber) > _externIOstreamCount) { return result_IO_invalidStreamNumber; }
    else if (streamNumber < 0) { pStream = _pExternIOstreams[(-streamNumber) - 1]; }    // external IO: stream number -1 => array index 0, etc.
    else {
        File* pFile{};
        execResult_type execResult = SD_fileChecks(pFile, streamNumber);    // operand: file number
        if (execResult != result_execOK) { return execResult; }
        pStream = static_cast<Stream*> (pFile);
    }
    return result_execOK;
}


// -----------------------------------------------------------
// perform file checks prior to performing actions on the file
// -----------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::SD_fileChecks(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, File*& pFile, int allowFileTypes)
{
    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }                      // file number
    int fileNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;
    execResult_type execResult = SD_fileChecks(pFile, fileNumber, allowFileTypes);
    return execResult;
}

Justina_interpreter::execResult_type Justina_interpreter::SD_fileChecks(bool argIsLong, bool argIsFloat, Val arg, File*& pFile, int allowFileTypes)
{
    // check file number (also perform related file and SD card object checks)
    if ((!argIsLong) && (!argIsFloat)) { return result_numberExpected; }                      // file number
    int fileNumber = argIsLong ? arg.longConst : arg.floatConst;

    execResult_type execResult = SD_fileChecks(pFile, fileNumber, allowFileTypes);
    return execResult;
}

Justina_interpreter::execResult_type Justina_interpreter::SD_fileChecks(File*& pFile, int fileNumber, int allowFileTypes)
{
    // check that SD card is initialised, file is open and file type (directory, file) is OK
    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if ((fileNumber < 1) || (fileNumber > MAX_OPEN_SD_FILES)) { return result_SD_invalidFileNumber; }
    if (!openFiles[fileNumber - 1].fileNumberInUse) { return result_SD_fileIsNotOpen; }
    pFile = &(openFiles[fileNumber - 1].file);
    if (allowFileTypes > 0) {       // 0: allow files and directories, 1: allow files, 2: allow directories
        if (pFile->isDirectory() != (allowFileTypes == 2)) { return result_SD_directoryNotAllowed; }
    }
    return result_execOK;
}


// ----------------------------------------------------------------
// streamNumber supplied as argument: the stream is determined here 
// global variables _pStreamIn, _pStreamOut and _streamNumberOut are NOT affected
// ------------------------------------------------------------------------------

int Justina_interpreter::readFrom(int streamNumber) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    int c = pStream->read();
    if ((streamNumber <= 0) && (c != 255)) {
        _appFlags |= appFlag_dataInOut;
        if (pStream == _pTCPstream) { _appFlags |= appFlag_TCPkeepAlive; }
    }
    return c;
}

int Justina_interpreter::readFrom(int streamNumber, char* buffer, int length) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    return static_cast<File*>(pStream)->read(buffer, length);                   // NOTE: stream MUST be a file -> appFlag_dataInOut must not be set
}



size_t Justina_interpreter::writeTo(int streamNumber, char c) {                         // allow to write 0xff as well
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->write(c);
}

size_t Justina_interpreter::writeTo(int streamNumber, char* s, int size) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->write(s);
}


size_t Justina_interpreter::printTo(int streamNumber, char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(c);
}

size_t Justina_interpreter::printTo(int streamNumber, unsigned char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(c);
}

size_t Justina_interpreter::printTo(int streamNumber, int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(i);
}

size_t Justina_interpreter::printTo(int streamNumber, unsigned int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(i);
}

size_t Justina_interpreter::printTo(int streamNumber, long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(l);
}

size_t Justina_interpreter::printTo(int streamNumber, unsigned long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(l);
}

size_t Justina_interpreter::printTo(int streamNumber, double d) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(d);
}

size_t Justina_interpreter::printTo(int streamNumber, char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(s);
}

size_t Justina_interpreter::printTo(int streamNumber, const char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->print(s);
}


size_t Justina_interpreter::printlnTo(int streamNumber, char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(c);
}

size_t Justina_interpreter::printlnTo(int streamNumber, unsigned char c) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(c);
}

size_t Justina_interpreter::printlnTo(int streamNumber, int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(i);
}

size_t Justina_interpreter::printlnTo(int streamNumber, unsigned int i) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(i);
}

size_t Justina_interpreter::printlnTo(int streamNumber, long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(l);
}

size_t Justina_interpreter::printlnTo(int streamNumber, unsigned long l) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(l);
}

size_t Justina_interpreter::printlnTo(int streamNumber, double d) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(d);
}

size_t Justina_interpreter::printlnTo(int streamNumber, char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(s);
}

size_t Justina_interpreter::printlnTo(int streamNumber, const char* s) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println(s);
}


size_t Justina_interpreter::printlnTo(int streamNumber) {
    Stream* pStream{ nullptr };
    if (determineStream(streamNumber, pStream, true) != result_execOK) { return 0; }   // if error, zero characters written but error is not returned to caller
    if (streamNumber <= 0) { _appFlags |= appFlag_dataInOut; }
    return pStream->println();
}


// output is sent to stream _pStreamIn (read) or _pStreamOut (write)
// The stream should be set first, by calling 'setStream'
// -------------------------------------------------------------------

int Justina_interpreter::read() {
    int c = _pStreamIn->read();
    if ((_streamNumberIn <= 0) && (c != 255)) {
        _appFlags |= appFlag_dataInOut;
        if (_pStreamIn == _pTCPstream) { _appFlags |= appFlag_TCPkeepAlive; }
    }
    return c;
}

int Justina_interpreter::read(char* buffer, int length) {
    return (static_cast <File*>(_pStreamIn))->read(buffer, length);         // Note: stream MUST be a file -> appFlag_dataInOut flag must not be set
}



size_t Justina_interpreter::write(char c) {                                 // write (only): allow to write character 0xff as well
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->write(c);
}

size_t Justina_interpreter::write(char* s, int size) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->write(s, size);
}



size_t Justina_interpreter::print(char c) {
    if (_streamNumberIn <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(c);
}

size_t Justina_interpreter::print(unsigned char c) {
    if (_streamNumberIn <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(c);
}

size_t Justina_interpreter::print(int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(i);
}

size_t Justina_interpreter::print(unsigned int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(i);
}

size_t Justina_interpreter::print(long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(l);
}

size_t Justina_interpreter::print(unsigned long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(l);
}

size_t Justina_interpreter::print(double d) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(d);
}

size_t Justina_interpreter::print(char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(s);
}

size_t Justina_interpreter::print(const char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->print(s);
}




size_t Justina_interpreter::println(char c) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(c);
}

size_t Justina_interpreter::println(unsigned char c) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(c);
}

size_t Justina_interpreter::println(int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(i);
}

size_t Justina_interpreter::println(unsigned int i) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(i);
}

size_t Justina_interpreter::println(long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(l);
}

size_t Justina_interpreter::println(unsigned long l) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(l);
}

size_t Justina_interpreter::println(double d) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(d);
}

size_t Justina_interpreter::println(char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(s);
}

size_t Justina_interpreter::println(const char* s) {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println(s);
}

size_t Justina_interpreter::println() {
    if (_streamNumberOut <= 0) { _appFlags |= appFlag_dataInOut; }
    return _pStreamOut->println();
}



