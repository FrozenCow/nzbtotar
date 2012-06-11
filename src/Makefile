CC=clang++
CFLAGS=-I/usr/include/libxml2 -Wl,-t -v
LDFLAGS=-fPIC -lpthread -lexpat -lxml2 -lvrb -Wl,-t -Wl,-v -v
OBJ=main.o crc32.o 

all: main

%.o: %.cpp
		$(CC) -c -o $@ $< $(CFLAGS)

unrar/libunrar.a:
	$(MAKE) -C unrar staticlib

#nzbfetch/libnzbfetch.a:
#	$(MAKE) -C nzbfetch staticlib

main: unrar/libunrar.a $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	@rm -rf *.o
	$(MAKE) -C unrar clean
