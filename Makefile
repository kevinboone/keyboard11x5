NAME=keyboard11x5

# Location of the main arduino installation, if you installed a package
# from a standard repository.
ARDUINO_DIR=/usr/share/arduino

# Directory that contains the Arduino library files. These are supplied
# in the form of source code, not complied code. This is the directory
# that contains main.cpp, USBAPI.h, etc. If you didn't get the Arduino
# library as part of a package, you'll have to specify this manually.
# Otherwise, specify the location relative to ARDUINO_DIR
LIBRARY_DIR=$(ARDUINO_DIR)/hardware/arduino/avr/cores/arduino

# Specify the location of the header files for the Pro Micro variant
# of Arduino. A directory under .arduino15 is what you get if you 
# use the Arduino IDE to install this support
VARIANT_INCLUDE=/home/kevin/.arduino15/packages/SparkFun/hardware/avr/1.1.13/variants/promicro

# Specify the commands to run for CC, CPP, etc. These may have been 
# installed as part of an Arduino software bundle, or separately
# Note the avrdude is only used (in this example) to upload to the board
OBJCOPY=avr-objcopy
CC=avr-gcc
CPP=avr-g++
AVRDUDE=avrdude

# To use this Makefile for uploading, you'll need to specify the 
# port and baud rate. /dev/ttuUSB0 is an alternative on some systems
UPLOAD_DEV=/dev/ttyACM0
UPLOAD_BAUD=57600

# Specify the object files that will make up the non-library part of
# the final executable. Each is assumed to be accompanied by a 
# corresponding .cpp file
PROG_OBJS=keyboard11x5.o Keyboard.o HID.o

# Specify the Arduino library files that are needed by the program. Some,
# like hooks.o, are likely to be needed in every program. Others will
# depend on the specific board features used.
# Note that some of the library is provided as C and some as C++ and,
# annoyingly, this does seem to change between versions.
LIB_C_OBJS=wiring.o hooks.o wiring_digital.o
LIB_CPP_OBJS=main.o Print.o USBCore.o HardwareSerial.o HardwareSerial1.o PluggableUSB.o CDC.o abi.o

# Set the CPU frequency in Hz. This value must be passed to the compiler
# as it is used by the Arduino library to calculate time delays
F_CPU=16000000

# Set the ATMEGA device type
MCU=atmega32u4

##### There should be no need to change anything below this point #####

TARGET=$(NAME).hex

INCLUDE=$(LIBRARY_DIR)

OBJECTS=$(PROG_OBJS) $(LIB_C_OBJS) $(LIB_CPP_OBJS)
DEPS := $(OBJECTS:.o=.deps)

# Note that the compile and build flags are chosen to minimize
# storage -- -ffunction-sections, etc.
LDFLAGS=-w -Os -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=$(MCU)
# The USB vendor ID 0x1b4f identifies SparkFun, but this value
#   is arbitrary. It, along with the USB product ID, is presented to
#   the host system as identifiers of the board. 
CFLAGS=-Os -Wall -ffunction-sections -fdata-sections -mmcu=$(MCU) -DF_CPU=$(F_CPU) -MMD -DUSB_VID=0x1bf4 -DUSB_PID=0x9204
CPPFLAGS=$(CFLAGS) -fno-exceptions -fno-threadsafe-statics
INCLUDES=-I $(VARIANT_INCLUDE) -I $(INCLUDE)

all: $(TARGET)

%.o: $(LIBRARY_DIR)/%.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDES) -c -o $@ $<

%.o: $(LIBRARY_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDES) -c -o $@ $<

$(NAME).elf: $(PROG_OBJS) $(LIB_CPP_OBJS) $(LIB_C_OBJS)
	$(CC) $(LDFLAGS) -o $(NAME).elf $(PROG_OBJS) $(LIB_CPP_OBJS) $(LIB_C_OBJS)

$(TARGET): $(NAME).elf
	$(OBJCOPY) -O ihex -R .eeprom  $(NAME).elf $(TARGET) 
	cp $(TARGET) binaries/

clean:
	rm -f *.o *.d $(TARGET) $(NAME).elf LMX*

# Before doing "make upload" we must reset the board to bootloader mode,
# We can either do this in software by toggling the baud rate or --
# if the board is completely hosed -- switching the RST pin low twice in 
# quick succession. Note that the baud-toggling method is very time-
# sensitive -- after setting to 1200, the baud will drop its USB and
# restart it in bootload mode. The host system has to recognize this 
# change, and reestablish the /dev/XXX device. This can take time; but
# if we wait too long, the board will drop out of bootloader mode.
upload: all
	stty -F $(UPLOAD_DEV) speed 1200
	sleep 1 
	stty -F $(UPLOAD_DEV) speed 57600 
	sleep 0.25 
	$(AVRDUDE) -v -p$(MCU) -cavr109 -P$(UPLOAD_DEV) -b$(UPLOAD_BAUD) -D -Uflash:w:$(TARGET):i

.PHONY: clean

