# promacro
Sending key presses using a gamepad isn't that hard.

I use my gamepad to play games on an emulator, with the following macros:

1. Left trigger maps to `space` (fast-forward in the emulator)
2. When the right stick forward/back axis is not in the neutral position, mash
   (rapidly toggle) the `x` key.

After using various unstable programs to do this (which randomly crash while interacting with the GUI, or wipe my settings), I realised that this is actually very simple to implement myself (in Linux, at least).

## Usage

First, note this is Linux-only.

If you want precisely the same macros that I use, and you own a Logitech F310,
then you're good to go. There is no config file, because otherwise 90% of the
code would be for handling the config file, and it would require some kind of
domain-specific language that would not be able to handle every use case.

Instead, modify the code in promacro.c to do what you want! Here are some hints:

### Interpreting joystick signals
`/dev/input/js0` is the file handle for your joystick. To figure out what the
input looks like, use `xxd`. In the case of the Logitech F310, each input
signal is 8 bytes, so watching the output of `xxd -c8 /dev/input/js0` as I
pressed buttons allowed me to figure out what the input format is.

In my case, it was:

* 32 bits (little endian) of timestamp
* 16 bits (little endian two's complement) of button value
* 16 bits of button identifier

The identifier is button the event is for. For binary buttons, the value is 1
on press, 0 on release. For analogue buttons, the value ranges between -32768
and 32767, with 0 being neutral.

### Sending key events

The keycodes you'll want to send are listed in `<linux/input-event-codes.h>`
(i.e. located in `/usr/include/linux/input-event-codes.h`).
