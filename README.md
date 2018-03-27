# ArduinoLightController

Simple step sequencer for up to 8 PWM output channels.
PWM is done in software and is probably not super precise,
but works well enough for controlling LED strips.

Controllable using a forth-like language over serial or UDP.

Designed and tested for ESP8266 boards,
but should be easily adaptable to other platforms supported by the Arduino toolchain.

It might be nice to add an OSC API someday,
but the Forth one is nice because everything is plaintext
and the same protocol is used over serial and UDP.

Can be configured to connect to any of a set of WiFi networks
listed in ```config.h```.

If ```PRESENCE_BROADCAST_PORT``` is not zero (it defaults to 7008).

## Compiling

- Copy ```config.h.example``` to ```config.h```.
- Replace "SomeNetwork","SomePassword" with the SSID and password for any WiFi network you want to connect to.
- Make sure you have the required libraries installed (namely, the ESP8266 'board' library)
- Compile in your favorite Arduino IDE
