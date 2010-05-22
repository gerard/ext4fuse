#!/bin/bash
set -e
source `dirname $0`/lib.sh

echo -n "`basename $0`: "

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
FUSE_MD5=`md5sum $MOUNTPOINT/link1 | cut -d\  -f1`
[ -z "$FUSE_MD5" ] && exit 1
e4test_fuse_umount
e4test_check_log

rm $FS
rm $TMP_FILE

if [ "$FUSE_MD5" = "$FILE_MD5" ]
then
    echo PASS
else
    echo FAIL
fi
