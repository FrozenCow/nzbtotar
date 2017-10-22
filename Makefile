#CC=clang++
CC=g++

CFLAGS+=-g
CFLAGS+=-pg
CFLAGS+=-O3
CFLAGS+=`pkg-config --cflags libxml-2.0`
#CFLAGS+=-Wl,-t -v

LDFLAGS+=-g
LDFLAGS+=-pg
LDFLAGS+=-O3
LDFLAGS+=-fPIC
LDFLAGS+=`pkg-config --libs libxml-2.0`
LDFLAGS+=-lpthread -lexpat -Lunrar -lunrar -lvrb
#LDFLAGS+=-Wl,-t -Wl,-v -v

OBJ=nzbtotar.o crc32.o nzbparser.o nzbdownload.o memoryfile.o common.o proconstream.o parserarvolume.o

all: nzbtotar

%.o: %.cpp
		$(CC) -c -o $@ $< $(CFLAGS)

unrar/libunrar.a:
	$(MAKE) -C unrar staticlib

#nzbfetch/libnzbfetch.a:
#	$(MAKE) -C nzbfetch staticlib

nzbtotar: unrar/libunrar.a $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	@rm -rf *.o
	$(MAKE) -C unrar clean
