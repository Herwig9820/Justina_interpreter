/*************************************************************************************************************************
*   Example Arduino sketch demonstrating Justina interpreter functionality												 *
*                                                                                                                        *
*   The Justina interpreter library is licensed under the terms of the GNU General Public License v3.0 as published      *
*   by the Free Software Foundation (https://www.gnu.org/licenses).                                                      *
*   Refer to GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter            *
*                                                                                                                        *
*   This example code is in the public domain                                                                            *
*                                                                                                                        *
*   2024, Herwig Taveirne                                                                                                *
*************************************************************************************************************************/

#include "Justina_TCPIP.h"

/*
    Setup an Arduino as a TCP/IP server or client.
    This code also maintains the connection: method maintainConnection() MUST BE CALLED REGULARLY from your program main loop. 
    This allows you to isolate your application (an HTTP server, ...) from this TCP/IP maintenance code.

    The constructor called will define whether Arduino is set up as a server or a client. 
    WiFi maintenance and TCP/IP connection maintenance is split into two different methods.
    Variable '_connectionState' maintains the state of the connection ('state machine'). If this maintained state
    (e.g., 'WiFi connected') does not correspond to the actual state (e.g., WiFi connection was lost) OR your application 
    requests a state change (e.g., 'switch off WiFi'), action is taken.
    
    A number of utility functions are provided to switch WiFi on or off, to allow a TCP/IP connections or not, etc.
	--------------------------------------------------------------------------------------------------------------------------
*/
		

// ******************************************************************
// ***                     class TCPconnection                    ***
// ******************************************************************

// -------------------
// *   constructor   *
// -------------------

// connect as TCP server
// ---------------------

TCPconnection::TCPconnection(const char SSID[], const char PASS[],
    const IPAddress serverAddress, const IPAddress  gatewayAddress, const IPAddress subnetMask, const IPAddress DNSaddress,
    const int serverPort, connectionState_type initialConnState) : _server(serverPort) {
    _SSID = SSID;
    _PASS = PASS;
    _serverAddress = serverAddress;
    _gatewayAddress = gatewayAddress;
    _subnetMask = subnetMask;
    _DNSaddress = DNSaddress;
    _isClient = false;
    _verbose = false;
    _resetWiFi = false;
    _WiFiEnabled = (initialConnState >= conn_2_WiFi_connected);
    _TCPenabled = (initialConnState == conn_4_TCP_clientConnected);
}

// constructor: connect as TCP client
// ----------------------------------
TCPconnection::TCPconnection(const char SSID[], const char PASS[], const IPAddress serverAddress, const int serverPort,
    connectionState_type initialConnState) : _server(serverPort) {

    _SSID = SSID;
    _PASS = PASS;
    _serverAddress = serverAddress;
    _serverPort = serverPort;
    _isClient = true;
    _verbose = false;
    _resetWiFi = false;
    _WiFiEnabled = (initialConnState >= conn_2_WiFi_connected);
    _TCPenabled = (initialConnState == conn_4_TCP_clientConnected);
}


// *********************************************************************************************
// ***   Connection maintenance. Call this function regularly from within the user program   ***
// *********************************************************************************************

// ---------------------------
// *   maintain connection   *
// ---------------------------

void TCPconnection::maintainConnection() {

    // variable '_connectionState' controls proper sequencing of tasks in these procedures:
    maintainWiFiConnection();
    maintainTCPconnection();
}


// ---------------------------------------------------------------------
// *   maintain WiFi connection, for Arduino as client and as server   *
// ---------------------------------------------------------------------

void TCPconnection::maintainWiFiConnection() {
    switch (_connectionState) {

        // state: WiFi is currently not connected
        // --------------------------------------
        case conn_0_WiFi_notConnected:
        {
            // WiFi is enabled AND it's time for a next WiFi connection attempt ?
            if (_WiFiEnabled && (_lastWiFiMaintenanceTime + WIFI_UP_CHECK_INTERVAL < millis())) {

                if (!_isClient) {                                                       // is server side ? set static server IP
                #if defined ARDUINO_ARCH_ESP32
                    WiFi.config(_serverAddress, _gatewayAddress, _subnetMask, _DNSaddress);
                #else
                    WiFi.config(_serverAddress, _DNSaddress, _gatewayAddress, _subnetMask);
                #endif
                }

                WiFi.begin((const char*)_SSID, (const char*)_PASS);
                _connectionState = conn_1_WiFi_waitForConnecton;
                if (_verbose) { Serial.println(_isClient ? "\r\n-- Trying to connect WiFi..." : "\r\n-- Trying to connect TCP/IP server to WiFi..."); }
                // remember time of this WiFi connection attempt
                _WiFiWaitingForConnectonAt = _lastWiFiMaintenanceTime = millis();       // remember time of last WiFi maintenance AND time of this WiFi connection attempt
                _resetWiFi = false;
            }
        }
        break;


        // state: currently waiting for WiFi connection
        // --------------------------------------------
        case conn_1_WiFi_waitForConnecton:
        {
            // WiFi is enabled AND it's time for a next WiFi connection check ?
            if (_WiFiEnabled && (_lastWiFiMaintenanceTime + WIFI_UP_CHECK_INTERVAL < millis())) {
                if (WiFi.status() == WL_CONNECTED) {                                    // WiFi is now connected ?
                    if (!_isClient) { _server.begin(); }                                // if Arduino as server, start server
                    _connectionState = conn_2_WiFi_connected;
                    if (_verbose) {
                        IPAddress IP = WiFi.localIP();
                        char s[100];
                        sprintf(s, "\r\n-- %sWiFi connected, local IP %d.%d.%d.%d (%ld dBm)", (_isClient ? "" : "TCP/IP server started. "), IP[0], IP[1], IP[2], IP[3], WiFi.RSSI());
                        Serial.println(s);
                    }
                }

                else {                                                                  // WiFi is not yet connected
                    // regularly report status ('still trying...' etc.)
                    if (_WiFiWaitingForConnectonAt + 5000 < millis() && _verbose) {
                        _WiFiWaitingForConnectonAt = millis();                          // for printing only
                        if (_verbose) { Serial.print("."); }
                    }
                }
                _lastWiFiMaintenanceTime = millis();                                    // remember time of last WiFi maintenance 
                _resetWiFi = false;                                                     // could have been set while not connected to WiFi
            }
        }
        break;


        // state: WiFi is connected
        // ------------------------
        default:
        {
            //  prepare for reconnection if connection is lost OR per user program request 
            if (_resetWiFi || (WiFi.status() != WL_CONNECTED)) {
                _connectionState = conn_0_WiFi_notConnected;
                if (_verbose) { Serial.println(_isClient ? "\r\n-- WiFi disconnected" : "\r\n-- WiFi disconnected, TCP/IP server stopped"); }
                WiFi.disconnect();
            #if !defined ARDUINO_ARCH_ESP32
                WiFi.end();
            #endif
                _lastWiFiMaintenanceTime = millis();                                    // remember time of last WiFi maintenance 
                _resetWiFi = false;
            }
        }
    }
}


// --------------------------------------------------------------------------
// *   maintain TCP connection, for Arduino set up as client or as server   *
// --------------------------------------------------------------------------

void TCPconnection::maintainTCPconnection() {
    if (_connectionState < conn_2_WiFi_connected) { return; }                           // even no WiFi yet ? nothing to do

    switch (_connectionState) {

        // state: WiFi is connected; TCP is disabled
        // -----------------------------------------
        case conn_2_WiFi_connected:
            if (_TCPenabled) {
                _connectionState = conn_3_TCPwaitForNewClient;
                if (_verbose) { Serial.println(_isClient ? "\r\n-- trying to connect to server" : "\r\n-- waiting for a client"); }
            }
            break;


        // state: waiting for a TCP client
        // -------------------------------
        case conn_3_TCPwaitForNewClient:
        {
            if (_TCPenabled) {
                if (_isClient) { _client.connect(_serverAddress, _serverPort); }        // Arduino as client ? connect to server
                else { _client = _server.available(); }                                 // Arduino as server ? get reference to client (if connected)


                if (_client) {
                    if (_verbose) {
                        IPAddress IP = _client.remoteIP();
                        char s[100];
                        sprintf(s, "\r\n-- %s, remote IP %d.%d.%d.%d", (_isClient ? "connected to server" : "client connected"), IP[0], IP[1], IP[2], IP[3]);
                        Serial.println(s);
                    }
                    _connectionState = conn_4_TCP_clientConnected;

                }
            }
            else {
                _connectionState = conn_2_WiFi_connected;
            }
        }
        break;


        // state: TCP client is connected
        // ------------------------------
        case conn_4_TCP_clientConnected:
        {
            if (!_client.connected() || !_TCPenabled) {
                _client.stop();
                if (_verbose) { Serial.println(_isClient ? "\r\n-- disconnected from server" : "\r\n-- client disconnected"); }
                _connectionState = conn_2_WiFi_connected;
            }
        }
    }
}


// *****************************
// ***   Utility functions   ***
// *****************************

// ------------------------------------------------------
// *   return a pointer to server resp. client object   *
// ------------------------------------------------------

WiFiServer* TCPconnection::getServer() {return _isClient ? nullptr : &_server; }
WiFiClient* TCPconnection::getClient() { return &_client; }


// ------------------------------------
// *   TCP/IP connection: settings    *
// ------------------------------------

void TCPconnection::WiFiOff() {                     // switch off WiFi antenna
    _resetWiFi = true;
    _WiFiEnabled = false;
}
void TCPconnection::WiFiRestart() {                 // restart Wifi: switch off first (if WiFi currently on), and start again
    _resetWiFi = true;
    _WiFiEnabled = true;
}
void TCPconnection::TCPdisable() {                  // disable TCP IO
    _client.stop();
    _TCPenabled = false;
}
void TCPconnection::TCPenable() {                   // enable TCP IO
    _TCPenabled = true;
}


// --------------------
// *   stop client    *
// --------------------

void TCPconnection::stopClient() {
    if (_connectionState == conn_4_TCP_clientConnected) {
        _client.stop();
        _connectionState = conn_3_TCPwaitForNewClient;
        if (_verbose) { Serial.println("\r\n-- stop client: client disconnected"); }
    }
}


// ----------------------------------
// *   set verbose mode ON or OFF   *
// ----------------------------------

void TCPconnection::setVerbose(bool verbose) { _verbose = verbose; }


// --------------------------------
// *   return connection state    *
// --------------------------------

TCPconnection::connectionState_type TCPconnection::getConnectionState() {
    return _connectionState;
}


