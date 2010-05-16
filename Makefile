CFLAGS  += $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=22 -std=gnu99 -g3
LDFLAGS += $(shell pkg-config fuse --libs)

ext4fuse: main.o
	$(CC) -o $@ $^ $(LDFLAGS)

fs:
	dd if=/dev/zero of=ext4fs.raw bs=128 count=$$((1024 * 1024))
	mke2fs -t ext4 ext4fs.raw
	mkdir t
	sudo mount -o loop ext4fs.raw t/
	mkdir -p t/dir1/dir2/dir3
	touch t/dir1/dir2/dir3/file
	cp -a ~/src/linux-2.6/Documentation t/
	sudo umount t/
	rmdir t

fs-random:
	dd if=/dev/urandom of=ext4fsr.raw bs=128 count=$$((1024 * 1024))
	mke2fs -t ext4 ext4fsr.raw

run:
	valgrind ./ext4fuse ext4fs.raw

clean:
	rm -f *.o ext4fuse ext4fs.raw ext4fsr.raw
