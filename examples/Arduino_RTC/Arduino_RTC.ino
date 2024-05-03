/*************************************************************************************************************************
*   Example code demonstrating how to use an RTC clock with Justina                                                     *
*                                                                                                                        *
*   The Justina interpreter library is licensed under the terms of the GNU General Public License v3.0 as published      *
*   by the Free Software Foundation (https://www.gnu.org/licenses).                                                      *
*   Refer to GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter            *
*                                                                                                                        *
*   This example code is in the public domain                                                                            *
*                                                                                                                        *
*   2024, Herwig Taveirne                                                                                                *
*************************************************************************************************************************/

#include "Justina.h"
#include <SdFat.h>
#include <SD.h>
#include <time.h>


/*

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/


// create Justina_interpreter object with default values: IO via Serial only, SD card allowed, default SD card CS pin.
Justina justina;


void dateTime(uint16_t* date, uint16_t* time)
{
    unsigned int year = 1980;
    byte month = 8;
    byte day = 8;
    byte hour = 1;
    byte minute = 2;
    byte second = 3;
    
    *date = FAT_DATE(year, month, day);
    *time = FAT_TIME(hour, minute, second);
}
// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);
    SdFile::dateTimeCallback((dateTime));

    justina.begin();                          // run interpreter (control will stay there until you quit Justina)
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}



