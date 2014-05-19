CXX = g++
CFLAGS = -O2 -Wall -Wno-unused -fmax-errors=5
LIBS = -lfltk -lfltk_images 
OBJS = Blend.o Bmp.o Clone.o Bitmap.o Map.o Stroke.o View.o Widget.o Button.o ToggleButton.o Field.o Separator.o Gui.o check.o main.o

default: $(OBJS)
	$(CXX) -o rendera $(OBJS) $(CFLAGS) $(LIBS)

%.o: %.cxx
	$(CXX) -c $< -o $*.o $(CFLAGS)

clean:
	@rm -f rendera *.o
	@echo "Clean."

