#!/bin/bash
function t0014 {
    FUSE_MD5=$(md5sum $MOUNTPOINT/`basename $TMP_FILE` | cut -d\  -f1)
}

function t0014-check {
    [ "$FUSE_MD5" = "$FILE_MD5" ]
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

# Recreate the same sparse file on the target fs
e4test_mount
NEWTMP=$MOUNTPOINT/`basename $TMP_FILE`
dd if=/dev/zero of=$NEWTMP bs=1024 seek=1024 count=0 &> /dev/null
dd if=$TMP_FILE.rnd of=$NEWTMP bs=1024 seek=512 conv=notrunc &> /dev/null
e4test_umount

# Check the md5 after mount using fuse
e4test_fuse_mount
e4test_run t0014
e4test_fuse_umount

rm $FS
rm $TMP_FILE

e4test_end t0014-check
