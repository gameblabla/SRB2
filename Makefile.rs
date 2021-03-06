PRGNAME     = opk/srb2.elf

# define regarding OS, which compiler to use
CC          = /opt/retrostone-toolchain/usr/bin/arm-linux-gcc
PORT		= sdl12
PROFILE		= 0

# change compilation / linking flag options
CFLAGS		=  -D_GNU_SOURCE=1 -D_REENTRANT $(shell /opt/gcw0-toolchain/mipsel-gcw0-linux-uclibc/sysroot/usr/bin/sdl-config --cflags)
CFLAGS		+= -Isrc -Isrc/$(PORT)
CFLAGS		+= -DDIRECTFULLSCREEN -DHAVE_SDL -DHAVE_MIXER -DNOHW -DCOMPVERSION -DHAVE_PNG -DHAVE_ZLIB -DNDEBUG -DNOMD5
CFLAGS		+= -DRS1 -DGCW0_OPTS -DNOPOSTPROCESSING -DGCW0_INPUT
CFLAGS		+= -Ofast -fsingle-precision-constant -fdata-sections -ffunction-sections -flto -fno-common
CFLAGS		+= -fno-builtin -finline-limit=60

ifeq ($(PROFILE), YES)
CFLAGS		+= -fprofile-generate=/usr/local/home/srb2_gcda
LDFLAGS		= -lgcov
else ifeq ($(PROFILE), APPLY)
CFLAGS		+= -fprofile-use
endif

LDFLAGS_SDL = -lSDL -lasound -lSDL_mixer -lmpg123 -lmikmod -lvorbisidec -logg

LDFLAGS     += -nodefaultlibs -lc -lgcc $(LDFLAGS_SDL) -lpng -lrt -lm -lz -Wl,-z,norelro -Wl,--build-id=none -Wl,-O1,--sort-common,--as-needed,--gc-sections -no-pie -flto -s

# Files to be compiled
SRCDIR    = ./src ./src/$(PORT) ./src/blua
VPATH     = $(SRCDIR)
SRC_C   = $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.c))
OBJ_C   = $(notdir $(patsubst %.c, %.o, $(SRC_C)))
OBJS     = $(OBJ_C)

# Rules to make executable
$(PRGNAME): $(OBJS)  
	$(CC) $(CFLAGS) $^ -o $(PRGNAME) $(LDFLAGS)

$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<


clean:
	rm -f *.o
