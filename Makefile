CFLAGS := -D_FORTIFY_SOURCE=2 -Wall -W -Wextra -pedantic -Werror -pedantic-errors $(shell imlib2-config --cflags) -O6
LDLIBS := -lImlib2 -lm
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin
MANDIR := $(PREFIX)/share/man/man1
DOCDIR := $(PREFIX)/share/doc/ssocr
DOCS   := AUTHORS COPYING INSTALL THANKS
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
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR) $(DESTDIR)$(DOCDIR)
	install -s -m 0755 ssocr $(DESTDIR)$(BINDIR)/ssocr
	install -m 0644 ssocr.1 $(DESTDIR)$(MANDIR)/ssocr.1
	gzip -9 $(DESTDIR)$(MANDIR)/ssocr.1
	install -m 0644 $(DOCS) $(DESTDIR)$(DOCDIR)

ssocr-dir:
	install -d ssocr-$(VERSION)
	install Makefile $(DOCS) *.[ch] *.in ssocr-$(VERSION)
	install -d ssocr-$(VERSION)/debian
	install debian/* ssocr-$(VERSION)/debian

debian/changelog:
	printf "ssocr ($(VERSION)-1) unstable; urgency=low\n\n  * Debian package of current ssocr version\n\n -- $(USER)  $(shell date -R)\n" >$@

deb: debian/changelog debian/control debian/rules ssocr-dir
	(cd ssocr-$(VERSION); fakeroot debian/rules binary; fakeroot debian/rules clean)

tar: ssocr-dir
	tar cvfj ssocr-$(VERSION).tar.bz2 ssocr-$(VERSION)

clean:
	$(RM) ssocr ssocr.1 *.o *~ testbild.png ssocr-manpage.html *.deb *.bz2
	$(RM) debian/changelog
	$(RM) -r ssocr-$(VERSION) ssocr-?.?.? ssocr-?.??.?

.PHONY: clean tar deb ssocr-dir install
