/***************************************************************************************
    Arduino Nano 33 IoT as TCP server and as TCP client

    Version:    v1.00 - 12/10/2021
    Author:     Herwig Taveirne

    Purpose: test connection and message transmission via WiFi between an Arduino nano 33 IoT board and a TCP IP terminal
    The nano 33 IoT board acts as TCP server, the terminal as TCP client


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


#include "myParser.h"
#include <avr/dtostrf.h>
#include <stdlib.h>
#include "myComm.h"
#include "secrets.h"


// global constants, variables and objects
// ---------------------------------------

constexpr pin_size_t HEARTBEAT_PIN { 9 };                                               // indicator leds
constexpr pin_size_t TCP_CONNECTED_PIN { 10 };

bool isRemoteMode { false };                                                            // currently in local (Serial) or remote (TCP) mode ? 
Stream* pTerminal = (Stream*) &Serial;                                                  // init pointer to Serial or TCP terminal
Calculator* pcalculator { nullptr };                                                    // pointer to Calculator object
const char SSID [] = SERVER_SSID, PASS [] = SERVER_PASS;                                // WiFi SSID and password                           
// connect as TCP server: create class object myTCPconnection
MyTCPconnection myTCPconnection( SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort );


// forward declarations
// --------------------

bool readChar( Stream* pTerminal, char& c );
void switchConsole();
void maintainTCPconn_heartbeat();
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
    myTCPconnection.setVerbose( true );                                                 // enable debug messages from within myTCPconnection


    // set callback function for WiFi and TCP connection state changes
    // a callback function allows to perform specific actions when specific events happen, without a need to modify code in calling (library) classes
    // ----------------------------------------------------------------------------------------------------------------------------------------------

    myTCPconnection.setConnCallback( (&onConnStateChange) );


    // not functionaly used, but required to circumvent a bug in sprintf function with %F, %E, %G specifiers 
    char s [10];
    dtostrf( 1.0, 4, 1, s );   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers

    // print sample / simple main menu for the user
    Serial.println( "\r\nPlease select 'i' for interpreter, 'z' for a message" );
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------


void loop() {
    myTCPconnection.maintainConnection();                           // important to execute regularly; place at beginning of loop()
    heartbeat();                                                    // blink a led to show program is running 


    do {
        char c;

        // read character from Serial, if a character is available (also if in remote mode)
        if ( readChar( &Serial, c ) ) {
            bool isTerminalCtrl = (c == 0x01);                      // local input only: control character 0x01 is used to toggle between local and remote mode      
            if ( isTerminalCtrl ) { switchConsole();  break; }  // swith between local and remote mode ?
            if ( isRemoteMode ) { break; }                         // in remote mode (TCP), local input (from Serial) is dicarded (exit do)
        }

        // read character from TCP client, if a character is available (even if server is in local mode, to avoid buffer overruns)
        else if ( readChar( myTCPconnection.getClient(), c ) ) {
            if ( !isRemoteMode ) { break; }                            // in local mode (Serial), remote input (from TCP client) is dicarded (exit do) 
        }

        // no character read: no character to process (exit do)
        else { break; }

        if ( c < ' ' ) { break; }                                      // remove control characters

        // sample menu options
        switch ( tolower( c ) ) {
        case 'i':
            pcalculator = new  Calculator;
            pcalculator->setCalcMainLoopCallback( (&maintainTCPconn_heartbeat) );
            pcalculator->run();
            delete pcalculator;
            break;                                // start interpreter
        case 'z': pTerminal->println( "This is the last letter of the English alphabet" ); break;
        default: pTerminal->println( "This is not a valid choice" );
        }
        pTerminal->println( "\r\nPlease select 'i' for interpreter, 'z' for a message" );
    } while ( false );
}


//
void switchConsole() {
    isRemoteMode = !isRemoteMode;                       // 0x01 ? swith between local and remote mode 
    // set client connection timeout (long if in remote mode (using TCP connection), short if currently using Serial)
    myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );

    // set pointer to Serial or TCP client (both belong to Stream class)
    pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
    char s [70]; sprintf( s, "\nConsole is now %s ", isRemoteMode ? "remote terminal" : "local" );
    Serial.println( s );
}


// read a character from Stream (TCPclient or Serial)
bool readChar( Stream* pTerminal, char& c ) {
    bool found = (pTerminal->available() > 0);
    if ( found ) { c = pTerminal->read(); }    // while terminal characters available for reading
    return found;
}


// *** callback function: called from within myTCPconnection.maintainConnection() when connection state changes
// note that, if the TCP connection status changes during execution of the user program, this will only be reflected 
// next time myTCPconnection.maintainConnection() is called (call should be placed at the start of the main loop() )

void onConnStateChange( connectionState_type  connectionState ) {
    static bool TCPconnected { false };

    TCPconnected = (connectionState == conn_2_TCPconnected);
    digitalWrite( TCP_CONNECTED_PIN, TCPconnected );                 // flag 'client connected'
}


// *** callback function: for general use, called from within myCalculator main loop  
//     every time an input character is processed or an instruction is executed

void maintainTCPconn_heartbeat() {
    myTCPconnection.maintainConnection();                           // important to execute regularly; place at beginning of loop()
    heartbeat();
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
