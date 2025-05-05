/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Read, write requests statistics
 */

#ifndef DMP_STAT_H
#define DMP_STAT_H

#include <linux/spinlock.h>

#define wrap_in_lock(ops)		\
({					\
	spin_lock(&stats->stat_lock);	\
	ops;				\
	spin_unlock(&stats->stat_lock);	\
})

struct dmp_stats {
	unsigned long long r_ops;
	unsigned long long w_ops;
	unsigned long long r_size_sum;
	unsigned long long w_size_sum;
	spinlock_t stat_lock; /* Spinlock for operations with dmp_stats */
};

static inline unsigned long long get_total_ops(struct dmp_stats *stats)
{
	unsigned long long rw_ops;

	wrap_in_lock(rw_ops = stats->r_ops + stats->w_ops);
	return rw_ops;
}

static inline unsigned long long get_total_avg_size(struct dmp_stats *stats)
{
	unsigned long long total_ops;
	unsigned long long r_size_sum;
	unsigned long long w_size_sum;

	wrap_in_lock({
		total_ops = stats->r_ops + stats->w_ops;
		r_size_sum = stats->r_size_sum;
		w_size_sum = stats->w_size_sum;
	});

	return total_ops ? (r_size_sum + w_size_sum) / total_ops : 0;
}

static inline unsigned long long get_read_ops(struct dmp_stats *stats)
{
	unsigned long long r_ops;

	wrap_in_lock(r_ops = stats->r_ops);
	return r_ops;
}

static inline unsigned long long get_read_avg_size(struct dmp_stats *stats)
{
	unsigned long long r_ops;
	unsigned long long r_size_sum;

	wrap_in_lock({
		r_ops = stats->r_ops;
		r_size_sum = stats->r_size_sum;
	});

	return stats->r_ops ? stats->r_size_sum / stats->r_ops : 0;
}

static inline unsigned long long get_write_ops(struct dmp_stats *stats)
{
	unsigned long long r_ops;

	wrap_in_lock(r_ops = stats->w_ops);
	return r_ops;
}

static inline unsigned long long get_write_avg_size(struct dmp_stats *stats)
{
	unsigned long long w_ops;
	unsigned long long w_size_sum;

	wrap_in_lock({
		w_ops = stats->w_ops;
		w_size_sum = stats->w_size_sum;
	});

	return stats->w_ops ? stats->w_size_sum / stats->w_ops : 0;
}

#endif
