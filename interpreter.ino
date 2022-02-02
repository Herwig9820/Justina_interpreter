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

#if AS_SERVER

enum requestState_type {                                            // controls receiving client request, processing and sending response
    req_0_init,
    req_1_readRequest,
    req_2_parseRequest,
    req_3_processRequest,
    req_4_sendResponse
};

bool keepAlive { true };
uint32_t requestsReceived { 0 };
const int maxRequestChars {700};//// logica anders: in immediate mode loopt buffer niet vol
const int maxCharsPretty {2000};//// verkleinen, print instructie per instructie

char request [maxRequestChars + 1] = "";
char pretty[maxCharsPretty];   
char parsingInfo[200];

const char SSID [] = SERVER_SSID, PASS [] = SERVER_PASS;
requestState_type requestState { req_0_init };
MyTCPconnection myTCPconnection( SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort );   // connect as TCP server


// forward declarations 
void init( int& receivedRequestChars );
void readRequest( int& receivedRequestChars );
void parseInstruction();
void processRequest( int& receivedRequestChars );
void sendResponse( int& receivedRequestChars );

#else
enum requestState_type {                                            // controls receiving client request, processing and sending response
    req_0_idle,
    req_1_sendRequest,
    req_2_receiveResponse,
    req_3_postProcess
};

uint32_t responsesReceived { 0 };
const char SSID [] = CLIENT_SSID, PASS [] = CLIENT_PASS;
requestState_type requestState { req_1_sendRequest };
MyTCPconnection myTCPconnection( SSID, PASS, remoteServerAddress, remoteServerPort );                               // connect as client

// forward declarations
void preProcess();
void sendRequest();
void receiveResponse();
void postProcess();
#endif


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
    delay( 5000 );                                                  // 'while(!Serial) {}' does not seem to work
    digitalWrite( 9, LOW );
    digitalWrite( 10, LOW );
    digitalWrite( TCP_REC_DATA_PIN, LOW );
    digitalWrite( TCP_SEND_DATA_PIN, LOW );

    myTCPconnection.setConnCallback( (&onConnStateChange) );        // callback function for connection state changes (state machine)
#if AS_SERVER
    Serial.println( "Starting server" );
    Serial.print( "WiFi firmware version  " ); Serial.println( WiFi.firmwareVersion() );
    myTCPconnection.setVerbose( true );                            // enable debug messages from within myTCPconnection
    isRemoteMode = false;
    myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );
    pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
    char s [70];
    sprintf( s, "\n========== Server mode: %s ==========", isRemoteMode ? "remote terminal" : "local" );
    Serial.println( s );


#else
    Serial.println( "Starting client" );
    Serial.print( "WiFi firmware version  " ); Serial.println( WiFi.firmwareVersion() );
    myTCPconnection.setVerbose( true );
    TCPenabled = true;
    myTCPconnection.requestAction( TCPenabled ? action_2_TCPkeepAlive : action_4_TCPdisable );
    pTerminal = myTCPconnection.getClient();
    char s [70];
    sprintf( s, "\n========== Terminal %s ==========", TCPenabled ? "enabled" : "disabled" );
    Serial.println( s );
#endif // AS_SERVER
    dtostrf( 1.0, 4, 1, s );   // not used, but needed to circumvent a bug in sprintf function with %F, %E, %G specifiers

}


void loop() {
    static int receivedRequestChars { 0 };
    myTCPconnection.maintainConnection();                           // important to execute regularly; place at beginning of loop()
    heartbeat();

#if AS_SERVER                                                       // act as a TCP server
    init( receivedRequestChars );
    readRequest( receivedRequestChars );
    parseInstruction( receivedRequestChars );
    processRequest( receivedRequestChars );
    sendResponse( receivedRequestChars );
    delay(100);////
#else                                                               // act as a TCP client 
    preProcess();
    sendRequest();
    receiveResponse();
    postProcess();
#endif
}



#if AS_SERVER

void init( int& receivedRequestChars ) {
    if ( requestState != req_0_init ) { return; }

    strcpy( request, "" );
    receivedRequestChars = 0;
    requestState = req_1_readRequest;
}

void readRequest( int& receivedRequestChars ) {                                        // receive and parse incoming message
    // state machine: only execute when ready to read characters (local AND remote, as long as there are characters available)
    if ( requestState != req_1_readRequest ) { return; }           // not currently reading request characters

    char c;
    bool charToProcess;
    bool isTerminalCtrl;

    do {
        do {
            if ( readChar( &Serial, c ) ) {          //  read character from Serial, if available
                isTerminalCtrl = (c == 1);
                if ( isTerminalCtrl || !isRemoteMode ) { break; }  // break for further processing if local mode OR is terminal ctrl character, otherwise discard character
            }
            else if ( readChar( myTCPconnection.getClient(), c ) ) {    // read character from remote client, if availabe (even if server is in local mode)
                isTerminalCtrl = false;
                if ( isRemoteMode ) { break; }       // break for further processing if remote mode, discard character if in local mode
            }
            else { digitalWrite( TCP_REC_DATA_PIN, LOW ); return; }             // no character available: exit routine
        } while ( true );

        digitalWrite( TCP_REC_DATA_PIN, HIGH ); //// niet correct

        if ( isTerminalCtrl ) {
            isRemoteMode = !isRemoteMode;
            myTCPconnection.requestAction( isRemoteMode ? action_2_TCPkeepAlive : action_4_TCPdisable );
            pTerminal = (isRemoteMode) ? myTCPconnection.getClient() : (Stream*) &Serial;
            char s [70]; sprintf( s, "\n========== Server mode: %s ==========", isRemoteMode ? "remote terminal" : "local" ); Serial.println( s );
            ////char s [70]; sprintf( s, "\n Server mode: %s ", isRemoteMode ? "remote terminal" : "local" ); Serial.println( s );
            return;  // exit to allow handling switch from/to remote in next call to  myTCPconnection.maintainConnection()
        }

        if ( (c < ' ') && (c != '\n') ) { return; }                     // skip control-chars except new line character
        if ( (c != '\n') && (receivedRequestChars == maxRequestChars) ) {
            request [receivedRequestChars] = '\0';
            return;
        }

        if ( c == '\n' ) {                                          // end of line character
            request [receivedRequestChars] = '\0';                             // 0 to maxChars - 1
        }
        else {                                      // printable character
            request [receivedRequestChars] = c;                             // 0 to maxChars - 1
            receivedRequestChars++;                                         // 1 to maxChars
        }

    } while ( c != '\n' );

    digitalWrite( TCP_REC_DATA_PIN, LOW );

    requestsReceived++;
    requestState = req_2_parseRequest;
}

void parseInstruction( int& receivedRequestChars ) {
    if ( requestState != req_2_parseRequest ) { return ; }
    uint8_t  result=  calculator.processSource(request , parsingInfo, pretty, maxCharsPretty);
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
/*
    if ( isFormula ) { // pretty print formula
        ////sprintf( response, "%.3G %c %.3G = %.3G\r\n", d1, operators [op], d2, result );
    }
    else if ( isCommand ) {
        if ( true ) { //// ( parsedToken [cnt].palphaConst != nullptr ) {
            sprintf( response, "command\r\n" );
            ////sprintf( response, "%s %s : OK\r\n", _resWords [resWord], parsedToken [cnt].palphaConst );
            ////delete [] parsedToken [cnt].palphaConst;

        }
    }
    else {  // print a response indicating the server time and the length of the request received
        sprintf( response, "Unknown cmd %ld: server time is %ld, request length %d\r\n", requestsReceived, millis() / 1000, receivedRequestChars );
    }
    */
    
    digitalWrite( TCP_SEND_DATA_PIN, HIGH );
    pTerminal->println( "--------------------------------------\r\npretty: " );
    if(strcmp(pretty, "")) {pTerminal->println(pretty);}
    pTerminal->println( parsingInfo );


    

    int writeError { false };
    reportWriteError( writeError );
    digitalWrite( TCP_SEND_DATA_PIN, LOW );

    // extend server connection time out (long or short) after last response to client
    if ( isRemoteMode ) { myTCPconnection.requestAction( (!writeError && keepAlive) ? action_2_TCPkeepAlive : action_3_TCPdoNotKeepAlive ); }

    requestState = req_0_init;
}


#else


void preProcess() {
}

void sendRequest() {
    char c;
    static int receivedResponseChars { 0 };
    static int heartbeatCount { 0 };
    bool charsSent { false };

    while ( readChar( &Serial, c ) ) {     // read all available characters from Serial and send immediately to server
        bool isTerminalCtrl = (c == 1);
        if ( isTerminalCtrl ) {
            TCPenabled = !TCPenabled;
            myTCPconnection.requestAction( TCPenabled ? action_2_TCPkeepAlive : action_4_TCPdisable );
            char s [70];
            sprintf( s, "\n========== Terminal %s ==========", TCPenabled ? "enabled" : "disabled" );
            Serial.println( s );
        }

        else if ( TCPconnected && TCPenabled ) {                      // connected to server: act as a terminal (user IO via Serial)
            heartbeatCount = 0; // no sending of automatic messages for a while 
            digitalWrite( TCP_SEND_DATA_PIN, HIGH );
            pTerminal->print( c );
            charsSent = true;
        }
    }

    if ( TCPconnected && TCPenabled ) {
        if ( heartbeatOccured ) {
            heartbeatOccured = false;
            heartbeatCount++;
            if ( heartbeatCount == 5 ) {
                heartbeatCount = 0;
                digitalWrite( TCP_SEND_DATA_PIN, HIGH );

                char s [100] = "";
                sprintf( s, "Client time is %ld s\r\n", millis() / 1000 );
                pTerminal->print( s );
                charsSent = true;
            }
        }

        int writeError { false };
        if ( charsSent ) { reportWriteError( writeError ); }
        digitalWrite( TCP_SEND_DATA_PIN, LOW );
    }
}

void receiveResponse() {
    char c;
    while ( readChar( pTerminal, c ) ) {     // read all available characters from server and send immediately to Serial
        digitalWrite( TCP_REC_DATA_PIN, HIGH );
        Serial.print( c );
    }










    digitalWrite( TCP_REC_DATA_PIN, LOW );

}

void postProcess() {

}
#endif


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
