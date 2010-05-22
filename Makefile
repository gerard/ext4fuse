CFLAGS  += $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=26 -std=gnu99 -g3 -Wall
LDFLAGS += $(shell pkg-config fuse --libs)

BINARY = ext4fuse

ext4fuse: e4flib.o fuse-main.o op_readlink.o
	$(CC) -o $@ $^ $(LDFLAGS)

test: $(BINARY)
	@for T in test/[0-9]*; do ./$$T; done

clean:
	rm -f *.o $(BINARY)

.PHONY: test
