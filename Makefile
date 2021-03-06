CPP    := g++ -Wall -g
LIB   := -lpthread -lrt -lfskit -lfskit_fuse -lpstat
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c) $(wildcard os/*.c)
CXSRCS:= $(wildcard *.cpp) $(wildcard os/*.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

VDEV := vdev

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

# change/override this for your OS
OS := LINUX

all: $(VDEV)

$(VDEV): $(OBJ)
	$(CPP) -o $(VDEV) $(OBJ) $(LIBINC) $(LIB) -D_VDEV_OS_$(OS)

install: $(VDEV)
	mkdir -p $(BINDIR)
	cp -a $(VDEV) $(BINDIR)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS) -D_VDEV_OS_$(OS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS) -D_VDEV_OS_$(OS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(VDEV)
