/***************************************************************************************
    Justina interpreter on Arduino Nano 33 IoT working as TCP server.

    Version:    v1.00 - xx/xx/2022
    Author:     Herwig Taveirne

    Purpose: demonstrate the Justina interpreter application
             running on a nano 33 IoT board running as TCP server

    Both the Justina interpreter and the TCP server (and client) software are
    available as libraries.
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

#define withTCP 1


// includes
// --------

#include "Justina.h"
#include <avr/dtostrf.h>        

#if withTCP
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
constexpr int STATUS_B_PIN{ 6 };
constexpr int STATUS_A_PIN{ 7 };
constexpr int ERROR_PIN{ 8 };

constexpr int HEARTBEAT_PIN{ 9 };                                                // indicator leds

#if withTCP
constexpr pin_size_t WiFi_CONNECTED_PIN{ 14 };
constexpr pin_size_t TCP_CONNECTED_PIN{ 15 };

constexpr char SSID[] = SERVER_SSID, PASS[] = SERVER_PASS;                            // WiFi SSID and password                           
// connect as TCP server: create class object myTCPconnection
TCPconnection myTCPconnection(SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort, conn_2_TCPconnected);

void onConnStateChange(connectionState_type  connectionState);
constexpr char menu[] = "+++ Please select:\r\n  'H' Help\r\n  '0' (Re-)start WiFi\r\n  '1' Disable WiFi\r\n  '2' Enable TCP\r\n  '3' Disable TCP\r\n  '4' Verbose TCP\r\n  '5' Silent TCP\r\n  'J' Start Justina interpreter\r\n";

#else
constexpr char menu[] = "+++ Please select:\r\n  'J' Start Justina interpreter\r\n";
#endif

bool withinApplication{ false };                                                       // init: currently not within an application
bool interpreterInMemory{ false };                                                     // init: interpreter is not in memory

bool errorCondition = false, statusA = false, statusB = false, dataInOut = false;

Stream* pAlternativeIO[3]{ static_cast<Stream*>(&Serial) ,                                                                  // alternative IO ports, if defined
                        static_cast<Stream*>(&Serial) ,
                        static_cast<Stream*>(&Serial) };
constexpr int terminalCount{ 3 };

Justina_interpreter* pJustina{ nullptr };                                                    // pointer to Justina_interpreter object

#ifdef ARDUINO_ARCH_RP2040
long progMemSize = pow(2, 16);
#else
long progMemSize = 2000;
#endif 

unsigned long heartbeatPeriod{ 500 };                                               // do not go lower than 500 ms
void heartbeat();
void execAction(char c);

// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(1000000);

    // define output pins
    pinMode(HEARTBEAT_PIN, OUTPUT);                                                   // blinking led for heartbeat
    pinMode(ERROR_PIN, OUTPUT);
    pinMode(STATUS_A_PIN, OUTPUT);
    pinMode(STATUS_B_PIN, OUTPUT);
    pinMode(DATA_IO_PIN, OUTPUT);

    pinMode(STOP_ABORT_PIN, INPUT_PULLUP);
    pinMode(KILL_PIN, INPUT_PULLUP);

#if withTCP
    pinMode(WiFi_CONNECTED_PIN, OUTPUT);                                              // 'TCP connected' led
    pinMode(TCP_CONNECTED_PIN, OUTPUT);                                               // 'TCP connected' led
#endif

    digitalWrite(HEARTBEAT_PIN, HIGH);                                                // while waiting for Serial to be ready
    digitalWrite(ERROR_PIN, HIGH);
    long tStart = millis();  //// check op overflow
    while (!Serial);                                                                    // native USB port only 
    long tEnd = millis();
    digitalWrite(ERROR_PIN, LOW);                                                      // high during wait for serial
    if (tEnd - tStart < 2000) { delay(2000 - (tEnd - tStart)); }
    digitalWrite(HEARTBEAT_PIN, LOW);                                                   // high during (minimum) 2 seconds                                                      

#if withTCP
    Serial.println("Starting TCP server");
    Serial.print("WiFi firmware version  "); Serial.println(WiFi.firmwareVersion()); Serial.println();
    myTCPconnection.setVerbose(false);                                                // disable debug messages from within myTCPconnection
    myTCPconnection.setKeepAliveTimeout(20 * 60 * 1000);                                // 20 minutes
    Serial.println("On the remote terminal, press ENTER to connect\r\n");
    // set callback function that will be executed when WiFi or TCP connection state changes 
    myTCPconnection.setConnCallback((&onConnStateChange));                            // set callback function
    // stream pAlternativeIO[0] is the default (input and output) console for Justina
    // from within Justina, all alternative IO streams can be used for IO, AND can be set as Justina console (input and output)
    pAlternativeIO[1] = static_cast<Stream*>(myTCPconnection.getClient());     // Justina: stream number -2 is TCP client (alt streams 0..2 => stream numbers -1..-3)
#endif

    // not functionaly used, but required to circumvent a bug in sprintf function with %F, %E, %G specifiers 
    char s[10];
    dtostrf(1.0, 4, 1, s);   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers    //// nog nodig ???

    // print sample / simple main menu for the user
    Serial.println(menu);

#ifdef RTClock
    SdFile::dateTimeCallback((dateTime));
#endif
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    heartbeat();                                                                        // blink a led to show program is running 
#if withTCP
    myTCPconnection.maintainConnection();                                               // maintain TCP connection
#endif

    char c;

    // read character from remote terminal (TCP client), if a character is available
    c = Serial.read();
    if (c != 0xff) {
        execAction(c);                                           // no character to process (also discard control characters)
        delay(100);
        while (Serial.available() > 0) { Serial.read(); }       // empty character buffer
    }
}


// ---------------------------------------------------------------
// Perform action according to selected option (menu is displayed)
// ---------------------------------------------------------------

void execAction(char c) {
    bool printMenu{ true };
    switch (tolower(c)) {

    #if withTCP
        // !!!!! NOTE: RP2040 MBED OS crashes if '0' or '1' menu options are entered twice in succession
        case '0':
            myTCPconnection.requestAction(action_1_restartWiFi);
            Serial.println("(Re-)starting WiFi...");
            break;

        case '1':
            myTCPconnection.requestAction(action_0_disableWiFi);
            Serial.println("Disabling WiFi...");
            break;

        case '2':
            myTCPconnection.requestAction(action_2_TCPkeepAlive);
            Serial.println("Enabling TCP...");                                     // needs WiFi to be enabled and connected
            Serial.println("On the remote terminal, press ENTER to connect");
            break;

        case '3':
            myTCPconnection.requestAction(action_4_TCPdisable);
            Serial.println("Disabling TCP...");
            break;

        case '4':                                                                       // set TCP server to verbose
            myTCPconnection.setVerbose(true);
            Serial.println("TCP server: verbose");
            break;

        case '5':                                                                       // set TCP server to silent
            myTCPconnection.setVerbose(false);
            Serial.println("TCP server: silent");
            break;

        #endif
        case 'j':
        #if !defined(ARDUINO_SAMD_NANO_33_IOT) && !defined(ARDUINO_ARCH_RP2040)
            Serial.println("interpreter does not run on this processor");            // interpreter does not run on this processor
            break;
        #endif

            // start interpreter: control will not return to here until the user quits, because it has its own 'main loop'
            heartbeatPeriod = 200;
            withinApplication = true;                                                   // flag that control will be transferred to an 'application'
            if (!interpreterInMemory) {
                pJustina = new  Justina_interpreter(pAlternativeIO, terminalCount, progMemSize);         // if interpreter not running: create an interpreter object on the heap

                // set callback function to avoid that maintaining the TCP connection AND the heartbeat function are paused as long as control stays in the interpreter
                // this callback function will be called regularly, e.g. every time the interpreter reads a character
                pJustina->setMainLoopCallback((&Justina_housekeeping));                    // set callback function to Justina_housekeeping routine in this .ino file (pass 'Justina_housekeeping' routine address to Justina_interpreter library)

                pJustina->setUserFcnCallback((&userFcn_readPort));                // pass user function addresses to Justina_interpreter library (return value 'true' indicates success)
                pJustina->setUserFcnCallback((&userFcn_writePort));
                pJustina->setUserFcnCallback((&userFcn_togglePort));
            }
            interpreterInMemory = pJustina->run();                                   // run interpreter; on return, inform whether interpreter is still in memory (data not lost)

            if (!interpreterInMemory) {                                               // return from interpreter: remove from memory as well ?
                delete pJustina;                                                     // cleanup and delete calculator object itself
                pJustina = nullptr;                                                  // only to indicate memory is released
            }

            heartbeatPeriod = 500;
            withinApplication = false;                                                  // return from application
            break;

        case 'h':
            printMenu = true;
            break;

        default:
            Serial.println("This is not a valid choice (enter 'H' for help)");
            printMenu = false;
            break;
    }
    if (printMenu) { Serial.println(menu); }                                                      // show menu again
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


#if withTCP

// ----------------------------------------------------------------------------
// *   Callback function executed when WiFi or TCP connection state changes   * 
// ----------------------------------------------------------------------------

// this callback routine is called from within TCPconnection.maintainConnection() at every change in connection state
// it allows the main program to take specific custom actions (in this case: printing messages and controlling a led)

void onConnStateChange(connectionState_type  connectionState) {
    static bool WiFiConnected{ false };
    static bool TCPconnected{ false };

    bool holdTCPconnected = TCPconnected;
    bool holdWiFiConnected = WiFiConnected;

    WiFiConnected = (connectionState == conn_1_wifiConnected) || (connectionState == conn_2_TCPconnected);
    digitalWrite(WiFi_CONNECTED_PIN, WiFiConnected);                                  // led indicates 'client connected' status 
    TCPconnected = (connectionState == conn_2_TCPconnected);
    digitalWrite(TCP_CONNECTED_PIN, TCPconnected);                                    // led indicates 'client connected' status 

    /* //// interfereert met Justina prompt : beter niet
    if (TCPconnected) { Serial.println("TCP connection established\r\n"); }                   // remote client just got connected: show on main terminal
    else if (holdTCPconnected) {                                                      // previous status was 'client connected'
        Serial.println("TCP connection lost or timed out");                   // inform local terminal about it
        Serial.println("On the remote terminal, press ENTER to reconnect");
    }
    */
}
#endif


// ------------------------------------------------------------
// *   SD library callback function to adapt file date and time
// ------------------------------------------------------------

// this callback function is called by the SD library

#ifdef RTClock
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
    static long pinChangeTimes[2]{ millis(), millis() };
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

    // bits 3-0: flags signaling specific Justina status conditions to caller
    constexpr long appFlag_errorConditionBit = 0x01L;       // Justina parsing or execution error has occured

    constexpr long appFlag_statusMask = 0x06L;               // status bits mask: status bits A and B
    constexpr long appFlag_statusAbit = 0x02L;               // status bits A and B: 4 different statuses (see below)
    constexpr long appFlag_statusBbit = 0x04L;
    constexpr long appFlag_idle = 0x00L;                     // idle status
    constexpr long appFlag_parsing = 0x02L;                  // parsing status
    constexpr long appFlag_executing = 0x04L;                // executing status
    constexpr long appFlag_stoppedInDebug = 0x06L;           // stopped in debug status

    constexpr long appFlag_dataInOut = 0x08L;                // external I/O (not to SD) is happening

    // bits 11-8: flags signaling specific calle    r status conditions to Justina
    constexpr long appFlag_consoleRequestBit = 0x0100L;
    constexpr long appFlag_killRequestBit = 0x0200L;
    constexpr long appFlag_stopRequestBit = 0x0400L;
    constexpr long appFlag_abortRequestBit = 0x0800L;

    heartbeat();                                                                        // blink a led to show program is running
#if withTCP
    myTCPconnection.maintainConnection();                                               // maintain TCP connection
#endif

    // request kill if debounced kill key press is detected 
    // request stop if debounced stop/abort key release is detected AND debounced key down time is less than the defined alternate function time
    // request abort if debounced stop/abort key down time is equal or more than the defined 'alternate function' time
    // --------------------------------------------------------------------------------------------------------------------------------------------

    uint8_t debouncedStates, wentDown, wentUp, isShortPress, isLongPress;
    uint8_t pinStates = (digitalRead(STOP_ABORT_PIN) << 1) + digitalRead(KILL_PIN);
    keyStates(pinStates, debouncedStates, wentDown, wentUp, isShortPress, isLongPress); // wentDown, wentUp, isShortPress, isLongPress: all one shot

    // application flags: submit to Justina 
    appFlags = (appFlags & ~(appFlag_consoleRequestBit | appFlag_killRequestBit | appFlag_stopRequestBit | appFlag_abortRequestBit));      // reset requests
    // short press: key goes up (edge) while time is less than threshold
    // long press: time passes threshold (edge) while key is still down
    if (isShortPress & (1 << 0)) { appFlags |= appFlag_consoleRequestBit; }
    if (isLongPress & (1 << 0)) { appFlags |= appFlag_killRequestBit; }
    if (isShortPress & (1 << 1)) { appFlags |= appFlag_stopRequestBit; }
    else if (isLongPress & (1 << 1)) { appFlags |= appFlag_abortRequestBit; Serial.print('A'); }

    // application flags: receive from Justina
    if (errorCondition ^ (appFlags & appFlag_errorConditionBit)) { errorCondition = (appFlags & appFlag_errorConditionBit);  digitalWrite(ERROR_PIN, errorCondition); }  // only write if change detected
    if (statusA ^ (appFlags & appFlag_statusAbit)) { statusA = (appFlags & appFlag_statusAbit);  digitalWrite(STATUS_A_PIN, statusA); }  // only write if change detected
    if (statusB ^ (appFlags & appFlag_statusBbit)) { statusB = (appFlags & appFlag_statusBbit);  digitalWrite(STATUS_B_PIN, statusB); }  // only write if change detected

    bool newDataLedState{false};
    static bool dataLedState {false};
    if (appFlags & appFlag_dataInOut) { newDataLedState = !dataLedState; } else { newDataLedState =false;}      // if data, toggle state, otherwise reset state
    if (newDataLedState != dataLedState) { dataLedState = newDataLedState;  digitalWrite(DATA_IO_PIN, dataLedState); }  // only write if change detected
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

void userFcn_readPort(const void** pdata, const char* valueType, const int argCount) {     // data: can be anything, as long as user function knows what to expect

    pAlternativeIO[0]->print("=== control is now in user c++ callback function: arg count = "); pAlternativeIO[0]->println(argCount);

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
        else if (isFloat) { *pFloat += 10. + i; }
        else {
            if (strlen(pText) >= 10) { pText[7] = '\0'; }  // do NOT increase the length of strings
            if (strlen(pText) >= 5) { pText[3] = pText[4]; pText[4] = '>'; }  // do NOT increase the length of strings
            else if (strlen(pText) >= 2) { pText[0] = '\0'; }       // change non-empty string into empty string 
            else if (strlen(pText) == 0) {}        // it is NOT allowed to increase the length of a string: you cannot change an empty string
        }

        // print a value
        pAlternativeIO[0]->print("    adapted value (argument "); pAlternativeIO[0]->print(i); pAlternativeIO[0]->print(") is now: ");      // but value
        if (isLong) { pAlternativeIO[0]->println(*pLong); }
        else if (isFloat) { pAlternativeIO[0]->println(*pFloat); }
        else { pAlternativeIO[0]->println(pText); }

    };
    pAlternativeIO[0]->println("=== leaving user c++ callback function");
    return;
}


// --------------------------------------
// example: a few other callback routines
// --------------------------------------

void userFcn_writePort(const void** pdata, const char* valueType, const int argCount) {
    pAlternativeIO[0]->println("*** Justina was here too ***");
    // do your thing here
};


void userFcn_togglePort(const void** pdata, const char* valueType, const int argCount) {
    pAlternativeIO[0]->println("*** Justina just passed by ***");
    // do your thing here
};
