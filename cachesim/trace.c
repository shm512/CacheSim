/* -*- mode:c; coding: utf-8 -*- */

#include "trace.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

enum
{
    LINE_BUF_SIZE = 1024,
    MAX_LINE_LENGTH = 1000,
    BITS_IN_BYTE = 8
};

/*!
  Структура описывает считываемую трассу.
  \brief Дескриптор трассы
 */
struct Trace
{
    FILE *f; //!< Файл, из которого ведется чтение
    FILE *log_f; //!< Файл, в который выводить ошибки
    char *path; //!< Путь к файлу
    int lineno; //!< Номер строки в файле
    TraceStep step; //!< Текущий считанный шаг
};

Trace *
trace_open(const char *path, FILE *log_f)
{
    Trace *t = (Trace *) calloc(1, sizeof(*t));
    if (!path) {
        t->path = strdup("<stdin>");
        t->f = stdin;
    } else {
        t->path = strdup(path);
        t->f = fopen(path, "r");
    }
    t->log_f = log_f;
    if (!t->f) {
        t = trace_close(t);
    }
    return t;
}

Trace *
trace_close(Trace *t)
{
    if (t) {
        if (t->f && t->f != stdin) {
            fclose(t->f);
        }
        free(t->path);
        free(t);
    }
    return NULL;
}

static int
trace_error(const Trace *t, const char *text)
{
    fprintf(t->log_f, "%s: %d: trace_next: %s\n", t->path, t->lineno, text);
    return -1;
}

static inline int
is_size(char ch)
{
    return (ch == '1' || ch == '2' || ch == '4' || ch == '8');
}

static inline unsigned char
get_last_byte(int value)
{
	return value & 0xFF;
}

/*!
  Функция распознаёт содержимое шага трассы, записанное в текстовом виде.
  \param step Указатель на структуру, описывающую шаг (туда и пишем)
  \param trace_line Строка, описывающая шаг трассы (непустая, без пробелов и комментариев)
  \return В случае успешного чтения возвращается 1,
  в случае ошибки при чтении возвращается -1. 
 */
static int
trace_step_parse(TraceStep *step, char *trace_line)
{
    errno = 0;
    long long value;
    char size;
    int data_read = sscanf(trace_line, "%c%c %x %c %lld",
        &step->op, &step->mem, &step->addr, &size, &value);
    if (errno) {
        return -1;
    }
    if (data_read == 3) {
        step->size = 1;
        step->value[0].flags = 1;
        return 1;
    } else if (data_read == 5 && is_size(size)) {
        step->size = size - '0';
        for (int i = step->size - 1; i >= 0; i--) {
            step->value[i].value = get_last_byte(value);
            value >>= BITS_IN_BYTE;
            step->value[i].flags = 1;
        } 
        return 1;
    } else {
        return -1;
    }
    if ((step->op != 'R' && step->op != 'W')
        || (step->mem != 'D' && step->mem != 'I'))
    {
        return -1;
    }
    return 1;
}

int
trace_next(Trace *t)
{
    char buf[LINE_BUF_SIZE], *p;
    int buflen;

    while (fgets(buf, sizeof(buf), t->f)) {
        t->lineno++;
        buflen = strlen(buf);
        if (buflen > MAX_LINE_LENGTH) {
            return trace_error(t, "line is too long");
        }
        if ((p = strchr(buf, '#'))) {
            *p = 0;
            buflen = strlen(buf);
        }
        while (buflen > 0 && isspace(buf[buflen - 1])) {
            --buflen;
        }
        buf[buflen] = 0;
        if (!buflen) {
            continue;
        }
        //пробелы и комментарии в конце удалены, строка непустая
        return trace_step_parse(&t->step, buf);
    }
    return 0;
}

TraceStep *
trace_get(Trace *t)
{
    return (t) ? &t->step : NULL;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
