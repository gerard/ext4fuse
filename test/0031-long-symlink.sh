#!/bin/bash
function t0031 {
    FUSE_MD5=`md5sum $MOUNTPOINT/link1 | cut -d\  -f1`
}

function t0031-check {
    [ "$FUSE_MD5" = "$FILE_MD5" ]
}

set -e
source `dirname $0`/lib.sh

# Long symlinks are those that have more than 60 chars.  This is a different
# scenario because in this case the link is not stored in the inode, but on a
# data block.

# Make a random file, and store the md5
TMP_FILE=`mktemp`
dd if=/dev/urandom of=$TMP_FILE bs=1024 count=1 &> /dev/null
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`
LONG_FILENAME=`seq 0 60 | tr -d '\n'`

e4test_make_LOGFILE
e4test_make_FS 16
e4test_make_MOUNTPOINT

# Copy the file in the FS and link it
e4test_mount
cp $TMP_FILE $MOUNTPOINT/$LONG_FILENAME
cd $MOUNTPOINT
ln -s $LONG_FILENAME link1
cd - > /dev/null
e4test_umount

# Check the md5 after mount using fuse and through the link
e4test_fuse_mount
e4test_run t0031
e4test_fuse_umount

rm $FS
rm $TMP_FILE

e4test_end t0031-check
