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

#include "Justina.h"
#include "secrets.h"               
#include "src/Justina_TCPIP.h"

/*
    Example code demonstrating how to setup an Arduino as a TCP/IP server.
    NOTE: by simply replacing the constructor for class TCPconnection (below), you can setup an Arduino as TCP/IP client.
    ---------------------------------------------------------------------------------------------------------------------

    This sketch demonstrates various Justina features, namely
    - setting up Arduino as a TCP/IP server in order to use a TCP/IP terminal as an additional IO device
    - using Justina system callbacks to maintain the TCP/IP connection, blink a heartbeat led and set status leds to indicate the TCP/IP connection state
    - using Justina user c++ functions to control the TCP/IP connection from within Justina

    BEFORE running this sketch, please enter WiFi SSID and password in file 'secrets.h'.
    Also, change static server address and port, and gateway address, subnet mask and DNS address (see 'Create TCP/IP connection object', below)

    See the example of an HTTP server built on top of a TCP/IP server. The TCP/IP server is maintained by regular calls to method 
    'myTCPconnection.maintainConnection()' (c++), while the HTTP server is written in Justina language. 
    While control is within Justina, the TCP/IP connection is maintained by regular system callbacks in the background. 

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/


// The 4 Arduino pins defined below will be set as output pins in setup(). 
// Connect each of these pins to the anode of a LED and connect each LED cathode to a terminal of a resistor. Wire the other terminal to ground.

constexpr int HEARTBEAT_PIN{ 9 };                                                   // signals that the program is running
constexpr int DATA_IO_PIN{ 5 };                                                     // signals Justina is sending or receiving data (from/to any external IO device) 

#if defined ARDUINO_ARCH_ESP32
constexpr int WiFi_CONNECTED_PIN{ 17 };                                             // ON indicates WiFi is connected 
constexpr int TCP_CONNECTED_PIN{ 18 };                                              // blink: TCP enabled but no terminal connected; ON: terminal connected
#else
constexpr int WiFi_CONNECTED_PIN{ 14 };                                             // ON indicates WiFi is connected 
constexpr int TCP_CONNECTED_PIN{ 15 };                                              // blink: TCP enabled but no terminal connected; ON: terminal connected
#endif

unsigned long heartbeatPeriod{ 1000 };                                              // 'long' heartbeat ON and OFF time: heartbeat led will blink at this (low) rate when control is not within Justina
constexpr char menu[] = "Please type 'J' to start Justina interpreter\r\n";


// -------------------------------
// Create TCP/IP connection object
// -------------------------------

// enter WiFi SSID and password in file secrets.h
constexpr char SSID[] = SERVER_SSID, PASS[] = SERVER_PASS;                          // WiFi SSID and password defined in secrets.h                          

// enter the correct server STATIC IP address and port here (CHECK / ADAPT your ROUTER settings as well)
// (if configured as a HTTP/IP client, this is the IP address and port of the server to connect to) 
const IPAddress serverAddress(192, 168, 0, 95);                                     // STATIC server IP (LAN)
const int serverPort = 8085;

// enter gateway address, subnet mask and DNS address here (not relevant if configured as HTTP/IP  client)
const IPAddress gatewayAddress(192, 168, 0, 1);
const IPAddress subnetMask(255, 255, 255, 0);
const IPAddress DNSaddress(195, 130, 130, 5);

// create TCP connection object to connect Arduino as TCP server
// last argument: desired connection state (choose WiFi not connected, WiFi connected or TCP connected)
TCPconnection myTCPconnection(SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort, TCPconnection::conn_4_TCP_clientConnected);

// initialize variable maintaining the current connection state
TCPconnection::connectionState_type _connectionState = TCPconnection::conn_0_WiFi_notConnected;    // assume not connected  


// ---------------------
// Create Justina object
// ---------------------

// define between 1 and 4 'external' input streams and output streams. 
constexpr int terminalCount{ 2 };                                                   // 2 streams defined (Serial and TCP/IP)

// first position: default Justina console AND default input and output streams for Justina 
Stream* pExternalInputs[terminalCount]{ &Serial, nullptr };                         // Justina input streams (Serial only)                                                                               
Print* pExternalOutputs[terminalCount]{ &Serial, nullptr };                          // Justina output streams (Serial; TCP/IP stream will be added in setup() )                                                      

// create Justina interpreter object
Justina justina(pExternalInputs, pExternalOutputs, terminalCount);


// --------------------
// forward declarations
// --------------------

void housekeeping(long& appFlags);
void heartbeat();

// Justina user c++ functions
void WiFiOff(void** const pdata, const char* const valueType, const int argCount, int& execError);
void WiFiRestart(void** const pdata, const char* const valueType, const int argCount, int& execError);
void TCPoff(void** const pdata, const char* const valueType, const int argCount, int& execError);
void TCPon(void** const pdata, const char* const valueType, const int argCount, int& execError);
void stopClient(void** const pdata, const char* const valueType, const int argCount, int& execError);

void setVerboseConnection(void** const pdata, const char* const valueType, const int argCount, int& execError);
long getConnectionState(void** const pdata, const char* const valueType, const int argCount, int& execError);
void getLocalIP(void** const pdata, const char* const valueType, const int argCount, int& execError);
void getRemoteIP(void** const pdata, const char* const valueType, const int argCount, int& execError);


// --------------------------------------------------------
// Define records with Justina user c++ function attributes
// --------------------------------------------------------

// arguments: Justina alias, c++ function, min. & max. argument count (checked during parsing in Justina) 
Justina::CppVoidFunction  const cppVoidFunctions[]{                                 // user c++ functions returning nothing
    // NOTE: cpp_ prefix in next aliases is not a requirement, but it indicates the function nature AND it highlights such function names in notepad++ (Justina language extension must be installed)
    {"cpp_WiFiOff", WiFiOff, 0, 0},
    {"cpp_WiFiRestart", WiFiRestart, 0, 0},
    {"cpp_TCPoff", TCPoff, 0, 0},
    {"cpp_TCPon", TCPon, 0, 0},
    {"cpp_stopClient", stopClient, 0, 0},
    {"cpp_setVerbose", setVerboseConnection, 1, 1},
    {"cpp_localIP", getLocalIP, 1, 1},
    {"cpp_remoteIP", getRemoteIP, 1, 1 }
};

Justina::CppLongFunction const cppLongFunctions[]{                                  // user c++ functions returning a long integer value
    {"cpp_connState", getConnectionState, 0, 0}
};


// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);

    // define output pins
    pinMode(DATA_IO_PIN, OUTPUT); digitalWrite(DATA_IO_PIN, LOW);                   // led to signal Justina IO
    pinMode(HEARTBEAT_PIN, OUTPUT); digitalWrite(HEARTBEAT_PIN, LOW);               // blinking led for heartbeat

    pinMode(WiFi_CONNECTED_PIN, OUTPUT); digitalWrite(WiFi_CONNECTED_PIN, LOW);     // 'WiFi connected' led
    pinMode(TCP_CONNECTED_PIN, OUTPUT); digitalWrite(TCP_CONNECTED_PIN, LOW);       // 'TCP connected' led (will blink if TCP enabled but not yet connected to client)


    // TCP connection
    // --------------
    _connectionState = TCPconnection::conn_0_WiFi_notConnected;
    myTCPconnection.setVerbose(true);                                               // true: enable debug messages from within myTCPconnection


    // Justina library
    // ---------------
    pExternalInputs[1] = static_cast<Stream*>(myTCPconnection.getClient());         // add TCP/IP client to available Justina IO streams (within Justina IO commands, this will be stream IO2)
    pExternalOutputs[1] = static_cast<Print*>(myTCPconnection.getClient());

    justina.setSystemCallbackFunction(&housekeeping);                               // set system callback function (see below); it will be called regularly while control is within Justina 

    justina.registerVoidUserCppFunctions(cppVoidFunctions, 8);                      // register user c++ functions returning nothing (void), function count
    justina.registerLongUserCppFunctions(cppLongFunctions, 1);                      // register user c++ functions returning a long, function count

    Serial.println(menu);
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    heartbeat();                                                                    // blink a led to show program is running
    myTCPconnection.maintainConnection();                                           // maintain TCP connection
    setConnectionStatusLeds();                                                      // set WiFi and TCP connection status leds 

    char c;
    if (Serial.available()) {                                                       // read a character from Serial and execute the indicated action
        c = Serial.read();
        if ((c == 'j') || (c == 'J')) {
            // start interpreter: control will not return to here until the user quits
            heartbeatPeriod = 500;                                                  // 'short' heartbeat ON and OFF time: heartbeat led will blink at a higher rate when control is within Justina                                              
            justina.begin();                                                        // start interpreter (control will stay there until quitting Justina)
            heartbeatPeriod = 1000;                                                 // 'long' heartbeat ON and OFF time: heartbeat led will blink at a lower rate when control is not within Justina                                              
            Serial.println(menu);
        }
    }
}


// ----------------------------------------
// *   Justina system callback function   *
// ----------------------------------------

void housekeeping(long& appFlags) {                                                 // appFlags: receive Justina status and send requests to Justina - not used here
    heartbeat();                                                                    // blink a led to show program is running
    myTCPconnection.maintainConnection();                                           // maintain TCP connection
    setConnectionStatusLeds();                                                      // set WiFi and TCP connection status leds 

    // signal that Justina is sending or receiving data (from any external IO device) by blinking a led
    bool newDataLedState{ false };
    static bool dataLedState{ false };
    if (appFlags & Justina::appFlag_dataInOut) { newDataLedState = !dataLedState; }
    else { newDataLedState = false; }                                               // if data, toggle state, otherwise reset state
    if (newDataLedState != dataLedState) { dataLedState = newDataLedState;  digitalWrite(DATA_IO_PIN, dataLedState); }  // only write if change detected
}


// ----------------------------------------------
// *   Blink a led to show program is running   * 
// ----------------------------------------------

void heartbeat() {
    // note: this is not a 'clock' because it does not measure the passing of fixed time intervals...
    // ...but the passing of MINIMUM time intervals 

    static bool ledOn{ false };
    static uint32_t lastHeartbeat{ 0 };                                                         // last heartbeat time in ms
    static uint32_t previousTime{ 0 };

    uint32_t currentTime = millis();
    // also handle millis() overflow after about 47 days
    if ((lastHeartbeat + heartbeatPeriod < currentTime) || (currentTime < previousTime)) {      // heartbeat period has passed
        lastHeartbeat = currentTime;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_PIN, ledOn);                                                     // change led state
    }
    previousTime = currentTime;
}


// -----------------------------------
// *   handle WiFi/TCP status leds   *
// -----------------------------------

void setConnectionStatusLeds() {

    static TCPconnection::connectionState_type oldConnectionState{ TCPconnection::conn_0_WiFi_notConnected };
    static uint32_t lastLedChangeTime{ 0 };
    static bool TCPledState{ false };

    _connectionState = myTCPconnection.getConnectionState();

    // TCP enabled and waiting for a client to connect ? blink 'TCP' led
    if (_connectionState == TCPconnection::conn_3_TCPwaitForNewClient) {

        // toggle led on/off state (blink led) ?
        uint32_t currentTime = millis();
        if ((lastLedChangeTime + 200 < currentTime) || (currentTime < lastLedChangeTime)) {     // (note: also handles millis() overflow after about 47 days)
            TCPledState = !TCPledState;
            digitalWrite(TCP_CONNECTED_PIN, TCPledState);                                       // BLINK: TCP enabled but client not yet connected                                         
            lastLedChangeTime = currentTime;
        }
    }

    // set WiFi connected & TCP connected leds
    if (oldConnectionState != _connectionState) {
        bool WiFiConnected = (_connectionState != TCPconnection::conn_0_WiFi_notConnected) && (_connectionState != TCPconnection::conn_1_WiFi_waitForConnecton);
        digitalWrite(WiFi_CONNECTED_PIN, WiFiConnected);                                        // ON: 'WiFi connected' 

        if (_connectionState != TCPconnection::conn_3_TCPwaitForNewClient) {                    // do not interfere with blinking TCP led
            bool TCPconnected = (_connectionState == TCPconnection::conn_4_TCP_clientConnected);
            digitalWrite(TCP_CONNECTED_PIN, TCPconnected);                                      // ON: 'client connected'
        }
    }

    oldConnectionState = _connectionState;
}


// *************************************************************************
// ***   Justina user c++ functions (Justina functionality extensions)   ***
// *************************************************************************

// --------------------------------------------------------------------------------------
// *   TCP/IP connection: switch OFF or restart WiFi, switch OFF or ON TCP connection   *
// --------------------------------------------------------------------------------------

 /*
    Justina call:
    -------------
    cpp_WiFiOff();
    cpp_WiFiRestart();
    cpp_TCPoff();
    cpp_TCPon();
*/

// switch off WiFi antenna
void WiFiOff(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.WiFiOff();
};

// restart WiFi: switch off first (if WiFi currently on), and start again
void WiFiRestart(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.WiFiRestart();
};

// disable TCP IO
void TCPoff(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.TCPdisable();
};

// enable TCP IO
void TCPon(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.TCPenable();
};


// -------------------
// *   stop client   *
// -------------------

 /*
    Justina call:
    -------------
    cpp_stopClient();
*/

void stopClient(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.stopClient();
};


// ------------------------------------------------
// *   TCP/IP connection: set verbose or silent   *
// ------------------------------------------------

 /*
    Justina call:
    -------------
    cpp_setVerbose(state);                                                                      // state = 0: silent, 1: verbose
*/

void setVerboseConnection(void** const pdata, const char* const valueType, const int argCount, int& execError) {

    // NOTE: if you trust the Justina caller and you know the argument type (long or float), you can skip the tests
    bool isLong = ((valueType[0] & Justina::value_typeMask) == Justina::value_isLong);
    bool isFloat = ((valueType[0] & Justina::value_typeMask) == Justina::value_isFloat);
    if (!isLong && !isFloat) { execError = 3104; return; }                                      // numeric argument expected                 

    // get values or pointers to values
    long arg = isLong ? *(long*)pdata[0] : *(float*)pdata[0];                                   // fraction is lost 
    if ((arg < 0) || (arg > 1)) { execError = 3100; return; }                                   // argument outside range

    myTCPconnection.setVerbose(bool(arg));
};


// --------------------------------------
// *   return TCP/IP connection state   *
// --------------------------------------

long getConnectionState(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call:
    -------------
    cpp_connState();

    connection state returned:
    0: WiFi not connected
    1: trying to connect to WiFi
    2: WiFi connected - TCP/IP OFF
    3: WiFi connected - TCP/IP ON and waiting for a client to connect
    4: WiFi connected - TCP/IP ON and client connected
 */

    return (long)myTCPconnection.getConnectionState();                                          // state = 0 to 4 (see connectionState_type enumeration)
};


// -------------------------------
// *   return local IP address   *
// -------------------------------

void getLocalIP(void** const pdata, const char* const valueType, const int argCount, int& execError) {

    // NOTE: the first Justina argument supplied must be a variable, containing string of at least 15 characters (+ terminating '\0')
    //       the local IP address (as a string) is returned to the first Justina argument.
    // NOTE: if you trust the Justina caller, you can skip the tests

  /*
    Justina call:
    -------------
    var a = "";
    cpp_localIP(a=space(15));
*/

    if (!(valueType[0] & 0x80)) { execError = 3110; return; }
    bool isString = ((valueType[0] & Justina::value_typeMask) == Justina::value_isString);
    if (!isString) { execError = 3103; return; }                                                // string expected      
    if (strlen(((char*)pdata[0])) < 15) { execError = 3106; return; }                           // string too short (to contain IP as string)


    *(char*)pdata[0] = '\0';                                                                    // init
    if (_connectionState >= TCPconnection::conn_2_WiFi_connected) {
        IPAddress IP = WiFi.localIP();
        sprintf((char*)pdata[0], "%d.%d.%d.%d", IP[0], IP[1], IP[2], IP[3]);
    }
};


// --------------------------------
// *   return remote IP address   *
// --------------------------------

void getRemoteIP(void** const pdata, const char* const valueType, const int argCount, int& execError) {

    // NOTE: the first Justina argument supplied must be a variable, containing string of at least 15 characters (+ terminating '\0')
    //       the remote IP address (as a string) is returned to the first Justina argument.
    // NOTE: if you trust the Justina caller, you can skip the tests

  /*
    Justina call:
    -------------
    var a = "";
    cpp_remoteIP(a=space(15));
*/

    if (!(valueType[0] & 0x80)) { execError = 3110; return; }
    bool isString = ((valueType[0] & Justina::value_typeMask) == Justina::value_isString);
    if (!isString) { execError = 3103; return; }                                                // string expected      
    if (strlen(((char*)pdata[0])) < 15) { execError = 3106; return; }                           // string too short (to contain IP as string)


    *(char*)pdata[0] = '\0';                                                                    // init
    if (_connectionState == TCPconnection::conn_4_TCP_clientConnected) {
        WiFiClient* client = static_cast<WiFiClient*>(myTCPconnection.getClient());
        IPAddress IP = client->remoteIP();
        sprintf((char*)pdata[0], "%d.%d.%d.%d", IP[0], IP[1], IP[2], IP[3]);
    }
};

