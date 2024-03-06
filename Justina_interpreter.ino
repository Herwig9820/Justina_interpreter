/***************************************************************************************
    Justina interpreter on Arduino Nano 33 IoT working as TCP server.

    Version:    v1.00 - xx/xx/2022
    Author:     Herwig Taveirne

    Purpose: demonstrate the Justina interpreter application
             running on a nano 33 IoT board running as TCP server

    Both the Justina interpreter and the TCP server (and client) software are
    available as libraries.
    See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter

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

#define WITH_TCPIP 0
#define WITH_OLED_SW_SPI 0              // note: hw SPI interferes with SD card breakout box (SD card gets corrupted) -> use SW SPI
#define WITH_OLED_HW_I2C 0              

// includes
// --------
#include "Justina.h"

// oled display
// https://github.com/olikraus/u8g2   
#if WITH_OLED_SW_SPI || WITH_OLED_SW_I2C
#include <U8g2lib.h>
#endif

#if WITH_TCPIP
#include "secrets.h"
#include "TCPclientServer.h"
#endif


// Global constants, variables and objects
// ---------------------------------------

// used I/O pins
// NOTE: SPI is used for data transfer with the optional SD card, and SPI pins 11, 12, 13 are not available for general I/O
//       The chip select pin for the SD card can be freely chosen. If not specified, the default (pin 10) will be used 

constexpr int KILL_PIN{ 3 };            // INPUTS
constexpr int STOP_ABORT_PIN{ 4 };

constexpr int DATA_IO_PIN{ 5 };   // OUTPUTS
constexpr int STATUS_A_PIN{ 6 };
constexpr int STATUS_B_PIN{ 7 };
constexpr int ERROR_PIN{ 8 };

constexpr int HEARTBEAT_PIN{ 9 };                                                // indicator leds

#if WITH_TCPIP

#if defined ESP32
constexpr int WiFi_CONNECTED_PIN{ 17 };
constexpr int TCP_CONNECTED_PIN{ 18 };
#else
constexpr int WiFi_CONNECTED_PIN{ 14 };
constexpr int TCP_CONNECTED_PIN{ 15 };
#endif

constexpr char SSID[] = SERVER_SSID, PASS[] = SERVER_PASS;                            // WiFi SSID and password                           
// connect as TCP server: create class object myTCPconnection
TCPconnection myTCPconnection(SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort, conn_4_TCP_connected);

constexpr char menu[] = "+++ Please select:\r\n  'H' Help\r\n  '0' Disable WiFi\r\n  '1' (Re-)start WiFi\r\n  '2' Enable TCP\r\n  '3' stop TCP\r\n  '4' Disable TCP\r\n  '5' Verbose TCP\r\n  '6' Silent TCP\r\n  '7' Print connection state\r\n  'J' Start Justina interpreter\r\n";

#else
constexpr char menu[] = "+++ Please select:\r\n  'J' Start Justina interpreter\r\n";
#endif

// OLED displays

// Define the dimension of the U8x8log window
#if WITH_OLED_SW_SPI || WITH_OLED_HW_I2C
#define U8LOG_WIDTH 16
#define U8LOG_HEIGHT 8
#endif


#if WITH_OLED_SW_SPI 
// although same physical pins, nano ESP32 pin numbering is different from other nano boards 
#if defined ESP32
constexpr int VMA437_OLED_CS_PIN{ 19 };
constexpr int VMA437_OLED_DC_PIN{ 20 };
constexpr int VMA437_OLED_CLK_PIN{ 23 };
constexpr int VMA437_OLED_MOSI_PIN{ 24 };
#else
constexpr int VMA437_OLED_CS_PIN{ 16 };
constexpr int VMA437_OLED_DC_PIN{ 17 };
constexpr int VMA437_OLED_CLK_PIN{ 20 };
constexpr int VMA437_OLED_MOSI_PIN{ 21 };
#endif

U8X8_SH1106_128X64_NONAME_4W_SW_SPI u8x8_spi(VMA437_OLED_CLK_PIN, VMA437_OLED_MOSI_PIN, VMA437_OLED_CS_PIN, VMA437_OLED_DC_PIN);  // SW SPI
U8X8LOG u8x8log_spi;        // Create a U8x8log object
uint8_t u8log_buffer_spi[U8LOG_WIDTH * U8LOG_HEIGHT];                                       // display width x height, in characters
#endif

#if WITH_OLED_HW_I2C
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8_i2c;
U8X8LOG u8x8log_i2c;        // Create a U8x8log object
uint8_t u8log_buffer_i2c[U8LOG_WIDTH * U8LOG_HEIGHT];                                       // display width x height, in characters
#endif

// NOTE (cfr. next line): OLED HW SPI is not compatible with SD card HW SPI
//U8X8_SH1106_128X64_NONAME_4W_HW_SPI u8x8(VMA437_OLED_CS_PIN, VMA437_OLED_DC_PIN);  




// base class (Print) pointer naar derived class object voor virtual print methods: OK


// Allocate static memory for the U8x8log window

// (Stream) pAltInput[0] & (Print) pAltOutput[0] are the default input and output channels for Justina
// higher array elements can be set to additional input or output devices (before calling Justina)

Stream* pAltInput[4]{ &Serial, nullptr, nullptr , nullptr };                                                            // alternative input ports
Print* pAltOutput[4]{ &Serial, nullptr, nullptr , nullptr };                                                            // alternative output ports

constexpr int terminalCount{ sizeof(pAltInput) / sizeof(pAltInput[1]) };

#if WITH_TCPIP
int TCPstreamBits{};                                                                // set of 4 bits, indicating TCP streams in the set of external streams  
connectionState_type _connectionState{ conn_0_wifi_notConnected };
#endif

Justina* pJustina{ nullptr };                                                    // pointer to Justina object

bool withinApplication{ false };                                                       // init: currently not within an application
bool interpreterInMemory{ false };                                                     // init: interpreter is not in memory

#if defined ESP32
int SD_CHIP_SELECT_PIN{ 10 };           // predefined in library for other boards
#endif 

unsigned long heartbeatPeriod{ 1000 };                                               // do not go lower than 500 ms

void heartbeat();
void execAction(char c);



//--------------------------------------
// >>> cpp functions external to Justina
//--------------------------------------

// forward declarations of cpp functions that are external to Justina (external cpp functions)
// the function parameters must be as shown below (please check out an example provided as part of the documentation)
// parameters:
//   - const void** pdata: array of pointers to Justina variables or constants. Justina variables can be changed from within the external cpp. Justina constants cannot: a pointer to a COPY of the constant is provided
//   - const char* valueType: array indicating the value type (long, float, char*) of the respective arguments provided
//   - the number of arguments provided
// the last 2 parametes can be used in case the arguments value types and/or the argument count are not known by the external cpp procedure

bool userFcn_returnBool(void** const pdata, const char* valueType, const int argCount, int& execError);
char userFcn_returnChar(void** const pdata, const char* const valueType, const int argCount, int& execError);
int  userFcn_returnInt(void** const pdata, const char* const valueType, const int argCount, int& execError);
long userFcn_returnLong(void** const pdata, const char* const valueType, const int argCount, int& execError);
long userFcn_returnLong_2(void** const pdata, const char* const valueType, const int argCount, int& execError);
float userFcn_returnFloat(void** const pdata, const char* const valueType, const int argCount, int& execError);
char* userFcn_return_pChar(void** const pdata, const char* const valueType, const int argCount, int& execError);

void userFcn_readPort(void** const pdata, const char* const valueType, const int argCount, int& execError);
void userFcn_writePort(void** const pdata, const char* const valueType, const int argCount, int& execError);
void userFcn_togglePort(void** const pdata, const char* const valueType, const int argCount, int& execError);


// for each user-defined (external cpp) function or command, a record must be created in one of the array variables below (depending on the function return value type)
// a record consists of 4 fields: 
//   - alias (adhering to Justina identifier naming convention) that will be used from within Justina to call the function or command, just like any other (internal cpp or Justina) function or Justina command
//   - cpp function / procedure name
//   - minimum and maximum number of arguments allowed in calls made by Justina. Absolute maximum is 8. If more arguments are provided, they will be discarded

// each array variable below (cppBoolFunctions, ...) must contain at least 1 entry and less than 256 entries 
// if no entries of a specific category, add at least one entry consisting of an empty string and a nullptr as Justina name and cpp function pointer, as follows: {"", nullptr, 0, 0} 
// entries with invalid Justina names or with a null pointer as c++ function pointer will be skipped

// maximum number of arguments is 8. If more arguments are provided by external cpp function, they will be discarded

Justina::CppBoolFunction const cppBoolFunctions[]{
    { "returnBool", userFcn_returnBool, 0,1 }
};

Justina::CppCharFunction const cppCharFunctions[]{
    { "returnChar", userFcn_returnChar, 0, 0 }
};

Justina::CppIntFunction const cppIntFunctions[]{
    { "returnInt", userFcn_returnInt, 0, 0 }
};

Justina::CppLongFunction const cppLongFunctions[]{
    { "returnLong1", userFcn_returnLong, 1, 1},
    { "returnLong2", userFcn_returnLong_2, 0, 8}
};

Justina::CppFloatFunction const cppFloatFunctions[]{
    { "returnFloat", userFcn_returnFloat, 0, 2},
};

Justina::Cpp_pCharFunction const cpp_pCharFunctions[]{
    {"return_pChar",userFcn_return_pChar, 1, 1}
};

// cpp procedures not returning a function result: a zero will be returned to Justina 

Justina::CppVoidFunction  const cppVoidFunctions[]{                   // NOTE: min. and max. argument count is not used for user commands
    {"readPort", userFcn_readPort, 0, 0},
    {"writePort", userFcn_writePort, 0, 0},
    {"togglePort", userFcn_togglePort, 0, 0}
};

// >>> ----------------------------------------------------------------------------------------------------


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);

    // define output pins
    pinMode(HEARTBEAT_PIN, OUTPUT);                                                   // blinking led for heartbeat
    pinMode(ERROR_PIN, OUTPUT);
    pinMode(STATUS_A_PIN, OUTPUT);
    pinMode(STATUS_B_PIN, OUTPUT);
    pinMode(DATA_IO_PIN, OUTPUT);

    pinMode(STOP_ABORT_PIN, INPUT_PULLUP);
    pinMode(KILL_PIN, INPUT_PULLUP);


#if WITH_TCPIP
    pinMode(WiFi_CONNECTED_PIN, OUTPUT);                                              // 'TCP connected' led
    pinMode(TCP_CONNECTED_PIN, OUTPUT);                                               // 'TCP connected' led
#endif

    bool ledState{ 0 };
    int loopCount{ 0 };
    do {
        ledState = !ledState;
        loopCount++;
        if (loopCount == 6) { break; }            // wait minimum 3 seconds (a non-native USB port will always return 'true')
        else { delay(500); }
    } while (true);

#if WITH_OLED_SW_SPI
    u8x8_spi.begin();
    u8x8_spi.setFont(u8x8_font_chroma48medium8_r);
    u8x8log_spi.begin(u8x8_spi, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer_spi);           // Start U8x8log, connect to U8x8, set the dimension and assign the static memory
    u8x8log_spi.setRedrawMode(0);		                                                // Set the U8x8log redraw mode. 0: Update screen with newline, 1: Update screen for every char  
#endif

#if WITH_OLED_HW_I2C
    u8x8_i2c.begin();
    u8x8_i2c.setFont(u8x8_font_chroma48medium8_r);
    u8x8log_i2c.begin(u8x8_i2c, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer_i2c);
    u8x8log_i2c.setRedrawMode(0);
#endif

#if WITH_TCPIP
    Serial.println("\r\nStarting TCP server");
#if !defined ESP32
    Serial.print("WiFi firmware version  "); Serial.println(WiFi.firmwareVersion()); Serial.println();
#endif
    myTCPconnection.setVerbose(false);                                                // disable debug messages from within myTCPconnection
    myTCPconnection.setKeepAliveTimeout(20 * 60 * 1000);                                // 20 minutes TCP keep alive timeout
    Serial.println("On the remote terminal, press ENTER to connect\r\n");

    pAltInput[1] = static_cast<Stream*>(myTCPconnection.getClient());     // Justina: stream number -2 is TCP client (alt streams 0..2 => stream numbers -1..-3)
    pAltOutput[1] = static_cast<Print*>(myTCPconnection.getClient());     // Justina: stream number -2 is TCP client (alt streams 0..2 => stream numbers -1..-3)

    TCPstreamBits = 0b0010;                         // set of 4 bits, indicating TCP streams in the set of external streams (b0: stream -1 -> b3: stream -4)
#endif


#if WITH_OLED_SW_SPI
    pAltOutput[2] = static_cast<Print*> (&u8x8log_spi);
    pAltOutput[2]->println("OLED (SPI) OK");
#endif

#if WITH_OLED_HW_I2C
    pAltOutput[3] = static_cast<Print*> (&u8x8log_i2c);
    pAltOutput[3]->println("OLED (I2C) OK");
#endif


    ////if (!lcd.begin(20, 4)) { Serial.println("*** LCD niet gevonden ***\r\n"); }
    ////else { lcd.println("hello Herwig"); }

    /*
    lcd.begin();                // Velleman
    delay(1000);
    lcd.println(" Hello World (^__^)");
    lcd.setCursor(0, 1);
    lcd.print("Velleman for makers");
    delay(5000);
    */

    // print sample / simple main menu for the user
    Serial.println(menu);
    Serial.print("Main> ");

#if defined RTClock
    SdFile::dateTimeCallback((dateTime));
#endif
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    heartbeat();                                                                        // blink a led to show program is running 
#if WITH_TCPIP
    maintainTCP(false);
#endif

    char c;

    // read character from remote terminal (TCP client), if a character is available
    c = Serial.read();
    if (c != 0xff) { execAction(c); }                                         // no character to process (also discard control characters)
}


// ---------------------------------------------------------------
// Perform action according to selected option (menu is displayed)
// ---------------------------------------------------------------

void execAction(char c) {
    bool printMenu{ false };

    bool isAction{ c > ' ' };
    if (isAction) { Serial.println(c); }

    switch (tolower(c)) {

    #if WITH_TCPIP
        // !!!!! NOTE: RP2040 MBED OS crashes if '0' or '1' menu options are entered twice in succession
        case '0':
            myTCPconnection.requestAction(action_0_disableWiFi, _connectionState);
            Serial.println("WiFi disabled");
            break;

        case '1':
            myTCPconnection.requestAction(action_1_restartWiFi, _connectionState);      // always
            Serial.println("(Re-)starting WiFi... this can take a moment");
            break;

        case '2':
            myTCPconnection.requestAction(action_2_TCPkeepAlive, _connectionState);
            Serial.println("TCP enabled. If a connection is not yet established, on the remote terminal, press ENTER to connect");                                     // needs WiFi to be enabled and connected
            break;

        case '3':
            myTCPconnection.requestAction(action_3_TCPdisConnect, _connectionState);
            Serial.println("TCP client disconnected");
            break;

        case '4':
            myTCPconnection.requestAction(action_4_TCPdisable, _connectionState);
            Serial.println("TCP disabled");
            break;

        case '5':                                                                       // set TCP server to verbose
            myTCPconnection.setVerbose(true);
            Serial.println("TCP server: verbose");
            break;

        case '6':                                                                       // set TCP server to silent
            myTCPconnection.setVerbose(false);
            Serial.println("TCP server: silent");
            break;

        case '7':                                                                       // set TCP server to silent
            Serial.print("Connection state: "); Serial.print(_connectionState);
            if (_connectionState == conn_4_TCP_connected) {
                WiFiClient* client = static_cast<WiFiClient*>(myTCPconnection.getClient());
                IPAddress IP = client->remoteIP();
                char IPstring[16];
                sprintf(IPstring, "%d.%d.%d.%d", IP[0], IP[1], IP[2], IP[3]);
                Serial.print(", remote IP = "); Serial.print(IPstring);
            }
            Serial.println();
            break;

            break;
        #endif


        case 'j':
        #if !defined(ARDUINO_ARCH_SAMD) && !defined(ARDUINO_ARCH_RP2040) && !defined(ESP32)
            Serial.println("interpreter does not run on this processor");            // interpreter does not run on this processor
            break;
        #endif

            // start interpreter: control will not return to here until the user quits, because it has its own 'main loop'
            heartbeatPeriod = 200;
            withinApplication = true;                                                   // flag that control will be transferred to an 'application'

            if (!interpreterInMemory) {                                                 // if interpreter not running: create an interpreter object on the heap

                // create Justina object 
                pJustina = new  Justina(pAltInput, pAltOutput, terminalCount, Justina::SD_runStart, SD_CHIP_SELECT_PIN);

                // set callback function to avoid that maintaining the TCP connection AND the heartbeat function are paused as long as control stays in Justina
                // this passes the address of 'Justina_housekeeping' (in this file) to Justina
                // Once Justina is running, it will call this function regularly then
                pJustina->setMainLoopCallback((&Justina_housekeeping));                 

                //--------------------------------------------
                // >>> CPP user functions: pass entry points
                //--------------------------------------------

                // inform Justina about the location of the information stored about the user-defined (external) cpp functions and commands

                // if no entries of a specific category, comment out the specific entry (below), OR provide an empty record for that category, like this: {"", nullptr, 0, 0}

                pJustina->registerBoolUserCppFunctions(cppBoolFunctions, sizeof(cppBoolFunctions) / sizeof(cppBoolFunctions[0]));
                pJustina->registerCharUserCppFunctions(cppCharFunctions, sizeof(cppCharFunctions) / sizeof(cppCharFunctions[0]));
                pJustina->registerIntUserCppFunctions(cppIntFunctions, sizeof(cppIntFunctions) / sizeof(cppIntFunctions[0]));
                pJustina->registerLongUserCppFunctions(cppLongFunctions, sizeof(cppLongFunctions) / sizeof(cppLongFunctions[0]));
                pJustina->registerFloatUserCppFunctions(cppFloatFunctions, sizeof(cppFloatFunctions) / sizeof(cppFloatFunctions[0]));
                pJustina->register_pCharUserCppFunctions(cpp_pCharFunctions, sizeof(cpp_pCharFunctions) / sizeof(cpp_pCharFunctions[0]));
                pJustina->registerVoidUserCppFunctions(cppVoidFunctions, sizeof(cppVoidFunctions) / sizeof(cppVoidFunctions[0]));
                // >>> ---------------------------------------------------------------------------------------------
            }
            interpreterInMemory = pJustina->begin();                                   // run interpreter; on return, inform whether interpreter is still in memory (data not lost)

            if (!interpreterInMemory) {                                               // return from interpreter: remove from memory as well ?
                delete pJustina;                                                     // cleanup and delete calculator object itself
            }

            Serial.println("+++ Enter 'H' for Help\r\n");
            heartbeatPeriod = 500;
            withinApplication = false;                                                  // return from application
            break;

        case 'h':
            printMenu = true;
            break;

        default:
            if (c > ' ') {
                Serial.println("This is not a valid choice (enter 'H' for help)");
            }
            break;
    }
    if (printMenu) { Serial.println(menu); }                                                      // show menu again

    if (isAction) { Serial.print("Main> "); }
}

// --------------------------------------
// Blink a led to show program is running 
// --------------------------------------

void heartbeat() {
    // note: this is not a clock because it does not measure the passing of fixed time intervals
    // but the passing of minimum time intervals (the millis() function itself is a clock)

    static bool ledOn{ false };
    static uint32_t lastHeartbeat{ 0 };                                           // last heartbeat time in ms
    static uint32_t previousTime{ 0 };

    uint32_t currentTime = millis();
    // also handle millis() overflow after about 47 days
    if ((lastHeartbeat + heartbeatPeriod < currentTime) || (currentTime < previousTime)) {               // time passed OR millis() overflow: switch led state
        lastHeartbeat = currentTime;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_PIN, ledOn);
    }
    previousTime = currentTime;
}


// ----------------------------------------------------
// *   TCP library: maintain connection and handle leds
// ----------------------------------------------------

#if WITH_TCPIP
void maintainTCP(bool resetKeepAliveTimer) {
    myTCPconnection.maintainConnection(_connectionState, resetKeepAliveTimer);                                               // maintain TCP connection

    // control WiFi and TCP indicator leds
    // -----------------------------------

    static connectionState_type oldConnectionState{ conn_0_wifi_notConnected };
    static uint32_t lastLedChangeTime{ 0 };
    static bool TCPledState{ false };

    // TCP enabled and waiting for a client to connect ? blink 'TCP' led
    bool TCPwaitForConnect = (_connectionState == conn_3_TCP_waitForConnection);
    if (TCPwaitForConnect) {           // blink TCP led
        uint32_t currentTime = millis();
        // also handle millis() overflow after about 47 days
        if ((lastLedChangeTime + 200 < currentTime) || (currentTime < lastLedChangeTime)) {               // time passed OR millis() overflow: switch led state
            TCPledState = !TCPledState;
            digitalWrite(TCP_CONNECTED_PIN, TCPledState);
            lastLedChangeTime = currentTime;
        }
    }

    // set WiFi connected & TCP connected leds
    if (oldConnectionState != _connectionState) {
        bool WiFiConnected = (_connectionState != conn_0_wifi_notConnected) && (_connectionState != conn_1_wifi_waitForConnecton);
        digitalWrite(WiFi_CONNECTED_PIN, WiFiConnected);                                  // led indicates 'client connected' status

        if (_connectionState != conn_3_TCP_waitForConnection) {                   // do not interfere with blinking TCP led
            bool TCPconnected = (_connectionState == conn_4_TCP_connected);
            digitalWrite(TCP_CONNECTED_PIN, TCPconnected);                        // led indicates 'client connected' status
        }
    }

    oldConnectionState = _connectionState;
}
#endif


// ------------------------------------------------------------
// *   SD library callback function to adapt file date and time
// ------------------------------------------------------------

// this callback function is called by the SD library

#if defined RTClock
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
#endif

// ---------------------------------------------------------------------------------------------------------------------------------------------
// debounce keys and return key state, key goes down, key goes up, is short press (when key goes up), is long press (while key is still pressed)
// ---------------------------------------------------------------------------------------------------------------------------------------------

void keyStates(uint8_t pinStates, uint8_t& debounced, uint8_t& wentDown, uint8_t& wentUp, uint8_t& isShortPress, uint8_t& isLongPress) {

    // constants
    static constexpr int keyCount = 2;
    static constexpr long debounceTime = 10;           // in ms
    static constexpr long alternateActionTime = 1500;     // in ms

    // static variables
    static long pinChangeTimes[keyCount]{ millis(), millis() };
    static uint8_t oldPinStates{ 0xff };                      // init: assume keys are up
    static uint8_t debouncedStates{ 0xff };
    static uint8_t oldDebouncedStates{ 0xff };                                                      // init: assume keys are up
    static uint8_t enableKeyActions{ 0xff };

    // debounce keys (normally not necessary, because interval between successive callback functions is large enough) 
    long now = millis();
    uint8_t pinChanges = pinStates ^ oldPinStates;                           // flag pin changes since previous sample

    for (int i = 0; i < keyCount; i++) {
        uint8_t pinMask = (1 << i);
        if (pinChanges & pinMask) { pinChangeTimes[i] = now; }        // pin change ? note the time
        // pins stable for more than minimum debounce time ? change debounced state
        else if (pinChangeTimes[i] + debounceTime < now) { debouncedStates = (debouncedStates & ~pinMask) | (pinStates & pinMask); }
    }
    oldPinStates = pinStates;

    // determine key actions
    uint8_t doPrimaryKeyActions{ false }, doAlternateKeyActions{ false };
    uint8_t debouncedStateChanges = debouncedStates ^ oldDebouncedStates;                           // flag debounced state changes since previous sample

    for (int i = 0; i < keyCount; i++) {
        uint8_t pinMask = (1 << i);
        bool keyGoesDown = debouncedStateChanges & (~debouncedStates) & pinMask;
        bool keyGoesUp = debouncedStateChanges & debouncedStates & pinMask;
        bool keyIsDown = (~debouncedStates) & pinMask;

        if (keyGoesDown) { enableKeyActions |= pinMask; }
        else if (keyGoesUp) {
            if ((enableKeyActions & pinMask) && (pinChangeTimes[i] + alternateActionTime >= now)) { doPrimaryKeyActions |= pinMask; }
        }
        else if (keyIsDown) {
            if ((enableKeyActions & pinMask) && (pinChangeTimes[i] + alternateActionTime < now)) { doAlternateKeyActions |= pinMask; enableKeyActions &= ~pinMask; }
        }
    }
    oldDebouncedStates = debouncedStates;

    // return values
    debounced = debouncedStates; wentDown = debouncedStateChanges & (~debouncedStates); wentUp = debouncedStateChanges & debouncedStates;
    isShortPress = doPrimaryKeyActions; isLongPress = doAlternateKeyActions;
    return;
}



// -----------------------------------------------------------------------------------------------------------------------------
// *   callback function to be called at regular intervals from any application not returning immediately to Arduino main loop()
// -----------------------------------------------------------------------------------------------------------------------------

// This callback function is used to avoid that specific actions are paused while control stays in an application for a longer period
// (1) maintaining the TCP connection (we don't want an application to have knowledge about where it gets its input and sends its output)
// (2) maintaining the heartbeat 
// (3) if the console is currently remote terminal: continue to provide a mechanism to gain back local control while in an application, 
//     e.g. if remote connection (TCP) is lost  

// in this program, this callback function is called at regular intervals from within the interpreter main loop  

void Justina_housekeeping(long& appFlags) {

    heartbeat();                                                                        // blink a led to show program is running
    // request kill if debounced kill key press is detected 
    // request stop if debounced stop/abort key release is detected AND debounced key down time is less than the defined alternate function time
    // request abort if debounced stop/abort key down time is equal or more than the defined 'alternate function' time
    // --------------------------------------------------------------------------------------------------------------------------------------------

    static bool errorCondition = false, statusA = false, statusB = false, dataInOut = false;

    uint8_t debouncedStates, wentDown, wentUp, isShortPress, isLongPress;
    uint8_t pinStates = (digitalRead(STOP_ABORT_PIN) << 1) + digitalRead(KILL_PIN);
    keyStates(pinStates, debouncedStates, wentDown, wentUp, isShortPress, isLongPress); // wentDown, wentUp, isShortPress, isLongPress: all one shot

    // application flags: submit to Justina 
    appFlags = (appFlags & ~Justina::appFlag_requestMask);      // reset requests
    // short press: key goes up (edge) while time is less than threshold
    // long press: time passes threshold (edge) while key is still down
    if (isShortPress & (1 << 0)) { appFlags |= Justina::appFlag_consoleRequestBit; }
    if (isLongPress & (1 << 0)) { appFlags |= Justina::appFlag_killRequestBit; }
    if (isShortPress & (1 << 1)) { appFlags |= Justina::appFlag_stopRequestBit; }
    else if (isLongPress & (1 << 1)) { appFlags |= Justina::appFlag_abortRequestBit; }

    // application flags: receive flags from Justina and set indicator leds (only set on/off if change detected)
    if (errorCondition ^ (appFlags & Justina::appFlag_errorConditionBit)) { errorCondition = (appFlags & Justina::appFlag_errorConditionBit);  digitalWrite(ERROR_PIN, errorCondition); }  // only write if change detected
    if (statusA ^ (appFlags & Justina::appFlag_statusAbit)) { statusA = (appFlags & Justina::appFlag_statusAbit);  digitalWrite(STATUS_A_PIN, statusA); }
    if (statusB ^ (appFlags & Justina::appFlag_statusBbit)) { statusB = (appFlags & Justina::appFlag_statusBbit);  digitalWrite(STATUS_B_PIN, statusB); }

    bool newDataLedState{ false };
    static bool dataLedState{ false };
    if (appFlags & Justina::appFlag_dataInOut) { newDataLedState = !dataLedState; }
    else { newDataLedState = false; }      // if data, toggle state, otherwise reset state
    if (newDataLedState != dataLedState) { dataLedState = newDataLedState;  digitalWrite(DATA_IO_PIN, dataLedState); }  // only write if change detected

#if WITH_TCPIP
    // appFlag bits 19-16 signal that Justina READ data from external IO streams -1 to -4
    // TCPstreamBits bits 3-0: set of 4 bits, indicating which streams in the set of external streams are TCP streams (b0: stream -1 -> b3: stream -4)
    // if characters sent by a TCP stream were read, this resets the TCP stream keepalive timer (NOT related to keepalive in a HTTP protocol) 
    maintainTCP(bool(appFlags & (TCPstreamBits << 16)));                                                // maintain TCP connection
#endif
}


// -----------------------------
// user callback functions: demo
// -----------------------------

// a callback function is a mechanism to allow a library (like Justina) to call a procedure in a user program without having any knowledge about the name of the procedure
// in Justina, the mechanism is used to allow the user to write specific procedures in C++ (not in Justina) and call them afterwards from within Justina
// 
// a user callback function should contain 3 parameters, as shown below
// parameter 1 (const void** pdata) is an array containing void pointers to a maximum of 8 arguments, passed by reference by Justina 
// parameter 2 (const char* valueType) is an array indicating the value type (long, float or char*), and whether the data is a Justina variable or constant
// parameter 3 (const int argCount) contains the number of arguments passed
// if data is present, the corresponding pointer passed will point to an integer, float or text (char*).
// the value pointed to can be the value stored in a Justina scalar variable or array element, or it can be a Justina constant 
// the data pointed to can always be changed, but changing a Justina constant or a Justina variable declared as constant, will have no effect once control returns to Justina
// in case the value pointed to is a LONG or a FLOAT stored in an ARRAY element, you actually have access to the complete array by setting a pointer to subsequent or preceding array elements
// if the value pointed to is a character string (char*), changing the pointer allows you to access all characters in the string
// 
// refer to Justina documentation to learn how to call a user procedure ('callback') from Justina and change the maximum number allowed
// 
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!                                                                                                                                   !!
// !! when returning changed values                                                                                                     !!
// !! - NEVER change the value type (float, character string)                                                                           !!
// !! - NEVER INCREASE the length of strings (you can change the characters in the string, however)                                     !! 
// !!   -> empty strings can not be changed at all (this would increase the length of the string)                                       !!
// !! - it is allowed to DECREASE the length of a string, but keep in mind that it will still occupy the same amount of memory          !!
// !!   -> exception: if you change it to an empty string, memory will be released (in Justina, an empty string is just a null pointer) !!
// !!                                                                                                                                   !!
// !! do NOT directly change the arguments received. However you cpointers in array 'pdata'                                             !!
// !! but you can change the data supplied (this will be without any effect for Justina constants after return)                         !!
// !!                                                                                                                                   !!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


// --------------------------------------------------
// example: show how to read and modify data supplied
// --------------------------------------------------

void userFcn_readPort(void** const pdata, const char* const valueType, const int argCount, int& execError) {     // data: can be anything, as long as user function knows what to expect

    pAltOutput[0]->print("=== control is now in user c++ callback function: arg count = "); pAltOutput[0]->println(argCount);

    for (int i = 0; i < argCount; i++) {
        // data available ?

        long* pLong{};          // pointer to long
        float* pFloat{};          // pointer to float
        char* pText{};          // character pointer

        // get value type and variable / constant info
        bool isLong = ((valueType[i] & 0x03) == 0x01);                                  // bits 2-0: value indicates value type (1=long, 2=float, 3=char*) 
        bool isFloat = ((valueType[i] & 0x03) == 0x02);
        bool isString = ((valueType[i] & 0x03) == 0x03);
        bool isVariable = (valueType[i] & 0x80);                                        // bit b7: '1' indicates 'variable', '0' means 'constant'

        // get a (pointer to a) value
        if (isLong) { pLong = (long*)pdata[i]; }                                        // copy a pointer to a long argument 
        else if (isFloat) { pFloat = (float*)pdata[i]; }                                // copy a pointer to a float argument
        else { pText = (char*)pdata[i]; }                                               // copy a pointer to a character string argument

        // change data (is safe -> after return, will have no effect for constants) - you can always check here for variable / constant (see above)
        if (isLong) { *pLong += 10 + i; }
        else if (isFloat) { *pFloat += 20. + i; }
        else {
            if (strlen(pText) >= 10) { pText[7] = '\0'; }  // do NOT increase the length of strings
            if (strlen(pText) >= 5) { pText[3] = pText[4]; pText[4] = '>'; }  // do NOT increase the length of strings
            else if (strlen(pText) >= 2) { pText[0] = '\0'; }       // change non-empty string into empty string 
            else if (strlen(pText) == 0) {}        // it is NOT allowed to increase the length of a string: you cannot change an empty string
        }

        // print a value
        pAltOutput[0]->print("    adapted value (argument "); pAltOutput[0]->print(i); pAltOutput[0]->print(") is now: ");      // but value
        if (isLong) { pAltOutput[0]->println(*pLong); }
        else if (isFloat) { pAltOutput[0]->println(*pFloat); }
        else { pAltOutput[0]->println(pText); }

    };
    pAltOutput[0]->println("=== leaving user c++ callback function");
}


// --------------------------------------
// example: a few other callback routines
// --------------------------------------

void userFcn_writePort(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    pAltOutput[0]->println("*** Justina was here ***");
    // do your thing here

};


void userFcn_togglePort(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    pAltOutput[0]->println("*** Justina just passed by ***");
    // do your thing here
};


// -----------------------------------------------
// >>> user cpp functions
// -----------------------------------------------
bool userFcn_returnBool(void** const pdata, const char* const valueType, const int argCount, int& execError) { return 123; }

char userFcn_returnChar(void** const pdata, const char* const  valueType, const int argCount, int& execError) { execError = 1234; return 'X'; }

int userFcn_returnInt(void** const pdata, const char* const valueType, const int argCount, int& execError) { return (int)-987; }

long userFcn_returnLong(void** const pdata, const char* const valueType, const int argCount, int& execError) { return (*(long*)pdata[0]) * 10; }

long userFcn_returnLong_2(void** const pdata, const char* const valueType, const int argCount, int& execError) { return 456; }

float userFcn_returnFloat(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    Serial.println("*** within 'userFcn_return float'");

    bool isLong = ((valueType[1] & 0x03) == 0x01);                                  // bits 2-0: value indicates value type (1=long, 2=float, 3=char*) 
    bool isFloat = ((valueType[1] & 0x03) == 0x02);
    float value = isLong ? (float)(*((long*)pdata[1])) + 10 : isFloat ? (*(float*)pdata[1]) + 20 : 1.23;
    return   value;
}

char* userFcn_return_pChar(void** const pdata, const char* const  valueType, const int argCount, int& execError) {

    if ((valueType[0] & Justina::value_typeMask) != Justina::value_isStringPointer) { Serial.println("user function: wrong value type"); return ""; }

    // NOTES: If you create a NEW char* object and return it, make sure you DELETE it later (also in a cpp user routine) in order not to create a memory leak.
    //        Do NOT return a local char* because it will go out of scope upon return, freeing the memory occupied by the char*.
    //        It is OK to return a global char* declared in this file. 
    //        Always respect the initial length of a char* : you may shorten the string stored in it, but you can't make it longer than the initial string.

    char* pText = ((char**)(pdata))[0];
    if (strlen(pText) < 3) { Serial.println("user function: string too short"); return ""; }

    Serial.print("in routine  test4: string is "); Serial.println(pText);
    Serial.print("               char[0] =  is "); Serial.println(pText[0]);
    Serial.print("               char[1] =  is "); Serial.println((pText[1]));

    pText[0] = 'Y';
    pText[1] = 'Z';
    Serial.print("               char[0] changed to "); Serial.println(pText[0]);
    Serial.print("               char[1] changed to "); Serial.println(pText[1]);

    bool isVariable = (valueType[0] & 0x80);                                        // bit b7: '1' indicates 'variable', '0' means 'constant'

    return ((char**)(pdata))[0];
}
// >>> --------------------------------------------------------------------------------------------------------------------
