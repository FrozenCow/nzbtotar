CC=clang++
CFLAGS=-Iunrar -Inzbfetch
LDFLAGS=-Lunrar -Lnzbfetch -lunrar -lnzbfetch -lpthread -lexpat
OBJ=main.o

all: main

%.o: %.c
		$(CC) -c -o $@ $< $(CFLAGS)

unrar/libunrar.a:
	$(MAKE) -C unrar staticlib

nzbfetch/libnzbfetch.a:
	$(MAKE) -C nzbfetch staticlib

main: unrar/libunrar.a nzbfetch/libnzbfetch.a $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDFLAGS)

clean:
	@rm -rf *.o
	$(MAKE) -C unrar clean
	$(MAKE) -C nzbfetch clean
