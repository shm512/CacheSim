/* -*- mode:c; coding: utf-8 -*- */

/*!
  \file cachesim.c
  \brief Главный файл симулятора кеша
 */

#include "abstract_memory.h"
#include "cache.h"
#include "common.h"
#include "direct_cache.h"
#include "full_cache.h"
#include "memory.h"
#include "parse_config.h"
#include "random.h"
#include "statistics.h"
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*!
  Выводим сообщения о некорректности аргументов и завершаем выполнение программы с кодом EXIT_FAILURE.
  Предполагается, что перед вызовом этой функции вся выделенная память уже освобождена.
 */
static inline void
die_bad_args(void)
{
    fprintf(stderr, "Invalid arguments\n");
    exit(EXIT_FAILURE);
}

/*!
  Выполняем операцию над памятью, указанную в шаге трассы
  \param ts Указатель на структуры описанию шага трассы
  \param m Указатель на структуру описания памяти
  \param info Указатель на структуру, хранящую статистику моделирования
 */
static void
operation_on_memory(
	TraceStep *ts,
	AbstractMemory *m,
	StatisticsInfo *info)
{
	if (!ts || !m || !info) {
		return;
	}
	if (ts->op == 'R') {
		statistics_add_read(info);
		m->ops->read(m, ts->addr, ts->size, NULL);
		m->ops->reveal(m, ts->addr, ts->size, ts->value);
	} else if (ts->op == 'W') {
		statistics_add_write(info);
		m->ops->write(m, ts->addr, ts->size, ts->value);
	} else {
		return;
	}
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        return 0;
    }
    char *fname = NULL;
    int print_config = 0, statistics = 0;
    int disable_cache = 0, dump_memory = 0;
    int fnames_count = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print-config") == 0) {
            print_config = 1;
        } else if (strcmp(argv[i], "--statistics") == 0) {
            statistics = 1;
        } else if (strcmp(argv[i], "--disable-cache") == 0) {
            disable_cache = 1;
        } else if (strcmp(argv[i], "--dump-memory") == 0) {
            dump_memory = 1;
        } else if (argv[i][0] == '-') {
            die_bad_args();
        } else {
            fname = argv[i];
            fnames_count++;
        }
    }
    if (fnames_count != 1) {
        die_bad_args();
    } 
    
    ConfigFile *cfg = config_file_parse(fname, stderr);
    if (!cfg) {
        fprintf(stderr, "%d\n", __LINE__);
        return EXIT_FAILURE;
    }
    
    if (print_config) {
        if (statistics || disable_cache || dump_memory || !fname) {
            die_bad_args();
        }
        config_file_print(cfg);
        config_file_free(cfg);
        return 0;
    }
    
    StatisticsInfo *info = statistics_create(cfg);
    AbstractMemory *mem = memory_create(cfg, NULL, info);
    Trace *t = trace_open(NULL, stderr);
    Random *rnd = NULL;
    int r, exit_code = 0;
    if (!mem || !t) {
		exit_code = EXIT_FAILURE;
        fprintf(stderr, "%d\n", __LINE__);
		goto finally;
	}
    if (!disable_cache) {
        info->hit_counter_needed = 1;
        rnd = random_create(cfg);
        mem = cache_create(cfg, NULL, info, mem, rnd);
        if (!rnd || !mem) {
            exit_code = EXIT_FAILURE;
            fprintf(stderr, "%d\n", __LINE__);
            goto finally;
        }
    }
    
    while ((r = trace_next(t))) {
		if (r == -1) {
			exit_code = EXIT_FAILURE;
			goto finally;
		}
		operation_on_memory(trace_get(t), mem, info);
	}
    mem->ops->flush(mem);
    
    if (dump_memory) {
		mem->ops->print_dump(mem, stdout);
	}
    
    if (statistics) {
		statistics_print(info, stdout);
	}
    
finally:
	trace_close(t);
    statistics_free(info);
    config_file_free(cfg);
    if (mem) {
		mem->ops->free(mem);
	}
    if (rnd) {
        rnd->ops->free(rnd);
    }
    return exit_code;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
