function e4test_make_LOGFILE {
    export LOGFILE="test/logs/`date +%y%m%d-%H:%M.%S`"
    mkdir -p `dirname $LOGFILE`
}

function e4test_make_MOUNTPOINT {
    [ ! -f "$FS" ] && echo "No FS"
    export MOUNTPOINT="$FS-mount"
}

function e4test_make_FS {
    export FS=`mktemp /tmp/ext4fuse-test.XXXXXXXX`
    dd if=/dev/zero of=$FS bs=$1 count=$((1024 * 1024)) &> /dev/null
    mke2fs -F -t ext4 $FS &> /dev/null
}

function e4test_mount {
    mkdir $MOUNTPOINT
    sudo mount -o loop -t ext4 $FS $MOUNTPOINT
}

function e4test_fuse_mount {
    mkdir $MOUNTPOINT
    if [ -z "$LOGFILE" ]
    then
        ./ext4fuse $FS $MOUNTPOINT
    else
        ./ext4fuse $FS $MOUNTPOINT $LOGFILE
    fi
}

function e4test_umount {
    sudo umount $MOUNTPOINT
    rmdir $MOUNTPOINT
}

function e4test_fuse_umount {
    fusermount -u $MOUNTPOINT
    rmdir $MOUNTPOINT
}

function e4test_check_log {
    if grep ASSERT $LOGFILE
    then
        exit 1
    fi
}

function e4test_mountpoint_struct_md5 {
    # Here we skip lost+found since user doesn't normally have permission to
    # read it.  find(1) sure has a trippy syntax...
    find $MOUNTPOINT -name lost+found -prune -o -name \* | sort | md5sum | cut -d\  -f1
}
