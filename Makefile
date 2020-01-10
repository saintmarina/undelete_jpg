#CFLAGS=-Wall -Wextra -Werror -O3
CFLAGS=-Wall -Wextra -Werror -O0 -g

ifeq ($(DEBUG),1)
CFLAGS+=-DDEBUG
endif

%.o: %.c undelete_jpg.h status_bar.h
	gcc ${CFLAGS} -c $< -o $@

undelete_jpg: undelete_jpg.o main.o status_bar.o
	gcc $^ -o $@

clean:
	rm -f *.o undelete_jpg