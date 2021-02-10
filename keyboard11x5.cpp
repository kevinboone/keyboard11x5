/**

keyboard11x5.cpp

This is the Pro Micro software for the 11x5 keyboard whose 
circuit diagram is shown in keyboard11x5_circuit.png.
Because 55 keys doesn't provide complete coverage even of commonly-used
keys, we need a two-layer design. So one key in the keyboard acts as a
kind of extra meta key (along with ctrl, shift, and alt) to change
the layer.

This implementation contains simple, timeout-based contact debouncing.
The debouncing is implemented as a state machine, with
one state for each key, so that the debounce does not prevent
one key locking out another. After all, it's not a bounce if two
keys are pressed in quick succession.

Note that the hardware design assumes active-low scanning. That is,
each column is set low, and then the row values are read. The
Pro Micro does not have built-in pull-down resistors, but it does have
built-in pull-up resistors. So using active-low scanning means we
don't need to include any external resistors. It does confuse the
logic a little, though, because when a zero is read from an input,
that indicates that a key in that particular row is down, not
up; but we store the keystates with 1 = pressed and 0 = released.

This is the state diagram for the debouce logic. All keys start
in the wait_press state. If a key is pressed, the key moves
to the wait_lockout state, where it waits -- ignoring all further
key events -- for the timeout period. The same process is repeated
in the wait_release state. The key press is emited to the host
when moving from the wait_press to press_lockout states, and the
release when moving from the wait_release to release_lockout
states.

                       + -- press -+
                       |   release |
		       |           V
  wait_press --press-> press_lockout --timeout-> wait_release --+
      ^                                                         |
      |                                                         |
      +---timeout--------- release_lockout <-------release------+
			   ^           |
                           + -- press -+
                               release 

Copyright (c)Kevin Boone, February 2021. Distributed according to the
terms of the GNU Public Licence, v3.0
*/

// If USE_SERIAL_MONITOR is defined, we output keystroke data 
//   using the Serial object, not the Keyboard object. This way we
//   should be able to see what is going on, without generating
//   keystrokes. We can simply cat the serial device in /dev to
//   collect the output
//#define USE_SERIAL_MONITOR

#include <stdint.h>
#include <Arduino.h>
#include <HardwareSerial.h>
#include <stdlib.h>
#include "Keyboard.h"

// Number of complete scans for which a key transition is ignored 
//    after the first, to reduce contact bounce. There's no obvious
//    way to calculate this value, and it will vary from one switch
//    type to another. If the value is too long, we risk locking
//    out genuine multiple key presses as well as bounces.
#define LOCKOUT_SCANS 200

typedef uint8_t BOOL;

// Define the actions a key can perform -- essentially press and release.
typedef enum
  {
  ACTION_RELEASE = 0,
  ACTION_PRESS
  } KeyAction;

// Number of columns in the key matrix. In this design, columns are
//   outputs, and the column pins will be set low in sequence to
//   do the scan
#define NUM_COLUMNS 11 

// Number of rows in the key matrix. In this design, rows are inputs,
//   and are read in sequence whilst each column pin is set low.
//   This means that a '0' indicates that a key is pressed, and a 
//   '1' indicates that the key is up
#define NUM_ROWS 5

// Define the Arduino pins that will be used as outputs to scan
//   the matrix columns. The first pin in this list is column zero.
uint8_t columns[NUM_COLUMNS] = 
  {2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 14};

// Define the Arduino pins that will be used as inputs, attached to 
//   the matrix rows. The first pin in this list is row zero. 
uint8_t rows[NUM_ROWS] = 
  {21, 20, 19, 18, 15};

// Temporary char array for printing number values in serial monitor
//   mode.
char s_v [10];

// The keysyms that will be emitted when the key at a specific
//   row and column is pressed. This matrix should look rather
//   like the keyboad itself. 

// Layer 0

static const uint8_t keysyms_l0[NUM_ROWS][NUM_COLUMNS] = 
  {
  { KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0' }, 
  {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', KEY_BACKSPACE},
  {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '/', KEY_RETURN},
  {KEY_LEFT_CTRL, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '?' }, 
  { KEY_LEFT_SHIFT, '\\', '\'', ':', KEY_TAB, ' ', KEY_LEFT_ARROW, \
      KEY_UP_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW, KEY_LEFT_ALT }  
  };

// Layer 1

static const uint8_t keysyms_l1[NUM_ROWS][NUM_COLUMNS] = 
  {
  { KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0' }, 
  {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', KEY_DELETE},
  {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '/', KEY_RETURN},
  {KEY_LEFT_CTRL, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '?' }, 
  { KEY_LEFT_SHIFT, '+', '#', '[', ']', '?', KEY_HOME, KEY_PAGE_UP, KEY_PAGE_DOWN, KEY_END, KEY_LEFT_GUI} 
  };

// Define the row and column that contain the layer-change key

#define LAYER_ROW 3
#define LAYER_COL 10

uint8_t layer = 0;

// A number representing the state of the individual
//   key in the finite state machine. 
typedef uint8_t KeyState;

// These are the states the key can be in (see state diagram above).
// All keys start in the wait_press state.
#define KEYSTATE_WAIT_PRESS 0
#define KEYSTATE_PRESS_LOCKOUT 1
#define KEYSTATE_WAIT_RELEASE 2
#define KEYSTATE_RELEASE_LOCKOUT 3

// The keystates matrix indicates the current state of each
//   key in the matrix.
static KeyState keystates[NUM_ROWS][NUM_COLUMNS];

// Lockout is the number of scan cycles that a key must wait, to
//   move out of the press_lockout or release_lockout states. 
// An entry for a specific key is set just after emitting a keypress
//   or key release event to the host. The values are all decremented
//   on each scan cycle, unless they are zero. The state transition
//   occurs when the lockout value goes from 1 to 0
static uint16_t lockout[NUM_ROWS][NUM_COLUMNS];

/**
 * setup()
 * Initialize the USB port or serial monitor, set the pin modes,
 * clear the key states, etc.
 */
void setup()
  {
#ifdef USE_SERIAL_MONITOR
  Serial.begin(9600); 
  Serial.println("Keyboard starting");
#else
  Keyboard.begin();
#endif
  for (uint8_t i = 0; i < NUM_COLUMNS; i++)
    {
    pinMode (columns[i], OUTPUT);
    digitalWrite (columns[i], 1);
    }
  for (uint8_t i = 0; i < NUM_ROWS; i++)
    {
    // Use INPUT_PULLUP so we don't need external resistors
    pinMode (rows[i], INPUT_PULLUP);
    }
  // Clear the key states
  memset (keystates, KEYSTATE_WAIT_PRESS, sizeof (keystates));

  memset (lockout, 0, sizeof (lockout));

    pinMode (1, OUTPUT); // FRIG
  }

/**
 * keyboard_emit()
 * Emit the specified keysym. Indicate a keypress if pressed
 * is 1, and a key release if it is 0.
 */
static void keyboard_emit (uint8_t keysym, KeyAction action)
  {
  /*
  if (pressed) 
    digitalWrite (1, 1); // FRIG
  else 
    digitalWrite (1,0);
  */
#ifdef USE_SERIAL_MONITOR
  itoa (keysym, s_v, 10);
  Serial.print (s_v);
  if (action == ACTION_PRESS)
    Serial.println (" down");
  else
    Serial.println (" up");
#else
  if (action == ACTION_PRESS)
    Keyboard.press (keysym);  
  else
    Keyboard.release (keysym);  
#endif
  }

/**
 * keyboard_key_action
 */
static void keyboard_key_action (uint8_t row, uint8_t col, KeyAction action)
  {
  if (row == LAYER_ROW && col == LAYER_COL)
    {
    if (action == ACTION_PRESS)
      {
      // TODO: perhaps light an LED when changing layer?
      layer = 1;
      }
    else
      {
      layer = 0;
      }
    }
  else
    {
    if (layer == 0)
      keyboard_emit (keysyms_l0[row][col], action);
    if (layer == 1)
      keyboard_emit (keysyms_l1[row][col], action);
    }
  }

/*
 * keyboard_do_row_col()
 * On each scan, handle a specific row,column combination. On entry
 * pressed==1 indicates that the key is down, pressed==0 
 * indicates that it is up.
 */
static void keyboard_do_row_col (uint8_t row, uint8_t col, BOOL pressed,
         BOOL timeout)
  {
  uint8_t oldstate = keystates[row][col], newstate = oldstate;
  switch (oldstate)
    {
    case KEYSTATE_WAIT_PRESS:
      if (pressed)
        {
        lockout[row][col] = LOCKOUT_SCANS; 
        keyboard_key_action (row, col, ACTION_PRESS);
        newstate = KEYSTATE_PRESS_LOCKOUT;
	}
      break;

    case KEYSTATE_PRESS_LOCKOUT:
      if (timeout)
        newstate = KEYSTATE_WAIT_RELEASE;
      break;

    case KEYSTATE_WAIT_RELEASE:
      if (!pressed)
        {
        lockout[row][col] = LOCKOUT_SCANS; 
        keyboard_key_action (row, col, ACTION_RELEASE);
        newstate = KEYSTATE_RELEASE_LOCKOUT;
        break;
	}

    case KEYSTATE_RELEASE_LOCKOUT:
      if (timeout)
        newstate = KEYSTATE_WAIT_PRESS;
      break;
    }

  keystates[row][col] = newstate;
  }

/*
 * loop()
 * Scan all columns and rows once. Call keyboard_do_row_col() for
 * each row/column pair.
 */
void loop()
  {
  // For each column...
  for (uint8_t col = 0; col < NUM_COLUMNS; col++)
    {
    // ...set the column output low...
    digitalWrite (columns[col], 0);
    // ... and then read the state of each row.
    for (uint8_t row = 0; row < NUM_ROWS; row++)
      {
      uint16_t l = lockout[row][col];
      uint8_t v = digitalRead (rows[row]);
      keyboard_do_row_col (row, col, !v, l == 1);
      if (l > 0) 
        {
	l--;
	lockout [row][col] = l;
	}
      }
    //delay(100); // We might need a delay when debugging
    digitalWrite (columns[col], 1);
    }
  }

