= Closet Master

This is a random collection of bits that are needed in my technical closet:

== Power distribution

The closet has a number of different units that need 12V DC.

Rather than have 17 wallwarts the output of a single large PSU is distributed.

Because I'm a curious type I wnat to keep an eye on the current being pulled from the PSU, so I use an RC shunt along with an lcd-backpack from my Luggage project: https://github.com/dren-dk/Luggage



== Meter reading

I have a number of meters that I want to read to push the data to emoncms.

The electrical and remote heating meters are read using an IR/serial interface like these:
http://www.ebay.com/itm/Optical-Probe-IEC1107-IEC61107-62056-21-with-USB-cable-Optical-Probe-for-Windows-/201860023477?hash=item2effcb84b5

Alternatively this interface can be used:
https://wiki.hal9k.dk/projects/kamstrup

The water meter is much harder to read as it doesn't have an accessible digital interface, so in stead I rely on OCR'ing an image from a web cam using ssocr:
https://www.unix-ag.uni-kl.de/~auerswal/ssocr/

The web cam needs even lighting to get a picture, so the lighting needs to be
switched on for taking a picture.


== Doorbell controller

The doorbells are controlled by


== Door lock monitoring

The front and back doors of the house have a switch for reading the state of the door lock.

The door lock switches are also used as an input to switch the garden door locks between home-lock and out-lock mode.


= Hardware

The hardware consists of:

* RPI: A Raspberry Pi for high level control and OCR'ing water meter via web cam.
* GPIO: A Leonardo pro micro hooked to the RPI for GPIO
* UI: ATmega324 for UI (LEDs, buttons and HD44100), hooked to the pro micro via serial.


== GPIO

The GPIO controller ATmega32U4 has the following connections:

| PIN           | Direction     | Description  |
| ------------- | ------------- | ------------ |
| PF7 | out+ | Outdoor beeper |
| PF4 | in-  | Back door lock |
| PF5 | in-  | Front door lock |
| PF6 | in-  | Front door bell |
| PD4 | out+ | Remote Out-lock |
| PC6 | out+ | Remote Home-lock |
| PD7 | out+ | Remote Unlock all |
| PE6 | in+  | Remote Green LED |
| PB4 | in+  | Remote Red LED |
| PD3 | tx   | UI serial tx |
| PD3 | rx   | UI serial rx |
| PB0 | out+ | Internal rx LED |
| PD5 | out+ | Internal tx LED |


== UI

The UI controller ATmega328 has the following connections:


| PIN           | Direction     | Description  | Extra |
| ------------- | ------------- | ------------ | ------|
| PC0 | adc  | Current sense |
| PC1 | adc  | Voltage sense |
| PD1 | tx   | Serial tx |
| PD2 | rx   | Serial rx |
| PB6 | out  | HD44100 D4 |
| PB7 | out  | HD44100 D5 |
| PD5 | out  | HD44100 D6 |
| PD7 | out  | HD44100 D7 |
| PD2 | out- | HD44100 RS |
| PD3 | out  | HD44100 Read/~~Write |
| PD4 | out  | HD44100 Enable |
| PB1 | pwm  | HD44100 contrast |
| PB5 | out+ | Internal LED |
| PC2 | out- | 1 LED RD    | 1 |
| PC3 | out- | 2 LED TD    | 2 |
| PC4 | out- | 3 LED REM   | 3 |
| PC5 | out- | 4 LED LOC   | 4 |
| PB0 | in+  | 5 Cursor    | 5 |
| A7  | in+  | 6 Scroll    | 6 |
| PB2 | in+  | 7 Enter     | 7 |
| PD7 | out- | 8 LED Test  | 8 |
|     |      | 9 +5v       | 9 |
|     |      | 10 gnd      | 10 |

=== UI - GPIO protocol

The UI controller is connected to the GPIO controller via UART at 115200 baud 8n1, each chunk of information is
delimited by newlines (\n)

The commands that the GPIO controller sends are:

| Command     | Meaning |
| ----------- | ------- |
| !d*32 char* | Sets the display content to the 32 chars |
| !l*int*     | Sets the LEDs to bits in the int, bit order: LOC=0, REM=1, TD=2, RD=3, TEST=4 |
| !b*int*     | Sets backlight level |
| !c*int*     | Sets contrast |

The data returned from the UI controller is a telegram which is transmitted every second or whenever a button changes state:

sprints("%d,%d,%d\n", buttons, mV, mA);

