/* -*- mode:c; coding: utf-8 -*- */

#include "memory.h"

#include <stdlib.h>

/*!
  Ограничения значений параметров
 */
enum
{
    MAX_MEM_SIZE = 1 * GiB, //!< Максимальный поддерживаемый размер ОЗУ модели
    MAX_READ_TIME = 100000, //!< Максимальное время чтения из ОЗУ в тактах
    MAX_WRITE_TIME = MAX_READ_TIME, //!< Максимальное время записи в озу в тактах
    MAX_WIDTH = 1024 //!< Максимальный размер блока памяти
};

struct Memory;
typedef struct Memory Memory;

/*!
  Структура описывает модель ОЗУ
  \brief Дескриптор модели ОЗУ
 */
struct Memory
{
    AbstractMemory b; //!< Базовые поля
    MemoryCell *mem; //!< Массив ячеек памяти
    int memory_size; //!< Размер ОЗУ (считанный из конфиг. файла)
    int memory_read_time; //!< Время чтения из ОЗУ
    int memory_write_time; //!< Время записи в ОЗУ
    int memory_width; //!< Полоса пропускания ОЗУ
};

/*!
  Освободить ресурсы
  \param a Указатель на структуру описания модели ОЗУ (в виде указателя на базовую структуру)
  \return Указатель NULL
 */
static AbstractMemory *
memory_free(AbstractMemory *a)
{
    if (a) {
        Memory *m = (Memory *)a;
        free(m->mem);
        free(m);
    }
    return NULL;
}

/*!
  Прочитать ячейки из ОЗУ
  \param a Указатель на структуру описания модели ОЗУ (в виде указателя на базовую структуру)
  \param addr Адрес в ОЗУ
  \param size Количество считываемых ячеек
  \param dst Указатель, куда копировать ячейки из ОЗУ
 */
static void
memory_read(
	AbstractMemory *a,
	memaddr_t addr,
	int size,
	MemoryCell *dst)
{
    Memory *m = (Memory *) a;
    // учитываем время, требуемое на выполнение операции чтения:
    int blocks_count = (size + m->memory_width - 1) / m->memory_width;
    //количество блоков округлено вверх
    statistics_add_counter(m->b.info, blocks_count * m->memory_read_time);
    if (dst) {
		// выполняем копирование данных:
		for (; size; ++addr, --size, ++dst) {
			if (addr >= m->memory_size) {
				break;
			}
			*dst = m->mem[addr];
		}
	}
}

/*!
  Записать ячейки в ОЗУ
  \param a Указатель на структуру описания модели ОЗУ (в виде указателя на базовую структуру)
  \param addr Адрес в ОЗУ
  \param size Количество записываемых ячеек
  \param src Указатель, откуда копировать ячейки в ОЗУ
 */
static void
memory_write(
	AbstractMemory *a,
	memaddr_t addr,
	int size,
	const MemoryCell *src)
{
    Memory *m = (Memory *) a;
    // учитываем время, требуемое на выполнение операции записи:
    int blocks_count = (size + m->memory_width - 1) / m->memory_width;
    //количество блоков округлено вверх
    statistics_add_counter(m->b.info, blocks_count * m->memory_write_time);
    // выполняем копирование данных:
    for (; size; ++addr, --size, ++src) {
		if (addr >= m->memory_size) {
			break;
		}
        m->mem[addr] = *src;
    }

}

/*!
  "Раскрыть" содержимое указанных ячеек ОЗУ.
  \param a Указатель на структуру описания модели ОЗУ (в виде указателя на базовую структуру)
  \param addr Адрес в ОЗУ
  \param size Количество считываемых ячеек
  \param src Указатель, откуда копировать ячейки в ОЗУ
 */
static void
memory_reveal(
	AbstractMemory *a,
	memaddr_t addr,
	int size,
	const MemoryCell *src)
{
    Memory *m = (Memory *) a;

    for (; size; ++addr, --size, ++src) {
        if (addr >= m->memory_size) {
			break;
		}
        m->mem[addr] = *src;
    }
}

/*!
  Фиксировать состояние памяти - в данном случае состояние всегда фиксировано, 
  поэтому функция ничего не делает
 */
static void
memory_flush(AbstractMemory *m)
{
	return;
}

enum DUMP_PARAMS
{
	COLS_COUNT = 16
};

/*!
    Печатает содержимое памяти.
    \param m Указатель на структуру описания модели памяти
    \param out_f Файл, куда печатается содержимое памяти
*/
void
memory_print_dump(AbstractMemory *a, FILE *f_out)
{
	Memory *m = (Memory *) a;
	for (int addr = 0; addr < m->memory_size; addr += COLS_COUNT) {
        printf("%08X", addr);
        for (int off = 0; off < COLS_COUNT; off++) {
            if (m->mem[addr + off].flags) {
				fprintf(f_out, " %02X", m->mem[addr + off].value);
			} else {
				fprintf(f_out, " ??");
			}
        }
        printf("\n");
    }
}


static AbstractMemoryOps memory_ops =
{
    memory_free,
    memory_read,
    memory_write,
    memory_reveal,
    memory_flush,
    memory_print_dump
};

/*!
  Создать модель ОЗУ
  \param cfg Указатель на структуру, хранящую конфигурационные параметры
  \param var_prefix Префикс имен параметров
  \param info Указатель на структуру, хранящую статистику моделирования
  \return Указатель на структуру описания модели ОЗУ (в виде указателя на базовую структуру),
  NULL в случае ошибки
 */
AbstractMemory *
memory_create(
	const ConfigFile *cfg,
	const char *var_prefix,
	StatisticsInfo *info)
{
    Memory *m = calloc(1, sizeof(*m));

    // заполняем базовые поля:
    m->b.ops = &memory_ops;
    m->b.info = info;

	const char fn[] = "memory_create";
	char buf[PARAM_BUF_SIZE];
    // считываем и проверяем параметр memory_size:
    int r = config_file_get_int(cfg,
		make_param_name(buf, sizeof(buf), var_prefix, "memory_size"),
		&m->memory_size);
	if (!r) {
		error_undefined(fn, buf);
		goto memory_create_failed;
	} else if (r < 0 || m->memory_size <= 0
        || m->memory_size > MAX_MEM_SIZE
        || m->memory_size % KiB != 0) {
		error_invalid(fn, buf);
		goto memory_create_failed;
	}
	// считываем и проверяем параметр memory_read_time:
    r = config_file_get_int(cfg,
		make_param_name(buf, sizeof(buf), var_prefix, "memory_read_time"),
		&m->memory_read_time);
	if (!r) {
		error_undefined(fn, buf);
		goto memory_create_failed;
	} else if (r < 0 || m->memory_read_time <= 0) {
		error_invalid(fn, buf);
		printf(": %d\n", m->memory_read_time);
		goto memory_create_failed;
	}
	// считываем и проверяем параметр memory_write_time:
    r = config_file_get_int(cfg,
		make_param_name(buf, sizeof(buf), var_prefix, "memory_write_time"),
		&m->memory_write_time);
	if (!r) {
		error_undefined(fn, buf);
		goto memory_create_failed;
	} else if (r < 0 || m->memory_write_time <= 0) {
		error_invalid(fn, buf);
		goto memory_create_failed;
	}
	// считываем и проверяем параметр memory_width:
    r = config_file_get_int(cfg,
		make_param_name(buf, sizeof(buf), var_prefix, "memory_width"),
		&m->memory_width);
	if (!r) {
		error_undefined(fn, buf);
		goto memory_create_failed;
	} else if (r < 0 || m->memory_width <= 0 || m->memory_width > MAX_MEM_SIZE) {
		error_invalid(fn, buf);
		goto memory_create_failed;
	}

    // создаем массив ячеек:
    m->mem = (MemoryCell*) calloc(m->memory_size, sizeof(m->mem[0]));
    
    return (AbstractMemory*) m;

memory_create_failed:
	free(m);
	return NULL;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
