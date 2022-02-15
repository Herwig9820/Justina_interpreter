/*
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
*/

#include "calculator.h"
#include <avr/dtostrf.h>
#include <stdlib.h>
#include "myComm.h"
#include "secrets.h"


constexpr pin_size_t HEARTBEAT_PIN { 9 };                    // indicator leds
constexpr pin_size_t TCP_CONNECTED_PIN { 10 };


bool isRemoteMode { false };
Stream* pTerminal;                                                  // pointer to Serial or TCP terminal



const char SSID [] = SERVER_SSID, PASS [] = SERVER_PASS;
MyTCPconnection myTCPconnection( SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort );   // connect as TCP server


// forward declarations
bool readChar( Stream* pTerminal, char& c );


void setup() {
    Serial.begin( 1000000 );

    pinMode( HEARTBEAT_PIN, OUTPUT );
    pinMode( TCP_CONNECTED_PIN, OUTPUT );

    digitalWrite( HEARTBEAT_PIN, HIGH );                                        // test all leds while waiting for Serial to be ready
    digitalWrite( TCP_CONNECTED_PIN, HIGH );
    delay( 4000 );                                                  // 'while(!Serial) {}' does not seem to work
    digitalWrite( HEARTBEAT_PIN, LOW );
    digitalWrite( TCP_CONNECTED_PIN, LOW );

    Serial.println( "Starting server" );
    Serial.print( "WiFi firmware version  " ); Serial.println( WiFi.firmwareVersion() );
    
    myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );
    myTCPconnection.setVerbose( true );                            // enable debug messages from within myTCPconnection
    myTCPconnection.setConnCallback( (&onConnStateChange) );        // callback function for connection state changes (state machine)
    
    pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
    
    char s [70];
    sprintf( s, "\n========== Server mode: %s ==========", isRemoteMode ? "remote terminal" : "local" );
    Serial.println( s );

    calculator.setHeartbeatCallback( (&heartbeat) );

    dtostrf( 1.0, 4, 1, s );   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers
}


void loop() {
    myTCPconnection.maintainConnection();                           // important to execute regularly; place at beginning of loop()
    heartbeat();

    do {
        char c;
        if ( readChar( &Serial, c ) ) {                                 // read character from Serial, if available (also if in remote mode)
            bool isTerminalCtrl = (c == 1);                              // 0x01 ? swith between local and remote mode ?
            if ( isTerminalCtrl ) {
                isRemoteMode = !isRemoteMode;
                isTerminalCtrl = false;
                myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );
                pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
                char s [70]; sprintf( s, "\n========== Server mode: %s ==========", isRemoteMode ? "remote terminal" : "local" ); Serial.println( s );
            }
            else if ( isRemoteMode ) { break; }                             // not in local mode ? discard character (exit)
        }
        else if ( readChar( myTCPconnection.getClient(), c ) ) {        // read character from remote client, if availabe (even if server is in local mode)
            if ( !isRemoteMode ) { break; }                            // not in remote mode ? discard character (exit) 
        }
        else {break;}                                                   // no character read

        if ( tolower( c ) == 't' ) { calculator.run(); }
    } while ( false );
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


// *** callback function: for general use, called from within myCalculator.run()  
//     every time an input character is processed or an instruction is executed

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
