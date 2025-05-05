#!bin/sh

# Build dmp module
make

# Install dmp module
insmod dmp.ko

# Create loop device
dd if=/dev/zero of=test_img.img bs=4k count=1
losetup -fP test_img.img

# Create dmp device mapper over loop device
LOOP_DEV=$(losetup -a | grep test_img.img | awk -F: '{print $1}')
SIZE=$(blockdev --getsz $LOOP_DEV)
TABLE="0 $SIZE dmp $LOOP_DEV"
dmsetup create test-dmp --table "$TABLE"

MAPPER_DEVICE="/dev/mapper/test-dmp"
SYSFS_ENTRY="/sys/module/dmp/dev_stats/dm-0/reqs"

# Write "Hello, World!" to dmp
TEST_STRING="Hello, World!"
TOTAL_BYTES=$((${#TEST_STRING} + 1))
echo "Write:"
echo "$TEST_STRING" | dd of=$MAPPER_DEVICE bs=1 count=$TOTAL_BYTES conv=notrunc

# Read requests statistics from sysfs
cat $SYSFS_ENTRY

# Read from dmp
echo "Read:"
dd if=$MAPPER_DEVICE bs=1 count=$TOTAL_BYTES status=none | cat

# Read requests statistics from sysfs
cat $SYSFS_ENTRY

# # Remove dmp
dmsetup remove test-dmp
# Remove loop device
losetup -d $LOOP_DEV
rm test_img.img

# Remove dmp module
rmmod dmp

exit 0
