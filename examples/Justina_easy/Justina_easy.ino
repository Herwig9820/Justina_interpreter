/*************************************************************************************************************************
*   Example Arduino sketch demonstrating Justina interpreter functionality												 *
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

/*
	Example code demonstrating how to create a Justina object with default values and run Justina 
	---------------------------------------------------------------------------------------------
    When a Justina object is created, a (list of) argument(s) can be supplied, defining the available 'channels' (streams)  
    available for external input and output (specified separately), an SD card mode and SD card chip select pin.
    This sketch creates a Justina object WITHOUT supplying a list of arguments, using default values instead. 

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/


// create Justina_interpreter object with default values: IO via Serial only, SD card allowed, default SD card CS pin.
Justina justina;


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);

    justina.begin();                          // run interpreter (control will stay there until you quit Justina)
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
