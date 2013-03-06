Temperature Logger

This code is based on the datalogger project hosted
[here](https://github.com/cnvogelg/ardu).

Hardware
========

Components
----------

  * Arduino Uno
  * [Adafruit Data Logger Shield][1]
  * 1 Push Button
  * 1 DS 18B20 Temperature Sensor
  
Wiring
------

  * DIGITAL 2 Input Push Button (toggle log recording on SD card)
  * DIGITAL 3 green LED (on => logger is ok)
  * DIGITAL 4 red LED   (on => logger is writing to SD card)
  * DIGITAL 7 data pin DS 18S20

See the arduino button [tutorial][2] on how to connect the button.
The two LEDs are already available on the shield you only have to
connect them to the DIGITAL ports of the Arduino. The temperature
sensor needs +5V and GND addtionally. Take these lines directly from
the shield. See the [datasheet][3] for the pin out of the sensor.

  [1]: http://www.ladyada.net/make/logshield/
  [2]: http://www.arduino.cc/en/Tutorial/button
  [3]: http://datasheets.maximintegrated.com/en/ds/DS18S20.pdf

Additional Libraries
====================

  * [RTClib](http://www.ladyada.net/make/logshield/rtc.html)
  * [OneWire](http://www.arduino.cc/playground/Learning/OneWire)

Operation
=========
  
Startup
-------

  * Insert an SD Card and make sure the clock battery is inserted.
  * Reset or power on Arduino.
  * With the unit connected to a usb port open the serial console in
    the Arduino IDE to see the output of the logger.
  
Reading the LED's
-----------------

  * When the unit is operating normally the green led should be on
    (almost) constantly. It only blinks off for a moment when the
    program takes and records an actual temperature reading.
  * The red led is on solidly---not blinking---when the software is
    logging readings to a file on the SD card.
  * If the red led blinks then something went wrong.
    * Check the SD Card.
    * Check the temperature sensor.
    * Check the real time clock.
    * Reset and try again.
    * See the serial console for a possible error message.

Pushing the Button
------------------

  * Pressing the button will instruct the software to log readings to
    a file on the SD card. The red led should light up.
  * Pressing the button a second time will disable logging and close
    the file. The red led will go out.
  * You can repeat the above sequence as many times as you want to
    resume logging to new files.

If the arduino is connected to your host computer you can communicate
with it in a slightly more sophisticated way.

Reading from the Serial Connection
----------------------------------

  * On startup the program prints out the temperature sensor type and
    code.
  * At regular intervals it prints out a timestamp and the
    temperature.
  * It also prints out messages describing any errors it encounters.

Writing to the Serial Connection
--------------------------------

You can type in a line of text and hit RETURN to enter commands.

  * Set the date by entering a line with the format yyyy-mm-dd for
    year month day of month. For example:

    2013-03-10

    for March 10, 2013.

  * Set the time by entering a line with the format hh:mm:ss for hours
    minutes seconds. (You could just enter hh:mm to let the seconds
    default to 0.) For example:
	
	09:30:25

  * By default the software will take a temperature reading every 10
    seconds. You can change this by entering a "d" followed by a
    number. This will make the time interval between temperature
    samples that number of seconds. For example to sample every two
    minutes enter:
	
	d 120

  * Enter "o" to open a log file on the SD card, and enter "c" to
    close it again.
	
  * Enter "r" to read the date and time of the clock.

Reading the Log Files
---------------------

The program automatically generates names for the log files of the
form yymmdd_a.log where yy is the last two digits of the year, mm and
dd are the month and day of month numbers and a is "a" for the first
file created that day, and "b" for the second and so on. Each entry in
the log file has 5 components:

  * the date as yyyy-mm-dd,
  * the time as hh:mm:ss,
  * the time as number of seconds since the epoch (1970-1-1),
  * the temperature in fahrenheit, and
  * the raw temperature reading as a hexadecimal number. (For the
    DS18B20 this is the temperature in celsius times 16.)

For example, a line will look something like this:

  2013-03-05 11:15:44 1362482144  72.50 168
