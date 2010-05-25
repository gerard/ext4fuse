#!/bin/bash
function t0030 {
    FUSE_MD5=`md5sum $MOUNTPOINT/link1 | cut -d\  -f1`
}

function t0030-check {
    [ "$FUSE_MD5" = "$FILE_MD5" ]
}

set -e
source `dirname $0`/lib.sh

# Make a random file, and store the md5
TMP_FILE=`mktemp`
dd if=/dev/urandom of=$TMP_FILE bs=1024 count=1 &> /dev/null
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`

e4test_make_LOGFILE
e4test_make_FS 16
e4test_make_MOUNTPOINT

# Copy the file in the FS and link it
e4test_mount
cp $TMP_FILE $MOUNTPOINT
cd $MOUNTPOINT
ln -s `basename $TMP_FILE` link1
cd - > /dev/null
e4test_umount

# Check the md5 after mount using fuse and through the link
e4test_fuse_mount
e4test_run t0030
e4test_fuse_umount

rm $FS
rm $TMP_FILE

e4test_end t0030-check
