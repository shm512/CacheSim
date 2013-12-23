/* -*- mode:c; coding: utf-8 -*- */

#include "full_cache.h"

#include <stdlib.h>
#include <string.h>

/*!
  Ограничения значений параметров
 */
enum
{
    MAX_CACHE_SIZE = 16 * MiB, //!< Максимальный поддерживаемый размер кеша
    MAX_READ_TIME = 100000, //!< Максимальное время чтения из кеша в тактах
    MAX_WRITE_TIME = MAX_READ_TIME, //!< Максимальное время записи в кеш в тактах
};

/*!
  Описание одного блока полноассоциативного кеша
  \brief Блок полноассоциативного кеша
 */
typedef struct FullCacheBlock
{
    memaddr_t addr; //!< Адрес по которому в основной памяти (mem) находится этот блок, NO_BLOCK - если этот блок свободен
    MemoryCell *mem; //!< Ячейки памяти, хранящиеся в блоке кеша
    int dirty; //!< Флаг того, что блок содержит данные, не сброшенные в память, только для write back кеша
} FullCacheBlock;

enum
{
    NO_BLOCK = -1 //!< Значение поля addr в случае, если блок свободен
};

struct FullCache;
typedef struct FullCache FullCache;

/*!
  Дополнительные операции, настраивающие поведение полноассоциативного кеша
  \brief Доп. операции полноассоциативного кеша
 */
typedef struct FullCacheOps
{
    /*!
      Функция должна выполнить синхронизацию между блоком кеша и памятью. Для кеша с прямой
      записью функция не делает ничего, для кеша с отложенной записью функция записывает
      грязный блок в память.
      \param c Указатель на дескриптор кеша
      \param b Указатель на финализируемый блок
     */
    void (*finalize)(FullCache *c, FullCacheBlock *b);
    
    /*!
     Поместить блок ОЗУ в полноассоциативный кеш
     \param c Указатель на структуру описания модели полноассоциативного кеша
     \param aligned_addr Адрес начала искомого блока
     \return Указатель на соответствующий блок кеша
    */
    FullCacheBlock *(*cache_place)(FullCache *c, memaddr_t aligned_addr);
} FullCacheOps;

/*!
  Полноассоциативный кеш
  \brief Дескриптор полноассоциативного кеша
 */
struct FullCache
{
    AbstractMemory b; //!< Базовые поля
    FullCacheOps full_ops; //!< Дополнительные операции полноассоциативного кеша
    Random *rnd; //!< Элемент случайности
    FullCacheBlock *blocks; //!< Блоки кеша
    AbstractMemory *mem; //!< Нижележащая память
    int cache_size; //!< Размер кеша (считывается из конф. файла)
    int block_size; //!< Размер одного блока кеша (считывается из конф. файла)
    int block_count; //!< Количество блоков кеша
    int cache_read_time; //!< Время выполнения чтения из кеша (считывается из конф. файла)
    int cache_write_time; //!< Время выполнения записи в кеш (считывается из конф. файла)
};

/*!
  Освободить ресурсы
  \param m Указатель на структуру описания модели полноассоциативного кеша (в виде указателя на базовую структуру)
  \return Указатель NULL
 */
static AbstractMemory *
full_cache_free(AbstractMemory *m)
{
    if (m) {
        FullCache *c = (FullCache *) m;
        if (c->mem) {
            c->mem->ops->free(c->mem);
        }
        for (int i = 0; i < c->block_count; i++) {
            free(c->blocks[i].mem);
        }
        free(c->blocks);
        free(c);
    }
    return NULL;
}

/*!
  Искать блок ОЗУ в полноассоциативном кеше
  \param c Указатель на структуру описания модели полноассоциативного кеша
  \param aligned_addr Адрес начала искомого блока
  \return Указатель на соответствующий блок кеша, если таковой есть, и NULL в противном случае
 */
static FullCacheBlock *
full_cache_find(FullCache *c, memaddr_t aligned_addr)
{
    for (int i = 0; i < c->block_count; i++) {
        if (c->blocks[i].addr == aligned_addr) {
            return &c->blocks[i];
        }
    }
    return NULL;
}

/*!
  Поместить блок ОЗУ в полноассоциативный кеш
  \param c Указатель на структуру описания модели полноассоциативного кеша
  \param aligned_addr Адрес начала искомого блока
  \return Указатель на соответствующий блок кеша
 */
static FullCacheBlock *
full_cache_random_place(FullCache *c, memaddr_t aligned_addr)
{  
    FullCacheBlock *b = NULL;
    for (int i = 0; i < c->block_count; i++) {
        if (c->blocks[i].addr == NO_BLOCK) {
            b = &c->blocks[i];
            break;
        }
    }
    if (!b) {
        int random_index = c->rnd->ops->next(c->rnd, c->block_count);
        b = &c->blocks[random_index];
        c->full_ops.finalize(c, b);
    }
    b->addr = aligned_addr;
    c->mem->ops->read(c->mem, b->addr, c->block_size, b->mem);
    return b;
}

/*!
  Прочитать ячейки из полноассоциативного кеша
  \param m Указатель на структуру описания модели полноассоциативного кеша (в виде указателя на базовую структуру)
  \param addr Адрес в полноассоциативном кеше
  \param size Количество считываемых ячеек
  \param dst Указатель, куда копировать ячейки из полноассоциативного кеша
 */
static void
full_cache_read(
    AbstractMemory *m,
    memaddr_t addr,
    int size,
    MemoryCell *dst)
{
    FullCache *c = (FullCache*) m;
    memaddr_t aligned_addr = addr & -c->block_size;
    statistics_add_counter(c->b.info, c->cache_read_time);
    FullCacheBlock *b = full_cache_find(c, aligned_addr);
    if (b) {
        statistics_add_hit_counter(c->b.info);
    } else {
        b = c->full_ops.cache_place(c, aligned_addr);
    }
    if (dst) {
		// выполняем копирование данных:
		memcpy(dst, b->mem + (addr - aligned_addr), size * sizeof(b->mem[0]));
	}
}

/*!
  Функция записи в кеш в случае сквозной записи
  \param m Указатель на структуру описания модели полноассоциативного кеша (в виде указателя на базовую структуру)
  \param addr Адрес в полноассоциативном кеше
  \param size Количество записываемых ячеек
  \param src Указатель, откуда копировать ячейки в полноассоциативного кеша
*/
static void
full_cache_wt_write(
    AbstractMemory *m,
    memaddr_t addr,
    int size,
    const MemoryCell *src)
{
    FullCache *c = (FullCache*) m;
    memaddr_t aligned_addr = addr & -c->block_size;
    FullCacheBlock *b = full_cache_find(c, aligned_addr);
    statistics_add_counter(c->b.info, c->cache_write_time);
    if (b) {
        memcpy(b->mem + (addr - aligned_addr), src, size * sizeof(b->mem[0]));
    }
    c->mem->ops->write(c->mem, addr, size, src);
}

/*!
  Функция записи в кеш в случае отложенной записи
  \param m Указатель на структуру описания модели полноассоциативного кеша (в виде указателя на базовую структуру)
  \param addr Адрес в полноассоциативном кеше
  \param size Количество записываемых ячеек
  \param src Указатель, откуда копировать ячейки в полноассоциативного кеша
*/
static void
full_cache_wb_write(
    AbstractMemory *m,
    memaddr_t addr,
    int size,
    const MemoryCell *src)
{
    FullCache *c = (FullCache*) m;
    memaddr_t aligned_addr = addr & -c->block_size;
    statistics_add_counter(c->b.info, c->cache_write_time);
    FullCacheBlock *b = full_cache_find(c, aligned_addr);
    if (!b) {
        b = c->full_ops.cache_place(c, aligned_addr);
    }
    memcpy(b->mem + (addr - aligned_addr), src, size * sizeof(b->mem[0]));
    b->dirty = 1;
}

/*!
  "Раскрыть" содержимое указанных ячеек ОЗУ (а, значит, и кеша, если эти ячейки там есть).
  \param m Указатель на структуру описания модели полноассоциативного кеша (в виде указателя на базовую структуру)
  \param addr Адрес в полноассоциативном кеше
  \param size Количество считываемых ячеек
  \param src Указатель, откуда копировать ячейки в полноассоциативного кеша
 */
static void
full_cache_reveal(
    AbstractMemory *m,
    memaddr_t addr,
    int size,
    const MemoryCell *src)
{
    FullCache *c = (FullCache*) m;
    memaddr_t aligned_addr = addr & -c->block_size;
    FullCacheBlock *b = full_cache_find(c, aligned_addr);
    if (b) {
        memcpy(b->mem + (addr - aligned_addr), src, size * sizeof(b->mem[0]));
    }
    c->mem->ops->reveal(c->mem, addr, size, src);
}

/*!
  Финализация блока кеша в случае сквозной записи (см. описание FullCacheOps->finalize)
*/
static void
full_cache_wt_finalize(FullCache *c, FullCacheBlock *b)
{
    return;
}

/*!
  Финализация блока кеша в случае отложенной записи записи (см. описание FullCacheOps->finalize)
*/
static void
full_cache_wb_finalize(FullCache *c, FullCacheBlock *b)
{
    if (b->dirty) {
        statistics_add_write_back_counter(c->b.info);
        c->mem->ops->write(c->mem, b->addr, c->block_size, b->mem);
        b->dirty = 0;
    }
}

/*!
    Фиксировать состояние памяти - кеш сбрасывает из кеша все грязные блоки
    (для кеша со сквозной записью такие блоки отсутствуют)
    \param m Указатель на структуру описания модели кеша (в виде указателя на базовую структуру)
*/
static void
full_cache_flush(AbstractMemory *m)
{
    FullCache *c = (FullCache*) m;
    for (int i = 0; i < c->block_count; i++) {
        if (c->blocks[i].addr != NO_BLOCK) {
            c->full_ops.finalize(c, &c->blocks[i]);
        }
    }
    c->mem->ops->flush(c->mem);
}

/*!
    Печатает содержимое нижележащей памяти.
    \param m Указатель на структуру описания модели памяти
    \param out_f Файл, куда печатается содержимое памяти
*/
void
full_cache_print_dump(AbstractMemory *m, FILE *f_out)
{
	FullCache *c = (FullCache*) m;
	c->mem->ops->print_dump(c->mem, f_out);
}

static AbstractMemoryOps full_cache_wt_ops =
{
    full_cache_free,
    full_cache_read,
    full_cache_wt_write,
    full_cache_reveal,
    full_cache_flush,
    full_cache_print_dump
};

static AbstractMemoryOps full_cache_wb_ops =
{
    full_cache_free,
    full_cache_read,
    full_cache_wb_write,
    full_cache_reveal,
    full_cache_flush,
    full_cache_print_dump
};

static inline int
is_correct_block_size(int block_size)
{
    return (block_size == 16) || (block_size == 32) || (block_size == 64);
}

/*!
  Создать модель полноассоциативного кеша
  \param cfg Указатель на структуру, хранящую конфигурационные параметры
  \param var_prefix Префикс имен параметров
  \param info Указатель на структуру, хранящую статистику моделирования
  \return Указатель на структуру описания модели полноассоциативного кеша (в виде указателя на базовую структуру),
  NULL в случае ошибки
 */
AbstractMemory *
full_cache_create(
    ConfigFile *cfg,
    const char *var_prefix,
    StatisticsInfo *info,
    AbstractMemory *mem,
    Random *rnd)
{
    char buf[PARAM_BUF_SIZE];
    FullCache *c = (FullCache*) calloc(1, sizeof(*c));
    c->b.info = info;
    c->rnd = rnd;
    c->mem = mem;
    c->blocks = NULL;
    c->block_count = 0;
    
    //определяем стратегию записи и соответственно задаём операции:
    const char fn[] = "full_cache_create";
    const char *strategy = config_file_get(cfg,
        make_param_name(buf, sizeof(buf), var_prefix, "write_strategy"));
    if (!strategy) {
        error_undefined(fn, buf);
        goto full_cache_create_failed;
    } else if (strcmp(strategy, "write-through") == 0) {
        c->b.ops = &full_cache_wt_ops;
        c->full_ops.finalize = full_cache_wt_finalize;
    } else if (strcmp(strategy, "write-back") == 0) {
        c->b.ops = &full_cache_wb_ops;
        c->full_ops.finalize = full_cache_wb_finalize;
        c->b.info->write_back_needed = 1;
    } else {
        error_invalid(fn, buf);
        goto full_cache_create_failed;
    }
    
    //определяем стратегию замещения и соответственно задаём функцию размещения:
    strategy = config_file_get(cfg,
        make_param_name(buf, sizeof(buf), var_prefix, "replacement_strategy"));
    if (!strategy) {
        error_undefined(fn, buf);
        goto full_cache_create_failed;
    } else if (strcmp(strategy, "random") == 0) {
        c->full_ops.cache_place = full_cache_random_place;
    } else {
        error_invalid(fn, buf);
        goto full_cache_create_failed;
    }

    // считываем и проверяем параметр block_size:
    int r = config_file_get_int(cfg,
        make_param_name(buf, sizeof(buf), var_prefix, "block_size"),
        &c->block_size);
    if (!r) {
		error_undefined(fn, buf);
		goto full_cache_create_failed;
	} else if (r < 0 || !is_correct_block_size(c->block_size)) {
		error_invalid(fn, buf);
		goto full_cache_create_failed;
    }
    // считываем и проверяем параметр cache_size:
    r = config_file_get_int(cfg,
		make_param_name(buf, sizeof(buf), var_prefix, "cache_size"),
		&c->cache_size);
	if (!r) {
		error_undefined(fn, buf);
		goto full_cache_create_failed;
	} else if (r < 0 || c->cache_size <= 0
        || c->cache_size > MAX_CACHE_SIZE
        || c->cache_size % c->block_size != 0)
    {
		error_invalid(fn, buf);
		goto full_cache_create_failed;
	}
    // считываем и проверяем параметр cache_read_time:
    r = config_file_get_int(cfg,
        make_param_name(buf, sizeof(buf), var_prefix, "cache_read_time"),
        &c->cache_read_time);
    if (!r) {
		error_undefined(fn, buf);
		goto full_cache_create_failed;
	} else if (r < 0 || c->cache_read_time <= 0
        || c->cache_read_time > MAX_READ_TIME)
    {
		error_invalid(fn, buf);
		goto full_cache_create_failed;
    }
    // считываем и проверяем параметр cache_write_time:
    r = config_file_get_int(cfg,
        make_param_name(buf, sizeof(buf), var_prefix, "cache_write_time"),
        &c->cache_write_time);
    if (!r) {
		error_undefined(fn, buf);
		goto full_cache_create_failed;
	} else if (r < 0 || c->cache_write_time <= 0
        || c->cache_write_time > MAX_WRITE_TIME)
    {
		error_invalid(fn, buf);
		goto full_cache_create_failed;
    }
    
    //выделяем блоки кэша:
    c->block_count = c->cache_size / c->block_size;
    c->blocks = calloc(c->block_count, sizeof(*c->blocks));
    for (int i = 0; i < c->block_count; i++) {
        c->blocks[i].addr = NO_BLOCK;
        c->blocks[i].mem = calloc(c->block_size, sizeof(c->blocks[i].mem[0]));
    }

    return (AbstractMemory*) c;
full_cache_create_failed:
    return full_cache_free((AbstractMemory*) c);
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
