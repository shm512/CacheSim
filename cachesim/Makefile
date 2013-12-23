CC = gcc
CFLAGS = -Wall -Werror -Wformat-security -Wignored-qualifiers -Winit-self -Wswitch-default -Wfloat-equal -Wshadow -Wpointer-arith -Wtype-limits -Wempty-body -Wlogical-op -Wstrict-prototypes -Wold-style-declaration -Wold-style-definition -Wmissing-parameter-type -Wmissing-field-initializers -Wnested-externs -Wno-pointer-sign -std=gnu99
LDFLAGS = -s
CFILES = $(wildcard *.c)
HFILES = $(wildcard *.h)
OBJECTS = $(CFILES:.c=.o)
TARGET = cachesim
all: $(TARGET)
cachesim: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@
clean:
	rm -f $(TARGET) *.o deps.make
include deps.make
deps.make: $(CFILES) $(HFILES)
	gcc -MM $(CFILES) > deps.make
