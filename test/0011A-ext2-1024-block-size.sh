export TEST_MKE2FS_USE_EXT2=1
export MKE2FS_EXTRA_OPTIONS="-b 1024"

source `dirname $0`/0011-file-integrity-large.sh
