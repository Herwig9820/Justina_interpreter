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

#include "Justina_TCPIP_multi.h"

/*
    Setup an Arduino as a TCP/IP server or client.
    This code also maintains the connection: method maintainConnection() MUST BE CALLED REGULARLY from your program main loop.
    This allows you to isolate your application (an HTTP server, ...) from this TCP/IP maintenance code.

    The constructor called will define whether Arduino is set up as a server or a client.
    WiFi maintenance and TCP/IP connection maintenance is split into two different methods.
    Variable '_WiFiState' maintains the state of the connection ('state machine'). If this maintained state
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
    const int serverPort, bool WiFiEnable, bool TCPenable, Stream** pStream, int TCPclientSlots) : _server(serverPort) {
    _SSID = SSID;
    _PASS = PASS;
    _serverAddress = serverAddress;
    _gatewayAddress = gatewayAddress;
    _subnetMask = subnetMask;
    _DNSaddress = DNSaddress;
    _setupAsClient = false;
    _verbose = false;
    _resetWiFi = false;
    _WiFiEnabled = WiFiEnable;
    _TCPenabled = TCPenable;

    _TCPclientSlots = _maxSessions = min(TCPclientSlots, MAX_CLIENT_SLOTS - 1);     // MAX_CLIENT_SLOTS: depends on board type. -1: for temporary 'new client' object

    // pointer to array of client data
    _pWiFiClientData = new WiFiClientData[_TCPclientSlots];              // this is the maximum for ESP32 & nano 33 IoT, given that there is a temporary 'newClient' as well (max 4 concurrent clients)
    _pSessionData = new SessionData[_TCPclientSlots];                      // equal size of _pWiFiClientData

    // store stream pointers to the client objects: pass to the calling c++ program
    for (int i = 0; i < _TCPclientSlots; i++) {
        pStream[i] = static_cast <Stream*>(&_pWiFiClientData[i].client);
    }
}



// ------------------
// *   destructor   *
// ------------------

TCPconnection::~TCPconnection() {
    delete[] _pWiFiClientData;
    delete[] _pSessionData;
}


// *********************************************************************************************
// ***   Connection maintenance. Call this function regularly from within the user program   ***
// *********************************************************************************************

// ---------------------------
// *   maintain connection   *
// ---------------------------

void TCPconnection::maintainConnection() {

    // variable '_WiFiState' controls proper sequencing of tasks in these procedures:
    maintainWiFiConnection();
    maintainTCPclients();
}


// ---------------------------------------------------------------------
// *   maintain WiFi connection, for Arduino as client and as server   *
// ---------------------------------------------------------------------

void TCPconnection::maintainWiFiConnection() {
    switch (_WiFiState) {

        // state: WiFi is currently not connected
        // --------------------------------------
        case conn_0_WiFi_notConnected:
        {
            // WiFi is enabled AND it's time for a next WiFi connection attempt ?
            if (_WiFiEnabled && (_lastWiFiMaintenanceTime + WIFI_UP_CHECK_INTERVAL < millis())) {

                if (!_setupAsClient) {                                                       // is server side ? set static server IP
                #if defined ARDUINO_ARCH_ESP32
                    WiFi.config(_serverAddress, _gatewayAddress, _subnetMask, _DNSaddress);
                #else
                    WiFi.config(_serverAddress, _DNSaddress, _gatewayAddress, _subnetMask);
                #endif
                }

                WiFi.begin((const char*)_SSID, (const char*)_PASS);
                _WiFiState = conn_1_WiFi_waitForConnecton;
                if (_verbose) { Serial.println(_setupAsClient ? "\r\n-- Trying to connect WiFi..." : "\r\n-- Trying to connect TCP/IP server to WiFi..."); }
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
                    if (!_setupAsClient) { _server.begin(); }                                // if Arduino as server, start server
                    _WiFiState = conn_2_WiFi_connected;
                    if (_verbose) {
                        IPAddress IP = WiFi.localIP();
                        Serial.printf("\r\n-- at %7lus: %sWiFi connected, local IP %d.%d.%d.%d (%ld dBm)\r\n", 
                            millis()/1000, (_setupAsClient ? "" : "TCP/IP server started. "), IP[0], IP[1], IP[2], IP[3], WiFi.RSSI());
                    }
                }

                else {                                                                  // WiFi is not yet connected
                    // regularly report status ('still trying...' etc.)
                    if (_WiFiWaitingForConnectonAt + WIFI_REPORT_INTERVAL < millis() && _verbose) {
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
                _WiFiState = conn_0_WiFi_notConnected;
                if (_verbose) { Serial.printf("\r\n-- at %7lus: %s\r\n", millis()/1000, _setupAsClient ? "WiFi disconnected" : "WiFi disconnected, TCP/IP server stopped"); }
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


// ---------------------------------------------------------------------------
// *   maintain TCP connections, for Arduino set up as client or as server   *
// ---------------------------------------------------------------------------

void TCPconnection::maintainTCPclients() {

    // Process active clients (do this first, if all slots are used then it might free up a slot needed by a new client)
    for (int i = 0; i < _TCPclientSlots; i++) {
        if (_pWiFiClientData[i].state != IDLE) {
            if ((!_pWiFiClientData[i].client.connected()) || (_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) {
                IPAddress clientIP{ 0,0,0,0 };                                        // init, in case session index would be inconsistent (safety)
                int sessionID = _pWiFiClientData[i].sessionIndex;
                int slot {-1};
                if (sessionID >= 0 && sessionID < _maxSessions) {                       // safety
                    slot = _pSessionData[sessionID].clientSlotNumber;
                    _pSessionData[sessionID].clientSlotNumber = -1;                     // remove reverse link from session data
                    clientIP = _pSessionData[sessionID].IP;
                }
                _pWiFiClientData[i].client.stop();
                _pWiFiClientData[i].state = IDLE;
                _pWiFiClientData[i].sessionIndex = -1;

                if (_verbose) { Serial.printf("\r\n-- at %7lus: session %d (CURR): client (slot %d) disconnected, remote IP %d.%d.%d.%d\r\n", 
                    millis() / 1000, sessionID, slot, clientIP[0], clientIP[1], clientIP[2], clientIP[3]); }
            }
        }
    }

    if ((_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) { return; }

    // Accept new client
    WiFiClient newClient = _server.available();
    if (newClient) {
        if (_verbose) {Serial.printf("\r\n-- at %7lus: new client found\r\n", millis()/1000); }
        // find a free client slot for the new client 
        int slot{};
        for (slot = 0; slot < _TCPclientSlots; slot++) {
            if (_pWiFiClientData[slot].state == IDLE) {
                _pWiFiClientData[slot].state = CONNECTED;               // not yet authenticated
                _pWiFiClientData[slot].client = newClient;
                // .sessionIndex: specify it when a session is found 
                break;
            }
        }
        if (slot == _TCPclientSlots) { newClient.stop(); }              // no free slots: stop the new client

        // check whether the new client can be linked to an existing session (based on IP address) or to a new session
        else {
            // assign to existing session ?
            bool foundSession{ false };
            bool isNewSession{false};
            int sessionID{};
            IPAddress clientIP = newClient.remoteIP();

            for (sessionID = 0; sessionID < _maxSessions; sessionID++) {
                if ((_pSessionData[sessionID].IP == clientIP) && (_pSessionData[sessionID].active)) {
                    _pSessionData[sessionID].clientSlotNumber = slot;           // link to client data
                    //_pSessionData[sessionID].lastActivity = millis();

                    _pWiFiClientData[slot].sessionIndex = sessionID;           // reverse link from client data
                    foundSession = true;
                    break;
                }
            }
            // assign to new session ?
            if (!foundSession) {
                for (sessionID = 0; sessionID < _maxSessions; sessionID++) {
                    if (!_pSessionData[sessionID].active) {
                        _pSessionData[sessionID].active = true;                                 // .active is set by the TCP server only (here) and is reset by the application layer
                        _pSessionData[sessionID].IP = clientIP;
                        _pSessionData[sessionID].clientSlotNumber = slot;                        // link to client data
                        //_pSessionData[sessionID].lastActivity = millis();

                        _pWiFiClientData[slot].sessionIndex = sessionID;                       // reverse link from client data
                        foundSession = isNewSession = true;
                        break;
                    }
                }
            }
            if (!foundSession) {
                _pWiFiClientData[slot].state = IDLE;
                _pWiFiClientData[slot].sessionIndex = -1;
                newClient.stop();                                                       // no free slots: stop the new client
            }
            else {
                if (_verbose) { Serial.printf("\r\n-- at %7lus: session %d (%s): client (slot %d) connected, remote IP %d.%d.%d.%d\r\n", 
                    millis() / 1000, sessionID, (isNewSession ? "NEW ": "CURR"), slot, clientIP[0], clientIP[1], clientIP[2], clientIP[3]); }
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

WiFiServer* TCPconnection::getServer() { return &_server; }


WiFiClient* TCPconnection::getSessionClient(int sessionID) {
    if ((_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) { return nullptr; }       // WiFi is not connected or TCP is disabled: session has no client linked
    if ((sessionID < 0) || (sessionID >= _maxSessions)) { return nullptr; }             // Safety check: session index must be valid
    if (!_pSessionData[sessionID].active) { return nullptr; }

    int clientSlot = _pSessionData[sessionID].clientSlotNumber;
    if ((clientSlot < 0) || (clientSlot >= _TCPclientSlots)) { return nullptr; }         // safety check: client slot must be valid

    if (_pWiFiClientData[clientSlot].state == IDLE) { return nullptr; }
    else { return &(_pWiFiClientData[clientSlot].client); }

}


// ----------------------------------------
// *   stop client linked to a session    *
// ----------------------------------------

void TCPconnection::stopSessionClient(int sessionID, bool keepSessionActive) {
    if ((_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) { return; }       // Exit if WiFi is not connected or TCP is disabled
    if ((sessionID < 0) || (sessionID >= _maxSessions)) { return; }             // Safety check: session index must be valid
    if (!_pSessionData[sessionID].active) { return; }

    int clientSlot = _pSessionData[sessionID].clientSlotNumber;
    if ((clientSlot < 0) || (clientSlot >= _TCPclientSlots)) { return; }         // safety check: client slot must be valid

    _pSessionData[sessionID].clientSlotNumber = -1;                               // Mark session as inactive and remove client link

    if (_pWiFiClientData[clientSlot].state != IDLE) {                       // If client is connected, stop it and clean up client state
        _pWiFiClientData[clientSlot].client.stop();
        _pWiFiClientData[clientSlot].state = IDLE;
        _pWiFiClientData[clientSlot].sessionIndex = -1;
        _pSessionData[sessionID].active = keepSessionActive;                // .active is set by the TCP server only and is reset by the application layer (here)

        // Log disconnection if verbose mode is enabled
        if (_verbose) {
            IPAddress clientIP = _pSessionData[sessionID].IP;                             // Get client slot and IP for this session
            Serial.printf("\r\n-- at %7lus: session %d (%s): client (slot %d) STOPPED, remote IP %d.%d.%d.%d\r\n", 
                millis() / 1000, sessionID, (keepSessionActive ? "KEEP" : "END "), clientSlot, clientIP[0], clientIP[1], clientIP[2], clientIP[3]);
        }
    }
}


// --------------------------
// *   get session data   ***
// --------------------------

bool TCPconnection::getSessionData(int sessionID, int& clientSlot, IPAddress& IP) {                           // function returns 'session active' status
    clientSlot = -1;                                                                    // init
    //lastActivity = 0;
    IP = { 0,0,0,0 };

    // note: if wifi is off or TCP is not enabled (temporarily for instance), sessions may still be active
    if ((sessionID < 0) || (sessionID >= _maxSessions)) { return false; }             // Safety check: session index must be valid

    bool sessionActive = _pSessionData[sessionID].active;
    if (sessionActive) {
        IP = _pSessionData[sessionID].IP;                                                   // Get client slot and IP for this session
        //lastActivity = _pSessionData[sessionID].lastActivity;

        if ((_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) { return sessionActive; } // WiFi is not connected or TCP is disabled: session can still be active

        clientSlot = _pSessionData[sessionID].clientSlotNumber;
        if ((clientSlot < 0) || (clientSlot >= _TCPclientSlots)) { clientSlot = -1; }           // safety check: client slot must be valid
        else if (_pWiFiClientData[clientSlot].state == IDLE) { clientSlot = -1; }          // is client connected ?
    }

    return sessionActive;
}


// ------------------------------------
// *   TCP/IP connection: settings    *
// ------------------------------------

void TCPconnection::WiFiOff() {                     // switch off WiFi antenna
    _WiFiEnabled = false;
    _resetWiFi = true;
}
void TCPconnection::WiFiOn() {                      // (re-)start Wifi: switch off first (if WiFi currently on), and start again
    _WiFiEnabled = true;
    _resetWiFi = true;
}
void TCPconnection::TCPdisable() {                  // disable TCP IO
    _TCPenabled = false;
}
void TCPconnection::TCPenable() {                   // enable TCP IO
    _TCPenabled = true;
}


// ----------------------------------
// *   set verbose mode ON or OFF   *
// ----------------------------------

void TCPconnection::setVerbose(bool verbose) { _verbose = verbose; }


// --------------------------
// *   return WiFi state    *
// --------------------------

TCPconnection::connectionState TCPconnection::getWiFiState() {
    return _WiFiState;                                                      // not connected, (re-)starting or connected (enumeration)
}


// -------------------------
// *   return TCP state    *
// -------------------------

long TCPconnection::getTCPclientCount() {
    int TCPstate{ -1 };                                                     // init: TCP disabled and/or WiFi not connected
    if ((_WiFiState == conn_2_WiFi_connected) && (_TCPenabled)) {
        TCPstate = 0;                                                       // re-init: no WiFi clients connected
        for (int slot = 0; slot < _TCPclientSlots; slot++) {
            if (_pWiFiClientData[slot].state != IDLE) { TCPstate++; }   // count how many WiFi clients are connected
        }
    }
    return TCPstate;                                                        // return -1 (TCP disabled and/or WiFi not connected) or number of WiFi clients connected
}


