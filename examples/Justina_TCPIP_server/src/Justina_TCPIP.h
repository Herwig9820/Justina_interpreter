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

#ifndef _JUSTINA_TCP_h
#define _JUSTINA_TCP_h
#
#if defined(ARDUINO_ARCH_RP2040)
#include <WiFiNINA_Generic.h>
#elif defined (ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#else
#include <WiFiNINA.h>
#endif

#include "Arduino.h"

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

class TCPconnection {

public:
    enum connectionState_type {
        conn_0_WiFi_notConnected,                                       // WiFi not yet connected

        conn_1_WiFi_waitForConnecton,                                   // waiting for WiFi to connect
        conn_2_WiFi_connected,                                          // WiFi connected - and TCP not yet connected (TCP disabled or no client)

        conn_3_TCPwaitForNewClient,                                     // waiting for TCP client
        conn_4_TCP_clientConnected,                                     // TCP server connected to a client 
    };

private:
    const char* _SSID, * _PASS;
    IPAddress _serverAddress, _gatewayAddress, _subnetMask, _DNSaddress;

    static constexpr unsigned long WIFI_UP_CHECK_INTERVAL{ 500 };       // minimum delay between two attempts to connect to WiFi (milliseconds) 
    static constexpr unsigned long WIFI_REPORT_INTERVAL{ 5000 };

    bool _verbose{};
    bool _resetWiFi{};
    bool _isClient{};
    int _serverPort{};

    bool _WiFiEnabled{};
    bool _TCPenabled{};

    // state machine: WiFi and client connection state
    connectionState_type _connectionState{ TCPconnection::conn_0_WiFi_notConnected };
    unsigned long _WiFiWaitingForConnectonAt{ millis() };               // timestamps in milliseconds
    unsigned long _lastWiFiMaintenanceTime{ millis() };  
   
    WiFiServer _server;                                                 // WiFi server object
    WiFiClient _client;                                                 // WiFi client object

    // private methods
    void maintainWiFiConnection();                                      // attempt to (re-)connect to WiFi
    void maintainTCPconnection();                                       // attempt to (re-)connect to a client, if available

public:
    // constructor: connect as server (with static server IP address)
    TCPconnection(const char SSID[], const char PASS[], const IPAddress serverAddress, const IPAddress  gatewayAddress, const IPAddress subnetMask,
        const IPAddress  DNSaddress, const int serverPort, connectionState_type initialConnState);

    // constructor: connect as client (pass server IP address and port to connect to)
    TCPconnection(const char SSID[], const char PASS[], const IPAddress serverAddress, const int serverPort, connectionState_type initialConnState);

    // utilities
    WiFiServer* getServer();                                            // (only if configured as server)
    WiFiClient* getClient();
    void maintainConnection();
    connectionState_type getConnectionState();
    void setVerbose(bool verbose);
    void WiFiOff();
    void WiFiRestart();
    void TCPdisable();
    void TCPenable();
    void stopClient();
};


#endif
