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


// includes
// --------

#include <avr/dtostrf.h>        
#include "secrets.h"
#include "myParser.h"
#include "myComm.h"


// Global constants, variables and objects
// ---------------------------------------

constexpr pin_size_t HEARTBEAT_PIN { 9 };                                               // indicator leds
constexpr pin_size_t WiFi_CONNECTED_PIN { 10 };
constexpr pin_size_t TCP_CONNECTED_PIN { 11 };
constexpr int terminalCount { 2 };

constexpr char SSID [] = SERVER_SSID, PASS [] = SERVER_PASS;                            // WiFi SSID and password                           
constexpr char menu [] = "+++ Please select:\r\n  '0' (Re-)start WiFi\r\n  '1' Disable WiFi\r\n  '2' Enable TCP\r\n  '3' Disable TCP\r\n  '4' Verbose TCP\r\n  '5' Silent TCP\r\n  '6' Remote console\r\n  '7' Local console\r\n  '8' Start Justina interpreter\r\n";

bool TCP_enabled { false };
bool console_isRemoteTerm { false };                                                    // init: console is currently local terminal (Serial) 
bool withinApplication { false };                                                       // init: currently not within an application
bool interpreterInMemory { false };                                                     // init: interpreter is not in memory

Calculator* pcalculator { nullptr };                                                    // pointer to Calculator object
// connect as TCP server: create class object myTCPconnection
MyTCPconnection myTCPconnection( SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort, conn_2_TCPconnected );
Stream* pConsole = (Stream*) &Serial;                                                   // init pointer to Serial or TCP terminal
Stream* pTerminal [terminalCount] { (Stream*) &Serial, myTCPconnection.getClient() };


// Forward declarations
// --------------------

void switchConsole();
void housekeeping( bool& requestQuit );
void onConnStateChange( connectionState_type  connectionState );
void heartbeat();


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin( 1000000 );

    // define output pins
    pinMode( HEARTBEAT_PIN, OUTPUT );                                                   // blinking led for heartbeat
    pinMode( WiFi_CONNECTED_PIN, OUTPUT );                                              // 'TCP connected' led
    pinMode( TCP_CONNECTED_PIN, OUTPUT );                                               // 'TCP connected' led

    digitalWrite( HEARTBEAT_PIN, HIGH );                                                // test leds while waiting for Serial to be ready
    digitalWrite( WiFi_CONNECTED_PIN, HIGH );
    digitalWrite( TCP_CONNECTED_PIN, HIGH );
    delay( 4000 );                                                                      // 'while(!Serial) {}' does not work
    digitalWrite( HEARTBEAT_PIN, LOW );
    digitalWrite( WiFi_CONNECTED_PIN, LOW );
    digitalWrite( TCP_CONNECTED_PIN, LOW );

    Serial.println( "Starting server" );
    Serial.print( "WiFi firmware version  " ); Serial.println( WiFi.firmwareVersion() ); Serial.println();

    myTCPconnection.setVerbose( false );                                                // disable debug messages from within myTCPconnection
    myTCPconnection.setKeepAliveTimeout( 20000 );
    // set callback function that will be executed when WiFi or TCP connection state changes 
    myTCPconnection.setConnCallback( (&onConnStateChange) );                            // set callback function

    // not functionaly used, but required to circumvent a bug in sprintf function with %F, %E, %G specifiers 
    char s [10];
    dtostrf( 1.0, 4, 1, s );   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers

    // print sample / simple main menu for the user
    pConsole->println( menu );
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    myTCPconnection.maintainConnection();                                               // important to execute regularly; place at beginning of loop()
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
        if ( Serial.available() > 0 ) {
            char localChar = Serial.read();
            // mechanism to gain back local control, e.g. if remote connection (TCP) lost 
            bool forceLocal = (localChar == 0x01);                                      // character read from Serial is 0x01 ? force switch to local console      
            if ( forceLocal && console_isRemoteTerm ) { switchConsole(); }              // if console is currently remote terminal (TCP client), set console to local
            if ( !console_isRemoteTerm ) { found = true; c = localChar; }               // if console is currently local terminal (Serial), accept character 
        }

        // read character from remote terminal (TCP client), if a character is available
        if ( myTCPconnection.getClient()->available() > 0 ) {
            char remoteChar = myTCPconnection.getClient()->read();
            if ( console_isRemoteTerm ) { found = true; c = remoteChar; }               // if console is currently remote terminal (TCP client), accept character
        }

        if ( !found || (c < ' ') ) { break; }                                           // no character to process (also discard control characters)


        // 2. Perform a action according to selected option (menu is displayed)
        // --------------------------------------------------------------------

        switch ( tolower( c ) ) {

        case '0':
            myTCPconnection.requestAction( action_1_restartWiFi );
            pConsole->println( "(Re-)starting WiFi..." );                               
            break;

        case '1':
            myTCPconnection.requestAction( action_0_disableWiFi );
            pConsole->println( "Disabling WiFi..." );                                         
            break;

        case '2':
            myTCPconnection.requestAction( action_2_TCPkeepAlive );
            pConsole->println( "Enabling TCP..." );                                     // needs WiFi to be enabled and connected
            break;

        case '3':
            if ( console_isRemoteTerm ) { pConsole->println( "Cannot disable TCP while console is remote" ); }
            else {
                myTCPconnection.requestAction( action_4_TCPdisable );
                pConsole->println( "Disabling TCP..." );
            }
            break;

        case '4':                                                                       // set TCP server to verbose
            myTCPconnection.setVerbose( true );
            pConsole->println( "TCP server: verbose" );
            break;

        case '5':                                                                       // set TCP server to silent
            myTCPconnection.setVerbose( false );
            pConsole->println( "TCP server: silent" );
            break;

        case '6':                                                                       // if console is currently local terminal, switch to remote
            if ( console_isRemoteTerm ) { pConsole->println( "Nothing to do" ); }
            else { switchConsole(); }                           
            break;

        case '7':                                                                       // if console is currently remote terminal, switch to local
            if ( !console_isRemoteTerm ) { pConsole->println( "Nothing to do" ); }
            else { switchConsole(); }                    

            break;

        case '8':
            pConsole->println();
            // start interpreter: control will not return to here until the user quits, because it has its own 'main loop'
            withinApplication = true;                                                   // flag that control will be transferred to an 'application'
            if ( !interpreterInMemory ) { pcalculator = new  Calculator( pConsole ); }  // if interpreter not running: create an interpreter object on the heap
            
            // set callback function to avoid that maintaining the TCP connection AND the heartbeat function are paused as long as control stays in the interpreter
            // this callback function will be called regularly, e.g. every time the interpreter reads a character
            pcalculator->setCalcMainLoopCallback( (&housekeeping) );                    // set callback function to housekeeping routine in this .ino file

            interpreterInMemory = pcalculator->run( pConsole, pTerminal, terminalCount );                                   // run interpreter; on return, inform whether interpreter is still in memory (data not lost)
            if ( !interpreterInMemory ) {                                               // interpreter not running anymore ?
                delete pcalculator;                                                     // cleanup and delete calculator object itself
                pcalculator = nullptr;                                                  // only to indicate memory is released
            }
            withinApplication = false;                                                  // return from application
            break;

        default:
            pConsole->println( "This is not a valid choice" );
        }
        pConsole->println( menu );                                                      // show menu again
    } while ( false );
}                                                                                       // loop()



// ---------------------------------------------------------------------------------
// *   Switch console to remote terminal (TCP client) or local terminal (Serial)   *
// ---------------------------------------------------------------------------------

void switchConsole() {
    if ( console_isRemoteTerm ) { pConsole->println( "Disconnecting terminal...\r\n-------------------------\r\n" ); }               // inform the remote user

    console_isRemoteTerm = !console_isRemoteTerm;
    // set client connection timeout (long if console is remote terminal (TCP connection), short if local terminal (Serial) but keep TCP enabled
    myTCPconnection.requestAction( console_isRemoteTerm ? action_2_TCPkeepAlive : action_3_TCPdoNotKeepAlive );

    // set pointer to Serial or TCP client (both belong to Stream class)
    pConsole = (console_isRemoteTerm) ? myTCPconnection.getClient() : (Stream*) &Serial;
    char s [40]; sprintf( s, "Console is now %s ", console_isRemoteTerm ? "remote terminal" : "local" );
    Serial.println( s );
    if ( console_isRemoteTerm ) {
        Serial.println( "On the remote terminal, press ENTER (a couple of times) to connect\r\n-------------------------\r\n" );
    }
}


// ----------------------------------------------------------------------------
// *   Callback function executed when WiFi or TCP connection state changes   * 
// ----------------------------------------------------------------------------

// this routine is called from within myTCPconnection.maintainConnection() at every change in connection state
// it allows the main program to take specific custom actions (in this case: printing messages an controlling a led)

void onConnStateChange( connectionState_type  connectionState ) {
    static bool WiFiConnected { false };
    static bool TCPconnected { false };

    bool holdTCPconnected = TCPconnected;
    bool holdWiFiConnected = WiFiConnected;

    WiFiConnected = (connectionState == conn_1_wifiConnected) || (connectionState == conn_2_TCPconnected);
    digitalWrite( WiFi_CONNECTED_PIN, WiFiConnected );                                  // led indicates 'client connected' status 
    TCPconnected = (connectionState == conn_2_TCPconnected);
    digitalWrite( TCP_CONNECTED_PIN, TCPconnected );                                    // led indicates 'client connected' status 

    if ( TCPconnected ) {
        Serial.println( "TCP connection established\r\n" );
        if ( !withinApplication ) { pConsole->println( menu ); }                        // if not within an application, print main menu on remote terminal
    }                   // remote client just got connected: show on main terminal
    
    else if ( holdTCPconnected ) {                                                      // previous status was 'client connected'
        if ( console_isRemoteTerm ) {                                                   // but still in remote mode: so probably a timeout (or a wifi issue, ...)
            Serial.println( "Console connection lost or timed out" );                   // inform local terminal about it 
            Serial.println( "On the remote terminal, press ENTER to reconnect" );
        }
    }
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

void housekeeping( bool& requestQuit ) {
    bool& forceLocal = requestQuit;                                                     // reference variable

    myTCPconnection.maintainConnection();                                               // maintain TCP connection
    heartbeat();                                                                        // blink a led to show program is running

    // if console is remote terminal (TCP), keep reading characters from local terminal (Serial) while in an application, to (1) avoid buffer overruns
    // and (2) continue to provide the mechanism to gain back local control if TCP connection seems to be lost
    // in the latter case we also need to inform the running application that it should abort ('request quit' return value)

    forceLocal = false;                                                                 // init  
    if ( console_isRemoteTerm ) {                                                       // console is currently remote terminal( TCP client)
        char c;
        if ( Serial.available() > 0 ) {
            c = Serial.read();
            forceLocal = (c == 0x01);                                                   // character read from Serial is 0x01 ? force switch to local console
            if ( forceLocal ) {
                pConsole->println( "Disconnecting remote terminal..." );                // inform remote user, in case he's still there
                switchConsole();                                                        // set console to local
            }
        }
    }
}


// --------------------------------------
// Blink a led to show program is running 
// --------------------------------------

void heartbeat() {
    // note: this is not a clock because it does not measure the passing of fixed time intervals
    // but the passing of minimum time intervals (the millis() function itself is a clock)

    static bool ledOn { false };
    static unsigned long lastHeartbeat { 0 };                                           // last heartbeat time in ms

    uint32_t currentTime = millis();
    if ( lastHeartbeat + 1000UL < currentTime ) {                                       // time passed: switch led state
        ledOn = !ledOn;
        digitalWrite( HEARTBEAT_PIN, ledOn );
        lastHeartbeat = currentTime;
    }
}
