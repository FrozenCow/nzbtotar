CC=clang++
CFLAGS=-O2 -fPIC
LDFLAGS=-lpthread
DEFINES=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

OBJ = crc32.o file.o nzb_fetch.o net.o nttp_connection.o parse_nzb.o post.o process.o queue.o segment.o server.o yenc.o compat/reallocf.o

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

all: sharedlib staticlib

sharedlib: $(OBJ)
	$(CC) -shared -o libnzbfetch.so $(LDFLAGS) $(OBJ)

staticlib: $(OBJ)
	ar -rs libnzbfetch.a $(OBJ)

clean:
	@rm -rf *.o libnzbfetch.so libnzbfetch.a
