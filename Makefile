CFLAGS=-Wall -Werror -O2 -g
LDFLAGS=-ldl -lbluetooth
OBJECTS=main.o wiimote_hw.o motion_input.o

all: wm

wm: $(OBJECTS)
	$(CC) -o wm $(OBJECTS) $(LDFLAGS)
