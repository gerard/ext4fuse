#!/bin/bash
function t0011 {
    FUSE_MD5=$(md5sum $TMP_FILE | cut -d\  -f1)
}

function t0011-check {
    [ "$FUSE_MD5" = "$FILE_MD5" ]
}

set -e
source `dirname $0`/lib.sh

e4test_make_LOGFILE
e4test_make_FS 1024
e4test_make_MOUNTPOINT

e4test_mount

TMP_FILE=$MOUNTPOINT/bigfile
dd if=/dev/urandom of=$TMP_FILE.0 bs=1024 count=1024 &> /dev/null
for i in `seq 1 9` ; do
    cat $TMP_FILE.$(($i - 1)) $TMP_FILE.$(($i - 1)) >> $TMP_FILE.$i
    rm $TMP_FILE.$(($i - 1))
done
mv $TMP_FILE.9 $TMP_FILE
FILE_MD5=`md5sum $TMP_FILE | cut -d\  -f1`

e4test_umount

# Check the md5 after mount using fuse
e4test_fuse_mount
e4test_run t0011
e4test_fuse_umount

rm $FS

e4test_end t0011-check
