# Teensy4WWVBsdr

I took code from https://github.com/DD4WH/Teensy-DCF77 and reworked it to monitor WWVB and to
use the new Teensy 4.0 and the audio board specific for Teensy 4.


## Hardware:

Teensy 4.0  https://www.pjrc.com/store/teensy40.html

Audio board (rev D)  https://www.pjrc.com/store/teensy3_audio.html

Color touchscreen https://www.pjrc.com/store/display_ili9341_touch.html



## Interconnections:


### Teensy 4.0 pins


   |  comment | pin | pin | comment |
   |-----|-----|-----|-----|				
   | GND | gnd | Vin | common with Vin(screen) |
   | | 0 | gnd |  |
   | | 1 | 3.3V | RESET (screen) |   
   | | 2 | 23 | MCLK (audio) |
   | | 3 | 22 | LED  (to screen via a series 100 ohm resistor)|
   | | 4 | 21 | BCLK (audio) |
   | D/C (screen)	| 5	| 20 | LRCLK (audio)|
|MEMCS (audio)|	6|	19|	SCL (audio)|
|DIN (audio) |7|18|SDA (audio)|
|DOUT (audio)|8|17||
|T_CS (screen)|9|  16|T_IRQ (screen) Not connected|
|SDCS (audio)|10|15|VOL (audio)|
|MOSI|11|14|CS (screen)|
|  MISO|12|  13	| SCK|
				


### ILI9341 pin assignment when using Teensy 4.0 Audio Board

|display pin |name| teensy pin|
|----|----|---|
|1 |VCC| common with Vin |				
 |2|GND| common with GND |	
 |3|CS|14|
 |4|RESET	|24 (3.3v)|			
 |5|D/C|	5|			
 |6|SDI (MOSI)|	11|			
 |7|SCK|13|
 |8|LED	|22 via 100 ohm resistor|			
 |9|SDO (MISO)|	12|			
|10|T_CLK	|13	Not Connected |		
|11|T_CS	|9	Not Connected|	
|12|T_DIN	|11	Not Connected|		
|13|T_DO	|12	Not Connected|		
|14|T_IRQ|	16	Not Connected|		

### pictures
I've added some pictures of my implementation.

For antenna I have the top section of a standard 5 gal bucket which I have sawn off.
I am using some salvaged enameled wire, about 30 turns (?).
Then inside the enclosure the connections from the antenna go to a circuit board
which has some capacitors soldered to it so as to make a resonant antenna.
From there the signal goes into the sound board.
The sound board and Teensy 4.0 are stacked.
From the stack I have connections to the LCD.

I've lined my plastic enclosure with aluminum foil.

The stack of audio/teensy is mounted to the enclosure.
The LCD, capacitor board and foil are just floating around in there...
I still have work to do on my enclosure.
