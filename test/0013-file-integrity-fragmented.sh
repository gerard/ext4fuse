#!/bin/bash
function t0013 {
    FUSE_MD5=$(md5sum $MOUNTPOINT/`basename $TMP_FILE` | cut -d\  -f1)
}

function t0013-check {
    [ "$FUSE_MD5" = "$FILE_MD5" ]
}

set -e
source `dirname $0`/lib.sh

# Make a random file, and store the md5
TMP_FILE=`mktemp`
dd if=/dev/urandom of=$TMP_FILE bs=1024 count=1024 &> /dev/null
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`

e4test_make_LOGFILE
e4test_make_FS 8
e4test_make_MOUNTPOINT

# Copy the file in the FS
e4test_mount

dd if=/dev/urandom of=$MOUNTPOINT/filler bs=1024 count=64 &> /dev/null
for x in `seq 1 106`; do
	cp $MOUNTPOINT/filler $MOUNTPOINT/filler.$x &> /dev/null || break
done
for x in `seq 2 2 48`; do
	rm $MOUNTPOINT/filler.$x
done

cp $TMP_FILE $MOUNTPOINT

e4test_umount

# Check the md5 after mount using fuse
e4test_fuse_mount
e4test_run t0013
e4test_fuse_umount

rm $FS
rm $TMP_FILE

e4test_end t0013-check
