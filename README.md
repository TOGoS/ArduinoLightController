# ArduinoLightController

Simple step sequencer for up to 8 PWM output channels.
PWM is done in software and is probably not super precise,
but works well enough for controlling LED strips.

Controlelable using a forth-like language over serial.

Maybe simplified controls should be made available by OSC,
but the existing Arduino OSC libraies have crappy APIs
so I may need to build my own header-only one.

Designed and tested for ESP8266 boards,
but should be easily adaptable to other platforms supported by the Arduino toolchain.
