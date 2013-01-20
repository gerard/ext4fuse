#!/bin/bash
function t0010 {
    FUSE_MD5=$(md5sum $MOUNTPOINT/`basename $TMP_FILE` | cut -d\  -f1)
}

function t0010-check {
    [ "$FUSE_MD5" = "$FILE_MD5" ]
}

set -e
source `dirname $0`/lib.sh

# Make a random file, and store the md5
TMP_FILE=`mktemp`
dd if=/dev/urandom of=$TMP_FILE bs=1024 count=$((1024 * 16)) &> /dev/null
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`

e4test_make_LOGFILE
e4test_make_FS 32

# Copy the file to the FS
e4test_debugfs_write $TMP_FILE

# Check the md5 after mount using fuse
e4test_make_MOUNTPOINT
e4test_fuse_mount
e4test_run t0010
e4test_fuse_umount

rm $FS
rm $TMP_FILE

e4test_end t0010-check
