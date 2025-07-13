/*************************************************************************************************************************
*   Example Arduino sketch demonstrating Justina interpreter functionality												 *
*                                                                                                                        *
*   The Justina interpreter library is licensed under the terms of the GNU General Public License v3.0 as published      *
*   by the Free Software Foundation (https://www.gnu.org/licenses).                                                      *
*   Refer to GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter            *
*                                                                                                                        *
*   This example code is in the public domain                                                                            *
*                                                                                                                        *
*   2024, 2025 Herwig Taveirne                                                                                                *
*************************************************************************************************************************/

#include "Justina_TCPIP_multi.h"

/*
    Setup an Arduino as a TCP/IP server.
    This code also maintains the connection: method maintainConnection() MUST BE CALLED REGULARLY from your program main loop.
    This allows you to isolate your application (an HTTP server, ...) from this TCP/IP maintenance code.

    WiFi maintenance and TCP/IP client connection maintenance is split into two different methods.

    A number of utility functions are provided to switch WiFi on or off, to allow a TCP/IP connections or not, etc.
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

    _TCPclientSlots = _maxSessions = min(TCPclientSlots, TCP_SOCKET_COUNT - 1);     // TCP_SOCKET_COUNT: depends on board type; -1: for temporary 'new client' object

    // pointer to array of client data
    _pWiFiClientData = new WiFiClientData[_TCPclientSlots];                         
    _pSessionData = new SessionData[_TCPclientSlots];                               // equal size of _pWiFiClientData

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

    // Variable '_WiFiState' maintains the state of the WiFiconnection('state machine').If this maintained state
    // (e.g., 'WiFi connected') does not correspond to the actual state(e.g., WiFi connection was lost) OR your application
    // requests a state change (e.g., 'switch off WiFi'), action is taken.

    switch (_WiFiState) {

        // state: WiFi is currently not connected
        // --------------------------------------
        case conn_0_WiFi_notConnected:
        {
            // WiFi is enabled AND it's time for a next WiFi connection attempt ?
            if (_WiFiEnabled && (_lastWiFiMaintenanceTime + WIFI_UP_CHECK_INTERVAL < millis())) {

                if (!_setupAsClient) {                                                      // is server side ? set static server IP
                #if defined ARDUINO_ARCH_ESP32
                    WiFi.config(_serverAddress, _gatewayAddress, _subnetMask, _DNSaddress);
                #else
                    WiFi.config(_serverAddress, _DNSaddress, _gatewayAddress, _subnetMask);
                #endif
                }

                WiFi.begin((const char*)_SSID, (const char*)_PASS);
                _WiFiState = conn_1_WiFi_waitForConnecton;
                if (_verbose) { _pDebugStream->printf("-- at %11.3fs: %s\r\n", millis() / 1000., (_setupAsClient ? "-- Trying to connect WiFi..." : "-- Trying to connect TCP/IP server to WiFi...")); }
                // remember time of this WiFi connection attempt
                _WiFiWaitingForConnectonAt = _lastWiFiMaintenanceTime = millis();           // remember time of last WiFi maintenance AND time of this WiFi connection attempt
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
                if (WiFi.status() == WL_CONNECTED) {                                        // WiFi is now connected ?
                    if (!_setupAsClient) { _server.begin(); }                               // if Arduino as server, start server
                    _WiFiState = conn_2_WiFi_connected;
                    if (_verbose) {
                        IPAddress IP = WiFi.localIP();
                        _pDebugStream->printf("-- at %11.3fs: %sWiFi connected, local IP %d.%d.%d.%d (%ld dBm)\r\n",
                            millis() / 1000., (_setupAsClient ? "" : "TCP/IP server started. "), IP[0], IP[1], IP[2], IP[3], WiFi.RSSI());
                    }
                }

                else {                                                                      // WiFi is not yet connected
                    // regularly report status ('still trying...' etc.)
                    if (_WiFiWaitingForConnectonAt + WIFI_REPORT_INTERVAL < millis() && _verbose) {
                        _WiFiWaitingForConnectonAt = millis();                              // for printing only
                        if (_verbose) { _pDebugStream->print("."); }
                    }
                }
                _lastWiFiMaintenanceTime = millis();                                        // remember time of last WiFi maintenance 
                _resetWiFi = false;                                                         // could have been set while not connected to WiFi
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
                if (_verbose) { _pDebugStream->printf("-- at %11.3fs: %s\r\n", millis() / 1000., _setupAsClient ? "WiFi disconnected" : "WiFi disconnected, TCP/IP server stopped"); }
                WiFi.disconnect();
            #if !defined ARDUINO_ARCH_ESP32
                WiFi.end();
            #endif
                _lastWiFiMaintenanceTime = millis();                                        // remember time of last WiFi maintenance 
                _resetWiFi = false;
            }
        }
    }
}


// ---------------------------------------------------------------------
// *   maintain TCP client connections, for Arduino set up as server   *
// ---------------------------------------------------------------------

void TCPconnection::maintainTCPclients() {

    /*
        An array of type 'WiFiClientData' maintains data of connected TCPIP clients in maximum 3 client slots.
        If a client slot has state CONNECTED, it does currently contain data about a connected client. If IDLE, the associated client is currently not connected.

        An array of type 'SessionData' maintains basic data about active sessions. 
        If a session has status ACTIVE, it is reserved for communication with a specific client IP address. A total of 3 sessions is available.
        
        The TCP layer (TCPIP server) connects and stops individual TCP clients and maintains TCP client connection states ('state machine').
        
        If a client connects, the TCPIP server will first try to link the client to an active session, based on the client IP address. 
        If no match occurs, the client is considered 'new' and it will be linked to an inactive session (if available); that session will then receive the state ACTIVE. 
        
        The higher level application (e.g., a HTTP server) that makes use of the TCPIP server for its communication must regularly scan for active sessions and, 
        if it finds one, communicate with the associated client (it will receive client requests and send responses).

        Once a response is sent, the application will instruct the TCPIP server to stop the client: the client state will then change to IDLE.
        If the application determines that no further communication with the client is needed, it informs the TCP server to change the session state to INACTIVE as well. 
        Example: if a HTTP client does not provide correct credentials or a session timeout occurs, the HTTP server can instruct the TCP server not only to stop the client, 
        but to end the session as well. The TCPIP server will NOT end a session by its own initiative. 
    */

    // A. Process currently CONNECTED clients
    // --------------------------------------

    for (int i = 0; i < _TCPclientSlots; i++) {
        if (_pWiFiClientData[i].state == CONNECTED) {
            bool connectionLost = (!_pWiFiClientData[i].client.connected()) || (_WiFiState < conn_2_WiFi_connected) || !_TCPenabled;
            bool connectionTimedOut = (millis() - _pWiFiClientData[i].connectedAt > _TCPconnectionTimeout);

            // if connection lost or connection timeout: stop the client
            if (connectionLost || connectionTimedOut) {

                // client MUST still be stopped !
                IPAddress clientIP{ 0,0,0,0 };                                      // init, in case session index would be inconsistent (safety)
                int sessionID = _pWiFiClientData[i].sessionIndex;
                int slot{ -1 };
                if (sessionID >= 0 && sessionID < _maxSessions) {                   // safety
                    slot = _pSessionData[sessionID].clientSlotID;
                    clientIP = _pSessionData[sessionID].IP;
                    _pSessionData[sessionID].clientSlotID = -1;                     // remove reverse link from session data
                }
                _pWiFiClientData[i].client.stop();
                _pWiFiClientData[i].state = IDLE;
                _pWiFiClientData[i].sessionIndex = -1;

                if (_verbose) {
                    _pDebugStream->printf("-- at %11.3fs: session %d (CURR): TCP connection %s. client (slot %d) stopped, remote IP %d.%d.%d.%d\r\n",
                        millis() / 1000., sessionID, (connectionLost ? "lost" : "timeout"), slot, clientIP[0], clientIP[1], clientIP[2], clientIP[3]);
                }
            }
        }
    }

    if ((_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) { return; }


    // B. Process a new client
    // -----------------------

    WiFiClient nextClient = _server.available();
    IPAddress clientIP = nextClient.remoteIP();
    int sessionID{};
    int clientSlot{};
    bool clientSlotFound{ false };

    if (!nextClient) { return; }

    // next message is not really needed
    // if (_verbose) { _pDebugStream->printf("-- at %11.3fs: 'new' client found\r\n", millis() / 1000.); }


    // B.1 check whether this is a CONNECTED client that is 'found' more than once
    //     note: this is possible because we continuously call _server.available()
    // ---------------------------------------------------------------------------

    // a client slot with state CONNECTED is always linked to an ACTIVE session: check if such a session exists
    for (sessionID = 0; sessionID < _maxSessions; sessionID++) {
        if (!_pSessionData[sessionID].active) { continue; }                         // session is active ? (If not, continue the search)
        if (_pSessionData[sessionID].IP != clientIP) { continue; }                  // client with this IP is already linked to a session ? (If not, continue the search)

        // found an active session for this IP: retrieve client slot the session is linked with
        clientSlot = _pSessionData[sessionID].clientSlotID;                         // retrieve the client slot that session uses
        if ((clientSlot < 0) || (clientSlot >= _TCPclientSlots)) { break; }         //safety: prevent writing to an invalid memory location                   

        // NOTE: DO NOT replace the current client with the 'new' client ('... = nextClient'): this will temporarily stall the connection
        _pWiFiClientData[clientSlot].state = CONNECTED;                             // safety: set client state to 'connected' (it should already be, because of the client slot link, just above)
        _pWiFiClientData[clientSlot].connectedAt = millis();                        // used for connection timeout
        _pWiFiClientData[clientSlot].sessionIndex = sessionID;                      // safety: reverse link
        clientSlotFound = true;                                                     // session data updated as well; nothing more to do

        return;                                                                     // OK: the 'new' client is an existing client, currently connected and linked to an active session
    }


    // B.2 it is indeed a new client: find a free client slot for it 
    // -------------------------------------------------------------
    for (clientSlot = 0; clientSlot < _TCPclientSlots; clientSlot++) {
        if (_pWiFiClientData[clientSlot].state == IDLE) {
            _pWiFiClientData[clientSlot].state = CONNECTED;
            _pWiFiClientData[clientSlot].connectedAt = millis();                    // used for connection timeout
            _pWiFiClientData[clientSlot].client = nextClient;
            clientSlotFound = true;
            break;
        }
    }

    if (!clientSlotFound) {                                                         // no free slots: stop the new client, not assigned to a session
        nextClient.stop();                                                          // stop the new client: all slots are currently taken
        if (_verbose) { _pDebugStream->printf("-- at %11.3fs: new client REJECTED: no free client slots\r\n", millis() / 1000.); }

        return;                                                                     // NOK (no free client slots)
    }

    // a free client slot was found. Next: update the session data
    bool sessionDataUpdated{ false };
    bool isNewSession{ false };                                                     // for debug print only


    // B.3 check whether the new client can be linked to an existing (active) session (based on IP address) 
    // ----------------------------------------------------------------------------------------------------
    for (sessionID = 0; sessionID < _maxSessions; sessionID++) {
        if ((_pSessionData[sessionID].IP == clientIP) && (_pSessionData[sessionID].active)) {
            _pSessionData[sessionID].clientSlotID = clientSlot;                     // link to client data
            _pWiFiClientData[clientSlot].sessionIndex = sessionID;                  // reverse link from client data
            sessionDataUpdated = true;
            break;
        }
    }


    // B.4 if the client could not be linked to an active session, check whether it can be linked to a new session
    // -----------------------------------------------------------------------------------------------------------
    if (!sessionDataUpdated) {
        for (sessionID = 0; sessionID < _maxSessions; sessionID++) {
            if (!_pSessionData[sessionID].active) {
                _pSessionData[sessionID].active = true;                             // .active is set by the TCP server only (here) and is reset by the application layer
                _pSessionData[sessionID].IP = clientIP;
                _pSessionData[sessionID].clientSlotID = clientSlot;                 // link to client data

                _pWiFiClientData[clientSlot].sessionIndex = sessionID;              // reverse link from client data
                sessionDataUpdated = isNewSession = true;
                break;
            }
        }
    }

    if (sessionDataUpdated) {
        if (_verbose) {
            _pDebugStream->printf("-- at %11.3fs: session %d (%s): client (slot %d) connected, remote IP %d.%d.%d.%d\r\n",
                millis() / 1000., sessionID, (isNewSession ? "NEW " : "CURR"), clientSlot, clientIP[0], clientIP[1], clientIP[2], clientIP[3]);
        }
    }

    else {
        _pWiFiClientData[clientSlot].state = IDLE;
        _pWiFiClientData[clientSlot].sessionIndex = -1;
        nextClient.stop();
        if (_verbose) { _pDebugStream->printf("-- at %11.3fs: new client REJECTED: no free session\r\n", millis() / 1000.); }
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

    int clientSlot = _pSessionData[sessionID].clientSlotID;
    if ((clientSlot < 0) || (clientSlot >= _TCPclientSlots)) { return nullptr; }        // safety check: client slot must be valid

    if (_pWiFiClientData[clientSlot].state == IDLE) { return nullptr; }
    else { return &(_pWiFiClientData[clientSlot].client); }

}


// ----------------------------------------
// *   stop client linked to a session    *
// ----------------------------------------

// this stops the TCP connection linked to a session and / or ends the session

void TCPconnection::stopSessionClient(int sessionID, bool endSession) {
    if ((_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) { return; }           // exit if WiFi is not connected or TCP is disabled
    if ((sessionID < 0) || (sessionID >= _maxSessions)) { return; }                 // safety check: session index must be valid
    if (!_pSessionData[sessionID].active) { return; }                               // safety

    int clientSlot = _pSessionData[sessionID].clientSlotID;
    if ((clientSlot < -1) || (clientSlot >= _TCPclientSlots)) { return; }           // safety check: client slot must be valid OR -1 (no client for session)

    // If client is connected, stop it and clean up client state
    if (clientSlot != -1) {
        if (_pWiFiClientData[clientSlot].state == CONNECTED) {                      // If client is connected, stop it and clean up client state
            _pWiFiClientData[clientSlot].client.stop();
            _pWiFiClientData[clientSlot].state = IDLE;
            _pWiFiClientData[clientSlot].sessionIndex = -1;

            _pSessionData[sessionID].clientSlotID = -1;                             // Mark session as inactive and remove client link

        }
    }

    // if 'keepSessionActive' is false, end session  
    if (endSession) { _pSessionData[sessionID].active = false; }                    // .active is set by the TCP server only and is reset by the application layer (here)

     // Log disconnection if verbose mode is enabled
    if (_verbose) {
        IPAddress clientIP = _pSessionData[sessionID].IP;                           // Get client slot and IP for this session
        if (clientSlot >= 0) {
            _pDebugStream->printf("-- at %11.3fs: session %d %s: client (slot %d) STOPPED, remote IP %d.%d.%d.%d\r\n",
                millis() / 1000., sessionID, (endSession ? "END   " : "(KEEP)"), clientSlot, clientIP[0], clientIP[1], clientIP[2], clientIP[3]);
        }
        else {
            _pDebugStream->printf("-- at %11.3fs: session %d END\r\n", millis() / 1000., sessionID);
        }
    }
}


// --------------------------
// *   get session data   ***
// --------------------------

// if a session is active AND a client is currently connected, the function returns TRUE. Otherwise, it returns FALSE. 

int TCPconnection::getSessionClient(int sessionID, IPAddress& IP) {                 // function returns 'session active' status
    int clientSlot = -1;                                                            // init
    IP = { 0,0,0,0 };

    // note: if wifi is off or TCP is not enabled (temporarily for instance), sessions may still be active
    if ((sessionID < 0) || (sessionID >= _maxSessions)) { return -1; }              // Safety check: session index must be valid

    bool sessionActive = _pSessionData[sessionID].active;
    if (sessionActive) {
        IP = _pSessionData[sessionID].IP;                                           // Get client slot and IP for this session

        if ((_WiFiState < conn_2_WiFi_connected) || !_TCPenabled) { return -1; }    // WiFi is not connected or TCP is disabled: session can still be active

        clientSlot = _pSessionData[sessionID].clientSlotID;
        if ((clientSlot < 0) || (clientSlot >= _TCPclientSlots)) { clientSlot = -1; return -1; }    // safety check: client slot must be valid
        else if (_pWiFiClientData[clientSlot].state == IDLE) { clientSlot = -1; return -1; }        // is client connected ?
    }

    // return connected client slot currently linked to active TCP session
    return clientSlot;
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


// --------------------------
// set TCP connection timeout
// --------------------------

void TCPconnection::setConnectionTimeout(unsigned long TCPconnectionTimeout) {
    _TCPconnectionTimeout = TCPconnectionTimeout;
}


// ----------------------------------
// *   set verbose mode ON or OFF   *
// ----------------------------------

void TCPconnection::setVerbose(bool verbose) { _verbose = verbose; }


// ----------------------------
// *   set debug out stream   *
// ----------------------------

void TCPconnection::setDebugStream(Stream* debugStream) { _pDebugStream = debugStream; }


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
            if (_pWiFiClientData[slot].state == CONNECTED) { TCPstate++; }   // count how many WiFi clients are connected
        }
    }
    return TCPstate;                                                        // return -1 (TCP disabled and/or WiFi not connected) or number of WiFi clients connected
}


