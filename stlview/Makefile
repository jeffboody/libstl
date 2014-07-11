TARGET   = stlview
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I. -DA3D_GLESv2_LOAX
LDFLAGS  = -L/usr/lib -Llibstl -lstl -Lloax -lloax -Lnet -lnet -La3d -la3d -lpthread -lm
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) a3d net loax stl
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: a3d net loax

a3d:
	$(MAKE) -C a3d -f Makefile.loax

net:
	$(MAKE) -C net

loax:
	$(MAKE) -C loax

stl:
	$(MAKE) -C libstl

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C a3d -f Makefile.loax clean
	$(MAKE) -C net clean
	$(MAKE) -C loax clean
	$(MAKE) -C libstl clean
	rm a3d net loax libstl

$(OBJECTS): $(HFILES)
