# dmp: Device Mapper Proxy

`dmp` is a Device Mapper target module that acts as a simple proxy to an underlying block device. Its primary purpose is to intercept I/O requests (reads and writes) directed at the DM device, collect statistics on these requests, and then pass them through unmodified to the underlying device. The collected statistics are exposed via the sysfs.

# Building and Loading
Build the module:
```bash
make
```

Load the module into the kernel:
```bash
sudo insmod dmp.ko
```

# Description
The `dmp` device mapper target takes one argument: the path to the underlying block device. It can be created using `dmsetup`:
```bash
dmsetup create $DMP_DEV_NAME --table "0 $SEC_COUNT dmp $BLOCK_DEV"
```

## sysfs interface
Statistics are exposed under `/sys/module/dmp/stat/`.
- Global Statistics:
  - Path: /sys/module/dmp/stat/volumes
  - Content: Aggregated read/write request counts and average sizes for all active dmp devices.
  - Access: Read-only.
- Per-Device Statistics:
  - Path: /sys/module/dmp/stat/<dm_device_name>/reqs (where <dm_device_name> is the name given to the dm device, e.g., dm-0).
  - Content: Read/write request counts and average sizes for that specific dmp device instance.
  - Access: Read-only.

The output format for reading these files is:
```bash
read:
 reqs: <read_request_count>
 avg size: <average_read_size>
write:
 reqs: <write_request_count>
avg size: <average_write_size>
total:
 reqs: <total_request_count>
 avg size: <average_total_size>
```

# Example of usage
Create a dmp device on top of the existing block device:
```bash
dmsetup create test-dmp --table "0 $SEC_COUNT dmp $BLOCK_DEV"
```
I/O to this device will update statistics.
Write data to the new dmp device:
```bash
dd if=/dev/urandom of=dev/mapper/test-dmp bs=1M count=16
```
Read the statistics:
```bash
cat /sys/module/dmp/stat/test-dmp/reqs
cat /sys/module/dmp/stat/volumes
```
