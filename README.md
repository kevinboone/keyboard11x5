# Keyboard11x5 -- Circuit and software design for a custom 55-key mechanical keyboard

This is the circuit and microcontroller software design for a custom
55-key mechanical keyboard, arranged in five rows of eleven keys.

Why a 55-key keyboard? It turns out that the 11x5 layout, with
conventional keycaps and spacing, is approximately the same size as a 
7" LCD screen. So equipment that uses such a screen, like 
[this](http://kevinboone.me/pi_wooden_case.html)
or [this](http://kevinboone.me/pi_peli.html) is well-suited to
a keyboard of this size.

There are published designs for small keyboards -- even very small
keyboards with 30 keys or fewer. Personally, I don't find such
keyboards very usable. There are no commercially available 11x5
keyboards (so far as I know), nor any published designs or 
software. This seems odd to me, because 55 keys allows for 
most of the keys that
most people use all the time. Of course, like most small keyboards, 
my design
allows for layer switching to get to less commonly-used keys.

The circuit diagram is very simple -- it's just a conventional
key matrix with five rows of eleven keys, connected via diodes
to a SparkFun Pro Micro microcontroller. The diodes are needed to
prevent multiple key-presses causing current to flow from
one microcontroller output to another -- an unhelpful and potentially
damaging state of affairs. More sophisticated designs call for
diodes in series with each switch. This makes it possible to detect
three or more simultaneous key-presses in the same row of column.
My software does not handle such a situation -- it's limited to
two simultaneous key-presses, and a full-size diode array would give
no additional benefit.

I'm not going to go into detail about the physical construction of
the keyboard. I built my keyswitch PCB using 55 
[SparkFun Cherry MX breakout boards](https://learn.sparkfun.com/tutorials/cherry-mx-switch-breakout-hookup-guide), glued together in a matrix.
Each individual board has links hand-soldered one to the next,
to create the rows and columns.
Each individual keyswitch has to be hand-soldered to the PCB.
The microcontroller board is attached to the bottom of the keyswitch PCB,
and connected to it using simple lengths of hookup wire. Assembly
is a long, tedious, and error-prone operation, compared to using a
ready-made, proprietary keyboard PCB and mounting plate.

I still haven't settled on the best keyboard layout -- of course, this
is endlessly adjustable in software. 

Please note that some of the files in this repository are Arduino library
sources, and have their own licence terms which can be seen in the individual
files. 

