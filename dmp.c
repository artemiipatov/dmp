// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device-mapper.h>
#include <linux/bio.h>

#include "stats.h"

#define dm_name_from_target(ti) \
(dm_disk(dm_table_get_md(ti->table))->disk_name)

/*
 * Each dmp device context
 */
struct dmp_c {
	struct dm_dev *dev; /* Underlying block device */
	struct dmp_stats stats; /* Stats of requests to underlying device */
	struct kobject kobj; /* Parent kobject for current dmp */
};

/*
 * Global statistics covering all devices
 */
static struct dmp_stats *global_stats;

/*
 * Shows statistics in sysfs
 */
static ssize_t print_stats(char *buf, struct dmp_stats *stats)
{
	return sysfs_emit(buf,
		"read:\n reqs: %llu\n avg size: %llu\nwrite:\n reqs: %llu\navg size: %llu\ntotal:\n reqs: %llu\n avg size: %llu\n",
		get_read_ops(stats),
		get_read_avg_size(stats),
		get_write_ops(stats),
		get_write_avg_size(stats),
		get_total_ops(stats),
		get_total_avg_size(stats));
}

/*
 * /sys/module/dmp/stat/volumes
 */
static ssize_t volumes_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	return print_stats(buf, global_stats);
}

static struct kobj_attribute volumes_attr = __ATTR_RO(volumes);

/*
 * /sys/module/dmp/stat/dm-<X>/reqs
 */
static ssize_t reqs_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	struct dmp_c *dmpc = container_of(kobj, struct dmp_c, kobj);

	return print_stats(buf, &dmpc->stats);
}

static struct kobj_attribute reqs_attr = __ATTR_RO(reqs);

static void dmp_kobj_release(struct kobject *kobj)
{
	/* kobject is released in dmp_dtr */
}

static struct kobj_type dmp_kobj_type = {
	.release = dmp_kobj_release,
	.sysfs_ops = &kobj_sysfs_ops,
};

static struct kset *dmp_dev_stats_kset;

/*
 * Construct a device mapper proxy
 * that collects statistics of read, write operations.
 * Statistics can be found in
 * /sys/module/dmp/dev_stats/dm-<X>/reqs for each device,
 * /sys/module/dmp/stat/volumes for all devices
 */
static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	int ret;
	struct dmp_c *dmpc;
	char *name;

	if (argc != 1) {
		ti->error = "Invalid argument count";
		return -EINVAL;
	}

	if (!dmp_dev_stats_kset) {
		ti->error = "Internal error (sysfs base missing)";
		return -EFAULT;
	}

	dmpc = kzalloc(sizeof(*dmpc), GFP_KERNEL);
	if (dmpc == NULL) {
		ti->error = "Failed to allocate dmp context";
		return -ENOMEM;
	}

	ret = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table),
			    &dmpc->dev);
	if (ret) {
		ti->error = "Failed to get the underlying device";
		goto dev_err;
	}

	name = dm_name_from_target(ti);
	ret = kobject_init_and_add(&dmpc->kobj, &dmp_kobj_type,
				   &dmp_dev_stats_kset->kobj, name);
	if (ret) {
		ti->error = "Failed to init kobject";
		goto kobj_err;
	}

	ret = sysfs_create_file(&dmpc->kobj, &reqs_attr.attr);
	if (ret) {
		ti->error = "Failed to create sysfs file";
		goto sysfs_err;
	}

	spin_lock_init(&dmpc->stats.stat_lock);

	ti->num_flush_bios = 1;
	ti->num_discard_bios = 1;
	ti->num_secure_erase_bios = 1;
	ti->num_write_zeroes_bios = 1;
	ti->private = dmpc;
	return 0;

sysfs_err:
	kobject_put(&dmpc->kobj);
kobj_err:
	dm_put_device(ti, dmpc->dev);
dev_err:
	kfree(dmpc);
	return ret;
}

static void dmp_dtr(struct dm_target *ti)
{
	struct dmp_c *dmpc = ti->private;

	sysfs_remove_file(&dmpc->kobj, &reqs_attr.attr);
	kobject_put(&dmpc->kobj);
	dm_put_device(ti, dmpc->dev);
	kfree(dmpc);
}

static void update_stats(struct dmp_stats *stats, struct bio *bio)
{
	unsigned int cur_size = bio->bi_iter.bi_size;

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		spin_lock(&stats->stat_lock);
		stats->r_size_sum += cur_size;
		stats->r_ops++;
		spin_unlock(&stats->stat_lock);
		break;
	case REQ_OP_WRITE:
		spin_lock(&stats->stat_lock);
		stats->w_size_sum += cur_size;
		stats->w_ops++;
		spin_unlock(&stats->stat_lock);
		break;
	default:
		break;
	}
}

/*
 * Identically remaps all requests to underlying device.
 * Collects read, write requests statistics
 */
static int dmp_map(struct dm_target *ti, struct bio *bio)
{
	struct dmp_c *dmpc = ti->private;

	update_stats(&dmpc->stats, bio);
	update_stats(global_stats, bio);
	bio_set_dev(bio, dmpc->dev->bdev);

	return DM_MAPIO_REMAPPED;
}

static void dmp_status(struct dm_target *ti, status_type_t type,
		       unsigned int status_flags, char *result,
		       unsigned int maxlen)
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
	default:
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

static int __init dmp_init(void)
{
	int ret;

	ret = dm_register_target(&dmp_target);
	if (ret < 0)
		return ret;

	/* Initialize kobject for all devices statistics */
	dmp_dev_stats_kset = kset_create_and_add("dev_stats", NULL,
						 &THIS_MODULE->mkobj.kobj);
	if (!dmp_dev_stats_kset) {
		ret = -ENOMEM;
		goto kset_err;
	}

	global_stats = kzalloc(sizeof(*global_stats), GFP_KERNEL);
	if (!global_stats) {
		ret = -ENOMEM;
		goto global_stats_err;
	}

	ret = sysfs_create_file(&dmp_dev_stats_kset->kobj, &volumes_attr.attr);
	if (ret)
		goto sysfs_err;

	return ret;

sysfs_err:
	kfree(global_stats);
global_stats_err:
	kset_unregister(dmp_dev_stats_kset);
kset_err:
	dm_unregister_target(&dmp_target);
	return ret;
}

static void __exit dmp_exit(void)
{
	sysfs_remove_file(&dmp_dev_stats_kset->kobj, &volumes_attr.attr);
	kfree(global_stats);
	kset_unregister(dmp_dev_stats_kset);
	dm_unregister_target(&dmp_target);
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_AUTHOR("Artemii Patov <patov.988@gmail.com>");
MODULE_DESCRIPTION(DM_NAME "Device mapper proxy");
MODULE_LICENSE("GPL");
