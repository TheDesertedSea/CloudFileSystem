#!/bin/bash
#
# A script to test if the basic functions of the files
# in CloudFS. Has to be run from the src directory.
#
TEST_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source $TEST_DIR/../../../scripts/paths.sh

REFERENCE_DIR="/tmp/cloudfstest"
LOG_DIR="/tmp/testrun-`date +"%Y-%m-%d-%H%M%S"`"
STAT_FILE="$LOG_DIR/stats"
THRESHOLD="16"
TAR_FILE="$TEST_DIR/small_test.tar.gz"

source $SCRIPTS_DIR/functions.sh

#
# Execute battery of test cases.
# expects that the test files are in $FUSE_MNT
# and the reference files are in $REFERENCE_DIR
# Creates the intermediate results in $LOG_DIR
#
process_args cloudfs --ssd-path $SSD_MNT_ --fuse-path $FUSE_MNT_ --threshold $THRESHOLD

# test setup
rm -rf $REFERENCE_DIR
mkdir -p $REFERENCE_DIR
mkdir -p $LOG_DIR

reinit_env

# get rid of disk cache
#$SCRIPTS_DIR/cloudfs_controller.sh x $CLOUDFSOPTS

#----
# Testcases
# assumes out test data does not have any hidden files(.* files)
# students should have all their metadata in hidden files/dirs
echo ""
echo "Executing mytest0"
(cd $FUSE_MNT && touch test_file)
(cd $FUSE_MNT && echo "Hello World" > test_file)
(cd $FUSE_MNT && ln -s /home/student/mnt/fuse/test_file test_file_tmp)
(cd $FUSE_MNT && ls -al)
(cd $FUSE_MNT && cat test_file_tmp)
(cd $FUSE_MNT && setfattr -n user.test -v "test" test_file)
(cd $FUSE_MNT && getfattr -n user.test test_file)
(cd $FUSE_MNT && chmod 444 test_file)
(cd $FUSE_MNT && cat test_file_tmp)
(cd $FUSE_MNT && cat test_file)
(cd $FUSE_MNT && ln /home/student/mnt/fuse/test_file test_file_tmp2)
(cd $FUSE_MNT && cat test_file_tmp2)
(cd $FUSE_MNT && ls -al)


#----
#destructive test : always do this test at the end!!
echo -ne "File removal test (rm -rf)        "
rm -rf $FUSE_MNT/*
LF="$LOG_DIR/files-remaining-after-rm-rf.out"

ls $FUSE_MNT > $LF
find $SSD_MNT \( ! -regex '.*/\..*' \) -type f >> $LF
find $S3_DIR \( ! -regex '.*/\..*' \) -type f >> $LF
nfiles=`wc -l $LF|cut -d" " -f1`
print_result $nfiles

# test cleanup
rm -rf $REFERENCE_DIR
rm -rf $LOG_DIR
exit 0
