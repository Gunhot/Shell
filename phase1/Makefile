CC = gcc
CFLAGS = -Og
LDLIBS = -lpthread
OBJ = csapp.o gunShell.o
PROGS = myShell

all: $(PROGS)

$(PROGS) : $(OBJ)
		$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)
.c.o:
		$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ)
	rm -rf *~ $(PROGS)

