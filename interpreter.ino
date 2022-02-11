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

#include "myParser.h"
#include <avr/dtostrf.h>
#include <stdlib.h>
#include "myComm.h"
#include "secrets.h"


constexpr pin_size_t HEARTBEAT_PIN { 9 };                    // indicator leds
// pin 10 (temp.) used within myComm.cpp
constexpr pin_size_t TCP_REC_DATA_PIN { 11 };
constexpr pin_size_t TCP_SEND_DATA_PIN { 12 };
constexpr pin_size_t TCP_CONNECTED_PIN { 13 };


bool isRemoteMode { false };
bool TCPconnected { false };
bool TCPenabled { false };
bool toggleHeartbeat { false }, heartbeatOccured { false };
unsigned long lastHeartbeat { 0 };                                  // last heartbeat time in ms
IPAddress remoteIP;
connectionState_type connectionState { conn_0_wifiNotConnected };   // last known WiFi and TCP connection state
Stream* pTerminal;                                                  // pointer to Serial or TCP terminal


enum requestState_type {                                            // controls receiving client request, processing and sending response
    req_0_init,
    req_1_readRequest,
    req_2_parseRequest,
    req_3_processRequest,
    req_4_sendResponse
};

bool keepAlive { true };
uint32_t requestsReceived { 0 };

const char SSID [] = SERVER_SSID, PASS [] = SERVER_PASS;
requestState_type requestState { req_0_init };
MyTCPconnection myTCPconnection( SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort );   // connect as TCP server


// forward declarations 
void init( int& receivedRequestChars );
void readRequest( int& receivedRequestChars );
void parseInstruction();
void processRequest( int& receivedRequestChars );
void sendResponse( int& receivedRequestChars );



// forward declarations
bool readChar( Stream* pTerminal, char& c );
void heartbeat();
void onConnStateChange( connectionState_type  connectionState );
void reportWriteError( int& writeError );
void TCPkeepAliveOrStop( const int& writeError, const bool& keepAlive );

void setup() {
    Serial.begin( 1000000 );

    pinMode( 8, OUTPUT );
    pinMode( 9, OUTPUT );
    pinMode( 10, OUTPUT );
    pinMode( TCP_REC_DATA_PIN, OUTPUT );                         // indicator leds 
    pinMode( TCP_SEND_DATA_PIN, OUTPUT );
    pinMode( TCP_CONNECTED_PIN, OUTPUT );

    digitalWrite( 9, HIGH );                                        // test all leds while waiting for Serial to be ready
    digitalWrite( 10, HIGH );
    digitalWrite( TCP_REC_DATA_PIN, HIGH );
    digitalWrite( TCP_SEND_DATA_PIN, HIGH );
    delay( 4000 );                                                  // 'while(!Serial) {}' does not seem to work
    digitalWrite( 9, LOW );
    digitalWrite( 10, LOW );
    digitalWrite( TCP_REC_DATA_PIN, LOW );
    digitalWrite( TCP_SEND_DATA_PIN, LOW );

    myTCPconnection.setConnCallback( (&onConnStateChange) );        // callback function for connection state changes (state machine)
    Serial.println( "Starting server" );
    Serial.print( "WiFi firmware version  " ); Serial.println( WiFi.firmwareVersion() );
    myTCPconnection.setVerbose( true );                            // enable debug messages from within myTCPconnection
    isRemoteMode = false;
    myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );
    pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
    char s [70];
    sprintf( s, "\n========== Server mode: %s ==========", isRemoteMode ? "remote terminal" : "local" );
    Serial.println( s );

    dtostrf( 1.0, 4, 1, s );   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers


    Serial.println("****** test");
    char c = 130;
    int i = 130;
    Serial.println( i ==  c );
    Serial.println( i == (int8_t) c );
    Serial.println( i == (uint8_t) c );
    Serial.println( i == (int16_t) c );
    Serial.println( i == (uint16_t) c );
    Serial.println( i == (int) c );
    Serial.println( i == (uint) c );
}


void loop() {
    static int receivedRequestChars { 0 };
    myTCPconnection.maintainConnection();                           // important to execute regularly; place at beginning of loop()
    heartbeat();

    init( receivedRequestChars );
    readRequest( receivedRequestChars );
    parseInstruction( receivedRequestChars );
    processRequest( receivedRequestChars );
    sendResponse( receivedRequestChars );
}




void init( int& receivedRequestChars ) {
    if ( requestState != req_0_init ) { return; }

    requestState = req_1_readRequest;
}

void readRequest( int& receivedRequestChars ) {                                        // receive and parse incoming message
    // state machine: only execute when ready to read characters (local AND remote, as long as there are characters available)
    if ( requestState != req_1_readRequest ) { return; }           // not currently reading request characters

    // fetch character
    char c;
    
    if ( readChar( &Serial, c ) ) {                                 // read character from Serial, if available (also if in remote mode)
        bool isTerminalCtrl = (c == 1);                                  // swith between local and remote mode ?
        if ( isTerminalCtrl ) {
            isRemoteMode = !isRemoteMode;
            isTerminalCtrl = false;
            myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );
            pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
            char s [70]; sprintf( s, "\n========== Server mode: %s ==========", isRemoteMode ? "remote terminal" : "local" ); Serial.println( s );
            return;  // exit to allow handling switch from/to remote in next call to  myTCPconnection.maintainConnection()
        }
        if ( isRemoteMode ) { return; }                             // not in local mode ? discard character (exit)

    }
    else if ( readChar( myTCPconnection.getClient(), c ) ) {        // read character from remote client, if availabe (even if server is in local mode)
        if ( !isRemoteMode ) { return; }                            // not in remote mode ? discard character (exit) 
    }
    
    else { return; }                                                         // no character available

    calculator.processCharacter( c );

    requestsReceived++;
    requestState = req_2_parseRequest;
}

void parseInstruction( int& receivedRequestChars ) {
    if ( requestState != req_2_parseRequest ) { return; }
    requestState = req_3_processRequest;
}

void processRequest( int& receivedRequestChars ) {                                             // process a parsed request
    // state machine: only execute when a parsed formula result can be calculated (or a text line is complete)
    if ( requestState != req_3_processRequest ) { return; }

    requestState = req_4_sendResponse;      // adapt state

}

void sendResponse( int& receivedRequestChars ) {
    // state machine: only execute when ready for printing a response to terminal (local or remote)
    if ( requestState != req_4_sendResponse ) { return; }
    if ( isRemoteMode && !TCPconnected ) { return; }                // wait with sending response until client is connected again

    digitalWrite( TCP_SEND_DATA_PIN, HIGH );
    int writeError { false };
    reportWriteError( writeError );
    digitalWrite( TCP_SEND_DATA_PIN, LOW );

    // extend server connection time out (long or short) after last response to client
    if ( isRemoteMode ) { myTCPconnection.requestAction( (!writeError && keepAlive) ? action_2_TCPkeepAlive : action_3_TCPdoNotKeepAlive ); }

    requestState = req_0_init;
}




// *** callback function: called from within myTCPconnection.maintainConnection() when connection state changes
// note that, if the TCP connection status changes during execution of the user program, this will only be reflected 
// next time myTCPconnection.maintainConnection() is called (call should be placed at the start of the main loop() )

void onConnStateChange( connectionState_type  connectionState ) {
    TCPconnected = (connectionState == conn_2_TCPconnected);
    if ( TCPconnected ) { remoteIP = ((WiFiClient*) pTerminal)->remoteIP(); }
    digitalWrite( TCP_CONNECTED_PIN, TCPconnected );                 // flag 'client connected'
}


// read a character from Stream (TCPclient or Serial)
bool readChar( Stream* pTerminal, char& c ) {
    bool found = (pTerminal->available() > 0);
    if ( found ) {     // while terminal characters available for reading
        c = pTerminal->read();
    }
    return found;
}


void reportWriteError( int& writeError ) {
    writeError = pTerminal->getWriteError();
    if ( writeError ) {
        Serial.print( "***** write error *** at " ); Serial.println( millis() );
        pTerminal->clearWriteError();
    }
}

void heartbeat() {
    uint32_t currentTime = millis();
    if ( lastHeartbeat + 1000UL < currentTime ) {  // heartbeat
        toggleHeartbeat = !toggleHeartbeat;
        heartbeatOccured = true;
        digitalWrite( HEARTBEAT_PIN, toggleHeartbeat );
        lastHeartbeat = currentTime;
    }
}
