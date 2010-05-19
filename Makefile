CFLAGS  += $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=22 -std=gnu99 -g3 -Wall
LDFLAGS += $(shell pkg-config fuse --libs)

ext4fuse: e4flib.o fuse-main.o
	$(CC) -o $@ $^ $(LDFLAGS)

fs:
	dd if=/dev/zero of=ext4fs.raw bs=128 count=$$((1024 * 1024))
	mke2fs -F -t ext4 ext4fs.raw
	mkdir -p t
	sudo mount -o loop ext4fs.raw t/
	mkdir -p t/dir1/dir2/dir3
	touch t/dir1/dir2/dir3/file
	cp -a ~/src/linux-2.6/Documentation t/
	sudo umount t/
	rmdir t

fs-random:
	dd if=/dev/zero of=ext4fsr.raw bs=1024 count=$$((1024 * 1024))
	mke2fs -F -t ext4 ext4fsr.raw
	mkdir -p t
	sudo mount -o loop ext4fsr.raw t/
	dd if=/dev/urandom of=t/randomfile bs=800 count=$$((1024 * 1024))
	md5sum t/randomfile > fs-random.md5
	sudo umount t
	rmdir t

mount:
	mkdir t
	./ext4fuse ext4fs.raw t/

mountr:
	mkdir t
	./ext4fuse ext4fs.raw t/

mvalgrind:
	mkdir -p t
	valgrind -v ./ext4fuse ext4fs.raw t/

umount:
	fusermount -u t/
	rmdir t

clean:
	rm -f *.o ext4fuse

mrproper: clean
	rm -f ext4fs.raw ext4fsr.raw
