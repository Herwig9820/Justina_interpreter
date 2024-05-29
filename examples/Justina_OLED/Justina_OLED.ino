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
#include <U8g2lib.h>                            // (install the U8g2lib library using the Arduino library manager)

/*
    Example code demonstrating how to connect multiple input and/or output devices to Justina
    -----------------------------------------------------------------------------------------
    Justina can receive data from / send data to maximum four IO / input only / output only devices.
    This sketch demonstrates how to set up
    - an OLED display with SH1106 controller communicating over software (SW) SPI as additional output device
    - an OLED display with SSD1306 controller communicating over I2C as additional input device
    next to Serial (connecting to the Arduino Serial Monitor or any other serial terminal).

    Output is sent to the OLED display(s) using the U8g2lib library (U8glib library for monochrome displays, 8x8 character output)
    This library (https://github.com/olikraus/u8g2) is licensed under the terms of the new-bsd license(two-clause bsd license).
    See https://github.com/olikraus/u8g2 for detailed licensing information.

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!   WHEN USING AN ARDUINO NANO RP2040, DO NOT USE AN OLED USING SW SPI (SOFTWARE SPI) TO COMMUNICATE.   !
!                                         ---                                                           !
!   => DO NOT SET 'WITH_OLED_SW_SPI' TO 1 (JUST BELOW) WITH THE ARDUINO NANO RP2040. IT DOESN'T WORK.   !
!   The issue MIGHT be located in library U8g2lib.                                                      !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/


// set directives to 0 if output device is not present, set to 1 if present, 0 if not (OLED: output only)  
#define WITH_OLED_SW_SPI 1                      // use SW SPI OLED ? Note: HW (hardware) SPI interferes with SD card breakout box, if used
#define WITH_OLED_HW_I2C 1                      // use HW I2C OLED ?

// Define the dimension of the U8x8log window
#define U8LOG_WIDTH 16                          // 16 characters wide
#define U8LOG_HEIGHT 8                          // 8 lines


// -------------------------
// OLED display using SW SPI
// -------------------------

#if WITH_OLED_SW_SPI 
// wiring: for specific pins, note that nano ESP32 pin numbering is different from other nano boards.
// the 'physical' pins do not change however.
#if defined ESP32
constexpr int VMA437_OLED_CS_PIN{ 19 };         // OLED chip select pin   
constexpr int VMA437_OLED_DC_PIN{ 20 };         // OLED data pin
constexpr int VMA437_OLED_CLK_PIN{ 23 };        // OLED clock pin           
constexpr int VMA437_OLED_MOSI_PIN{ 24 };       // OLED MOSI pin            
#else           
constexpr int VMA437_OLED_CS_PIN{ 16 };         // OLED chip select pin 
constexpr int VMA437_OLED_DC_PIN{ 17 };         // OLED data pin
constexpr int VMA437_OLED_CLK_PIN{ 20 };        // OLED clock pin
constexpr int VMA437_OLED_MOSI_PIN{ 21 };       // OLED MOSI pin
#endif


// NOTE (cf. next line): OLED hardware (HW) SPI is not compatible with SD card HW SPI => do NOT do this:
// U8X8_SH1106_128X64_NONAME_4W_HW_SPI u8x8(VMA437_OLED_CS_PIN, VMA437_OLED_DC_PIN);

// create object for 128x64 dot OLED with on-board SH1106 controller 
// for other OLEDs, check out the OLED library documentation to select the correct object type
U8X8_SH1106_128X64_NONAME_4W_SW_SPI u8x8_spi(VMA437_OLED_CLK_PIN, VMA437_OLED_MOSI_PIN, VMA437_OLED_CS_PIN, VMA437_OLED_DC_PIN);  // SW SPI

U8X8LOG u8x8log_spi;                                                                // create object implementing text window with automatic vertical scrolling                                                                       
uint8_t u8log_buffer_spi[U8LOG_WIDTH * U8LOG_HEIGHT];                               // allocate memory for display (width x height in characters)
#endif


// -------------------------
// OLED display using HW I2C
// -------------------------

#if WITH_OLED_HW_I2C
// wiring: see Arduino SPI library documentation (data and clock lines, ground and Vcc)
// create object for 128x64 dot OLED with onboard SSD1306 controller 
// for other OLEDs, check out the OLED library documentation to select the correct object type
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8_i2c;
U8X8LOG u8x8log_i2c;                                                                // create object implementing text window with automatic vertical scrolling
uint8_t u8log_buffer_i2c[U8LOG_WIDTH * U8LOG_HEIGHT];                               // allocate memory for display (width x height characters)
#endif


// ---------------------
// Create Justina object
// ---------------------

// define between 1 and 4 'external' input streams and output streams. 
constexpr int terminalCount{ 3 };                                                   // 3 streams defined (Serial; two (optional) OLED displays)

// pExternalInputs[0] & pExternalOutput[0] point to the default input and output streams for Justina AND function as the default Justina console 
Stream* pExternalInputs[terminalCount]{ &Serial, nullptr, nullptr };                // Justina input streams (Serial only)                                                 
Print* pExternalOutput[terminalCount]{ &Serial, nullptr, nullptr };                 // Justina output streams (Serial; OLEDs will be added in setup() )                                                     

// create Justina interpreter object
Justina justina(pExternalInputs, pExternalOutput, terminalCount);


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);


// initialize OLED displays and set as Justina output stream
// ---------------------------------------------------------

// !!! IMPORTANT NOTE regarding the fonts: u8g2 and u8x8 assume ISO/IEC 8859 encoding, which is not what the Arduino environment uses. !!!
// !!! For character codes higher than 0xA0, characters printed on the OLED display might not be the characters you expect.            !!!
// !!! to print such a character correctly on the OLED display, look up its ISO/IEC 8859 code and print 'char(code)' to the OLED.      !!!


#if WITH_OLED_SW_SPI
    u8x8_spi.begin();                                                               // initialize OLED object                                                                   
    u8x8_spi.setFont(u8x8_font_5x7_f);                                              // set font
    u8x8log_spi.begin(u8x8_spi, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer_spi);       // initialize OLED text window object, connect to U8x8, set character size and assign memory
    u8x8log_spi.setRedrawMode(0);		                                            // set the U8x8log redraw mode. 0: Update screen with newline, 1: Update screen for every char  

    pExternalOutput[1] = static_cast<Print*> (&u8x8log_spi);                        // add SW SPI OLED to available Justina output streams (in Justina IO commands, this will be stream IO2)
    pExternalOutput[1]->println("OLED (SPI) OK");                                   // test OLED output
#endif                                  

#if WITH_OLED_HW_I2C                                    
    u8x8_i2c.begin();                                                               // initialize OLED object                                                                   
    u8x8_i2c.setFont(u8x8_font_5x7_f);                                              // set font
    u8x8log_i2c.begin(u8x8_i2c, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer_i2c);       // initialize OLED text window object, connect to U8x8, set character size and assign memory
    u8x8log_i2c.setRedrawMode(0);                                                   // set the U8x8log redraw mode. 0: Update screen with newline, 1: Update screen for every char

    pExternalOutput[2] = static_cast<Print*> (&u8x8log_i2c);                        // add I2C OLED to available Justina output streams (in Justina IO commands, this will be stream IO3)
    pExternalOutput[2]->println("OLED (I2C) OK");                                   // test OLED output
#endif


    // run interpreter (control will stay there until you quit Justina)
    justina.begin();
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    // empty loop()
}
