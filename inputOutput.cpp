/***************************************************************************************
    Justina interpreter library for Arduino Nano 33 IoT and Arduino RP2040.

    Version:    v1.00 - xx/xx/2022
    Author:     Herwig Taveirne

    Justina is an interpreter which does NOT require you to use an IDE to write
    and compile programs. Programs are written on the PC using any text processor
    and transferred to the Arduino using any serial terminal capable of sending files.
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

            openFiles[i].fileNumberInUse = true;
            openFiles[i].filePath = filePathInCapitals;                          // delete when file is closed
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
            openFiles[i].fileNumberInUse = true;
            openFiles[i].filePath = new char[dirPathLength + 1 + strlen(openFiles[i].file.name()) + 1];                  // not counted for memory leak testing     
            strcpy(openFiles[i].filePath, dirPath);
            strcat(openFiles[i].filePath, "/");
            strcat(openFiles[i].filePath, openFiles[i].file.name());
            fileNumber = i + 1;
            break;
        }
    }

    ++_openFileCount;

    // currently older open files ? Check that the same file is not opened twice now
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            if (i + 1 != fileNumber) {
                // compare the filepath for the newly opened file with the previously opened open file
                if (strcasecmp(openFiles[i].filePath, openFiles[fileNumber - 1].filePath) == 0) {        // comparing file refs doesn't work (why?): compare file paths + names
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

void Justina_interpreter::printDirectory(Stream* pOut, File dir, int indentLevel) {
    constexpr int step{ 2 }, defaultSizeAttrColumn{ 20 }, minimumColumnSpacing{ 4 };

    while (true) {

        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        for (uint8_t i = 1; i <= indentLevel * step; i++) {
            pOut->print(" ");
        }
        pOut->print(entry.name());
        if (entry.isDirectory()) {
            pOut->println("/");
            printDirectory(pOut, entry, indentLevel + 1);
        }
        else {
            // files have sizes, directories do not
            int len = indentLevel * step + strlen(entry.name());
            if (len < defaultSizeAttrColumn - minimumColumnSpacing) {      // 
                for (int i = len; i < defaultSizeAttrColumn; i++) { pOut->print(" "); }
            }
            else {
                for (int i = 0; i < minimumColumnSpacing; i++) { pOut->print(" "); }
            }
            pOut->println(entry.size());

        }
        entry.close();
    }
}

Justina_interpreter::execResult_type Justina_interpreter::SD_listFiles(Stream* pOut) {

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }

    /*
    // print to SERIAL (fixed) but include date and time stamp
    SdVolume volume{};
    SdFile root{};

    Serial.println("\nSD card: files (name, date, size in bytes): ");

    volume.init(_SDcard);
    root.openRoot(volume);
    root.ls(LS_R | LS_DATE | LS_SIZE);      // to SERIAL (not to _console)
    */

    // print to console but without date and time stamp
    SDLib::File SDroot = SD.open("/");
    pOut->println("\nSD card: files (name, size in bytes): ");
    printDirectory(pOut, SDroot, 0);

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


// -----------------------------------------------------------------------------------
// check for valid stream number (file or I/O) prior to performing actions on the file 
// -----------------------------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::checkStream(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, Stream*& pStream, int& streamNumber) {
    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }                      // file number
    streamNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;

    if (streamNumber == 0) { pStream = static_cast<Stream*> (_pConsole); }
    else if (streamNumber < 0) {
        if (( - streamNumber) > _altIOstreamCount) { return result_IO_invalidStreamNumber; }              
        pStream = static_cast<Stream*>(_pAltIOstreams[(-streamNumber) - 1]);     // stream number -1 => array index 0, etc.
    }
    else {      // file
        File* pFile{};
        execResult_type execResult = SD_fileChecks(pFile, streamNumber);    // operand: file number
        if (execResult != result_execOK) { return execResult; }
        pStream = static_cast<Stream*> (pFile);
    }
    return result_execOK;
}


// ----------
//
// ----------

bool Justina_interpreter::pathValid(char* path) {

    // SD library allows to run into issues if path is not valid (hanging, nivalid creation of directories / files)
    // this routine performs a few basic checks: 
    // - path should start with a space
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
            if (strcasecmp(openFiles[i].filePath, path) == 0) { return true; }
        }
    }
    return false;
}
