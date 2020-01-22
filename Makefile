CFLAGS=-Wall -Wextra -Wpedantic -Werror -O3

ifeq ($(DEBUG),1)
CFLAGS+=-DDEBUG -g -O0
endif

%.o: %.c undelete_jpg.h status_bar.h

undelete_jpg: undelete_jpg.o main.o status_bar.o

clean:
	$(RM) *.o undelete_jpg
