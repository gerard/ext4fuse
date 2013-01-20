#!/bin/bash
function t0015 {
    for i in `seq 1 16`
    do
        FUSE_MD5[$i]=$(md5sum $MOUNTPOINT/`basename $TMP_FILE.$i` | cut -d\  -f1)
    done
}

function t0015-check {
    for i in `seq 1 16`
    do
        [ "${FUSE_MD5[i]}" = "$FILE_MD5" ]
    done
}

set -e
source `dirname $0`/lib.sh

# Make a random file, and store the md5
TMP_FILE=`mktemp`
dd if=/dev/urandom of=$TMP_FILE bs=1024 count=16 &> /dev/null
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`

e4test_make_LOGFILE
e4test_make_FS 32

# Copy the file to the FS
for i in `seq 1 16`
do
    mv $TMP_FILE $TMP_FILE.$i
    e4test_debugfs_write $TMP_FILE.$i
    mv $TMP_FILE.$i $TMP_FILE
done

# Check the md5 after mount using fuse
e4test_make_MOUNTPOINT
e4test_fuse_mount
e4test_run t0015
e4test_fuse_umount

rm $FS
rm $TMP_FILE

e4test_end t0015-check
