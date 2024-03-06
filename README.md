# Justina_interpreter

Justina is both an easy-to-use but capable programming language for Arduino and an interpreter, developed and built around a few objectives. 
* On top of the list: simplicity for the user. Justina is a structured language, but it’s non-object oriented (as opposed to the powerful but more complex c++ language). It has some similarities with Basic, a language that has been around for quite some time.
* Equally important: it was built with Arduino in mind - more specifically, 32-bit Arduino’s: boards with a SAMD processor (like the nano 33 IoT), the nano ESP32 and the nano RP2040.
  
Justina does not impose any requirements or restrictions related to hardware (pin assignments, interrupts, timers,... - it does not use any), nor does it need to have any knowledge about it for proper operation.
The Justina syntax has been kept as simple as possible. A program consists of statements. A statement either consists of
*	a single expression (always yielding a result).
*	a command, starting with a keyword, optionally followed by a list of expressions (such a statement is called a command, because it ‘does’ something without actually calculating a result)

Because Justina is an interpreted language, a Justina  program is not compiled into machine language but it is parsed into so called tokens before execution. Parsing is a fast process, which makes Justina the ideal tool for quick prototyping. Once it is installed as an Arduino library, call Justina from within an Arduino c++ program and you will have the Justina interpreter ready to receive commands, evaluate expressions and execute Justina programs. 
You can enter statements directly in the command line of the Arduino IDE (the Serial monitor by default, a TCP IP client, ...) and they will immediately get executed, without any programming.

# Example
Typing  pinMode( 17, OUTPUT); digitalWrite(17, HIGH) (+ ENTER) in the command line, will write a HIGH value to pin 17.

# A few highlights
*	More than 250 built-in functions, commands and operators, 70+ predefined symbolic constants.
*	More than 30 functions directly targeting Arduino IO ports and memory, including some new.
*	Extended operator set includes relational, logical, bitwise operators, compound assignment operators, pre- and postfix increment operators.
*	Two angle modes: radians and degrees.
*	Scalar and array variables.
*	Floating-point, integer and string data types.
*	Perform integer arithmetic and bitwise operations in decimal or hexadecimal number format.
*	Display settings define how to display calculation results: output width, number of digits / decimals to display, alignment, base (decimal, hex), …
*	Input and output: Justina reads data from / writes data to multiple input and output devices (for instance connected via Serial, TCP client, SD card files – if an SD card reader is connected). You can even switch the console from the default (typically Serial) to a TCP IP client, etc. 
*	In Justina, input and output commands work with argument lists: for instance, with only one statement, you can read a properly formatted textline from a terminal or an SD card file and parse its contents into a series of variables.

# Programming
*	Write program functions with mandatory and optional parameters, accepting scalar and array arguments. When calling a function, variables (including arrays) are passed by reference. Constants and results of expressions are passed by value. 
*	Variables or constants declared within a program are either global (accessible throughout the Justina program), local (accessible within a Justina function) or static (accessible within one Justina function, value preserved between calls)
*	Variables not declared within a program but by a user from the command line, are called user variables (or user constants)
*	Programs have access to user variables and users have access to global program variables (from the command line. User variables preserve their values when a program is cleared or another program is loaded.
*	Parsing and execution errors are clearly indicated, with error numbers identifying the nature of the error. 
*	Error trapping: if enabled, an error will not terminate a program, instead the error can be handled in code (either in the procedure where the error occurred or in a 'caller' procedure). It’s even possible to trap an error in the command line

# Program editing
You can use any text editor to write and edit your programs. But you might consider using Notepad++ as text editor, because a specific 'User Defined Language' (UDL) file for Justina is available to provide Justina syntax highlighting.

# Debugging
When a program is stopped (either by execution of the ‘stop’ command, by user intervention or by an active breakpoint) debug mode is entered. You can then single step the program, execute statements until the end of a loop, a next breakpoint…
Breakpoints can be activated based on a trigger expression or a hit count. You can also include a list of ‘view expressions’ for each breakpoint, and Justina will automatically trace specific variables or even expressions, letting you watch their values change as you single step through the program or a breakpoint is hit.
While a procedure is stopped in debug mode, you can manually review the procedure’s local and static variable contents or view the call stack.

# Integration with c++
1.	If enabled, system callbacks allow the Arduino program to perform periodic housekeeping tasks beyond the control of Justina (e.g., maintaining a TCP connection, producing a beep when an error is encountered, aborting, or stopping a Justina program...). For that purpose, a set of system flags passes information back and forth between the main Arduino program and Justina at regular intervals (without the need for interrupts).
2.	Time-critical user routines, functions targeting specific hardware and functions extending Justina functionality in general can be written in c++, given an alias and 'registered' (using a standard mechanism), informing Justina about the minimum and maximum number of arguments and the return type. From then onward, these C++ functions can be called just like any other Justina function, with the same syntax, using the alias as function name and passing scalar or array variables as arguments.

# Examples
A number of c++ example files, demonstrating how to call Justina, are provided in the repository folder 'examples':
* Justina_easy: simply calling Justina, that's it
* Justina_userCPP: demonstrating how to write user c++ functions for use in a Justina program (for time critical or specific hardware oriented stuff or to extend built-in Justina functionality...)
* Justina_systemCallback: using system callback functions to ensure that procedures that need to be executed at regular intervals (e.g. maintaining a TCP connection, etc.) continue to be executed while control is within Justina; to detect stop, abort, 'console reset' and kill requests (for example when a user presses a 'program abort' pushbutton),... and this without the need for Justina to have any knowledge about the hardware (pins, ...) used
* Justina_multiTerminal: demonstrating the setup for using two monochrome 128 x 64 dot OLED displays AND / OR a TCP terminal program on your computer as alternative output devices for Justina, next to Serial

A few Justina language example files are provided in the repository folder 'extras/Justina_language_examples':


# Documentation
Further documentation is provided in repository ' extras' folder. 
