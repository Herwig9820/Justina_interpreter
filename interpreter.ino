/***************************************************************************************
    Arduino Nano 33 IoT as TCP server and as TCP client

    Version:    v1.00 - 12/10/2021
    Author:     Herwig Taveirne

    Purpose: demonstrate the Pigeon interpreter application
             running on a nano 33 IoT board running as TCP server

    Both the Pigeon interpreter and the TCP server software are available as libraries
    See GitHub for more information and documentation ************

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************************/


// includes
// --------

#include <avr/dtostrf.h>        //// nodig ?
#include <stdlib.h>             //// nodig ?
#include "secrets.h"
#include "myParser.h"
#include "myComm.h"


// global constants, variables and objects
// ---------------------------------------

constexpr pin_size_t HEARTBEAT_PIN { 9 };                                               // indicator leds
constexpr pin_size_t TCP_CONNECTED_PIN { 10 };
constexpr char SSID [] = SERVER_SSID, PASS [] = SERVER_PASS;                            // WiFi SSID and password                           
constexpr char menu [] = "Please select: 'r' for remote', 'l' for local, 'i' for interpreter\r\n               'v' for verbose TCP, 's' for silent TCP";

bool isRemoteMode { false };                                                            // init: currently in local mode (Serial) 
bool withinApplication { false };

Stream* pTerminal = (Stream*) &Serial;                                                  // init pointer to Serial or TCP terminal
Calculator* pcalculator { nullptr };                                                    // pointer to Calculator object
// connect as TCP server: create class object myTCPconnection
MyTCPconnection myTCPconnection( SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort );


// forward declarations
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
    pinMode( TCP_CONNECTED_PIN, OUTPUT );                                               // 'TCP connected' led

    digitalWrite( HEARTBEAT_PIN, HIGH );                                                // test leds while waiting for Serial to be ready
    digitalWrite( TCP_CONNECTED_PIN, HIGH );
    delay( 4000 );                                                                      // 'while(!Serial) {}' does not work
    digitalWrite( HEARTBEAT_PIN, LOW );
    digitalWrite( TCP_CONNECTED_PIN, LOW );

    Serial.println( "Starting server" );
    Serial.print( "WiFi firmware version  " ); Serial.println( WiFi.firmwareVersion() );

    // set client connection timeout (long if in remote mode (using TCP connection), short if currently using Serial)
    myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );
    myTCPconnection.setVerbose( false );                                                // disable debug messages from within myTCPconnection


    // set callback function for WiFi and TCP connection state changes
    // a callback function allows to perform specific actions when specific events happen, without a need to modify code in calling (library) classes
    myTCPconnection.setConnCallback( (&onConnStateChange) );


    // not functionaly used, but required to circumvent a bug in sprintf function with %F, %E, %G specifiers 
    char s [10];
    dtostrf( 1.0, 4, 1, s );   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers

    // print sample / simple main menu for the user
    pTerminal->println( menu );
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    myTCPconnection.maintainConnection();                                               // important to execute regularly; place at beginning of loop()
    heartbeat();                                                                        // blink a led to show program is running 

    // loop only once, to alow breaks
    do {

        // 1. read character (if available) from Serial, and if none available, read a character (if available) from TCP client
        //    this helps avoiding input buffer overruns AND it provides a mechanism to gain back local control if TCP connection is lost
        //    while console is remote terminal (TCP client), setting console back to local terminal (Serial)
        //    characters from inactive input channel are discarded, with one exception (mechanism to gain back local control)
        // -----------------------------------------------------------------------------------------------------------------------------

        char c;

        // read character from Serial, if a character is available (also if console is currently remote terminal (TCP client))
        if ( Serial.available() > 0 ) {
            c = Serial.read();

            // mechanism to gain back local control, e.g. if remote connection (TCP) lost 
            bool forceLocal = (c == 0x01);                                              // character from Serial read is 0x01 ?       
            if ( forceLocal ) {
                if ( isRemoteMode ) { switchConsole(); }                                // if console is currently remote terminal (TCP client), set console to local
                break;                                                                  // discard this character
            }
            if ( isRemoteMode ) { break; }                                              // if console is currently remote terminal (TCP client), discard character from Serial 
        }

        // no character from Serial: read character from TCP client, if a character is available (also if console is currently local terminal (Serial))
        else if ( myTCPconnection.getClient()->available() > 0 ) {
            c = myTCPconnection.getClient()->read();
            Serial.print("TCP char: "); Serial.println(c, HEX);
            if ( !isRemoteMode ) { break; }                                             // if console is currently local terminal (Serial), discard character from TCP client
        }

        // no character read: no character to process
        else { break; }

        if ( c < ' ' ) { break; }                                                       // remove control characters


        // 2. Perform a action according to selected option (menu is displayed)
        // --------------------------------------------------------------------

        switch ( tolower( c ) ) {

        case 'v':
            myTCPconnection.setVerbose( true );
            pTerminal->println( "TCP server: verbose" );                                // set TCP server to verbose
            break;

        case 's':
            myTCPconnection.setVerbose( false ); break;                                 // set TCP server to silent
            pTerminal->println( "TCP server: silent" );

        case 'r':
            if ( !isRemoteMode ) { switchConsole(); }                                   // if console is currently local terminal, switch to remote
            break;

        case 'l':
            if ( isRemoteMode ) { switchConsole(); }                                    // if console is currently remote terminal, switch to local
            break;

        case 'i':
            // start interpreter: control will not return to here until the user quits, because it has its own 'main loop'
            withinApplication = true;                                                   // flag that control will be transferred to an 'application'
            pcalculator = new  Calculator( pTerminal );                                 // create an interpreter object on the heap
            // to avoid that maintaining the TCP connection AND the heartbeat function are paused as long as control stays in the interpreter,
            // which can be installed as an independent library without any knowledge about which procedures to call, a callback function is used.
            // This callback function will be called regularly, e.g. every time the interpreter reads a character (same as here)
            pcalculator->setCalcMainLoopCallback( (&housekeeping) );                    // set callback function to housekeeping routine in this .ino file
            pcalculator->run();                                                         // start interpreter
            delete pcalculator;                                                         // cleanup and delete calculator object itself
            withinApplication = false;
            break;

        default:
            pTerminal->println( "This is not a valid choice" );
        }
        pTerminal->println( menu );                                                     // show menu again
    } while ( false );
}                                                                                       // loop()



// ---------------------------------------------------------------------------------
// *   Switch console to remote terminal (TCP client) or local terminal (Serial)   *
// ---------------------------------------------------------------------------------

void switchConsole() {
    isRemoteMode = !isRemoteMode;                        
    // set client connection timeout (long if in remote mode (using TCP connection), short if currently using Serial)
    myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );

    // set pointer to Serial or TCP client (both belong to Stream class)
    pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
    char s [40]; sprintf( s, "\nConsole is now %s ", isRemoteMode ? "remote terminal" : "local" );
    Serial.println( s );
    if ( isRemoteMode ) {
        Serial.println( "On the remote terminal, press ENTER (a couple of times) to connect" );
    }

}


// *** callback function: called from within myTCPconnection.maintainConnection() when connection state changes
// note that, if the TCP connection status changes during execution of the user program, this will only be reflected 
// next time myTCPconnection.maintainConnection() is called (call should be placed at the start of the main loop() )

void onConnStateChange( connectionState_type  connectionState ) {
    static bool TCPconnected { false };

    bool lastConn = TCPconnected;

    TCPconnected = (connectionState == conn_2_TCPconnected);
    digitalWrite( TCP_CONNECTED_PIN, TCPconnected );                 // flag 'client connected'
    if ( TCPconnected ) {
        pTerminal->println( "Connected" );
        if ( !withinApplication ) { pTerminal->println( menu ); }
        Serial.println( "Remote terminal is now connected" );
    }
    else if ( lastConn ) {          // connection changed to 'not connected'
        if ( isRemoteMode ) {         // but still in remote mode: so probably a timeout (or a wifi issue, ...)
            Serial.println( "Console connection lost or timed out" );
            Serial.println( "On the remote terminal, press ENTER to reconnect" );
        }
    }
}


// *** callback function: for general use, called from within myCalculator main loop  
//     every time an input character is processed or an instruction is executed

void housekeeping( bool& requestQuit ) {
    // application is running
    myTCPconnection.maintainConnection();                           // important to execute regularly; place at beginning of loop()
    heartbeat();
    requestQuit = false; // init  
    // if in remote mode, keep reading characters from Serial while in an application, (1) to avoid overflow
    // and (2) to be able to send 'request quit' command to running application and gain back local control
    if ( isRemoteMode ) {
        char c;
        if ( Serial.available() > 0 ) {
            c = Serial.read();
            requestQuit = (c == 0x01);
            if ( requestQuit ) { switchConsole(); }        // request quit
        }
    }
}


// *** 

void heartbeat() {
    static bool toggleHeartbeat { false }, heartbeatOccured { false };
    static unsigned long lastHeartbeat { 0 };                                  // last heartbeat time in ms

    uint32_t currentTime = millis();
    if ( lastHeartbeat + 1000UL < currentTime ) {  // heartbeat
        toggleHeartbeat = !toggleHeartbeat;
        heartbeatOccured = true;
        digitalWrite( HEARTBEAT_PIN, toggleHeartbeat );
        lastHeartbeat = currentTime;
    }
}
