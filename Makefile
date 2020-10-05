PRGNAME     = srb2

# define regarding OS, which compiler to use
CC          = cc

# change compilation / linking flag options
CFLAGS		=  -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT -I/usr/include/libpng16
CFLAGS		+=  -fno-exceptions -Isrc/sdl12
CFLAGS		+= -DDIRECTFULLSCREEN -DHAVE_SDL -DHAVE_MIXER -DNOHW -DCOMPVERSION -DHAVE_PNG -DHAVE_OPENMPT -DHAVE_ZLIB -DNDEBUG
CFLAGS		+= -DGCW0 -DNOPOSTPROCESSING -DLOWMEMORY

CFLAGS		+= -Ofast -fdata-sections -ffunction-sections -fsingle-precision-constant -flto

LDFLAGS     =  $(shell sdl-config --libs)  -lpng -lrt -lgme -lcurl -lm -lopenmpt -lz -lSDL_mixer -flto -Wl,--as-needed -Wl,--gc-sections -s

# Files to be compiled
SRCDIR    = ./src ./src/sdl12 ./src/blua
VPATH     = $(SRCDIR)
SRC_C   = $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.c))
OBJ_C   = $(notdir $(patsubst %.c, %.o, $(SRC_C)))
OBJS     = $(OBJ_C)

# Rules to make executable
$(PRGNAME): $(OBJS)  
	$(CC) $^ -o $(PRGNAME) $(LDFLAGS)

$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<


clean:
	rm -f *.o
