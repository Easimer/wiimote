INCLUDE_DIR=$(realpath include/)
IMGUI_INCLUDES=-I $(realpath imgui/) -I $(realpath imgui/backends/)
CFLAGS=-Wall -Werror \
	   -O2 -g \
	   -I $(INCLUDE_DIR) $(IMGUI_INCLUDES) \
	   -D IMGUI_IMPL_OPENGL_LOADER_GLAD
CXXFLAGS=-Wall -Werror \
		 -O2 -g \
		 -I $(INCLUDE_DIR) $(IMGUI_INCLUDES) \
		 -D IMGUI_IMPL_OPENGL_LOADER_GLAD
OBJECTS=main.o

LIBGLAD=glad/glad.a
LIBWIIMOTE=wiimote/wiimote.a
LIBIMGUI=imgui.a

LDFLAGS= $(LIBGLAD) $(LIBWIIMOTE) $(LIBIMGUI) -ldl -lbluetooth -lSDL2 -lm

all: wm

wm: $(OBJECTS) $(LIBGLAD) $(LIBWIIMOTE) $(LIBIMGUI)
	$(CXX) -o wm $(OBJECTS) $(LDFLAGS)

glad/glad.a:
	CFLAGS="$(CFLAGS)" $(MAKE) -C glad

wiimote/wiimote.a: wiimote/motion_input.c wiimote/wiimote_hw.c
	CFLAGS="$(CFLAGS)" $(MAKE) -C wiimote

imgui.a:
	$(MAKE) -f Makefile.imgui

clean:
	rm -f $(OBJECTS)
	$(MAKE) -f Makefile.imgui clean
	$(MAKE) -C wiimote clean
	$(MAKE) -C glad clean
.PHONY: clean
