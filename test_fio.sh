#!bin/sh

# Build dmp module
make

# Install dmp module
insmod dmp.ko

LOOP_FILE="loopfile.img"
MAPPER_DEVICE="/dev/mapper/test-dmp"
DMP_DEVICE_NAME="test-dmp"

# Create loop device
dd if=/dev/zero of=$LOOP_FILE bs=1G count=1
losetup -fP $LOOP_FILE

# Create dmp device mapper over loop device
LOOP_DEV=$(losetup -a | grep $LOOP_FILE | awk -F: '{print $1}')
SIZE=$(blockdev --getsz $LOOP_DEV)
TABLE="0 $SIZE dmp $LOOP_DEV"
dmsetup create $DMP_DEVICE_NAME --table "$TABLE"

# Verify using fio: 1 GB of concurrent random writes with different block sizes.
# Note: using --direct=1 everywhere
fio --name=ls --ioengine=libaio --iodepth=8 --rw=randwrite \
--bssplit=4k/30:8k/30:16k/30:512k/10 --direct=1 --numjobs=8 \
--size=1G --filename=$MAPPER_DEVICE --verify=sha256 --do_verify=0 \
--verify_fatal=0 --verify_only=0 --verify_state_save=1 --group_reporting

fio --name=ls --ioengine=libaio --iodepth=8 --rw=randread \
--bssplit=4k/30:8k/30:16k/30:512k/10 --direct=1 --numjobs=8 \
--size=1G --filename=$MAPPER_DEVICE --verify=sha256 --do_verify=1 \
--verify_fatal=0 --verify_only=1 --verify_state_load=1 --group_reporting

# Remove dmp
dmsetup remove test-dmp
# Remove loop device
losetup -d $LOOP_DEV
# Remove auxiliary files
rm $LOOP_FILE

# Remove dmp module
rmmod dmp

exit 0
