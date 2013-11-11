USBXLATER - Frank Zhao

Nov 11 2013:
	USB virtual COM port functionality is working, if a PS3 is not detected, then the VCP mode will be entered, and you should be able to use it to analyze some USB stuff

	passthrough mode can start but is not sending to the FS interface yet, I need to figure out why it's not working

	major improvements to debug, and HC allocation
	fixed many hardfaults
	added COM port commands

Nov 8 2013:
	please see
	http://eleccelerator.com/usbxlater-preview/
	right now this is just a rough firmware enough to play BF4

	there is some work-in-progress code that extends the functionality to include
	VCP and traffic analysis capabilities, these are untested