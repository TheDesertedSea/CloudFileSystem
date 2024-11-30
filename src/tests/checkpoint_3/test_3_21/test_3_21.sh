#!/bin/bash
#
# A script to test if the basic functions of the files 
# in CloudFS. Has to be run from the ./src/scripts/ 
# directory.
# 
TEST_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source $TEST_DIR/../../../scripts/paths.sh
source $SCRIPTS_DIR/functions.sh

THRESHOLD="32"
AVGSEGSIZE="4"
LOG_DIR="/tmp/testrun-`date +"%Y-%m-%d-%H%M%S"`"
TESTDIR=$FUSE_MNT_
TEMPDIR="/tmp/cloudfstest"
STATFILE="$LOG_DIR/stats"
TARFILE="$TEST_DIR/big_test.tar.gz"
CACHE_SIZE="16"

#
# Execute battery of test cases.
# expects that the test files are in $TESTDIR
# and the reference files are in $TEMPDIR
# Creates the intermediate results in $LOGDIR
#

function execute_part3_tests()
{

    echo "Executing test_3_21"
    reinit_env
    
    # create the cloud file
    echo "Copying files"
    cp $TEST_DIR/largefile $TESTDIR
    cp $TEST_DIR/largefile $TEMPDIR
    
    sleep 1

    echo "Checking for data integrity(largefile)                 "
    TEST_FILE="largefile"
    (cd $TESTDIR && find $TEST_FILE  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 | awk '{print $1}' > $LOG_DIR/md5sum.out.master)
    (cd $TEMPDIR && find $TEST_FILE  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 | awk '{print $1}' > $LOG_DIR/md5sum.out)
    diff $LOG_DIR/md5sum.out.master $LOG_DIR/md5sum.out
    print_result $?

    ls -al $FUSE_MNT/
    ls -al $SSD_MNT_/.cache/

    # unmount and remount
    sync
    sleep 2
    $SCRIPTS_DIR/cloudfs_controller.sh u
    sleep 2
    $SCRIPTS_DIR/umount_disks.sh

    ls -al $S3_DIR/cloudfs/
    sleep 2
    $SCRIPTS_DIR/mount_disks.sh

    sync
    sleep 2
    $SCRIPTS_DIR/cloudfs_controller.sh x --ssd-path $SSD_MNT_ --fuse-path $FUSE_MNT_ --threshold $THRESHOLD --avg-seg-size $AVGSEGSIZE --cache-size $CACHE_SIZE


    ls -al $FUSE_MNT/
    ls -al $SSD_MNT_/.cache/

    dd if=$FUSE_MNT/largefile bs=16384 count=1 > /dev/null


    ls -al $SSD_MNT_/.cache/
}

# Main
process_args cloudfs --ssd-path $SSD_MNT_ --fuse-path $FUSE_MNT_ --threshold $THRESHOLD --avg-seg-size $AVGSEGSIZE --cache-size $CACHE_SIZE
#----

rm -rf $TEMPDIR
mkdir -p $TEMPDIR
mkdir -p $LOG_DIR

#run the actual tests
execute_part3_tests

#----
# test cleanup
rm -rf $TEMPDIR
rm -rf $LOG_DIR

exit 0