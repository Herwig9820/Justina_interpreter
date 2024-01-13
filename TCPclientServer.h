// myComm.h

/*
    Test Arduino Nano 33 IoT as TCP server

    Version:    v1.xx - 3/4/2021
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


#ifndef _TCP_CLIENT_SERVER.h
#define _TCP_CLIENT_SERVER.h
#
#if defined(ARDUINO_ARCH_RP2040)
#include <WiFiNINA_Generic.h>
#elif defined (ESP32)
#include <WiFi.h>
#else
#include <WiFiNINA.h>
#endif

#include "Arduino.h"


enum connectionState_type {
    conn_0_wifiNotConnected,                                            // wifi not yet connected
    conn_1_wifiConnected,                                               // wifi connected, but server not yet connected to a client
    conn_2_TCPwaitForConnection,                                        // TCP enabled but not connected
    conn_3_TCPconnected,                                                 // server connected to a client 
    
    conn_11_wifiNoSuccessConnecting                                     // only used for [sys] message
};

enum connectionAction_type {
    action_0_disableWiFi,
    action_1_restartWiFi,                                                   // if not started: start WiFi; if started, then stop and restart
    action_2_TCPkeepAlive,                                                  // assumes WiFi is connected
    action_3_TCPdisConnect,                                             // assumes WiFi is connected
    action_4_TCPdisable
};

// *** Class TCPconnection: declarations ***

class TCPconnection {                                               // class to control connection to wifi and a client, if available

private:
    const char* _SSID, * _PASS;
    IPAddress _serverAddress, _gatewayAddress, _subnetMask, _DNSaddress; //// const

    //// keep alive delay: default aanpassen via function call
    static const unsigned long _wifiConnectDelay { 500 };                   // minimum delay between two attempts to connect to wifi (milliseconds) //// static weg ???
    static const unsigned long _TCPconnectDelay { 500 };                  // minimum delay between stopping and connecting client
    static const unsigned long _isServer_stopDelay { 1000 };              // server: delay before stopping connection to client (and continue listening foe new client)
    static const unsigned long _isServer_keepAliveTimeOut { 60 * 60 * 1000 };       // server: default connection timeout after connection to client
    static const unsigned long _isClient_stopDelay { 1000 };              // client: delay before stopping connection
    static const unsigned long _isClient_keepAliveTimeOut { 10 * 1000 };            // client: default connection timeout after connection to server  

    bool _verbose;
    bool _resetWiFi;
    bool _isClient;
    int _serverPort;

    bool _WiFiEnabled;
    bool _TCPenabled;
    bool _TCPconnTimeoutEnabled;
    connectionState_type _connectionState;                       // state machine: wifi and client connection state
    unsigned long _lastWifiConnectAttempt;                            // timestamps in milliseconds
    unsigned long _lastTCPconnectAttempt;
    unsigned long _keepAliveUntil;
    unsigned long _keepAliveTimeOut;

    WiFiServer _server;                                                     // wifi server object
    WiFiClient _client;                                                     // wifi client object

    // private methods
    void changeConnectionState( connectionState_type newState );                             // change connection state and report to sysTerminal
    void printConnectionStateInfo( connectionState_type newState );
    void maintainWiFiConnection();                                          // attempt to (re-)connect to wifi
    void maintainTCPconnection(bool resetKeepAliveTimer);                                        // attempt to (re-)connect to a client, if available

public:
    // public methods 
    TCPconnection( const char SSID [], const char PASS [],          // constructor: pass IP addresses and server port
        const IPAddress serverAddress, const IPAddress  gatewayAddress, const IPAddress subnetMask, const IPAddress  DNSaddress, const int serverPort, connectionState_type initialConnState );
    TCPconnection( const char SSID [], const char PASS [], const IPAddress serverAddress, const int serverPort, connectionState_type initialConnState );
    void maintainConnection(connectionState_type &connectionState, bool resetKeepAliveTimer=false);                                              // attempt to (re-)connect to wifi and to a client, if available

    void setVerbose( bool verbose );
    void setKeepAliveTimeout(unsigned long keepAliveTimeOut);
    void requestAction( connectionAction_type action, connectionState_type& connState);
    WiFiServer* getServer();    // only if configured as server
    WiFiClient* getClient();
};


#endif
