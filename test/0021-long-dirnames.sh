#!/bin/bash

# The point of long dirnames is to test the dcache, which has different
# behaviour if the dirname doesn't fit to the dcache entry.

function t0021 {
    FUSE_MD5=`e4test_mountpoint_struct_md5`
}

function t0021-check {
    [ "$FUSE_MD5" = "$DIRS_MD5" ]
}

set -e
source `dirname $0`/lib.sh

e4test_make_LOGFILE
e4test_make_FS 128
e4test_make_MOUNTPOINT

PREFIX=veryverylongprefix-long-enough-so-it-doesnt-fit-to-dcache-entry-alone
e4test_mount
for SUFIX_A in a b c d e f
do
    for SUFIX_B in a b c d e f
    do
        mkdir -p $MOUNTPOINT/$PREFIX-$SUFIX_A-$SUFIX_B/{0,1,2,3,4,5,6,7,8,9}
    done
done
DIRS_MD5=`e4test_mountpoint_struct_md5`
e4test_umount

e4test_fuse_mount
e4test_run t0021
e4test_fuse_umount

rm $FS

e4test_end t0021-check
