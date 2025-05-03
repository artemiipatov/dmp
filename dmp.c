#include <linux/module.h>
#include <linux/init.h>
#include <linux/device-mapper.h>
#include <linux/bio.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/printk.h>

struct dmp_c {
        struct dm_dev *dev;
        unsigned long long r_ops;
        unsigned long long w_ops;
        unsigned long long rw_ops;
        unsigned long long r_size_sum;
        unsigned long long w_size_sum;
        unsigned long long rw_size_sum;
        struct kobject kobj;
        spinlock_t stat_lock;
};

static unsigned long long get_avg(unsigned long long size, unsigned long long ops) {
        return ops ? size / ops : 0;
}

static ssize_t volumes_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        struct dmp_c *dmpc = container_of(kobj, struct dmp_c, kobj);

        return sprintf(buf,
                  "read:\n reqs: %llu\n avg size: %llu\nwrite:\n reqs: %llu\n avg size: %llu\ntotal:\n reqs: %llu\n avg size: %llu\n",
                  dmpc->r_ops,
                  get_avg(dmpc->r_size_sum, dmpc->r_ops),
                  dmpc->w_ops,
                  get_avg(dmpc->w_size_sum, dmpc->w_ops),
                  dmpc->rw_ops,
                  get_avg(dmpc->rw_size_sum, dmpc->rw_ops));
}

static void dmp_kobj_release(struct kobject *kobj) {
        kfree(kobj);
}

static struct kobj_attribute volumes_attr = __ATTR_RO(volumes);

/*
 * /sys/module/dmp/stat/volumes
 */
static struct attribute *dmp_attrs[] = {
        &volumes_attr.attr,
        NULL,
};

/*
 * /sys/module/dmp/stat/
 */
static struct attribute_group dmp_stat_group = {
        .name = "stat",
        .attrs = dmp_attrs,
};

static struct kobj_type dmp_kobj_type = {
        .release = dmp_kobj_release,
        .sysfs_ops = &kobj_sysfs_ops,
};

/*
 * Construct a device mapper proxy that collects statistics of I/O operations: <dev_path>
 */
static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
        struct dmp_c *dmpc;
        int ret;

        if (argc != 1) {
                ti->error = "Invalid argument count";
                return -EINVAL;
        }

        dmpc = kzalloc(sizeof(*dmpc), GFP_KERNEL);
        if (dmpc == NULL) {
                ti->error = "Cannot allocate dmp context";
                return -ENOMEM;
        }

        ret = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &dmpc->dev);
	if (ret) {
                kfree(dmpc);
		ti->error = "Device lookup failed";
                return ret;
	}

        ret = kobject_init_and_add(&dmpc->kobj, &dmp_kobj_type, &THIS_MODULE->mkobj.kobj, "%s", dm_table_device_name(ti->table));
                if (ret) {
	        dm_put_device(ti, dmpc->dev);
                kfree(dmpc);
		ti->error = "Cannot init kobject";
                return ret;
        }

        ret = sysfs_create_group(&dmpc->kobj, &dmp_stat_group);
        if (ret) {
                kobject_put(&dmpc->kobj);
                dm_put_device(ti, dmpc->dev);
                kfree(dmpc);
		ti->error = "Cannot create sysfs group";
                return ret;
        }

        spin_lock_init(&dmpc->stat_lock);

	ti->num_flush_bios = 1;
	ti->num_discard_bios = 1;
	ti->num_secure_erase_bios = 1;
	ti->num_write_zeroes_bios = 1;
	ti->private = dmpc;
	return 0;
}

static void dmp_dtr(struct dm_target *ti)
{
	struct dmp_c *dmpc = ti->private;
        sysfs_remove_group(&dmpc->kobj, &dmp_stat_group);
        kobject_put(&dmpc->kobj);
	dm_put_device(ti, dmpc->dev);
	kfree(dmpc);
}

/*
* Return zeros only on reads
*/
static int dmp_map(struct dm_target *ti, struct bio *bio)
{
	struct dmp_c *dmpc = ti->private;
        unsigned int cur_size = bio->bi_iter.bi_size;
        unsigned int cur_sec = bio_sectors(bio);

        /*
         * Update statistics
         */
        switch (bio_op(bio)) {
	case REQ_OP_READ:
                spin_lock(&dmpc->stat_lock);
                dmpc->r_size_sum += cur_size;
                dmpc->rw_size_sum += cur_size;
		dmpc->r_ops++;
                dmpc->rw_ops++;
                spin_unlock(&dmpc->stat_lock);
		break;
	case REQ_OP_WRITE:
                spin_lock(&dmpc->stat_lock);
                dmpc->w_size_sum += cur_size;
                dmpc->rw_size_sum += cur_size;
                dmpc->w_ops++;
                dmpc->rw_ops++;
                spin_unlock(&dmpc->stat_lock);
                break;
        }

	bio_set_dev(bio, dmpc->dev->bdev);
        return DM_MAPIO_REMAPPED;
}

static void dmp_status(struct dm_target *ti, status_type_t type,
			  unsigned int status_flags, char *result, unsigned int maxlen)
{
	struct dmp_c *dmpc = ti->private;
	size_t sz = 0;

	switch (type) {
	case STATUSTYPE_INFO:
		result[0] = '\0';
		break;

	case STATUSTYPE_TABLE:
		DMEMIT("%s", dmpc->dev->name);
		break;
	}
}

static struct target_type dmp_target = {
	.name   = "dmp",
	.version = {1, 0, 0},
	.features = DM_TARGET_PASSES_INTEGRITY,
	.module = THIS_MODULE,
	.ctr    = dmp_ctr,
	.dtr    = dmp_dtr,
	.map    = dmp_map,
	.status = dmp_status,
};

module_dm(dmp);
MODULE_AUTHOR("Artemii Patov <patov.988@gmail.com>");
MODULE_DESCRIPTION(DM_NAME "Device mapper proxy that collects statistics of I/O operations");
MODULE_LICENSE("GPL");
