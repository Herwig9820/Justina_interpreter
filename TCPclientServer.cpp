/*
    Test Arduino Nano 33 IoT as TCP server

    Version:    v1.00 - 3/4/2021
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


#include "TCPclientServer.h"


// *** Class TCPconnection ***

// connect as TCP server
TCPconnection::TCPconnection( const char SSID [], const char PASS [],
    const IPAddress serverAddress, const IPAddress  gatewayAddress, const IPAddress subnetMask, const IPAddress DNSaddress,
    const int serverPort, connectionState_type initialConnState ) : _server( serverPort ) {                // constructor
    _SSID = SSID;                                           
    _PASS = PASS;
    _serverAddress = serverAddress;
    _gatewayAddress = gatewayAddress;
    _subnetMask = subnetMask;
    _DNSaddress = DNSaddress;
    _callbackFcn = nullptr;
    _isClient = false;
    _verbose = false;
    _resetWiFi = false;
    _WiFiEnabled = (initialConnState == conn_1_wifiConnected) || (initialConnState == conn_2_TCPconnected);
    _TCPenabled = (initialConnState == conn_2_TCPconnected);
    _keepAliveTimeOut = _isServer_keepAliveTimeOut;     // default
}

// connect as TCP client
TCPconnection::TCPconnection( const char SSID [], const char PASS [], const IPAddress serverAddress, const int serverPort, connectionState_type initialConnState ) : _server( serverPort ) {                // constructor
    _SSID = SSID;
    _PASS = PASS;
    _serverAddress = serverAddress;
    _serverPort = serverPort;
    _callbackFcn = nullptr;
    _isClient = true;
    _verbose = false;
    _resetWiFi = false;
    _WiFiEnabled = (initialConnState == conn_1_wifiConnected) || (initialConnState == conn_2_TCPconnected);
    _TCPenabled = (initialConnState == conn_2_TCPconnected);
    _keepAliveTimeOut = _isClient_keepAliveTimeOut;     // default
}


void TCPconnection::setVerbose( bool verbose ) { _verbose = verbose; }

void TCPconnection::printRemoteIP(){
    IPAddress IP = _client.remoteIP();
    char s [100];
    sprintf(s, "Remote IP %d.%d.%d.%d", IP[0], IP[1], IP[2], IP[3]);
    Serial.println(s);
}

void TCPconnection::setConnCallback( void (*func)(connectionState_type connectionState) ) { _callbackFcn = func; }

WiFiServer* TCPconnection::getServer() { return &_server; }

WiFiClient* TCPconnection::getClient() { return &_client; }

connectionState_type TCPconnection::getConnectionState() { return _connectionState; }

void TCPconnection::setKeepAliveTimeout( unsigned long keepAliveTimeOut ) {
    _keepAliveTimeOut = keepAliveTimeOut;
    _keepAliveUntil = _keepAliveTimeOut + millis();
    _TCPconnTimeoutEnabled = (_keepAliveTimeOut != 0);
}

void TCPconnection::requestAction( connectionAction_type action ) { // only one action may be set
    bool setTimeOut { false };
    unsigned long TCPtimeout;
    _resetWiFi = (action == action_0_disableWiFi) || (action == action_1_restartWiFi);

    if ( action == action_2_TCPkeepAlive ) {
        TCPtimeout = _keepAliveTimeOut; setTimeOut = true;
    }

    if ( (action == action_3_TCPdoNotKeepAlive) || (action == action_4_TCPdisable) ) {
        TCPtimeout = (_isClient ? _isClient_stopDelay : _isServer_stopDelay); setTimeOut = true;
    }  // start of connection lost timeout period

    if ( setTimeOut ) {
        _keepAliveUntil = TCPtimeout + millis();
        _TCPconnTimeoutEnabled = (TCPtimeout != 0);
    }

    _WiFiEnabled = _WiFiEnabled || (action == action_1_restartWiFi);
    _WiFiEnabled = _WiFiEnabled && (!(action == action_0_disableWiFi));

    _TCPenabled = _TCPenabled || (action == action_2_TCPkeepAlive) || (action == action_3_TCPdoNotKeepAlive);
    _TCPenabled = _TCPenabled && (!(action == action_4_TCPdisable));
}

void TCPconnection::maintainConnection() {
    // variable 'connectionState' controls proper sequencing of tasks in these procedures:
    maintainWiFiConnection();                                                    // if currently not connected to wifi (or connection lost): (try to re-)connect
    maintainTCPconnection();                                                  // if a client is not connected: (try to re-)connect
}


// *** handle WiFi connection, for client and for server side ***

void TCPconnection::maintainWiFiConnection() {
    switch ( _connectionState ) {                                           // connection state

    case conn_0_wifiNotConnected:
        // state: not yet connected to wifi (or connection was lost) OR user request to reset WiFi => (re-)connect to wifi ***
        if ( _WiFiEnabled && (_lastWifiConnectAttempt + _wifiConnectDelay < millis()) ) {     // time out before next WiFi connection attempt reached ?

            if ( _verbose ) { printConnectionStateInfo( conn_0_wifiNotConnected ); }
            if ( !_isClient ) {                                             // if server side (remember: static server IP !)
                WiFi.config( _serverAddress, _DNSaddress,
                    _gatewayAddress, _subnetMask );
            }
            if ( WiFi.begin( (const char*) _SSID,
                (const char*) _PASS ) == WL_CONNECTED ) {
                if ( !_isClient ) { _server.begin(); }                      // if client side: start server
                changeConnectionState( conn_1_wifiConnected );
            }
            else {                                                              // wifi connection timeout: no success
                if ( _verbose ) { printConnectionStateInfo( conn_11_wifiNoSuccessConnecting ); } // but real state does not change (only used for printing)
            }
            _lastWifiConnectAttempt = millis();                             // remember time of last TCP connection attempt
            _resetWiFi = false;                                                    // could have been set while not connected to WiFi
        }
        break;

    default:
        // state: wifi connected => prepare for reconnect if in the meantime connection was lost OR per user program request
        if ( _resetWiFi || (WiFi.status() != WL_CONNECTED) ) {
            changeConnectionState( conn_0_wifiNotConnected );
            WiFi.disconnect();
            WiFi.end();
            _lastWifiConnectAttempt = millis();                             // remember time of last TCP connection attempt
            _resetWiFi = false;
        }
    }
}


// *** handle TCP connection, for client and for server side ***

void TCPconnection::maintainTCPconnection() {
    if ( _connectionState < conn_1_wifiConnected ) { return; }              // even no wifi yet ? nothing to do

    switch ( _connectionState ) {                                           // connection state

    case conn_1_wifiConnected:
        // state: connected to wifi but no TCP connection => start TCP connection
        if ( _TCPenabled && (_lastTCPconnectAttempt + _TCPconnectDelay < millis()) ) {       // time out before next TCP connection attempt reached ?
            if ( !_isClient ) { _client = _server.available(); }            // if server side: attempt to connect to client
            unsigned int startMeas = millis();
            // NOTE: occasionally, a stall occurs while IN _client.connect method and the system hangs
            bool isConnected = _isClient ? _client.connect( _serverAddress, _serverPort ) : _client.connected();

            if ( isConnected ) {
                // if server immediately needs to recognize client connection, add this line here: if ( _isClient ) { _client.println(""); } 
                unsigned long TCPtimeout = _keepAliveTimeOut;  // start of connection lost timeout period
                _keepAliveUntil = TCPtimeout + millis();
                _TCPconnTimeoutEnabled = (TCPtimeout != 0);

                changeConnectionState( conn_2_TCPconnected );               // success: TCP connection live
            }
            _lastTCPconnectAttempt = millis();                              // remember time of last TCP connection attempt
        }
        break;

    default:
        unsigned int startMeas = millis();
        // NOTE: occasionally, a stall occurs while IN _client.connected method and the system hangs
        bool clientConnectionEnd = ((!_client.connected()) || (_TCPconnTimeoutEnabled && (_keepAliveUntil < millis())));

        if ( clientConnectionEnd ) {   // client still connected ? (or still unread data)
            changeConnectionState( conn_1_wifiConnected );
            _client.stop();
            _lastTCPconnectAttempt = millis();                              // remember time of last TCP connection attempt
        }
    }
}

void TCPconnection::changeConnectionState( connectionState_type newState ) {  // *** change connection state and report to serial monitor
    if ( _verbose ) { printConnectionStateInfo( newState ); }            // before _connectionState is changed
    _connectionState = newState;
    if ( _callbackFcn != nullptr ) { ; _callbackFcn( _connectionState ); }
}

void TCPconnection::printConnectionStateInfo( connectionState_type newState ) {
    char stateChange [40], reason [40], s [100];
    IPAddress IP;
    sprintf( stateChange, "[TCP debug] at %ld s: S%d->S%d", millis() / 1000, _connectionState, newState );

    switch ( newState ) {
    case conn_0_wifiNotConnected:
        if ( _connectionState == conn_0_wifiNotConnected ) {
            sprintf( s, "[TCP debug] at %ld s: S%d Connecting to WiFi, SSID = %s", millis() / 1000, newState, _SSID );
        }
        else {
            if ( WiFi.status() != WL_CONNECTED ) { strcpy( reason, "WiFi connection lost. Reconnecting in a moment... " ); }   
            else { strcpy( reason, "Disabling WiFi" ); }
            sprintf( s, "%s %s", stateChange, reason );
        }
        break;

    case conn_11_wifiNoSuccessConnecting:               // status only used as a flag for printing this message
        sprintf( s, "[TCP debug] at %ld s: S%d Trying again...", millis() / 1000, _connectionState ); // use existing status 
        break;

    case conn_1_wifiConnected:
        if ( _connectionState == conn_0_wifiNotConnected ) {  // previous state
            IP = WiFi.localIP();
            sprintf( s, ("%s Connected to WiFi, IP %d.%d.%d.%d (%ld dBm)"), stateChange, IP [0], IP [1], IP [2], IP [3], WiFi.RSSI() );

        }
        else {
            if ( !_client.connected() ) { strcpy( reason, "Other side disconnected" ); }   
            else { strcpy( reason, "Stopping TCP connection" ); }
            sprintf( s, "%s %s", stateChange, reason );

        }
        break;

    case conn_2_TCPconnected:
        IPAddress IP = _client.remoteIP();
        sprintf( s, "%s Connected, remote IP %d.%d.%d.%d", stateChange, IP [0], IP [1], IP [2], IP [3] );
        break;
    }

    Serial.println( s );
}
