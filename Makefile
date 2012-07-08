ifeq ($(shell [ -d /usr/local/include/osxfuse ] && echo osxfuse),osxfuse)
CFLAGS  += -I/usr/local/include/osxfuse
LDFLAGS += -lfuse
else
ifeq ($(shell which pkg-config), )
$(error You need to install pkg-config in order to compile this sources)
endif
CFLAGS  += $(shell pkg-config fuse --cflags)
LDFLAGS += $(shell pkg-config fuse --libs)
endif

CFLAGS  += -DFUSE_USE_VERSION=26 -std=gnu99 -g3 -Wall -Wextra

ifeq ($(shell uname), Darwin)
CFLAGS  += -mmacosx-version-min=10.5

# fuse.pc pulls this flag in for me, but it seems that some old versions don't
CFLAGS  += -D_FILE_OFFSET_BITS=64
endif

ifeq ($(shell uname), FreeBSD)
CFLAGS  += -I/usr/local/include -L/usr/local/lib
LDFLAGS += -lexecinfo
endif

BINARY = ext4fuse
SOURCES += fuse-main.o logging.o extents.o disk.o super.o inode.o dcache.o
SOURCES += op_read.o op_readdir.o op_readlink.o op_init.o op_getattr.o op_open.o

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
