/* -*- mode:c; coding: utf-8 -*- */

/*!
  \file common.h
  \brief Типы данных, константы и прототипы функций общего назначения
 */

#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

/*! Тип для представления адресов */
typedef int memaddr_t;

/*! Одна ячейка памяти (один байт)
  \brief Описание ячейки памяти
 */
typedef struct MemoryCell
{
    unsigned char value; //!< Значение ячейки
    unsigned char flags; //!< 0, если значение не известно (начальное состояние), 1, если значение известно
} MemoryCell;

/*! Полезные константы */
enum
{
    KiB = 1024,
    MiB = 1024 * 1024,
    GiB = 1024 * 1024 * 1024
};

enum
{
    PARAM_BUF_SIZE = KiB,//! Размер строкового буфера под параметр
    MAX_ADDRESS    = GiB //! Максимальный адрес (первый недопустимый)
};

/*! функция формирует имя конфигурационного параметра, приписывая к основной части имени префикс имени.
  Для формирования строки имени используется передаваемый буфер, адрес начала которого возвращается.
  \param buf Буфер для формирования имени конфигурационного параметра
  \param size Размер буфера
  \param prefix Префикс имени (допускается передавать NULL)
  \param name Основная часть имени
  \return Адрес буфера buf
 */
char *make_param_name(
	char *buf,
	int size,
	const char *prefix,
	const char *name);

/*! Функция выводит сообщение об ошибке "Конфигурационный параметр не определен".
  \param func Имя функции, в которой диагностирована ошибка
  \param param Имя конфигурационного параметра
 */
void error_undefined(const char *func, const char *param);

/*! Функция выводит сообщение об ошибке "Конфигурационный параметр имеет недопустимое значение".
  \param func Имя функции, в которой диагностирована ошибка
  \param param Имя конфигурационного параметра
 */
void error_invalid(const char *func, const char *param);

/*! Функция, приводящая strcmp к формату, требуемому функциями stdlib.h (qsort, bsearch).
 */
int strcmp_wrapper(const void *p1, const void *p2);

#endif

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
