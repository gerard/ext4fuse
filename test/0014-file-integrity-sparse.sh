#!/bin/bash

UNEVEN_BYTES=1048577

function t0014 {
    FUSE_MD5=$(md5sum $MOUNTPOINT/`basename $TMP_FILE` | cut -d\  -f1)
    FUSE_BYTES=$(cat $MOUNTPOINT/`basename $TMP_FILE.uneven` | wc -c)
}

function t0014-check {
    [ "$FUSE_MD5" = "$FILE_MD5" -a "$FUSE_BYTES" = "$UNEVEN_BYTES" ]
}

set -e
source `dirname $0`/lib.sh

# Make a sparse 1MB file w/4k allocated in the middle, and store the md5
TMP_FILE=`mktemp`
dd if=/dev/zero of=$TMP_FILE bs=1024 seek=1024 count=0 &> /dev/null
dd if=/dev/urandom of=$TMP_FILE.rnd bs=1024 count=4 &> /dev/null
dd if=$TMP_FILE.rnd of=$TMP_FILE bs=1024 seek=512 conv=notrunc &> /dev/null
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`

e4test_make_LOGFILE
e4test_make_FS 2
e4test_make_MOUNTPOINT

e4test_mount

# Test A: recreate the same sparse file on the target fs
NEWTMP=$MOUNTPOINT/`basename $TMP_FILE`
dd if=/dev/zero of=$NEWTMP bs=1024 seek=1024 count=0 &> /dev/null
dd if=$TMP_FILE.rnd of=$NEWTMP bs=1024 seek=512 conv=notrunc &> /dev/null

# Test B: create a fully sparse file whose length is not block-aligned
truncate -s $UNEVEN_BYTES $MOUNTPOINT/`basename $TMP_FILE`.uneven

e4test_umount

# Check the md5 (test A) and byte count (test B) after mount using fuse
e4test_fuse_mount
e4test_run t0014
e4test_fuse_umount

rm $FS
rm $TMP_FILE
rm $TMP_FILE.rnd

e4test_end t0014-check
