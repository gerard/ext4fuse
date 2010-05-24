CFLAGS  += $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=26 -std=gnu99 -g3 -Wall
LDFLAGS += $(shell pkg-config fuse --libs)

BINARY = ext4fuse
SOURCES += e4flib.o fuse-main.o op_readlink.o op_init.o disk.o super.o inode.o
SOURCES += logging.o extents.o

$(BINARY): $(SOURCES)
	$(CC) -o $@ $^ $(LDFLAGS)

test-slow: $(BINARY)
	@for T in test/[0-9]*; do ./$$T; done

test: $(BINARY)
	@for T in test/[0-9]*; do SKIP_SLOW_TESTS=1 ./$$T; done

clean:
	rm -f *.o $(BINARY)
	rm -rf test/logs

.PHONY: test
