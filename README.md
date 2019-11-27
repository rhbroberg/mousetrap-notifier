# mousetrap-notifier

Build a better mousetrap?  Too difficult!  Build a notification system for your existing mousetraps?  That's more like it.

I'm using an ESP8266 nodemcu. Wiring:
    GPIO 16 (D0) is wired to RST with a 10k resistor (so that the rst pin works during deep sleep) to enable deep sleep.
    GPIO 12 (D6) goes to 1 end of the mousetrap (like the bail); ground goes to the trigger arm.  It is essentially an Normally Closed (NC) switch, which opens when the trap is triggered and the bail and the arm are no longer in contact with each other.

The sketch deep sleeps for a while (not yet fully configurable), wakes up, and checks the NC switch.  If it is freshly open, send URL to a notification service (currently ifttt.com).  The device will auto-arm once the circuit goes back to closed.  Only one notification is sent once sprung until re-armed.

OTA Flashing:
Within 1s of reset button being pressed, hold the flash button to enter over-the-air update mode: from here the Arudino IDE on the same subnet can flash OTA.  After the flash it will auto-reset and resume operation.

LED status patterns:

1 blink every 10s: While device is in OTA mode the led will blink to remind you the radio is on

5 fast blinks: at the beginning and end of the image receipt in OTA mode

8 quick blinks: when the trap is sprung, if message sent successfully

24 very fast blinks: when the trap is spring, if message failed to send

3 short blinks: when the trap is re-armed

----

The posted notification includes the trap name and a number representing the battery level (from Vcc, requires calibration)
