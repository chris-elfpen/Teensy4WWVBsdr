# Teensy4WWVBsdr

I took code from https://github.com/DD4WH/Teensy-DCF77 and reworked it in order to listen to WWVB and to use the new Teensy 4.0 and the audio board specific for Teensy 4.

Hardware:

teensy 4.0  https://www.pjrc.com/store/teensy40.html

Audio board (rev D)  https://www.pjrc.com/store/teensy3_audio.html

Color touchscreen https://www.pjrc.com/store/display_ili9341_touch.html

Interconnections:


Teensy 4.0 pin functions				
				
|GND|gnd|Vin|Vin(screen)|
||0|gnd|GND (screen)|
||1|3.3V|RESET (screen)|


		        1	                 3.3V	RESET (screen)
		        2	                 23	MCLK (audio)
		        3	                 22	LED ( screen via a series 100 ohm resistor)
		        4	                 21	BCLK (audio)
D/C (screen)	5	            20	LRCLK (audio)
	MEMCS (audio)	6	            19	SCL (audio)
	DIN (audio)	7               18	SDA (audio)
	DOUT (audio)8	              17	
	T_CS (screen)	9	            16	T_IRQ (screen)
	SDCS (audio)10	            15	VOL (audio)
	    MOSI	11	              14	  CS (screen)
	    MISO	12	              13	  SCK
				
ILI9341 Pin	W/Teensy 4.0 Audio Board			
VCC				
GND				
CS	14			
RESET	24			
D/C	5			
SDI (MOSI)	11			
SCK	13			
LED	22			
SDO (MISO)	12			
T_CLK	13			
T_CS	9			
T_DIN	11			
T_DO	12			
T_IRQ	16			
