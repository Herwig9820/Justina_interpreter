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

#include "Justina.h"
#include "secrets.h"               
#include "src/Justina_TCPIP_multi.h"

/*
    Example code demonstrating how to setup an Arduino as a TCP/IP server handling up to 3 concurrent client connections
    --------------------------------------------------------------------------------------------------------------------

    This sketch uses Justina callbacks to maintain the TCP/IP connection, blink a heartbeat led and set status leds to indicate the TCP/IP connection state

    BEFORE running this sketch, please enter WiFi SSID and password in file 'secrets.h'.
    Also, change static server address and port, and gateway address, subnet mask and DNS address (see 'Create TCP/IP connection object', below)

    When Justina is running, it will regularly call method 'myTCPconnection.maintainConnection()' in the background to maintain the TCP/IP connection,
    while Justina is performing higher-level tasks.

    An example can be found in Justina language file 'web_cal2.jus', which implements an HTTP server, built on top of this TCP/IP server,
    capable of communicating with three concurrent HTTP clients. 

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/


// input pins: connect each pin to one terminal of a pushbutton. Connect the other pushbutton terminal to GND (ground)
constexpr int CONS_KILL_PIN{ 3 };                                           // read 'reset console' and 'kill Justina' requests
constexpr int STOP_ABORT_PIN{ 4 };                                          // read 'stop running program' and 'abort running code' requests

// output pins: connect each output pin to the anode of a LED and connect each cathode to one terminal of a resistor. Wire the other terminal to ground. 
constexpr int DATA_IO_PIN{ 5 };                                             // signals Justina is sending or receiving data (from any external IO device) 
constexpr int STATUS_A_PIN{ 6 };                                            // status A and B: Justina status
constexpr int STATUS_B_PIN{ 7 };
constexpr int ERROR_PIN{ 8 };                                               // a Justina error occurred (e.g., division by zero)  
constexpr int HEARTBEAT_PIN{ 9 };                                           // a square wave is output to indicate 'Justina is running'                                                

#if defined ARDUINO_ARCH_ESP32
constexpr int WiFi_CONNECTED_PIN{ 17 };                                     // ON indicates WiFi is connected 
constexpr int TCP_CONNECTED_PIN{ 18 };                                      // blink: TCP enabled but no terminal connected; ON: terminal connected
#else
constexpr int WiFi_CONNECTED_PIN{ 14 };                                     // ON indicates WiFi is connected 
constexpr int TCP_CONNECTED_PIN{ 15 };                                      // blink: TCP enabled but no terminal connected; ON: terminal connected
#endif

constexpr char menu[] = "Please type 'J' to start Justina interpreter\r\n";


// variables
unsigned long heartbeatPeriod{ 1000 };                                      // 'long' heartbeat ON and OFF time: heartbeat led will blink at this (low) rate when control is not within Justina

constexpr int TCPclientSlots{ 3 };
constexpr int terminalCount{ TCPclientSlots + 1 };                          // Serial and room for 3 TCP/IP clients

// ---------------------
// Create Justina object
// ---------------------

// define between 1 and 4 'external' input streams and output streams. 

// first position: default Justina console AND default input and output streams for Justina 
Stream* pExternalInputs[terminalCount]{ &Serial, nullptr, nullptr, nullptr };       // Justina input streams (nullptr: TCP/IP input stream to be added)
Print* pExternalOutputs[terminalCount]{ &Serial, nullptr, nullptr, nullptr };       // Justina output streams (nullptr: TCP/IP output stream to be added)                                                       

// create Justina interpreter object
Justina justina(pExternalInputs, pExternalOutputs, terminalCount, Justina::SD_runAutoStart);


// -------------------------------
// Create TCP/IP connection object
// -------------------------------

// enter WiFi SSID and password in file secrets.h
constexpr char SSID[] = SERVER_SSID, PASS[] = SERVER_PASS;                          // WiFi SSID and password defined in secrets.h                          

// enter the correct server STATIC IP address and port here (CHECK / ADAPT your ROUTER settings as well)
// (if configured as a HTTP/IP client, this is the IP address and port of the server to connect to) 
const IPAddress serverAddress(192, 168, 1, 45);                                     // STATIC server IP (LAN)
const int serverPort = 8085;                                                        // server port

// enter gateway address, subnet mask and DNS address here (only relevant if configured as server)
const IPAddress gatewayAddress(192, 168, 1, 254);
const IPAddress subnetMask(255, 255, 255, 0);
const IPAddress DNSaddress(195, 130, 130, 5);


// create TCP connection object to connect Arduino as TCP server
Stream** pClientStreams = &(pExternalInputs[1]);                                      // new variable for clarity only; pExternalInputs[0] : Serial stream
// external output streams will be set to external input streams in setup()
TCPconnection myTCPconnection(SSID, PASS, serverAddress, gatewayAddress, subnetMask, DNSaddress, serverPort, true, true, pClientStreams, TCPclientSlots);

// --------------------
// forward declarations
// --------------------

void Justina_housekeeping(long& appFlags);
void heartbeat();

// Justina user c++ functions: interface logic between a Justina program and user-defined c++ routines 
void WiFiOff(void** const pdata, const char* const valueType, const int argCount, int& execError);
void WiFiOn(void** const pdata, const char* const valueType, const int argCount, int& execError);
void TCPoff(void** const pdata, const char* const valueType, const int argCount, int& execError);
void TCPon(void** const pdata, const char* const valueType, const int argCount, int& execError);
void setConnectionTimeout(void** const pdata, const char* const valueType, const int argCount, int& execError);

void setVerbose(void** const pdata, const char* const valueType, const int argCount, int& execError);
void getLocalIP(void** const pdata, const char* const valueType, const int argCount, int& execError);

void stopSessionClient(void** const pdata, const char* const valueType, const int argCount, int& execError);

long getSessionClient(void** const pdata, const char* const valueType, const int argCount, int& execError);
long getWiFiState(void** const pdata, const char* const valueType, const int argCount, int& execError);
long getTCPclientCount(void** const pdata, const char* const valueType, const int argCount, int& execError);


// --------------------------------------------------------
// Define records with Justina user c++ function attributes
// --------------------------------------------------------

// Here, you define which Justina function or command name must be used in a Justina program to call the corresponding c++ function.
// Each record consists of the Justina name, the c++ function name, and the minimum and maximum argument count (checked during parsing in Justina). 
// Records must be grouped by function return type.
// See the documentation for an overview of all function return types. It also explains how to define your own Justina commands.

// NOTE: the 'uf_' prefix in next aliases is not a requirement, but it indicates the function nature AND it highlights such function names if notepad++
// is used as an editor (the Justina language extension must be installed).

// user c++ functions returning nothing as function result
Justina::CppVoidFunction  const cppVoidFunctions[]{
    {"uf_WiFiOff", WiFiOff, 0, 0},
    {"uf_WiFiOn", WiFiOn, 0, 0},
    {"uf_TCPoff", TCPoff, 0, 0},
    {"uf_TCPon", TCPon, 0, 0},
    {"uf_setConnectionTimeout", setConnectionTimeout, 1, 1},
    {"uf_stopSessionClient", stopSessionClient, 2,2},                               // stop the TCP client linked to a session. parameters: sessionID, keepSessionActive 
    {"uf_setVerbose", setVerbose, 1, 1},                                            // set verbose mode. parameter: verbose (true) or silent (false)
    {"uf_getLocalIP", getLocalIP, 1, 1},                                            // get server IP address. parameter: local IP (passed on exit)
};

// user c++ functions returning a Justina integer value (32-bit signed integer)
Justina::CppLongFunction const cppLongFunctions[]{
    {"uf_getWiFiState", getWiFiState, 0, 0},                                        // return WiFi connection state (enumeration)
    {"uf_getClientCount", getTCPclientCount, 0, 0},                                 // return TCP client connection count or -1 (no WiFi or TCP not enabled)
    {"uf_getSessionData", getSessionClient, 2, 2}                                   // return TCP client slot linked to a session. Parameters: session ID, (return parameter): client IP
};

// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);

    delay(2000);

    // connect a pushbutton to pin STOP_ABORT_PIN and a pushbutton to pin CONS_KILL_PIN; connect the other pin of the pushbuttons to ground
    // each button recognizes a short and a long key press (> 1500 ms), so in total we have 4 actions (stop, abort, reset console, kill) 
    pinMode(STOP_ABORT_PIN, INPUT_PULLUP);
    pinMode(CONS_KILL_PIN, INPUT_PULLUP);

    pinMode(HEARTBEAT_PIN, OUTPUT);                 // blinks faster while Justina is running, slower while not in Justina
    pinMode(STATUS_A_PIN, OUTPUT);                  // status A & status B leds OFF: Justina is idle, status A led ON, B led OFF: parsing, ...
    pinMode(STATUS_B_PIN, OUTPUT);                  // ...status A led OFF, led B ON: executing, both leds ON: stopped (idle) in debug mode
    pinMode(ERROR_PIN, OUTPUT);                     // ON when a user error occurs (e.g., division by zero)
    pinMode(DATA_IO_PIN, OUTPUT);                   // ON when bytes are sent or received to/from an IO device (this sketch: Serial)

    pinMode(WiFi_CONNECTED_PIN, OUTPUT);            // 'WiFi connected' led
    pinMode(TCP_CONNECTED_PIN, OUTPUT);             // 'TCP connected' led (will blink if TCP enabled but not yet connected to client)

    bool lampTest = false;                          // delay 4 seconds to give Serial time to start; in the mean time 2 seconds LED test
    do {
        delay(2000);
        lampTest = !lampTest;
        digitalWrite(HEARTBEAT_PIN, lampTest);
        digitalWrite(STATUS_A_PIN, lampTest);
        digitalWrite(STATUS_B_PIN, lampTest);
        digitalWrite(ERROR_PIN, lampTest);
        digitalWrite(DATA_IO_PIN, lampTest);
        digitalWrite(WiFi_CONNECTED_PIN, lampTest);
        digitalWrite(TCP_CONNECTED_PIN, lampTest);
    } while (lampTest);


    // TCP connection
    // --------------
    // TCP/IP output stream pointers (set here) are identical to input stream pointers (set in 'TCPconnection' constructor) but must still be defined
    for (int i = 1; i <= 3; i++) { pExternalOutputs[i] = static_cast<Print*> (pExternalInputs[i]); }

    myTCPconnection.setVerbose(true);                                               // true: enable debug messages from within myTCPconnection
    myTCPconnection.setDebugStream(&Serial);
    myTCPconnection.TCPdisable();                                                   // disable TCP IO (will be enabled by Justina program)
    myTCPconnection.WiFiOn();

    // register system callback function and user (cpp) Justina extensions 
    // -------------------------------------------------------------------
    justina.setSystemCallbackFunction(&Justina_housekeeping);                       // set system callback function (see below); it will be called regularly while control is within Justina 

    justina.registerVoidUserCppFunctions(cppVoidFunctions, 8);                      // register user c++ functions returning nothing
    justina.registerLongUserCppFunctions(cppLongFunctions, 3);                      // register user c++ functions returning a long

    heartbeatPeriod = 500;                                                          // 'short' heartbeat ON and OFF time: heartbeat led will blink at a higher rate when control is within Justina                                              
    justina.begin();                                                                // start interpreter (control will stay there until quitting Justina)
    heartbeatPeriod = 1000;                                                         // 'long' heartbeat ON and OFF time: heartbeat led will blink at a lower rate when control is not within Justina                                              
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


// ----------------------------------------------
// *   Blink a led to show program is running   * 
// ----------------------------------------------

void heartbeat() {
    // note: this is not a 'clock' because it does not measure the passing of fixed time intervals...
    // ...but the passing of MINIMUM time intervals 

    static bool ledOn{ false };
    static uint32_t lastHeartbeat{ 0 };                                                     // last heartbeat time in ms
    static uint32_t previousTime{ 0 };

    uint32_t currentTime = millis();
    // also handle millis() overflow after about 47 days
    if ((lastHeartbeat + heartbeatPeriod < currentTime) || (currentTime < previousTime)) {  // heartbeat period has passed
        lastHeartbeat = currentTime;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_PIN, ledOn);                                                 // change led state
    }
    previousTime = currentTime;
}


// -----------------------------------
// *   handle WiFi/TCP status leds   *
// -----------------------------------

void setConnectionStatusLeds() {

    constexpr uint32_t TCPledOnTime{ 10 }, TCPledOffTime{ 2890 };

    static uint32_t lastLedChangeTime{ 0 };
    static bool TCPblinkingLedState{ false };

    bool WiFiLedOn = (myTCPconnection.getWiFiState() == myTCPconnection.conn_2_WiFi_connected);
    bool TCPledOn{ false };                                                // init

    // TCP enabled and waiting for a client to connect ? prepare to blink 'TCP' led
    int TCPclientCount = myTCPconnection.getTCPclientCount();                               // -1 = WiFi off and/or TCP off
    if (WiFiLedOn) {
        if (TCPclientCount == 0) {                                                          // WiFi on, TCP enabled but no clients connected: prepare to blink TCP led
            uint32_t currentTime = millis();
            int32_t ledStateTime = (TCPblinkingLedState ? TCPledOnTime : TCPledOffTime);    // time between led state changes (on + off period: do not select a multiple of heartbeat period)
            if (((lastLedChangeTime + ledStateTime) < currentTime) || (currentTime < lastLedChangeTime)) { // (note: also handles millis() overflow after about 47 days)
                TCPledOn = !TCPledOn;
                lastLedChangeTime = currentTime;
            }
        }
        else if (TCPclientCount > 0) { TCPledOn = true; }
    }

    digitalWrite(WiFi_CONNECTED_PIN, WiFiLedOn);                                            // ON: 'WiFi connected' 
    digitalWrite(TCP_CONNECTED_PIN, TCPledOn);                                              // blinking: WiFi connected and TCP enabled but no clients connected: ON: at least 1 client connected
}


// ********************************************************************************************************
// ***   interfaces between Justina and user-defined c++ functions (Justina functionality extensions)   ***
// ********************************************************************************************************

// ------------------------------------------------------------------------------------------------------------------
// *   TCP/IP connection: switch OFF or restart WiFi, switch OFF or ON TCP connection, set TCP connection timeout   *
// ------------------------------------------------------------------------------------------------------------------

 /*
    Justina call:
    -------------
    uf_WiFiOff();
    uf_WiFiOn();
    uf_TCPoff();
    uf_TCPon();
    uf_setConnectionTimeout(connectionTimeout);
*/

// switch off WiFi antenna
void WiFiOff(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.WiFiOff();
};

// restart WiFi: switch off first (if WiFi currently on), and start again
void WiFiOn(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.WiFiOn();
};

// disable TCP IO, keepActive
void TCPoff(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.TCPdisable();
};

// enable TCP IO
void TCPon(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    myTCPconnection.TCPenable();
};

// set connection timeout
void setConnectionTimeout(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    bool isLong = ((valueType[0] & Justina::value_typeMask) == Justina::value_isLong);
    bool isFloat = ((valueType[0] & Justina::value_typeMask) == Justina::value_isFloat);
    if (!isLong && !isFloat) { execError = Justina::execResult_type::result_arg_numberExpected; return; }    // numeric argument expected                 

    // get values or pointers to values
    unsigned long TCPconnectionTimeout = isLong ? *(long*)pdata[0] : *(float*)pdata[0];     // fraction is lost 
    myTCPconnection.setConnectionTimeout(TCPconnectionTimeout);
};


// ----------------------------------------------------------------------------------------
// *   stop the underlying client for a session. If endSession is true, end the session   *
// ----------------------------------------------------------------------------------------

 /*
    Justina call:
    -------------
    uf_stopSessionClient(sessionID, endSession);
*/

// stop the TCPIP client currently linked to a session
// if endSession is true, then end the session (and, if a client is connected, stop the client)

void stopSessionClient(void** const pdata, const char* const valueType, const int argCount, int& execError) {
    // NOTE: if you trust the Justina caller and you know the argument type (long or float), you can skip the tests
    int sessionID{};
    bool endSession{ true };
    bool isLong[2]{};
    for (int i = 0; i <= 1; i++) {
        isLong[i] = ((valueType[i] & Justina::value_typeMask) == Justina::value_isLong);
        bool isFloat = ((valueType[i] & Justina::value_typeMask) == Justina::value_isFloat);
        if (!isLong[i] && !isFloat) { execError = Justina::execResult_type::result_arg_numberExpected; return; }      // numeric argument expected                 
    }
    sessionID = isLong ? (*(long*)pdata[0] - 1) : ((long)(*(float*)pdata[0])) - 1;              // Justina caller uses base 1 for session ID: convert
    if ((sessionID < 0) || (sessionID >= TCPclientSlots)) { execError = Justina::execResult_type::result_arg_outsideRange; return; }    // argument outside range

    endSession = (bool)isLong[1] ? (bool)(*(long*)pdata[1]) : (bool)(*(float*)pdata[1]);
    myTCPconnection.stopSessionClient(sessionID, endSession);
};


// ------------------------------------------------
// *   TCP/IP connection: set verbose or silent   *
// ------------------------------------------------

 /*
    Justina call:
    -------------
    uf_setVerbose(state);                   // state = 0: silent, 1: verbose
*/

void setVerbose(void** const pdata, const char* const valueType, const int argCount, int& execError) {

    // NOTE: if you trust the Justina caller and you know the argument type (long or float), you can skip the tests
    bool isLong = ((valueType[0] & Justina::value_typeMask) == Justina::value_isLong);
    bool isFloat = ((valueType[0] & Justina::value_typeMask) == Justina::value_isFloat);
    if (!isLong && !isFloat) { execError = Justina::execResult_type::result_arg_numberExpected; return; }           // numeric argument expected                 

    // get values or pointers to values
    long state = isLong ? *(long*)pdata[0] : *(float*)pdata[0];                                                     // fraction is lost 
    if ((state < 0) || (state > 1)) { execError = Justina::execResult_type::result_arg_outsideRange; return; }      // argument outside range

    myTCPconnection.setVerbose(bool(state));
};


// ------------------------------------
// *   return WiFi connection state   *
// ------------------------------------

long getWiFiState(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call:
    -------------
    uf_getWiFiState();

    connection state returned:
    0: WiFi not connected
    1: trying to connect to WiFi
    2: WiFi connected
 */

    return (long)myTCPconnection.getWiFiState();                    // state = 0 to 4 (see connectionState enumeration)
};


// -------------------------------------------
// *   return TCP clients connection state   *
// -------------------------------------------

long getTCPclientCount(void** const pdata, const char* const valueType, const int argCount, int& execError) {

 /*
    Justina call:
    -------------
    uf_getTCPstate();

    connection state returned:
    -1:     no WiFi or TCP disabled
    0 to n: number of connected clients
 */

    return (long)myTCPconnection.getTCPclientCount();               // -1 = WiFi off and/or TCP disabled
};


// -------------------------------
// *   return local IP address   *
// -------------------------------

void getLocalIP(void** const pdata, const char* const valueType, const int argCount, int& execError) {

    // NOTES: (1) the first Justina argument supplied must be a variable, containing string of at least 15 characters (+ terminating '\0')
    //            the local IP address (as a string) is returned to the first Justina argument.
    //        (2) if you trust the Justina caller, you can skip the tests
    //        (3) do NOT return as a char* as a user cpp function result (see comments on returning strings in the documentation)

  /*
    Justina call:
    -------------
    var a = "";
    uf_getLocalIP(a=space(15));
*/

    if (!(valueType[0] & 0x80)) { execError = Justina::execResult_type::result_arg_variableExpected; return; }
    bool isString = ((valueType[0] & Justina::value_typeMask) == Justina::value_isString);
    if (!isString) { execError = Justina::execResult_type::result_arg_stringExpected; return; }                         // string expected      
    if (strlen(((char*)pdata[0])) < 15) { execError = Justina::execResult_type::result_arg_stringTooShort; return; }    // string too short (to contain IP as string)


    *(char*)pdata[0] = '\0';                                                                // init
    IPAddress IP = { 0,0,0,0 };                                                             // init
    if (myTCPconnection.getWiFiState() == myTCPconnection.conn_2_WiFi_connected) {
        IP = WiFi.localIP();                                                                // defined via WiFi(NINA).h
    }
    sprintf((char*)pdata[0], "%d.%d.%d.%d", IP[0], IP[1], IP[2], IP[3]);
};


// ----------------------------------------
// *   return session client IP address   *
// ----------------------------------------

long getSessionClient(void** const pdata, const char* const valueType, const int argCount, int& execError)
{
    // the first argument supplies the session ID
    // the second Justina argument supplied must be a variable, containing a string of at least 15 characters (+ terminating '\0')
    // the remote IP address (as a string) is returned to the second Justina argument.
    // NOTE: if you trust the Justina caller, you can skip the tests

  /*
    Justina call:
    -------------
    var a = "";
    clientSlot = uf_getSessionData(sessionID, IP=space(15));                                    // IP must be a variable long enough to receive a 15-character string
*/

    // NOTE: if you trust the Justina caller and you know the argument type (long or float), you can skip the tests

    // on entry: test all argument types, test entry values
    // ----------------------------------------------------
    bool isLong = ((valueType[0] & Justina::value_typeMask) == Justina::value_isLong);
    bool isFloat = ((valueType[0] & Justina::value_typeMask) == Justina::value_isFloat);
    if (!isLong && !isFloat) { execError = Justina::execResult_type::result_arg_numberExpected; return -1; }   // numeric argument expected                 

    int sessionID = isLong ? (*(long*)pdata[0] - 1) : ((long)(*(float*)pdata[0])) - 1;          // Justina caller uses base 1 for session ID
    if ((sessionID < 0) || (sessionID >= TCPclientSlots)) { execError = Justina::execResult_type::result_arg_outsideRange; return -1; }      // argument outside range

    // IP argument: must be a 15-character string on entry on entry
    if (!(valueType[1] & 0x80)) { execError = Justina::execResult_type::result_arg_variableExpected; return -1; }
    bool isString = ((valueType[1] & Justina::value_typeMask) == Justina::value_isString);
    if (!isString) { execError = Justina::execResult_type::result_arg_stringExpected; return -1; }                          // string expected      
    if (strlen(((char*)pdata[1])) < 15) { execError = Justina::execResult_type::result_arg_stringTooShort; return -1; }     // string too short (to contain IP as string)


    // call TCPconnection method 'getSessionClient'
    // -----------------------------------------
    IPAddress IP{};
    int clientSlot = myTCPconnection.getSessionClient(sessionID, IP);


    // populate return argument
    // ------------------------

    *(char*)pdata[1] = '\0';                                                        // init
    sprintf((char*)pdata[1], "%d.%d.%d.%d", IP[0], IP[1], IP[2], IP[3]);

    // return the client slot and IP address of a currently connected client for a session. If no connected client, return -1 
    return clientSlot;
};


// -------------------------------------------------------------------------------------
// *   Debounce keys and return key state, key goes down, key goes up,                 *
// *   is short press (when key goes up), is long press (while key is still pressed)   *
// -------------------------------------------------------------------------------------

void keyStates(uint8_t pinStates, uint8_t& debounced, uint8_t& wentDown, uint8_t& wentUp, uint8_t& isShortPress, uint8_t& isLongPress) {

    // constants
    static constexpr int keyCount = 2;                                              // 2 pushbuttons
    static constexpr long debounceTime = 10;                                        // in ms
    static constexpr long alternateActionTime = 1500;                               // in ms (long press pushbutton)

    // static variables
    static long pinChangeTimes[keyCount]{ millis(), millis() };
    static uint8_t oldPinStates{ 0xff };                                            // init: assume keys are up
    static uint8_t debouncedStates{ 0xff };
    static uint8_t oldDebouncedStates{ 0xff };
    static uint8_t enableKeyActions{ 0xff };

    // debounce keys  
    // -------------
    long now = millis();
    uint8_t pinChanges = pinStates ^ oldPinStates;                                  // flag pin changes since previous sample

    for (int i = 0; i < keyCount; i++) {
        uint8_t pinMask = (1 << i);
        if (pinChanges & pinMask) { pinChangeTimes[i] = now; }                      // pin change ? note the time
        // pins stable for more than minimum debounce time ? change debounced state
        else if (pinChangeTimes[i] + debounceTime < now) { debouncedStates = (debouncedStates & ~pinMask) | (pinStates & pinMask); }
    }
    oldPinStates = pinStates;

    // determine key actions
    // ---------------------
    uint8_t doPrimaryKeyActions{ false }, doAlternateKeyActions{ false };
    uint8_t debouncedStateChanges = debouncedStates ^ oldDebouncedStates;           // flag debounced state changes since previous sample

    for (int i = 0; i < keyCount; i++) {
        uint8_t pinMask = (1 << i);
        bool keyGoesDown = debouncedStateChanges & (~debouncedStates) & pinMask;
        bool keyGoesUp = debouncedStateChanges & debouncedStates & pinMask;
        bool keyIsDown = (~debouncedStates) & pinMask;

        if (keyGoesDown) { enableKeyActions |= pinMask; }
        else if (keyGoesUp) {
            if ((enableKeyActions & pinMask) && (pinChangeTimes[i] + alternateActionTime >= now)) { doPrimaryKeyActions |= pinMask; }
        }
        else if (keyIsDown) {
            if ((enableKeyActions & pinMask) && (pinChangeTimes[i] + alternateActionTime < now)) { doAlternateKeyActions |= pinMask; enableKeyActions &= ~pinMask; }
        }
    }
    oldDebouncedStates = debouncedStates;

    // return values
    // -------------
    debounced = debouncedStates; wentDown = debouncedStateChanges & (~debouncedStates); wentUp = debouncedStateChanges & debouncedStates;
    isShortPress = doPrimaryKeyActions; isLongPress = doAlternateKeyActions;
    return;
}



// ----------------------------------------
// *   Justina system callback function   *
// ----------------------------------------

    // The callback function communicates with Justina via a set of 32 application flags, some used to pass the Justina status to the callback function and 
    // some to read back 'requests' provided by the callback function. Some of the flags are unassigned (full information can be found in the user manual).

void Justina_housekeeping(long& appFlags) {

    // 1.execute procedures at specific intervals
    // ------------------------------------------
    heartbeat();                                                                    // blink a led to show program is running

    myTCPconnection.maintainConnection();                                           // maintain TCP connection
    setConnectionStatusLeds();                                                      // set WiFi and TCP connection status leds 

    // (you would add calls to other methods requiring execution at regular intervals here)
    // ...


    // 2. detect stop, abort, console reset and kill requests and submit to Justina   
    // ----------------------------------------------------------------------------
    static bool errorCondition = false, statusA = false, statusB = false, dataInOut = false;

    uint8_t debouncedStates, wentDown, wentUp, isShortPress, isLongPress;
    uint8_t pinStates = (digitalRead(STOP_ABORT_PIN) << 1) | digitalRead(CONS_KILL_PIN);
    keyStates(pinStates, debouncedStates, wentDown, wentUp, isShortPress, isLongPress);

    appFlags = (appFlags & ~Justina::appFlag_requestMask);                          // reset requests
    // short press: key goes up (edge) while time is less than threshold
    // long press: time passes threshold (edge) while key is still down
    if (isShortPress & (1 << 0)) { appFlags |= Justina::appFlag_consoleRequestBit; }
    if (isLongPress & (1 << 0)) { appFlags |= Justina::appFlag_killRequestBit; }
    if (isShortPress & (1 << 1)) { appFlags |= Justina::appFlag_stopRequestBit; }
    else if (isLongPress & (1 << 1)) { appFlags |= Justina::appFlag_abortRequestBit; }


    // 3. use four leds to indicate status and error condition 
    // -------------------------------------------------------
    // application flags: read Justina flags and set indicator leds (only adapt if change detected)
    if (errorCondition ^ (appFlags & Justina::appFlag_errorConditionBit)) {
        errorCondition = (appFlags & Justina::appFlag_errorConditionBit);  digitalWrite(ERROR_PIN, errorCondition);
    }
    // status A & status B:  both leds OFF: idle, status A led ON: parsing, status B led ON: executing, both leds ON: stopped in debug mode  
    if (statusA ^ (appFlags & Justina::appFlag_statusAbit)) { statusA = (appFlags & Justina::appFlag_statusAbit);  digitalWrite(STATUS_A_PIN, statusA); }
    if (statusB ^ (appFlags & Justina::appFlag_statusBbit)) { statusB = (appFlags & Justina::appFlag_statusBbit);  digitalWrite(STATUS_B_PIN, statusB); }

    bool newDataLedState{ false };
    static bool dataLedState{ false };
    if (appFlags & Justina::appFlag_dataInOut) { newDataLedState = !dataLedState; }
    else { newDataLedState = false; }                                       // if data, toggle state, otherwise reset state
    if (newDataLedState != dataLedState) { dataLedState = newDataLedState;  digitalWrite(DATA_IO_PIN, dataLedState); }  // only write if change detected
}

