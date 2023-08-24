CC = gcc
CFLAGS = -Og
LDLIBS = -lpthread
OBJ = csapp.o gunShell.o
PROGS = myShell

all: $(PROGS) fclean

$(PROGS) : $(OBJ)
		$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)
.c.o:
		$(CC) $(CFLAGS) -c $<

fclean :
	rm -f $(OBJ)

clean:
	rm -rf *~ $(PROGS)

