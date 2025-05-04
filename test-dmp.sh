#!bin/sh

make
insmod dmp.ko

dd if=/dev/zero of=test_backing.img bs=4k count=1
losetup -fP test_backing.img

LOOP_DEV=$(losetup -a | grep test_backing.img | awk -F: '{print $1}')
SIZE=$(blockdev --getsz $LOOP_DEV)
TABLE="0 $SIZE dmp $LOOP_DEV"
dmsetup create test-dmp --table "$TABLE"

TEST_STRING="HELLO LINEAR DM!"
TOTAL_BYTES=$((${#TEST_STRING} + 1))
echo "Write:"
echo "$TEST_STRING" | sudo dd of=/dev/mapper/test-dmp bs=1 count=$TOTAL_BYTES conv=notrunc
cat /sys/module/dmp/dev_stats/dm-0/reqs

echo "Read:"
dd if=/dev/mapper/test-dmp bs=1 count=$TOTAL_BYTES status=none | cat
cat /sys/module/dmp/dev_stats/dm-0/reqs

dmsetup remove test-dmp
losetup -d $LOOP_DEV
rm test_backing.img
rmmod dmp
