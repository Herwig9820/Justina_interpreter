# Justina_interpreter

Justina is both an interpreter and an easy-to-use but capable programming language for Arduino. It was developed and built around a few objectives. 
* On top of the list: simplicity for the user. Justina is a structured language, but it’s non-object oriented (as opposed to the powerful but more complex c++ language). It has some similarities with Basic, a language that has been around for quite some time.
* Equally important: it was built with Arduino in mind - more specifically, 32-bit Arduino’s: boards with a SAMD processor (like the nano 33 IoT), the nano ESP32 and the nano RP2040.
  
Justina does not impose any requirements or restrictions related to hardware (pin assignments, interrupts, timers,... - it does not use any), nor does it need to have any knowledge about it for proper operation.
The Justina syntax has been kept as simple as possible. A program consists of statements. A statement either consists of
*	a single expression (always yielding a result).
*	a command, starting with a keyword, optionally followed by a list of expressions (such a statement is called a command, because it ‘does’ something without actually calculating a result)

Because Justina is an interpreted language, a Justina  program is not compiled into machine language but it is parsed into so called tokens before execution. Parsing is a fast process, which makes Justina the ideal tool for quick prototyping. Once it is installed as an Arduino library, call Justina from within an Arduino c++ program and you will have the Justina interpreter ready to receive commands, evaluate expressions and execute Justina programs. 
You can enter statements directly in the command line of the Arduino IDE (the Serial monitor by default, a TCP IP client, ...) and they will immediately get executed, without any programming.

#### A basic example, without programming: set the value of an Arduino pin
Typing  *pinMode( 17, OUTPUT); digitalWrite(17, HIGH)* (+ ENTER) in the command line, will write a HIGH value to pin 17. Typing *digitalRead(17)* will then read back the value from pin 17, which will be '1' of course.

![image](https://github.com/Herwig9820/Justina_interpreter/assets/74488682/d5d3b732-3c81-41a1-b9d5-e79214ece220)

Statements you type are echoed after the Justina prompt (“Justina>“), so you have a nice history. Multiple statements can be entered at the same time, separated by semicolons.
The result of  the last expression entered in the command line is printed on the next line. In this example: both digitalWrite and digitalRead are functions, digitalWrite returning the value written to the pin (1 is the value of predefined constant HIGH) and digitalRead reading back that same value from the pin .


# A few highlights
*	More than 250 built-in functions, commands and operators, 70+ predefined symbolic constants.
*	More than 30 functions directly targeting Arduino IO ports and memory, including some new.
*	Extended operator set includes relational, logical, bitwise operators, compound assignment operators, pre- and postfix increment operators.
*	Two angle modes: radians and degrees.
*	Scalar and array variables.
*	Floating-point, integer and string data types.
*	Perform integer arithmetic and bitwise operations in decimal or hexadecimal number format.
*	Display settings define how to display calculation results: output width, number of digits / decimals to display, alignment, base (decimal, hex), …
* Input and output: Justina reads data from / writes data to multiple input and output devices (connected via Serial, TCP IP, SPI, I2C...). You can even switch the console from the default (typically Serial) to another input or output device (for instance, switch console output to an OLED screen).
* With an SD card breakout board connected via SPI, Justina creates, reads and writes SD card files...

 ![image](https://github.com/Herwig9820/Justina_interpreter/assets/74488682/ef5ce67e-6a07-4944-8282-93b94ed63ae3)
* In Justina, input and output commands work with argument lists: for instance, with only one statement, you can read a properly formatted textline from a terminal or an SD card file and parse its contents into a series of variables.



# Programming
*	Write program functions with mandatory and optional parameters, accepting scalar and array arguments. When calling a function, variables (including arrays) are passed by reference. Constants and results of expressions are passed by value. 
*	Variables or constants declared within a program are either global (accessible throughout the Justina program), local (accessible within a Justina function) or static (accessible within one Justina function, value preserved between calls)
*	Variables not declared within a program but by a user from the command line, are called user variables (or user constants)
*	Programs have access to user variables and users have access to global program variables (from the command line. User variables preserve their values when a program is cleared or another program is loaded.
*	Parsing and execution errors are clearly indicated, with error numbers identifying the nature of the error. 
*	Error trapping: if enabled, an error will not terminate a program, instead the error can be handled in code (either in the procedure where the error occurred or in a 'caller' procedure). It’s even possible to trap an error in the command line

# Justina program editing
You can use any text editor to write and edit your programs. But you might consider using Notepad++ as text editor, because a specific 'User Defined Language' (UDL) file for Justina is available to provide Justina syntax highlighting.

![image](https://github.com/Herwig9820/Justina_interpreter/assets/74488682/75c00179-27c6-429a-b155-97d2e8d06414)


# Debugging
When a program is stopped (either by execution of the ‘stop’ command, by user intervention or by an active breakpoint) debug mode is entered. You can then single step the program, execute statements until the end of a loop, a next breakpoint…

Breakpoints can be activated based on a trigger expression or a hit count. You can also include a list of ‘view expressions’ for each breakpoint, and Justina will automatically trace specific variables or even expressions, letting you watch their values change as you single step through the program or a breakpoint is hit.

![image](https://github.com/Herwig9820/Justina_interpreter/assets/74488682/39b60741-2eb0-4418-947d-ea6a9da27035)

While a procedure is stopped in debug mode, you can also manually review the procedure’s local and static variable contents or view the call stack.

# Integration with c++
1.	If enabled, system callbacks allow the Arduino program to perform periodic housekeeping tasks beyond the control of Justina (e.g., maintaining a TCP connection, producing a beep when an error is encountered, aborting, or stopping a Justina program...). For that purpose, a set of system flags passes information back and forth between the main Arduino program and Justina at regular intervals (without the need for interrupts).
2.	Time-critical user routines, functions targeting specific hardware and functions extending Justina functionality in general can be written in c++, given an alias and 'registered' (using a standard mechanism), informing Justina about the minimum and maximum number of arguments and the return type. From then onward, these C++ functions can be called just like any other Justina function, with the same syntax, using the alias as function name and passing scalar or array variables as arguments.

# Arduino c++ examples
A number of c++ example files, demonstrating how to call Justina, are provided in the repository folder 'examples':
* Justina_easy: simply call Justina, and that's it
* Justina_systemCallback: use system callback functions to
  * ensure that procedures that need to be executed at regular intervals continue to be executed while control is within Justina. In this ecample: blinking a heartbeat led
  * to detect stop, abort, 'console reset' and kill requests (for example when a user presses a 'program abort' pushbutton),...
  * to set status leds reporting the interpreter state (idle, stopped in debug mode, parsing, executing) and indicating whether a user or program error occured (e.g. to blink a led)
and this without the need for Justina to have any knowledge about the hardware (pins, ...) used
* Justina_userCPP: demonstrates how to write user c++ functions for use in a Justina program (for time critical or specific hardware oriented stuff or to extend built-in Justina functionality...). These c++ functions can then be called from Justina (from the Justina command line or from a Justina program), just like any other Justina function, with the same syntax, using an alias as function name and passing scalar or array variables as arguments
* Justina_userCPP_lib: demonstrates how to create a Justina user c++ 'library' file. 
* Justina_OLED: demonstrates how to add additional IO devices for use by Justina (in this example: output only)
  * using an OLED display with SH1106 controller communicating over software (SW) SPI as additional output device
  * using an OLED display with SSD1306 controller communicating over I2C as additional input device 
* Justina_TCPIP: demonstrates various Justina features, namely
  * setting up a TCP/IP server for use as an additional Justina IO channel
  * using Justina system callbacks to maintain the TCP/IP connection, blink a heartbeat led and set status leds to indicate the TCP/IP connection state
  * using Justina user c++ functions to control the TCP/IP connection from within Justina

# Justina language examples
A few Justina language example files are provided in the repository folder 'extras/Justina_language_examples'. These text files obey the 8.3 file format, to make them compatible with the Arduino SD card file system. Also, they all have the '.jus' extension: opening these files in Notepad++ will automatically invoke Justina language highlighting (if the the Justina language extension is installed).

![image](https://github.com/Herwig9820/Justina_interpreter/assets/74488682/18d82b3d-26ec-4c1a-9722-71c86fd4621d)

The example files are:
* start.jus: can be used as startup program (if your Arduino is equipped with an SD card board). It sets things like the angle mode, number formatting etc.
* myFirst.jus: a really simple Justina program
* fact.jus: a recursive method to calculate factorials
* input.jus: ask for user input; parse and execute that input within a running program
* overlap.jus: two method to print lines with overlapping print fields
* SD_test.jus: perform some basic SD card tests
* SD_parse.jus: write formatted data to an SD card, read it back and immediately parse this data into variables
* web_calc.jus: a web server creating a scientific calculator on a web page

 
# Documentation
Full documentation is provided in the repository ' extras' folder. 
