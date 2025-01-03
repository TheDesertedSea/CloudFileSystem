#!/bin/bash
#
# A script to test if the basic functions of the files 
# in CloudFS. Has to be run from the ./src/scripts/ 
# directory.
# 
TEST_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source $TEST_DIR/../../../scripts/paths.sh
source $SCRIPTS_DIR/functions.sh

THRESHOLD="64"
AVGSEGSIZE="4"
LOG_DIR="/tmp/testrun-`date +"%Y-%m-%d-%H%M%S"`"
TESTDIR=$FUSE_MNT_
TEMPDIR="/tmp/cloudfstest"
STATFILE="$LOG_DIR/stats"
TARFILE="$TEST_DIR/big_test.tar.gz"
CACHE_SIZE="16"
STAT_FILE="$LOG_DIR/stats"

#
# Execute battery of test cases.
# expects that the test files are in $TESTDIR
# and the reference files are in $TEMPDIR
# Creates the intermediate results in $LOGDIR
#

function execute_part3_tests()
{

    echo "Executing test_3_2"
    reinit_env
    
    echo "Untaring big_test.tar.gz"

    # create the test data in FUSE dir
    untar $TARFILE $TESTDIR 
    # create a reference copy on ext2
    untar $TARFILE $TEMPDIR
    
    sleep 6

    echo -ne "Checking data integrity               "

    (cd $TEMPDIR && ls -lR --time-style=+|grep -v '^total'|grep -v 'cache' > $LOG_DIR/expected.txt)
    (cd $TESTDIR && ls -lR --time-style=+|grep -v '^total'|grep -v 'cache' > $LOG_DIR/real.txt)
    diff $LOG_DIR/expected.txt $LOG_DIR/real.txt
    print_result $?

    echo -ne "Checking for snapshot creation        "
    snapshot_num=$($SCRIPTS_DIR/snapshot $FUSE_MNT/.snapshot s) 
    if [ $? -ne 0 ]; then
      print_result 1 
      exit
    else
      print_result 0
    fi
    
    sleep 1

    ls -al $FUSE_MNT/

    collect_stats > $STAT_FILE
    echo -e "\nCloud statistics -->"
    echo "Capacity usage in cloud : $(get_cloud_max_usage $STAT_FILE)"

    # unmount and remount
    sync
    sleep 2
    $SCRIPTS_DIR/cloudfs_controller.sh u

    ls -al /home/student/mnt/ssd

    sleep 2
    $SCRIPTS_DIR/umount_disks.sh

    ls -al /home/student/mnt/ssd
    
    collect_stats > $STAT_FILE
    echo -e "\nCloud statistics -->"
    echo "Capacity usage in cloud : $(get_cloud_max_usage $STAT_FILE)"

    ls -al $S3_DIR/cloudfs/
    sleep 2
    $SCRIPTS_DIR/format_disks.sh
    $SCRIPTS_DIR/mount_disks.sh

    sync
    sleep 2
    $SCRIPTS_DIR/cloudfs_controller.sh x --ssd-path $SSD_MNT_ --fuse-path $FUSE_MNT_ --threshold $THRESHOLD --avg-seg-size $AVGSEGSIZE --cache-size $CACHE_SIZE

    ls -al $TEST_DIR/.cache
    ls -al $FUSE_MNT/
    # restore a snapshot
    echo -ne "Checking for snapshot restore         "
    $SCRIPTS_DIR/snapshot $FUSE_MNT/.snapshot r $snapshot_num
    if [ $? -ne 0 ]; then
      print_result 1 
      exit
    else
      print_result 0
    fi
    
    sleep 1
    echo -ne "Checking for data integrity           "
    (cd $TEMPDIR && ls -lR --time-style=+|grep -v '^total'|grep -v 'cache' > $LOG_DIR/expected.txt)
    (cd $TESTDIR && ls -lR --time-style=+|grep -v '^total'|grep -v 'cache' > $LOG_DIR/real.txt)
    diff $LOG_DIR/expected.txt $LOG_DIR/real.txt
    print_result $?
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
