#!/bin/bash
function t0001 {
    ls $MOUNTPOINT > /dev/null
}

function t0001-check {
    true
}

set -e
source `dirname $0`/lib.sh

e4test_make_LOGFILE
e4test_make_FS 1
e4test_make_MOUNTPOINT

e4test_fuse_mount
e4test_run t0001
e4test_fuse_umount

rm $FS

e4test_end t0001-check
