#CFLAGS=-Wall -Werror -O2 -g
LDFLAGS=-ldl -lbluetooth
OBJECTS=wiimote_hw.o motion_input.o

all: wiimote.a

wiimote.a: $(OBJECTS)
	$(AR) rcs wiimote.a $(OBJECTS)

clean:
	rm -f $(OBJECTS) wiimote.a
.PHONY: clean
