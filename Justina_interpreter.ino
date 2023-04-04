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

#define withTCP 0

// includes
// --------

#include "Justina.h"
#include <avr/dtostrf.h>        
#if withTCP
#include "secrets.h"
#include "TCPserverClient.h"
#endif


//// test
uint32_t startTime = millis();
bool quitRequested = false;


// Global constants, variables and objects
// ---------------------------------------

constexpr int HEARTBEAT_PIN{ 9 }; ////                                               // indicator leds
constexpr int ERROR_PIN{ 10 };
constexpr int STATUS_A_PIN{ 11 };
constexpr int STATUS_B_PIN{ 12 };
constexpr int WAIT_FOR_USER_PIN{ 13 };

#if withTCP
constexpr pin_size_t WiFi_CONNECTED_PIN{ 11 };
constexpr pin_size_t TCP_CONNECTED_PIN{ 12 };
constexpr char SSID[] = SERVER_SSID, PASS[] = SERVER_PASS;                            // WiFi SSID and password                           
constexpr char menu[] = "+++ Please select:\r\n  '0' (Re-)start WiFi\r\n  '1' Disable WiFi\r\n  '2' Enable TCP\r\n  '3' Disable TCP\r\n  '4' Verbose TCP\r\n  '5' Silent TCP\r\n  '6' Remote console\r\n  '7' Local console\r\n  '8' Start Justina interpreter\r\n";

#else
constexpr char menu[] = "+++ Please select:\r\n  '8' Start Justina interpreter\r\n";
#endif

constexpr int terminalCount{ 2 };

bool TCP_enabled{ false };
bool console_isRemoteTerm{ false };                                                    // init: console is currently local terminal (Serial) 
bool withinApplication{ false };                                                       // init: currently not within an application
bool interpreterInMemory{ false };                                                     // init: interpreter is not in memory

bool errorCondition = false, statusA = false, statusB = false, waitingForUser = false;

Stream* pConsole = (Stream*)&Serial;                                                   // init pointer to Serial or TCP terminal
Stream* pAlternate = (Stream*)&Serial;

Justina_interpreter* pJustina{ nullptr };                                                    // pointer to Justina_interpreter object

#if withTCP
// connect as TCP server: create class object myTCPconnection
TCPconnection myTCPconnection(SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort, conn_2_TCPconnected);
Stream* pTerminal[terminalCount]{ (Stream*)&Serial, myTCPconnection.getClient() };
void switchConsole();
void onConnStateChange(connectionState_type  connectionState);
#else
Stream* pTerminal[terminalCount]{ (Stream*)&Serial };
#endif


#ifdef ARDUINO_ARCH_RP2040
long progMemSize = 30000;
#else
long progMemSize = 2000;
#endif 

unsigned long heartbeatPeriod{ 500 };                                               // do not go lower than 500 ms
void heartbeat();


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(1000000);

    // define output pins
    pinMode(HEARTBEAT_PIN, OUTPUT);                                                   // blinking led for heartbeat
    /*
    pinMode(ERROR_PIN, OUTPUT);                                                         // error
    pinMode(STATUS_A_PIN, OUTPUT);                                                         // error
    pinMode(STATUS_B_PIN, OUTPUT);                                                         // error
    pinMode(WAIT_FOR_USER_PIN, OUTPUT);                                                         // error
    */

#if withTCP
    pinMode(WiFi_CONNECTED_PIN, OUTPUT);                                              // 'TCP connected' led
    pinMode(TCP_CONNECTED_PIN, OUTPUT);                                               // 'TCP connected' led
#endif

    digitalWrite(HEARTBEAT_PIN, HIGH);                                                // while waiting for Serial to be ready
    delay(5000);                                                                      // Non-native USB ports: 'while(!Serial) {}' does not work
    digitalWrite(HEARTBEAT_PIN, LOW);
    while (!Serial);                                                                    // wait for serial port to connect. Needed for native USB port only 

#if withTCP
    Serial.println("Starting server");
    Serial.print("WiFi firmware version  "); Serial.println(WiFi.firmwareVersion()); Serial.println();

    myTCPconnection.setVerbose(false);                                                // disable debug messages from within myTCPconnection
    myTCPconnection.setKeepAliveTimeout(20000);
    // set callback function that will be executed when WiFi or TCP connection state changes 
    myTCPconnection.setConnCallback((&onConnStateChange));                            // set callback function
#endif

    // not functionaly used, but required to circumvent a bug in sprintf function with %F, %E, %G specifiers 
    char s[10];
    dtostrf(1.0, 4, 1, s);   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers    //// nog nodig ???

    // print sample / simple main menu for the user
    pConsole->println(menu);

#ifdef RTClock
    SdFile::dateTimeCallback((dateTime));
#endif
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    ////myTCPconnection.maintainConnection();                                               // important to execute regularly; place at beginning of loop()
    heartbeat();                                                                        // blink a led to show program is running 

    // loop only once, to alow breaks
    do {

        // 1. Read character (if available) from Serial, and read a character (if available) from TCP client.
        //    Characters from inactive input channel are discarded (with one exception: mechanism to gain back local control).
        //    Reading from both streams helps avoiding input buffer overruns AND it provides a mechanism to gain back local control if TCP connection...
        //    ...is lost while console is remote terminal (TCP client), setting console back to local terminal (Serial)
        //
        //    Note that when control is passed to an application (like the interpreter), execution of this main loop is suspended.
        //    The application is supposed to have its own main loop (see further down)
        // -----------------------------------------------------------------------------------------------------------------------------

        bool found = false;
        char c;

        // read character from local terminal (Serial), if a character is available
        if (Serial.available() > 0) {
            char localChar = Serial.read();
        #if withTCP
            // mechanism to gain back local control, e.g. if remote connection (TCP) lost 
            bool forceLocal = (localChar == 0x01);                                      // character read from Serial is 0x01 ? force switch to local console      
            if (forceLocal && console_isRemoteTerm) { switchConsole(); }              // if console is currently remote terminal (TCP client), set console to local
        #endif
            if (!console_isRemoteTerm) { found = true; c = localChar; }               // if console is currently local terminal (Serial), accept character 
        }

    #if withTCP
        // read character from remote terminal (TCP client), if a character is available
        if (myTCPconnection.getClient()->available() > 0) {
            char remoteChar = myTCPconnection.getClient()->read();
            if (console_isRemoteTerm) { found = true; c = remoteChar; }               // if console is currently remote terminal (TCP client), accept character
        }
    #endif

        if (!found || (c < ' ')) { break; }                                           // no character to process (also discard control characters)


        // 2. Perform a action according to selected option (menu is displayed)
        // --------------------------------------------------------------------

        switch (tolower(c)) {

        #if withTCP
            case '0':
                myTCPconnection.requestAction(action_1_restartWiFi);
                pConsole->println("(Re-)starting WiFi...");
                break;

            case '1':
                myTCPconnection.requestAction(action_0_disableWiFi);
                pConsole->println("Disabling WiFi...");
                break;

            case '2':
                myTCPconnection.requestAction(action_2_TCPkeepAlive);
                pConsole->println("Enabling TCP...");                                     // needs WiFi to be enabled and connected
                break;

            case '3':
                if (console_isRemoteTerm) { pConsole->println("Cannot disable TCP while console is remote"); }
                else {
                    myTCPconnection.requestAction(action_4_TCPdisable);
                    pConsole->println("Disabling TCP...");
                }
                break;

            case '4':                                                                       // set TCP server to verbose
                myTCPconnection.setVerbose(true);
                pConsole->println("TCP server: verbose");
                break;

            case '5':                                                                       // set TCP server to silent
                myTCPconnection.setVerbose(false);
                pConsole->println("TCP server: silent");
                break;

            case '6':                                                                       // if console is currently local terminal, switch to remote
                if (console_isRemoteTerm) { pConsole->println("Nothing to do"); }
                else { switchConsole(); }
                break;

            case '7':                                                                       // if console is currently remote terminal, switch to local
                if (!console_isRemoteTerm) { pConsole->println("Nothing to do"); }
                else { switchConsole(); }

                break;
            #endif
            case '8':
                //// test
                startTime = millis();
                quitRequested = false;
                //// end test

            #if !defined(ARDUINO_SAMD_NANO_33_IOT) && !defined(ARDUINO_ARCH_RP2040)
                pConsole->println("interpreter does not run on this processor");            // interpreter does not run on this processor
                break;
            #endif

                // start interpreter: control will not return to here until the user quits, because it has its own 'main loop'
                heartbeatPeriod = 250;
                withinApplication = true;                                                   // flag that control will be transferred to an 'application'
                if (!interpreterInMemory) {
                    pJustina = new  Justina_interpreter(pConsole,pAlternate, progMemSize);         // if interpreter not running: create an interpreter object on the heap

                    // set callback function to avoid that maintaining the TCP connection AND the heartbeat function are paused as long as control stays in the interpreter
                    // this callback function will be called regularly, e.g. every time the interpreter reads a character
                    pJustina->setMainLoopCallback((&housekeeping));                    // set callback function to housekeeping routine in this .ino file (pass 'housekeeping' routine address to Justina_interpreter library)

                    pJustina->setUserFcnCallback((&userFcn_readPort));                // pass user function addresses to Justina_interpreter library (return value 'true' indicates success)
                    pJustina->setUserFcnCallback((&userFcn_writePort));
                    pJustina->setUserFcnCallback((&userFcn_togglePort));
                }
                interpreterInMemory = pJustina->run(pConsole, pTerminal, terminalCount);                                   // run interpreter; on return, inform whether interpreter is still in memory (data not lost)

                if (!interpreterInMemory) {                                               // return from interpreter: remove from memory as well ?
                    delete pJustina;                                                     // cleanup and delete calculator object itself
                    pJustina = nullptr;                                                  // only to indicate memory is released
                }

                heartbeatPeriod = 500;
                withinApplication = false;                                                  // return from application
                break;

            default:
                pConsole->println("This is not a valid choice");
        }
        pConsole->println(menu);                                                      // show menu again
    } while (false);
}                                                                                       // loop()


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

// ---------------------------------------------------------------------------------
// *   Switch console to remote terminal (TCP client) or local terminal (Serial)   *
// ---------------------------------------------------------------------------------

void switchConsole() {
    if (console_isRemoteTerm) { pConsole->println("Disconnecting terminal...\r\n-------------------------\r\n"); }               // inform the remote user

    console_isRemoteTerm = !console_isRemoteTerm;
    // set client connection timeout (long if console is remote terminal (TCP connection), short if local terminal (Serial) but keep TCP enabled
    myTCPconnection.requestAction(console_isRemoteTerm ? action_2_TCPkeepAlive : action_3_TCPdoNotKeepAlive);

    // set pointer to Serial or TCP client (both belong to Stream class)
    pConsole = (console_isRemoteTerm) ? myTCPconnection.getClient() : (Stream*)&Serial;
    char s[40]; sprintf(s, "Console is now %s ", console_isRemoteTerm ? "remote terminal" : "local");
    Serial.println(s);
    if (console_isRemoteTerm) {
        Serial.println("On the remote terminal, press ENTER (a couple of times) to connect\r\n-------------------------\r\n");
    }
}


// ----------------------------------------------------------------------------
// *   Callback function executed when WiFi or TCP connection state changes   * 
// ----------------------------------------------------------------------------

// this routine is called from within myTCPconnection.maintainConnection() at every change in connection state
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

    if (TCPconnected) {
        Serial.println("TCP connection established\r\n");
        if (!withinApplication) { pConsole->println(menu); }                        // if not within an application, print main menu on remote terminal
    }                   // remote client just got connected: show on main terminal

    else if (holdTCPconnected) {                                                      // previous status was 'client connected'
        if (console_isRemoteTerm) {                                                   // but still in remote mode: so probably a timeout (or a wifi issue, ...)
            Serial.println("Console connection lost or timed out");                   // inform local terminal about it 
            Serial.println("On the remote terminal, press ENTER to reconnect");
        }
    }
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

// -----------------------------------------------------------------------------------------------------------------------------
// *   callback function to be called at regular intervals from any application not returning immediately to Arduino main loop()
// -----------------------------------------------------------------------------------------------------------------------------

// This callback function is used to avoid that specific actions are paused while control stays in an application for a longer period
// (1) maintaining the TCP connection (we don't want an application to have knowledge about where it gets its input and sends its output)
// (2) maintaining the heartbeat 
// (3) if the console is currently remote terminal: continue to provide a mechanism to gain back local control while in an application, 
//     e.g. if remote connection (TCP) is lost  

// in this program, this callback function is called at regular intervals from within the interpreter main loop  

void housekeeping(bool& requestQuit, long& appFlags) {

    // application flag bits:flags signaling specific Justina status conditions
    constexpr long appFlag_errorConditionBit = 0x01L;       // bit 0: a Justina parsing or execution error has occured
    constexpr long appFlag_statusAbit = 0x10L;              // status bits A and B: bits 5 and 4. Justina status (see below)
    constexpr long appFlag_statusBbit = 0x20L;
    constexpr long appFlag_waitingForUser = 0x40L;

    bool& forceLocal = requestQuit;                                                     // reference variable

    heartbeat();                                                                        // blink a led to show program is running

    /*
    if (errorCondition ^ (appFlags & appFlag_errorConditionBit)) { errorCondition = (appFlags & appFlag_errorConditionBit);  digitalWrite(ERROR_PIN, errorCondition); }  // only write if change detected
    if (statusA ^ (appFlags & appFlag_statusAbit)) { statusA = (appFlags & appFlag_statusAbit);  digitalWrite(STATUS_A_PIN, statusA); }  // only write if change detected
    if (statusB ^ (appFlags & appFlag_statusBbit)) { statusB = (appFlags & appFlag_statusBbit);  digitalWrite(STATUS_B_PIN, statusB); }  // only write if change detected
    if (waitingForUser ^ (appFlags & appFlag_waitingForUser)) { waitingForUser = (appFlags & appFlag_waitingForUser);  digitalWrite(WAIT_FOR_USER_PIN, waitingForUser); }  // only write if change detected
    */

    //// test 'force quit' vanuit main: 
    ////if (!quitRequested && ((startTime + 20000) < millis())) { quitRequested = true; forceLocal = true;  }


#if withTCP
    myTCPconnection.maintainConnection();                                               // maintain TCP connection

    // if console is remote terminal (TCP), keep reading characters from local terminal (Serial) while in an application, to (1) avoid buffer overruns
    // and (2) continue to provide the mechanism to gain back local control if TCP connection seems to be lost
    // in the latter case we also need to inform the running application that it should abort ('request quit' return value)

    forceLocal = false;                                                                 // init  
    if (console_isRemoteTerm) {                                                       // console is currently remote terminal( TCP client)
        char c;
        if (Serial.available() > 0) {
            c = Serial.read();
            forceLocal = (c == 0x01);                                                   // character read from Serial is 0x01 ? force switch to local console
            if (forceLocal) {
                pConsole->println("Disconnecting remote terminal...");                // inform remote user, in case he's still there
                switchConsole();                                                        // set console to local
            }
        }
    }
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

void userFcn_readPort(const void** pdata, const char* valueType, const int argCount) {     // data: can be anything, as long as user function knows what to expect

    pConsole->print("=== control is now in user c++ callback function: arg count = "); pConsole->println(argCount);

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
        pConsole->print("    adapted value (argument "); pConsole->print(i); pConsole->print(") is now: ");      // but value
        if (isLong) { pConsole->println(*pLong); }
        else if (isFloat) { pConsole->println(*pFloat); }
        else { pConsole->println(pText); }

    };
    pConsole->println("=== leaving user c++ callback function");
    return;
}


// --------------------------------------
// example: a few other callback routines
// --------------------------------------

void userFcn_writePort(const void** pdata, const char* valueType, const int argCount) {
    pConsole->println("*** Justina was here too ***");
    // do your thing here
};


void userFcn_togglePort(const void** pdata, const char* valueType, const int argCount) {
    pConsole->println("*** Justina just passed by ***");
    // do your thing here
};
