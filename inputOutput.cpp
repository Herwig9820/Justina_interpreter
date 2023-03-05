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

#define printCreateDeleteListHeapObjects 0
#define printProcessedTokens 0
#define debugPrint 0
#define printParsedStatementStack 0

// *****************************************************************
// ***        class Justina_interpreter - implementation         ***
// *****************************************************************

// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::initSD() {

    if (_SDinitOK) { result_execOK; }          // card is initialised: nothing to do

    _openFileCount = 0;
    _activeFileNum = 0;                     // console is active for I/O

    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) { openFiles->fileNumberInUse = false; }

    if (!_SDcard.init(SPI_HALF_SPEED, _SDcardChipSelectPin)) { return result_SD_noCardOrCardError; }
    if (!SD.begin(SD_CHIP_SELECT_PIN)) { return result_SD_noCardOrCardError; }

    _SDinitOK = true;
    return result_execOK;
}


// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type  Justina_interpreter::ejectSD() {

    if (!_SDinitOK) { result_execOK; }          // card is NOT initialised: nothing to do

    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (openFiles[i].fileNumberInUse) {
            openFiles[i].fileNumberInUse = false;                                   // slot is open again
            openFiles[i].file.close();                                              // does not return errors
        }
    }

    _openFileCount = 0;
    _activeFileNum = 0;                // console is active for I/O

    _SDinitOK = false;
    SD.end();
    return result_execOK;
}


// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type Justina_interpreter::open(int& fileNumber, char* filePath, int mode) {

    fileNumber = 0;                                                     // init: no file number yet

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if (_openFileCount == MAX_OPEN_SD_FILES) { return result_SD_maxOpenFilesReached; }                               // max. open files reached

    // currently open files ? Check that the same file is not opened twice
    if (_openFileCount > 0) {
        for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
            if (strcmp(openFiles[i].file.name(), filePath) == 0) { return result_SD_fileAlreadyOpen; }
        }
    }

    File f = SD.open(filePath, mode);
    if (!f) { return result_SD_couldNotOpenFile; }                                              // could not open file

    // at least one free slot: find it 
    for (int i = 0; i < MAX_OPEN_SD_FILES; ++i) {
        if (!openFiles[i].fileNumberInUse) {
            openFiles[i].fileNumberInUse = true;
            openFiles[i].file = f;
            fileNumber = i + 1;
            break;
        }
    }

    _activeFileNum = fileNumber;                                // make this file the active file for I/O
    ++_openFileCount;

    return result_execOK;
}


// --------------------------------
//
// --------------------------------

Justina_interpreter::execResult_type Justina_interpreter::close(int fileNumber) {

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }
    if ((fileNumber <= 0) || (fileNumber > MAX_OPEN_SD_FILES)) { return result_SD_invalidFileNumber; }

    if (!openFiles[fileNumber - 1].fileNumberInUse) { return result_SD_fileIsNotOpen; }

    openFiles[fileNumber - 1].file.close();                                             // does not return errors
    openFiles[fileNumber - 1].fileNumberInUse = false;
    if (_activeFileNum == fileNumber) { _activeFileNum = 0; }                     // currently active file for I/O is now closed: console is now active for I/O
    --_openFileCount;

    return result_execOK;
}


// ------------------------------------------------------------
// list all files in the card with date and size TO SERIAL PORT
// ------------------------------------------------------------

Justina_interpreter::execResult_type Justina_interpreter::listFiles() {
    SdVolume volume{};
    SdFile root{};

    if (!_SDinitOK) { return result_SD_noCardOrCardError; }

    _pConsole->println("\nSD card: files (name, date, size in bytes): ");

    volume.init(_SDcard);
    root.openRoot(volume);
    root.ls(LS_R | LS_DATE | LS_SIZE);      // to SERIAL (not to _console)

    return result_execOK;
}
