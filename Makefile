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

ssocr-manpage.html: ssocr.1
	rman -f html -r '' $< >$@

install: all
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)
	install -s -m 0755 ssocr $(DESTDIR)$(BINDIR)/ssocr
	install -m 0644 ssocr.1 $(DESTDIR)$(MANDIR)/ssocr.1
	gzip -9 $(DESTDIR)$(MANDIR)/ssocr.1

deb: debian/changelog debian/control debian/rules clean
	mkdir ssocr-$(VERSION)
	cp -r Makefile *.[ch] *.in debian ssocr-$(VERSION)
	(cd ssocr-$(VERSION); fakeroot debian/rules binary)

tar: clean
	mkdir ssocr-$(VERSION)
	cp -r Makefile *.[ch] *.in ssocr-$(VERSION)
	tar cvfj ssocr-$(VERSION).tar.bz2 ssocr-$(VERSION)

clean:
	$(RM) ssocr ssocr.1 *.o *~ testbild.png ssocr-manpage.html *.deb *.bz2
	$(RM) -r ssocr-$(VERSION)
