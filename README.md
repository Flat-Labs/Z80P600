Z80P600
=======

A board and its firmware to replace (and enhance) the Z80 CPU of a SCI Prophet600.

Devised as a tribute to the amazing work of Gligli (http://gligli.github.io/p600fw/) and Imogen (https://github.com/image-et-son/p600fw),
it offers an alternative to Teensy board and its clones. 

Presets, settings, sequences and arpeggios are now stored on a separate F-RAM chip. Thus, all of these won't be lost during upgrades

The firmware functionalities are (almost) absolutely identical to the last Imogen's 2022 version, it only has been adapted to the new board. 
Please refer to the two above addresses for user manuals and informations.
I just added (Beta version) a misc command (From Tape + SEQ1) to wipe the settings out and set them to default . As tuning values are part of the settings, this obviously retunes the synth. 



Compiling and flashing the Firmware
===================================

Requirements:
- AVR-GCC toolchain
- avrdude uploader

For both these pieces of software on Windows, please refer to https://tinusaur.com/guides/avr-gcc-toolchain/.
A very clear and efficient guide to set up the toolchain. I used it with complete success on my windows box.

- python3 (https://www.python.org/downloads/)

- an USBASP programmer with six-pin header, such as this one (https://www.ebay.com/itm/116556456138) (Europe)
  or this one (https://www.ebay.com/itm/382191022734) (US)
  
Compiling:
----------
	> cd firmware
	> make

This will produce p600firmware.hex, which you can flash onto the board.

Flashing:
---------
1) For the first time.
	- Connect the Z80P600 board to the USBASP programmer (respect the 6-pins header pinout...)
	- Plug the USBASP into the PC
	- First, flash the fuses of the ATMEGA1281 (this has to be done only once, the first time):
	
		> avrdude -c usbasp -p m1281 -U lfuse:w:0x4f:m -U hfuse:w:0xdf:m -U efuse:w:0xf5:m -U lock:w:0xff:m
			
	- and flash the firmware:
	
		> avrdude -c usbasp -p m1281 -U flash:w:"p600firmware.hex":a
	
	To reflash a new version of firmware, only use the above avrdude command, as fuses have already been flashed.

	- Unplug the USBASP programmer of PC, disconnect the board, and plug it in the Z80 socket of P600
	- You're done ;)

2) Sysex upgrades 
	Sysex upgrades work (From Tape + To Tape while powering on) and can be used to succesively upgrade the firmware through MIDI, provided that an older firmware has already been flashed once.

IMPORTANT NOTE : As the P600 presets, settings, sequences and arpeggios are now stored on a separate F-RAM, uploading (USBASP or sysex) a new firmware won't erase them.
Hence the option to reset the settings, just in case...



Board and PCB
=============

Schematics and PCB have been drawn using Kikad (V9 or above) : https://www.kicad.org/
The prototypes have been made (PCB + assembly) by JLC PCB, and the BOM file includes JLCPCB references for the parts.
The only part that is not included is the 40-pin DIL socket (Mouser ref. https://www.mouser.fr/ProductDetail/437-1501064000106161), and it has to be hand-soldered.

Everything is working great, but the board LED seems a bit too bright for my taste...
So the R2 resitor value (for the LED) may be increased to something between 470 and 1000 ohms.



License
=======

Everything is under GPL v3 license, except files that have their own license in the header.
