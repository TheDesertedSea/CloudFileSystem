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
THRESHOLD="4"
AVGSEGSIZE="4"
AVGSEGSIZE="4"
MINSEGSIZE="1"
MAXSEGSIZE="8"
TEST_FILE="largefile"
MODIFIED_FILE="largefile.modified"
FILE_SIZE=$(wc -c < $TEST_DIR/$TEST_FILE)
CLOUD_USAGE="cloud_usage"

source $SCRIPTS_DIR/functions.sh
NODEDUP=0
#
# Execute battery of test cases.
# expects that the test files are in $FUSE_MNT_
# and the reference files are in $REFERENCE_DIR
# Creates the intermediate results in $LOG_DIR
#

process_args cloudfs --ssd-path $SSD_MNT_ --fuse-path $FUSE_MNT_ --threshold $THRESHOLD --avg-seg-size $AVGSEGSIZE --min-seg-size $MINSEGSIZE --max-seg-size $MAXSEGSIZE

# test setup
rm -rf $REFERENCE_DIR
mkdir -p $REFERENCE_DIR
mkdir -p $LOG_DIR

reinit_env

#----
# Testcases assumes that test data does not have any hidden files(.* files)
# Students should have all their metadata in hidden files/dirs
echo ""
echo "Executing test_2_16"
echo -e "Running cloudfs in dedup mode\n"

collect_stats > $STAT_FILE
echo -e "\nCloud statistics -->"
echo "Current usage in cloud : $(get_cloud_current_usage $STAT_FILE)"

#Copy the test file into the fuse folder and the reference folder
echo -e "Copying test file into the fuse folder..."
# cp  $TEST_DIR/$TEST_FILE $FUSE_MNT/$TEST_FILE
# cp  $TEST_DIR/$TEST_FILE $REFERENCE_DIR/$TEST_FILE
cp $TEST_DIR/file_a $FUSE_MNT/file_a
cp $TEST_DIR/file_b $FUSE_MNT/file_b
cp $TEST_DIR/file_a $REFERENCE_DIR/file_a
cp $TEST_DIR/file_b $REFERENCE_DIR/file_b


sleep 4

collect_stats > $STAT_FILE
echo -e "\nCloud statistics -->"
echo "Current usage in cloud : $(get_cloud_current_usage $STAT_FILE)"

# PWDSAVE=$PWD
# cd $REFERENCE_DIR && find $TEST_FILE  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out.master
# cd $FUSE_MNT_ && find $TEST_FILE  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out
# cd $PWDSAVE
PWDSAVE=$PWD
cd $REFERENCE_DIR && find file_a  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out.master
cd $FUSE_MNT_ && find file_a  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out
cd $PWDSAVE

# diff $LOG_DIR/md5sum.out.master $LOG_DIR/md5sum.out
# print_result $?
diff $LOG_DIR/md5sum.out.master $LOG_DIR/md5sum.out
print_result $?

echo -ne "Original File: Basic file content test(md5sum) for file_b "
PWDSAVE=$PWD
cd $REFERENCE_DIR && find file_b  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out.master
cd $FUSE_MNT_ && find file_b  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out
cd $PWDSAVE

diff $LOG_DIR/md5sum.out.master $LOG_DIR/md5sum.out
print_result $?

# file_a has 4608 bytes, file_b has 4608 bytes, overwrite file_a with file_b 18 rounds, 256 bytes each round
echo -ne "Overwrite file_a with file_b 18 rounds   "
for ((i=0; i<18; i++)); do
    # 计算偏移量
    offset=$((i * 256))
    # 使用 dd 复制并覆写 file_a 的偏移量处
    dd if=$FUSE_MNT_/file_b of=$FUSE_MNT_/file_a bs=256 count=1 seek=$i conv=notrunc
    dd if=$REFERENCE_DIR/file_b of=$REFERENCE_DIR/file_a bs=256 count=1 seek=$i conv=notrunc
done

echo -ne "Original File: Basic file content test(md5sum) file_b(after) "
PWDSAVE=$PWD
cd $REFERENCE_DIR && find file_b  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out.master
cd $FUSE_MNT_ && find file_b  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out
cd $PWDSAVE

diff $LOG_DIR/md5sum.out.master $LOG_DIR/md5sum.out
print_result $?

echo -ne "Overwrited file_a: Basic file content test(md5sum), compare overwirited file_a"
PWDSAVE=$PWD
cd $REFERENCE_DIR && find file_a  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out.master
cd $FUSE_MNT_ && find file_a  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOG_DIR/md5sum.out
cd $PWDSAVE

diff $LOG_DIR/md5sum.out.master $LOG_DIR/md5sum.out
print_result $?

collect_stats > $STAT_FILE
echo -e "\nCloud statistics -->"
echo "Current usage in cloud : $(get_cloud_current_usage $STAT_FILE)"

ls -al $S3_DIR/cloudfs

#----
#destructive test : always do this test at the end!!
echo -ne "\nFile removal test (rm -rf)        "
rm -rf $FUSE_MNT/*
LF="$LOG_DIR/files-remaining-after-rm-rf.out"

ls $FUSE_MNT_ > $LF
find $SSD_MNT_ \( ! -regex '.*/\..*' \) -type f >> $LF
find $S3_DIR \( ! -regex '.*/\..*' \) -type f >> $LF
nfiles=`wc -l $LF|cut -d" " -f1`
print_result $nfiles

# test cleanup
rm -rf $REFERENCE_DIR
rm -rf $LOG_DIR
exit 0
