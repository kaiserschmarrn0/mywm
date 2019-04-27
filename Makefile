SRC = mywm.c workspace.c window.c rounded.c action.c
OBJ = $(SRC:.c=.o)

PREFIX = /usr/local

all: mywm

.c.o:
	$(CC) -I/usr/X11R6/include -c  $<

mywm: $(OBJ)
	$(CC) -o $@ $(OBJ) -g -O3 -I/usr/X11R6/include -L/usr/X11R6/lib -lm -lxcb -lxcb-shape -lxcb-keysyms -lxcb-ewmh -lxcb-icccm

install: mywm
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f mywm $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/mywm

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mywm $(OBJ)

clean:
	rm -f mywm $(OBJ)
