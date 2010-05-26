#!/bin/bash
function t0012 {
    OUT_TMPFILE=`mktemp`
    TESTED_FILE=$MOUNTPOINT/`basename $TMP_FILE`

    # Shake the file around
    dd if=$TESTED_FILE skip=1023 bs=2 count=1 >> $OUT_TMPFILE 2> /dev/null
    dd if=$TESTED_FILE skip=1023 bs=1 count=1 >> $OUT_TMPFILE 2> /dev/null
    dd if=$TESTED_FILE skip=1023 bs=99 count=999 >> $OUT_TMPFILE 2> /dev/null
    T0012_MD5=$(md5sum $OUT_TMPFILE | cut -d\  -f1)
    rm $OUT_TMPFILE
}

function t0012-check {
    [ "$T0012_MD5" = "$FILE_MD5" ]
}

set -e
source `dirname $0`/lib.sh

# Make a random file, and store the md5
TMP_FILE=`mktemp`
dd if=/dev/urandom of=$TMP_FILE bs=1024 count=$((1024 * 16)) &> /dev/null
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`

e4test_make_LOGFILE
e4test_make_FS 32
e4test_make_MOUNTPOINT

# Copy the file in the FS
e4test_mount
cp $TMP_FILE $MOUNTPOINT
t0012
FILE_MD5=$T0012_MD5
e4test_umount

# Check the md5 after mount using fuse
e4test_fuse_mount
e4test_run t0012
e4test_fuse_umount

rm $FS
rm $TMP_FILE

e4test_end t0012-check
