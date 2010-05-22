#!/bin/bash
set -e
source `dirname $0`/lib.sh

echo -n "`basename $0`: "

e4test_make_LOGFILE
e4test_make_FS 16
e4test_make_MOUNTPOINT

e4test_fuse_mount
e4test_fuse_umount
e4test_check_log

rm $FS
echo PASS
