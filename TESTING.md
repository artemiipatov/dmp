# Testing the dmp Device Mapper
This document describes the automated test scripts available for verifying the functionality and data integrity of the `dmp` Device Mapper kernel module.

# Test Script 1: `test_dd.sh`
Purpose: Perform a basic data integrity test using `dd` utility. This test verifies that data is correctly passed through the `dmp` device to the underlying storage.

- Workload:
  - Generates 16MB of random data and saves it to data.in.
  - Writes this 16MB of random data to the /dev/mapper/test-dmp device. Since dmp is a pass-through, this data should end up on the underlying loopback device.
  - Reads 16MB of data directly from the underlying loopback device (/dev/loopX) into data.out.
  - Uses dd with oflag=direct to ensure direct I/O, bypassing page cache effects in the test itself.
- Verification:
  - Compares the contents of data.in and data.out files.
  - Reads and prints the content of `/sys/module/dmp/dev_stats/volumes` using `cat`. This allows you to visually inspect the total read/write requests and sizes recorded by the `dmp` module during the write phase of the test.

How to run:
```bash
sudo bash test_dd.sh
```

# Test Script 2: `test_fio.sh`
Purpose: Perform a data integrity test under a significant concurrent random I/O workload using `fio` and its verification features.
- Workload:
  - Configured with rw=randwrite and rw=randread using `fio` verification features (verify=sha256, verify_state_save, verify_state_load, verify_only). The test writes data with verification state saved, then reads specifically to verify against the saved state
  - Utilizes bssplit=4k/30:8k/30:16k/30:512k/10 to generate a mix of random I/O sizes (30% 4KB, 30% 8KB, 30% 16KB, 10% 512KB).
  - Sets iodepth=8 and numjobs=8. This creates a high concurrency workload.
  - Uses --direct=1 for direct I/O.
  - Attempts to write/read 128MB per job, totaling 1GB of I/O.
- Verification: `fio` performs the SHA256 checksum verification internally during the randread job.

How to run:
```bash
sudo bash test_fio.sh
```

## Local Run
System configuration:
- Operating System: Ubuntu 24.04.2 LTS
- CPU: AMD Ryzen 5 5600H
- RAM: 16GB
- `fio` Version: 3.36

The output of the local run shows that `fio` successfully verified the `dmp` module:
```bash
ls: (g=0): rw=randwrite, bs=(R) 4096B-512KiB, (W) 4096B-512KiB, (T) 4096B-512KiB, ioengine=libaio, iodepth=8
...
fio-3.36
Starting 8 processes
Jobs: 8 (f=8): [w(8)][100.0%][w=86.7MiB/s][w=10.7k IOPS][eta 00m:00s]
ls: (groupid=0, jobs=8): err= 0: pid=132656: Tue May  6 01:46:13 2025
  write: IOPS=28.9k, BW=367MiB/s (385MB/s)(8192MiB/22307msec); 0 zone resets
    slat (usec): min=16, max=4061, avg=68.67, stdev=150.04
    clat (usec): min=14, max=1482.0k, avg=2134.88, stdev=20291.47
     lat (usec): min=36, max=1482.0k, avg=2203.55, stdev=20293.96
    clat percentiles (usec):
     |  1.00th=[   219],  5.00th=[   281], 10.00th=[   318], 20.00th=[   367],
     | 30.00th=[   412], 40.00th=[   457], 50.00th=[   515], 60.00th=[   603],
     | 70.00th=[   766], 80.00th=[  1369], 90.00th=[  5735], 95.00th=[  7635],
     | 99.00th=[ 10028], 99.50th=[ 11863], 99.90th=[308282], 99.95th=[425722],
     | 99.99th=[943719]
   bw (  KiB/s): min=  432, max=1459164, per=100.00%, avg=378139.15, stdev=63895.96, samples=344
   iops        : min=   38, max=138722, avg=28504.37, stdev=4818.09, samples=344
  lat (usec)   : 20=0.01%, 50=0.03%, 100=0.08%, 250=2.24%, 500=45.61%
  lat (usec)   : 750=21.22%, 1000=7.01%
  lat (msec)   : 2=6.41%, 4=4.36%, 10=11.98%, 20=0.82%, 50=0.07%
  lat (msec)   : 100=0.03%, 250=0.01%, 500=0.09%, 750=0.01%, 1000=0.01%
  lat (msec)   : 2000=0.01%
  cpu          : usr=22.95%, sys=4.61%, ctx=89634, majf=0, minf=96
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=100.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.1%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,645155,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=8

Run status group 0 (all jobs):
  WRITE: bw=367MiB/s (385MB/s), 367MiB/s-367MiB/s (385MB/s-385MB/s), io=8192MiB (8590MB), run=22307-22307msec
ls: (g=0): rw=randread, bs=(R) 4096B-512KiB, (W) 4096B-512KiB, (T) 4096B-512KiB, ioengine=libaio, iodepth=8
...
fio-3.36
Starting 8 processes
Jobs: 8 (f=8): [V(8)][100.0%][r=1455MiB/s][r=171k IOPS][eta 00m:00s]
ls: (groupid=0, jobs=8): err= 0: pid=132828: Tue May  6 01:46:19 2025
  read: IOPS=124k, BW=1568MiB/s (1645MB/s)(8192MiB/5223msec)
    slat (usec): min=5, max=810, avg= 8.99, stdev= 4.13
    clat (usec): min=14, max=10589, avg=450.65, stdev=421.14
     lat (usec): min=22, max=10597, avg=459.64, stdev=421.60
    clat percentiles (usec):
     |  1.00th=[  188],  5.00th=[  219], 10.00th=[  237], 20.00th=[  265],
     | 30.00th=[  293], 40.00th=[  318], 50.00th=[  347], 60.00th=[  379],
     | 70.00th=[  416], 80.00th=[  482], 90.00th=[  668], 95.00th=[ 1020],
     | 99.00th=[ 2573], 99.50th=[ 3032], 99.90th=[ 4686], 99.95th=[ 5276],
     | 99.99th=[ 6915]
   bw (  MiB/s): min= 1410, max= 1829, per=100.00%, avg=1589.01, stdev=16.76, samples=80
   iops        : min=40805, max=176772, avg=122744.50, stdev=5645.59, samples=80
  lat (usec)   : 20=0.01%, 50=0.01%, 100=0.05%, 250=14.02%, 500=67.78%
  lat (usec)   : 750=10.06%, 1000=2.93%
  lat (msec)   : 2=3.42%, 4=1.54%, 10=0.19%, 20=0.01%
  cpu          : usr=79.81%, sys=19.07%, ctx=8806, majf=0, minf=8295
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=100.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.1%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=645155,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=8

Run status group 0 (all jobs):
   READ: bw=1568MiB/s (1645MB/s), 1568MiB/s-1568MiB/s (1645MB/s-1645MB/s), io=8192MiB (8590MB), run=5223-5223msec
```
