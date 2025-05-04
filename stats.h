/*
* Read-write operations statistics
*/

#ifndef DMP_STAT_H
#define DMP_STAT_H

struct dmp_stats {
        unsigned long long r_ops;
        unsigned long long w_ops;
        unsigned long long r_size_sum;
        unsigned long long w_size_sum;
};

static inline unsigned long long get_total_ops(struct dmp_stats *stat) {
        return stat->r_ops + stat->w_ops;
}

static inline unsigned long long get_total_avg_size(struct dmp_stats *stat) {
        unsigned long long total_ops = stat->r_ops + stat->w_ops;
        return total_ops ? (stat->r_size_sum + stat->w_size_sum) / total_ops : 0;
}

static inline unsigned long long get_read_ops(struct dmp_stats *stat) {
        return stat->r_ops;
}

static inline unsigned long long get_read_avg_size(struct dmp_stats *stat) {
        return stat->r_ops ? stat->r_size_sum / stat->r_ops : 0;
}

static inline unsigned long long get_write_ops(struct dmp_stats *stat) {
        return stat->w_ops;
}

static inline unsigned long long get_write_avg_size(struct dmp_stats *stat) {
        return stat->w_ops ? stat->w_size_sum / stat->w_ops : 0;
}

#endif