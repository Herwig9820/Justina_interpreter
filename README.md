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
