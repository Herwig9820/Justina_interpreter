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

/*
    Example code demonstrating the use of Justina system callbacks
	--------------------------------------------------------------
	The purpose of system callbacks (executed in the background, multiple times per second), is to
    - ensure that procedures that need to be executed at regular intervals (e.g. maintaining a TCP         
      connection, etc.) continue to be executed while control is within Justina                            
    - detect stop, abort, console reset and kill requests, e.g., to request aborting a running             
      Justina program stuck in an endless loop, when a user presses a pushbutton wired to an input pin                             
    - retrieve the Justina interpreter state (idle, parsing, executing, stopped in      
      debug mode, error), for instance to blink a led or produce a beep when a user error is made      
    without the need for Justina to have any knowledge about the hardware (pins, ...) and without using timer interrupts.                 

    This sketch demonstrates the use of system callbacks to blink a heartbeat led; detect stop, abort,
    console reset and kill requests and switch indicator leds displaying the current Justina status ON or OFF. 

    MORE INFORMATION: see Justina USER MANUAL, available on GitHub
*/


// input pins
constexpr int CONS_KILL_PIN{ 3 };                                           // read 'reset console' and 'kill Justina' requests
constexpr int STOP_ABORT_PIN{ 4 };                                          // read 'stop running program' and 'abort running code' requests

// output pins: connect each output pin to the anode of a LED and connect each cathode to one terminal of a resistor. Wire the other terminal to ground. 
constexpr int DATA_IO_PIN{ 5 };                                             // signals Justina is sending or receiving data (from any external IO device) 
constexpr int STATUS_A_PIN{ 6 };                                            // status A and B: Justina status
constexpr int STATUS_B_PIN{ 7 };
constexpr int ERROR_PIN{ 8 };                                               // a Justina error occurred (e.g., division by zero)  
constexpr int HEARTBEAT_PIN{ 9 };                                           // a square wave is output to indicate 'Justina is running'                                                


// create Justina_interpreter object with default values: IO via Serial only, SD card allowed, default SD card CS pin.
Justina justina;

unsigned long heartbeatPeriod{ 1000 };                                      // heartbeat ON and OFF time

void Justina_housekeeping(long& appFlags);                                  // forward declarations
void heartbeat();

// -------------------------------
// *   Arduino setup() routine   *
// -------------------------------

void setup() {
    Serial.begin(115200);
    delay(5000);

    // connect a pushbutton to pin STOP_ABORT_PIN and a pushbutton to pin CONS_KILL_PIN; connect the other pin of the pushbuttons to ground
    // each button recognizes a short and a long key press (> 1500 ms), so in total we have 4 actions (stop, abort, reset console, kill) 
    pinMode(STOP_ABORT_PIN, INPUT_PULLUP);
    pinMode(CONS_KILL_PIN, INPUT_PULLUP);

    pinMode(HEARTBEAT_PIN, OUTPUT); digitalWrite(HEARTBEAT_PIN, LOW);       // blinks faster while Justina is running, slower while not in Justina
    pinMode(STATUS_A_PIN, OUTPUT); digitalWrite(STATUS_A_PIN, LOW);         // status A & status B leds OFF: Justina is idle, status A led ON, B led OFF: parsing, ... 
    pinMode(STATUS_B_PIN, OUTPUT); digitalWrite(STATUS_B_PIN, LOW);         // ...status A led OFF, led B ON: executing, both leds ON: stopped (idle) in debug mode
    pinMode(ERROR_PIN, OUTPUT); digitalWrite(ERROR_PIN, LOW);               // ON when a user error occurs (e.g., division by zero)
    pinMode(DATA_IO_PIN, OUTPUT); digitalWrite(DATA_IO_PIN, LOW);           // ON when bytes are sent or received to/from an IO device (this sketch: Serial)

    justina.setSystemCallbackFunction(&Justina_housekeeping);               // set callback function; it will be called regularly while control is within Justina 

    heartbeatPeriod = 500;                                                  // 'short' heartbeat ON and OFF time: heartbeat led will blink at a higher rate when control is within Justina                                              
    justina.begin();                                                        // run interpreter (control will stay there until you quit Justina)           
    heartbeatPeriod = 1000;                                                 // 'long' heartbeat ON and OFF time: heartbeat led will blink at a lower rate when control is not within Justina                                              

    Serial.println("Justina session ended");
}


// ------------------------------
// *   Arduino loop() routine   *
// ------------------------------

void loop() {
    heartbeat();                                                            // returned from Justina; keep the led blinking

    // (you would add calls to other methods requiring execution at regular intervals here)
    // ...
}


// ----------------------------------------------
// *   Blink a led to show program is running   * 
// ----------------------------------------------

void heartbeat() {
    // note: this is not a 'clock' because it does not measure the passing of fixed time intervals
    // but the passing of MINIMUM time intervals 

    static bool ledOn{ false };
    static uint32_t lastHeartbeat{ 0 };                                     // last heartbeat time in ms
    static uint32_t previousTime{ 0 };

    uint32_t currentTime = millis();
    // also handle millis() overflow after about 47 days
    if ((lastHeartbeat + heartbeatPeriod < currentTime) || (currentTime < previousTime)) {  // heartbeat period has passed
        lastHeartbeat = currentTime;
        ledOn = !ledOn;
        digitalWrite(HEARTBEAT_PIN, ledOn);                                 // change led state
    }
    previousTime = currentTime;
}


// -------------------------------------------------------------------------------------
// *   Debounce keys and return key state, key goes down, key goes up,                 *
// *   is short press (when key goes up), is long press (while key is still pressed)   *
// -------------------------------------------------------------------------------------

void keyStates(uint8_t pinStates, uint8_t& debounced, uint8_t& wentDown, uint8_t& wentUp, uint8_t& isShortPress, uint8_t& isLongPress) {

    // constants
    static constexpr int keyCount = 2;                                      // 2 pushbuttons
    static constexpr long debounceTime = 10;                                // in ms
    static constexpr long alternateActionTime = 1500;                       // in ms (long press pushbutton)

    // static variables
    static long pinChangeTimes[keyCount]{ millis(), millis() };
    static uint8_t oldPinStates{ 0xff };                    // init: assume keys are up
    static uint8_t debouncedStates{ 0xff };
    static uint8_t oldDebouncedStates{ 0xff };
    static uint8_t enableKeyActions{ 0xff };

    // debounce keys  
    // -------------
    long now = millis();
    uint8_t pinChanges = pinStates ^ oldPinStates;                          // flag pin changes since previous sample

    for (int i = 0; i < keyCount; i++) {
        uint8_t pinMask = (1 << i);
        if (pinChanges & pinMask) { pinChangeTimes[i] = now; }              // pin change ? note the time
        // pins stable for more than minimum debounce time ? change debounced state
        else if (pinChangeTimes[i] + debounceTime < now) { debouncedStates = (debouncedStates & ~pinMask) | (pinStates & pinMask); }
    }
    oldPinStates = pinStates;

    // determine key actions
    // ---------------------
    uint8_t doPrimaryKeyActions{ false }, doAlternateKeyActions{ false };
    uint8_t debouncedStateChanges = debouncedStates ^ oldDebouncedStates;   // flag debounced state changes since previous sample

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
    heartbeat();                                                            // blink a led to show program is running

    // (you would add calls to other methods requiring execution at regular intervals here)
    // ...

    
    // 2. detect stop, abort, console reset and kill requests and submit to Justina   
    // ----------------------------------------------------------------------------
    static bool errorCondition = false, statusA = false, statusB = false, dataInOut = false;

    uint8_t debouncedStates, wentDown, wentUp, isShortPress, isLongPress;
    uint8_t pinStates = (digitalRead(STOP_ABORT_PIN) << 1) | digitalRead(CONS_KILL_PIN);
    keyStates(pinStates, debouncedStates, wentDown, wentUp, isShortPress, isLongPress);

    appFlags = (appFlags & ~Justina::appFlag_requestMask);                  // reset requests
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


