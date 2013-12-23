/* -*- mode:c; coding: utf-8 -*- */

#include "random.h"
#include "common.h"

#include <stdlib.h>
#include <time.h>

Random *
random_free(Random *rnd)
{
    if (rnd) {
        free(rnd);
    }
    return NULL;
}

int
random_next(Random *rnd, int n)
{
    return (int) (rand() / (RAND_MAX + 1.0) * n);
}

static RandomOps random_ops =
{
    random_free,
    random_next,
};

Random *
random_create(ConfigFile *cfg)
{
    char buf[PARAM_BUF_SIZE];
    Random *rnd = (Random*) calloc(1, sizeof(*rnd));
    rnd->ops = &random_ops;

    int r = config_file_get_int(cfg,
		make_param_name(buf, PARAM_BUF_SIZE, NULL, "seed"),
		&rnd->seed);
    if (!r) {
        rnd->seed = time(NULL);
    } else if (r < 0 || rnd->seed <= 0) {
        error_invalid("random_create", buf);
        free(rnd); rnd = NULL;
    }
    
    srand(rnd->seed);
    return rnd;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
