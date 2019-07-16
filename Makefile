SRC = mywm.c rounded.c window.c workspace.c action.c mouse.c snap.c margin.c color.c
OBJ = $(SRC:.c=.o)

PREFIX = /usr/local

all: mywm

.c.o:
	$(CC) -I/usr/X11R6/include -I/usr/include/freetype2 -I/usr/X11R6/include/freetype2 -c $<

mywm: $(OBJ)
	$(CC) -o $@ $(OBJ) -g -I/usr/X11R6/include -I/usr/include/freetype2 -L/usr/X11R6/include/freetype2 -L/usr/X11R6/lib -lm -lxcb -lxcb-shape -lxcb-keysyms -lxcb-ewmh -lxcb-icccm -lfreetype -lX11 -lX11-xcb -lXft

install: mywm
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f mywm $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/mywm

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mywm $(OBJ)

clean:
	rm -f mywm $(OBJ)
