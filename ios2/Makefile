VAR = proj2
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic -pthread
CC = gcc
RM = rm -f

$(VAR) : $(VAR).c
		$(CC) $(CFLAGS) -o $(VAR) $(VAR).c

clean :
	$(RM) *.o $(VAR)
zip:
	zip proj2.zip *.c Makefile
