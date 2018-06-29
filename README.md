
## Introduction 

XRoar is a Tandy Color Computer('CoCo') and Dragon32/64 emulator. CoCo model 3 is not supported.

## LOADING GAMES 

The firmware ROM files in /apps/xroar are essential. If the emulator starts with a screen full of @, it means that it failed to find the files.
Refer to the manual(xroar.pdf), chapter 6 for more information on these files.

To load a program, open the main menu, chose 'Load File' and select the file. It will try to start the file automatically if 'autorun' is enabled(by default).
If it doesn't start, here are the general commands :

- Disks(.vdk, .dsk) : Type[Enter] to list the files and see the executable. For Dragon, type RUN"FILE.BAS" or RUN"FILE.BIN".
  For Coco, type RUN"FILE.BAS" or LOADM"FILE.BIN":EXEC

- Tapes(.cas) : Type CLOADM:EXEC (machine code) or CLOAD:RUN (Basic). Dos emulation must be disabled.

- Cartridges(.rom, .bin, .ccc) : they should all autorun, if it doesn't run, the file isn't supported.

Before loading another game, you may need ro reset the machine and/or toggle DOS emulation.
Sometimes, a software needs specific commands to start. Consult the game manual.

## JOYSTICKS

There are 3 modes : Right joystick(default), Left Joystick, keyboard arrows. Cycle through these modes with Wiimote/Classic + button or GameCube Z.

CoCo and Dragon have two joysticks ports(left and right) and most games use them. By default, the first Wiimote/GameCube controller is the right joystick.
Depending on the software, the first player may be the right or left joystick. In joystick mode, A/2 button is Coco/Dragon 'fire button'.
If a game doesn't support joystick, you may switch to 'keyboard' mode. In this mode the dpad/stick emulates the Coco arrow keys, and A button is key Space.
Other keys are mapped to Wii buttons (can be reconfigured in the virtual keyboard) :

 * [1] = Wiimote button 1 / GameCube button X
 * [Y] = Wiimote/Classic/GC Y
 * [Enter] = Wiimote/Classic/GC B
 * [Space] = Wiimote/Classic/GC A (Joystick mode)
 * Fire Button = Wiimote/Classic/GC button A (Keyboard mode)

Note that the Wiimote pointer emulates the CoCo/Dragon mouse.(rarely useful).
The buttons used for opening the menu/virtual keyboard, or switching modes cannot be reconfigued.


In the settings menu, you will find two options to adjust the sticks and dpad.

- Joy sensitivity : if the stick isn't responding well, try to toggle this option.

- Dpad delay : It's 'off' by default, and should stay like this in most cases. When set to 'on', it tries to simulate a stick, it sends a
  continuous press by small range. It can be useful for mouse based software, or games like Arkanoid.

## CONTROLS

Here are the menu controls and special buttons used in-game.

### Wiimote:
<pre>
 A/2     : Confirm  
 B/1     : Back  
 \-      : Open virtual keyboard  
 \+      : Select joystick mode/toggle option  
 Home    : Open main menu/help  
</pre>

### Classic controller:
<pre>
 A       : Confirm
 B       : Back
 \-      : Open virtual keyboard
 \+      : Select joystick mode/toggle option
 Home    : Open main menu/help
</pre>
### GameCube controller:
<pre>
 A      : Confirm
 B      : Back
 L      : Toggle option
 Z      : Select joystick mode
 Start  : Open main menu/help
</pre>
## VIRTUAL KEYBOARD

Open it with Wiimote/Classic - or GC pad L button. Select the key(s) with the Wiimote pointer, or dpad/stick and push 'Done'.
Sometimes, the key isn't sent correctly, it may be better to bind it in this case.
You can bind a key to most buttons. Select the key with the dpad and press + to open the mapper. From there, press the desired button.

## ARTIFACT COLOURS

The artifact colours can be redefined with the -rgb option written in xroar.conf or sent as a plugin argument.
To activate the palette, go to 'Settings', 'Colour mode', and select 'User defined'.
You need to specify 4 colours in hexadecimal seperated by ':', the two main colours and their darker tone. 

Here's an example to reproduce the (incorrect) Green/Blue Pal-m, write this line in xroar.conf file :

-rgb 4aa208:008c64:64f0ff:0080ff

The colors are :
4aa208 is Green
008c64 is Dark Green
64f0ff is Blue
0080ff is Dark Blue

To find the hexadecimal value of the color you want, use your favorite Paint program, or go there for example : http://rapidtables.com/web/color/RGB_Color.htm

If you want to send it as a plugin argument, modify coco.ini or dragon.ini :

arguments=-default-machine|cocous|-rgb|4aa208:008c64:64f0ff:0080ff|-run|{device}:/{path}/{name}

## ARGUMENTS SUMMARY

Here are the Wii specific options that can be used in xroar.conf, or sent by Wiiflow(and others plugin compatible loaders) :

-240p : Enable 240p video mode.
-tvfilter 0 : Scanlines filter. Change 0 to 1,2 or 3 to activate it.
-bilinear : Enable bilinear filter. It's disabled by default.
-rgb 4aa208:008c64:64f0ff:0080ff : Change the values to use custom colors for artifact colors.
-wide : Enable fit to screen when in Wii 16:9 video mode.

When used as a plugin, the space must be seperated by the character '|' like -tvfilter|1

## NOTES

The option -joy-left from XRoar Wii v0.1 when used as a plugin argument has no effect now. The joysticks are handled differently, the first found Wii controller will always be
the right Coco/Dragon joystick at startup.

## CHANGES

0.4 :

* Long filenames truncated in the file browser.
* Moved options in the menus to compensate crt overscan.
* Recompiled with DevkitPPC 31.
* Some cleanups.


0.3 :

* Added the ability to define the artifact colours with -rgb option.
* Toggle bilinear filter. Use -bilinear argument.
* Scanlines filter with 4 settings : off (0), light, medium, dark. Use -tvfilter 0 (or 1,2,3).
* 240p option. Use -240p argument.
* Wide screen option. Use -wide argument.
* Removed -joy0 argument in coco.ini and dragon.ini(not used anymore). 
* Added -tvfilter 0 argument in coco.ini and dragon.ini. Change 0 value to activate the scanlines filter.
* Added xroar.conf in /apps/xroar. I forgot it in the previous releases!


0.2.5 :

* Added Blue-Green Pal-m palettes used by brazilian CoCo. It's just a guess on the colors...


0.2 :

* Added a Gui with almost all XRoar options.
* Virtual keyboards.
* A mapper for Dragon/CoCo keys.
* Tapes support.

## KNOWN ISSUES

- When a rom file (.ccc, .rom) is launched, it's replacing the DOS cartridge. You can re-enable DOS in the settings menu. However, most of the time, even after
  a reset, the emulator is stuck on a green screen. You'll have to load a DOS rom manually or just quit...

- Saving a snapshot on a USB device takes a long time.

- The key pressed on the virtual keyboard isn't always sent correctly.

- 240p can result in wrong colors.

- For text based software, use an usb keyboard.

## LINKS

http://archive.worldofdragon.org/
http://www.lcurtisboyle.com/nitros9/coco_game_list.html

## CREDITS

XRoar emulator : Ciaran Anscomb
Wii port : Wiimpathy (2017)
You! You read the README, you're One in a billion! 



