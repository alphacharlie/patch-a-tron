# Patch-a-Tron

## Patch-a-Tron-O-Matic

## An Arduino based 48x8 analog patch bay 

  This sketch is uses an Arduino to control 3 CD22M3494 16x8 analog crosspoint arrays ICs. User Interface is provided via LCD/touchscreen and RA8875 controller 
  (available from Adafruit). This was primarily designed as a patch bay for synth/circuit bending/electronic music. But it could be used for any circuit that needs to switch or route analog signals...
	 
  I'm using it for the 'Mother-of-all-Circuit-Bends'... (My custom Roland TR-626 Rythm Composter)
	 
  Video of it in action connected to Roland TR drum synths from the 80's is here - https://www.youtube.com/watch?v=JZRpz0zVbfM and here - https://www.youtube.com/watch?v=f6S4W0K_GYc
	 

## Current Status - Stable

  Software is currently stable. I have yet to draw up a detailed schematic or user guide, But it's pretty easy to see how it is wired from the source code. 
  The 3 ICs are tied together on the x8 side of the array to provide one 48x8 analog patchbay. CD22M3494 switch control is bit-banged via parallel 
  addressing and CS pins. Different 'memories' are stored in EEPROM so that patterns can be saved and recalled. Bends can also be triggered via MIDI.
		
  The project contains two sketches - PatchaTron and PatchaTronX. They differ in how the memory is stored. (PatchaTronX stores each bend, 
  any 8 of which can be active at once.) 


## Contributing

  Please report any bugs to info@boffinry.org

## Acknowledgements 

  Uses the RA8875 library (and hardware) from Adafruit Industries.
