#!bin/sh

# Build dmp module
make

# Install dmp module
insmod dmp.ko

INPUT_DATA="data.in"
OUTPUT_DATA="data.out"
LOOP_FILE="loopfile.img"
MAPPER_DEVICE="/dev/mapper/test-dmp"
DMP_DEVICE_NAME="test-dmp"

# Create loop device
dd if=/dev/zero of=$LOOP_FILE bs=1M count=16
losetup -fP $LOOP_FILE

# Create dmp device mapper over loop device
LOOP_DEV=$(losetup -a | grep $LOOP_FILE | awk -F: '{print $1}')
SIZE=$(blockdev --getsz $LOOP_DEV)
TABLE="0 $SIZE dmp $LOOP_DEV"
dmsetup create $DMP_DEVICE_NAME --table "$TABLE"

# Test case: write 16MB of random data to device mapper,
# read from the underlying storage, and compare.
# Note: using oflag=direct everywhere.
dd if=/dev/urandom of=$INPUT_DATA bs=1M count=16 conv=notrunc oflag=direct
dd if=$INPUT_DATA of=$MAPPER_DEVICE bs=1M count=16 conv=notrunc oflag=direct
dd if=$LOOP_FILE of=$OUTPUT_DATA bs=1M count=16 conv=notrunc oflag=direct

cat /sys/module/dmp/stat/volumes

if cmp -s "$INPUT_DATA" "$OUTPUT_DATA"; then
        printf 'PASSED: data was written successfully to the underlying device\n'
else
        printf 'FAILED: data was NOT writte successfully to the underlying device\n'
fi

# Remove dmp
dmsetup remove test-dmp
# Remove loop device
losetup -d $LOOP_DEV
# Remove auxiliary files
rm $LOOP_FILE $INPUT_DATA $OUTPUT_DATA

# Remove dmp module
rmmod dmp

exit 0
