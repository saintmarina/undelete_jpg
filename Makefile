#CFLAGS=-Wall -Wextra -Werror -O3
CFLAGS=-Wall -Wextra -Wpedantic -Werror -O0

ifeq ($(DEBUG),1)
CFLAGS+=-DDEBUG -g
endif

%.o: %.c undelete_jpg.h status_bar.h

undelete_jpg: undelete_jpg.o main.o status_bar.o

clean:
	$(RM) *.o undelete_jpg
