#!/bin/bash
function t0000 {
    e4test_fuse_mount
    e4test_sleep 1
    e4test_fuse_umount
}

function t0000-check {
    true
}

set -e
source `dirname $0`/lib.sh

e4test_make_LOGFILE
e4test_make_FS 16
e4test_make_MOUNTPOINT

e4test_run t0000
e4test_end t0000-check

rm $FS
