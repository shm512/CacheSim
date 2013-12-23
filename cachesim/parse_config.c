/* -*- mode:c; coding: utf-8 -*- */

#include "parse_config.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

enum
{
    LINE_BUF_SIZE = 1024,
    MAX_LINE_LENGTH = 1000,
    MIN_VECTOR_RESERVED = 64
};

/*!
  Структура хранит один конфигурационный параметр
  \brief Описание одного конфигурационного параметра
 */
typedef struct ConfigParam
{
    char *name;  //!< Имя параметра - обязательно должно оставаться первым полем!
    char *value; //!< Значение параметра
    int line;    //!< Строка, на которой он объявлен
} ConfigParam;

/*!
  Структура описывает считанные в память конфигурационные параметры.
  \brief Дескриптор конфигурационного файла
 */
struct ConfigFile
{
    int size; //!< Число использованных элементов в векторе
    int reserved; //!< Число элементов, под которые в векторе зарезервирована память
    ConfigParam *params; //!< Вектор параметров (реализованный массивом переменной длины)
};

/*!
    Начальная создание и инициализация дескриптора конфигурационного файла
    \return Указатель на созданный дескриптор или NULL в случае ошибки
 */
static ConfigFile *
config_file_create(void)
{
    ConfigFile *cfg = (ConfigFile*) calloc(1, sizeof(*cfg));
    if (!cfg) {
        return NULL;
    }
    cfg->size = 0;
    cfg->reserved = MIN_VECTOR_RESERVED;
    cfg->params = calloc(cfg->reserved, sizeof(*cfg->params));
    return cfg;
}

ConfigFile *
config_file_free(ConfigFile *cfg)
{
    if (cfg) {
        for (int i = 0; i < cfg->size; i++) {
            free(cfg->params[i].name);
            free(cfg->params[i].value);
        }
        free(cfg->params);
        free(cfg);
    }
    return NULL;
}

/*!
    Функция сравнения структур описания конфигурационного параметра в формате, требуемом функциями библиотеки stdlib.h.
    Лексикографически сравниваются их имена,
    в случае совпадения тех сравниваются номера строк, на которых они объявлены
 */
static int
config_param_cmp(const void *p1, const void *p2)
{
    ConfigParam *cp1 = (ConfigParam *) p1, *cp2 = (ConfigParam *) p2;
    int tmp = strcmp(cp1->name, cp2->name);
    return (tmp == 0) ? cp1->line - cp2->line : tmp;
}

static inline int
is_correct_name_symb(char ch)
{
    return (isalpha(ch) || isdigit(ch) || ch == '_' || ch == '-');
}

static inline void
syntax_error(const char *fpath, int line, FILE *log_f)
{
    fprintf(log_f, "Syntax error in line %d of %s\n", line, fpath);
}

/*!
    Функция пропускает пробельные символы, двигая указатель по буферу
    \param ch Сдвигаемый указатель
    \param end Указатель на конец рассматриваемой части буфера, по его достижению завершается
 */
static inline void
skip_spaces(char **ch, const char *end)
{
    while (*ch < end && isspace(**ch)) {
        ++*ch;
    }
}

/*!
    Функция ищет первый (с минимальным значениeм line для второго обнаружения) повторяющийся параметр в массиве.
    Второе обнаружение параметра гарантированно произойдёт сразу же после первого: так сортировали.
    \param params Массив параметров
    \param params_c Число параметров в массиве
    \return Указатель на первый повторяющийся параметр или NULL, если такового не обнаружено
 */
static const ConfigParam *
first_dup_param(const ConfigParam *params, int params_c)
{
    //проверка на совпадение параметров
    const ConfigParam *first = NULL;
    for (int i = 1; i < params_c; i++) {
        if (strcmp(params[i].name, params[i-1].name) == 0
            && (!first || params[i].line < first->line))
        {
            first = &params[i];
        }
    }
    return first;
}

ConfigFile *
config_file_parse(const char *path, FILE *log_f)
{
    if (!log_f) {
        return NULL;
    }
    FILE *conf_f = fopen(path, "r");
    if (!path || !conf_f) {
        fprintf(log_f, "Failed to open %s for reading\n", path);
        return NULL;
    }
    ConfigFile *cfg = config_file_create();
    if (!cfg) {
        goto config_file_parse_failed;
    }
    
    //разбор конфигурационного файла:
    char strbuf[LINE_BUF_SIZE];
    int line = 0;
    while (fgets(strbuf, sizeof(strbuf), conf_f)) {
        line++;
        char *ch = strbuf;
        char *end = strchr(strbuf, '#');
        if (!end) end = strbuf + strlen(strbuf);
        do {
            if (end == ch) {
                    goto next_iteration;
            }
            end--;
        } while (isspace(*end));
        end++;  //end - указатель на первый пробел конца
        skip_spaces(&ch, end);
        if (ch == end) {
            continue;
        }
        //пробелы в начале и конце пропущены
        char *name_begin = ch;
        while (ch < end && is_correct_name_symb(*ch)) {
            ch++;
        }
        if (ch == end || ch == name_begin || (!isspace(*ch) && *ch != '=')) {
            syntax_error(path, line, log_f);
            goto config_file_parse_failed;
        }
        //границы NAME определены
        if (!isalpha(*name_begin) && *name_begin != '_') {
            syntax_error(path, line, log_f);
            goto config_file_parse_failed;
        }
        int i = cfg->size++;
        if (cfg->size > cfg->reserved) {
            cfg->params = realloc(cfg->params,
                (cfg->reserved *= 2) * sizeof(*cfg->params));
        }
        cfg->params[i].value = NULL;
        cfg->params[i].line = line;
        cfg->params[i].name = strndup(name_begin, ch - name_begin);
        //NAME записано, ищем '=', затем VALUE:
        skip_spaces(&ch, end);
        if (ch == end) {
            cfg->params[i].value = strdup("");
        } else if (*ch == '=') {
            ch++;
            skip_spaces(&ch, end);
            cfg->params[i].value = strndup(ch, end - ch);
        } else {
            syntax_error(path, line, log_f);
            goto config_file_parse_failed;
        }
        
    next_iteration:
        ;
    }
    
    qsort(cfg->params, cfg->size, sizeof(*cfg->params), config_param_cmp);
    const ConfigParam *duplicate = first_dup_param(cfg->params, cfg->size);
    if (duplicate) {
        fprintf(log_f, "Duplicate parameter %s in %s\n",
            duplicate->name, path);
        goto config_file_parse_failed;
    }
    goto config_file_parse_finally;

config_file_parse_failed:
    cfg = config_file_free(cfg);
config_file_parse_finally:
    fclose(conf_f);
    return cfg;
}

const char *
config_file_get(const ConfigFile *cfg, const char *name)
{
    //чёрная магия: поле name - первое в ConfigParam
    ConfigParam *param =
        bsearch(&name, cfg->params, cfg->size, sizeof(*cfg->params), strcmp_wrapper);
    return (param) ? param->value : NULL;
}

int
config_file_get_int(const ConfigFile *cfg, const char *name, int *p_value)
{
    const char *str = config_file_get(cfg, name);
    if (!str) {
        return 0;
    }
    char *eptr = NULL;
    errno = 0;
    int value = strtol(str, &eptr, 10);
    if (*eptr || errno) {
        return -1;
    }
    *p_value = value;
    return 1;
}

void
config_file_print(const ConfigFile *cfg)
{
    for (int i = 0; i < cfg->size; i++) {
        printf("%s = \"%s\"\n", cfg->params[i].name, cfg->params[i].value);
    }
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
