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

#define printHeapObjectCreationDeletion 0
#define printProcessedTokens 0
#define debugPrint 0
#define printParsedStatementStack 0

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

void  Justina_interpreter::SD_closeAllFiles() {

    if (!_SDinitOK) { return; }          // card is NOT initialised: nothing to do

    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            openFiles[i].fileNumberInUse = false;                                   // slot is open again
            openFiles[i].file.close();                                              // does not return errors
        }
    }
    _openFileCount = 0;
}


// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type Justina_interpreter::SD_open(int& fileNumber, char* filePath, int mode) {

    fileNumber = 0;                                                     // init: no file number yet

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if (_openFileCount == MAX_OPEN_SD_FILES) { return result_SD_maxOpenFilesReached; }                               // max. open files reached

    // create temp string
    int len = strlen(filePath);
    char* filePathInCapitals = new char[len + 1];                                                                                 // same length as original, space for terminating 
    strcpy(filePathInCapitals, filePath);   // copy original string
    for (int i = 0; i < len; i++) { filePathInCapitals[i] = toupper(filePathInCapitals[i]); }

    // currently open files ? Check that the same file is not open already
    if (_openFileCount > 0) {
        for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
            if (openFiles[i].fileNumberInUse) {
                if (strcasecmp(openFiles[i].file.name(), filePath) == 0) {
                    delete[] filePathInCapitals;
                    return result_SD_fileAlreadyOpen;
                }
            }
        }
    }

    File f = SD.open(filePathInCapitals, mode);
    delete[] filePathInCapitals;                                                            // delete temp string
    if (!f) { return result_SD_couldNotOpenFile; }                                              // could not open file (in case caller ignores this error, file number returned is 0)

    // find a free file number 
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (!openFiles[i].fileNumberInUse) {
            openFiles[i].fileNumberInUse = true;
            openFiles[i].file = f;
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

Justina_interpreter::execResult_type Justina_interpreter::SD_openNext(int& fileNumber, File directory, int mode) {

    fileNumber = 0;                                                     // init: no next file opened

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if (_openFileCount == MAX_OPEN_SD_FILES) { return result_SD_maxOpenFilesReached; }                               // max. open files reached

    // it's not possible to check whether the next file is open already, because we don't know which file will be opened as next. We'll check afterwards
    File f = directory.openNextFile(mode);

    // file evaluates to false: assume last file in directory is currently open and no more files are available. Do not return error, file number 0 indicates 'last file reached'
    if (!f) { return result_execOK; }

    // find a free file number and assign it to this file
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (!openFiles[i].fileNumberInUse) {
            openFiles[i].fileNumberInUse = true;
            openFiles[i].file = f;
            fileNumber = i + 1;
            break;
        }
    }

    ++_openFileCount;

    // currently older open files ? Check that the same file is not opened twice now
    if (_openFileCount > 1) {
        for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
            if (openFiles[i].fileNumberInUse) {
                if (i + 1 != fileNumber) {
                    if (strcasecmp(openFiles[i].file.name(), f.name()) == 0) {        // comparing file refs won't work: compare file names
                        openFiles[fileNumber - 1].file.close();                                             // close newer file ref again
                        openFiles[fileNumber - 1].fileNumberInUse = false;
                        --_openFileCount;
                        return result_SD_fileAlreadyOpen;                                                   // return with error
                    }
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
    --_openFileCount;
}


// ------------------------------------------------------------
// list all files in the card with date and size TO SERIAL PORT
// ------------------------------------------------------------

void Justina_interpreter::printDirectory(File dir, int indentLevel) {
    constexpr int step{ 2 }, defaultSizeAttrColumn{ 20 }, minimumColumnSpacing{ 4 };

    while (true) {

        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        for (uint8_t i = 1; i <= indentLevel * step; i++) {
            _pConsole->print(" ");
        }
        _pConsole->print(entry.name());
        if (entry.isDirectory()) {
            _pConsole->println("/");
            printDirectory(entry, indentLevel + 1);
        }
        else {
            // files have sizes, directories do not
            int len = indentLevel * step + strlen(entry.name());
            if (len < defaultSizeAttrColumn - minimumColumnSpacing) {      // 
                for (int i = len; i < defaultSizeAttrColumn; i++) { _pConsole->print(" "); }
            }
            else {
                for (int i = 0; i < minimumColumnSpacing; i++) { _pConsole->print(" "); }
            }
            _pConsole->println(entry.size());

        }
        entry.close();
    }
}

Justina_interpreter::execResult_type Justina_interpreter::SD_listFiles() {
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
    _pConsole->println("\nSD card: files (name, size in bytes): ");
    printDirectory(SDroot, 0);

    return result_execOK;
}


// ------------------------------------------------
// perform file checks before executing file method
// ------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::SD_fileChecks(long argIsLongBits, long argIsFloatBits, Val arg, long argIndex, File& file, int allowFileTypes)
{
    // check file number (also perform related file and SD card object checks)
    if ((!(argIsLongBits & (0x1 << argIndex))) && (!(argIsFloatBits & (0x1 << argIndex)))) { return result_numberExpected; }                      // file number
    int fileNumber = (argIsLongBits & (0x1 << argIndex)) ? arg.longConst : arg.floatConst;

    execResult_type execResult = SD_fileChecks(file, fileNumber, allowFileTypes);
    return execResult;
}

Justina_interpreter::execResult_type Justina_interpreter::SD_fileChecks(bool argIsLong, bool argIsFloat, Val arg, File& file, int allowFileTypes)
{
    // check file number (also perform related file and SD card object checks)
    if ((!argIsLong) && (!argIsFloat)) { return result_numberExpected; }                      // file number
    int fileNumber = argIsLong ? arg.longConst : arg.floatConst;

    execResult_type execResult = SD_fileChecks(file, fileNumber, allowFileTypes);
    return execResult;
}

Justina_interpreter::execResult_type Justina_interpreter::SD_fileChecks(File& file, int fileNumber, int allowFileTypes)
{
    // check file number (also perform related file and SD card object checks)
    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if ((fileNumber < 1) || (fileNumber > MAX_OPEN_SD_FILES)) { return result_SD_invalidFileNumber; }
    if (!openFiles[fileNumber - 1].fileNumberInUse) { return result_SD_fileIsNotOpen; }
    file = openFiles[fileNumber - 1].file;
    if (allowFileTypes > 0) {       // 0: allow files and directories, 1: allow files, 2: allow directories
        if (file.isDirectory() != (allowFileTypes == 2)) { return result_SD_directoryNotAllowed; }
    }
    return result_execOK;
}

