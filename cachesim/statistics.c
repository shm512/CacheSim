/* -*- mode:c; coding: utf-8 -*- */

#include "statistics.h"

#include <stdlib.h>

void
statistics_add_counter(StatisticsInfo *info, int clock_counter)
{
    info->clock_counter += clock_counter;
}

void
statistics_add_hit_counter(StatisticsInfo *info)
{
    info->hit_counter++;
}

void
statistics_add_read(StatisticsInfo *info)
{
    info->read_counter++;
}

void
statistics_add_write(StatisticsInfo *info)
{
    info->write_counter++;
}

void
statistics_add_write_back_counter(StatisticsInfo *info)
{
    info->write_back_counter++;
}

StatisticsInfo *
statistics_create(ConfigFile *cfg)
{
    StatisticsInfo *st = calloc(1, sizeof(*st));
    return st;
}

StatisticsInfo *
statistics_free(StatisticsInfo *info)
{
    if (info) {
        free(info);
    }
    return NULL;
}

void
statistics_print(StatisticsInfo *info, FILE *out_f)
{
    if (!info || !out_f) {
        return;
    }
    fprintf(out_f, "clock count: %d\n", info->clock_counter);
    fprintf(out_f, "reads: %d\n", info->read_counter);
    fprintf(out_f, "writes: %d\n", info->write_counter);
    if (info->hit_counter_needed) {
        fprintf(out_f, "read hits: %d\n", info->hit_counter);
    }
    if (info->write_back_needed) {
        fprintf(out_f, "cache block writes: %d\n", info->write_back_counter);
    }
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
