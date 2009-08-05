CFLAGS := -Wall -W -pedantic -Werror -pedantic-errors $(shell imlib2-config --cflags) -O6
LDFLAGS := $(shell imlib2-config --libs) -lm
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin
MANDIR := $(PREFIX)/share/man/man1
VERSION := $(shell sed -n 's/^.*VERSION.*\(".*"\).*/\1/p' defines.h)

all: ssocr ssocr.1

ssocr: ssocr.o imgproc.o help.o

ssocr.o: ssocr.c ssocr.h defines.h imgproc.h help.h
imgproc.o: imgproc.c defines.h imgproc.h help.h
help.o: help.c defines.h imgproc.h help.h

ssocr.1: ssocr.1.in
	sed -e "s/@VERSION@/$(VERSION)/" -e "s/@DATE@/$(shell date -I)/" <$< >$@

install: all
	install -s -m 0755 ssocr $(DESTDIR)$(BINDIR)/ssocr
	install -m 0644 ssocr.1 $(DESTDIR)$(MANDIR)/ssocr.1
	gzip -9 $(DESTDIR)$(MANDIR)/ssocr.1

clean:
	$(RM) ssocr ssocr.1 *.o *~ testbild.png
