# minimal CFLAGS definition (try if compilation fails with default CFLAGS)
#CFLAGS  := $(shell if command -v imlib2-config >/dev/null; then imlib2-config --cflags; else pkg-config --cflags imlib2; fi)
# default CFLAGS definition
CFLAGS  := -D_FORTIFY_SOURCE=2 -Wall -W -Wextra -pedantic -fstack-protector-all $(shell if command -v imlib2-config >/dev/null; then imlib2-config --cflags; else pkg-config --cflags imlib2; fi) -O3
LDLIBS  := -lm $(shell if command -v imlib2-config >/dev/null; then imlib2-config --libs; else pkg-config --libs imlib2; fi)
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin
MANDIR  := $(PREFIX)/share/man/man1
DOCDIR  := $(PREFIX)/share/doc/ssocr
DOCS    := AUTHORS COPYING INSTALL README THANKS NEWS
VERSION := $(shell sed -n 's/^.*VERSION.*\(".*"\).*/\1/p' defines.h)
CRYEARS := $(shell sed -n 's/^.*fprintf.*Copyright.*\(2004-2[0-9][0-9][0-9]\).*Erik.*Auerswald.*$$/\1/p' help.c)
MANYEAR := $(shell sed -n 's/^.*fprintf.*Copyright.*2004-\(2[0-9][0-9][0-9]\).*Erik.*Auerswald.*$$/\1/p' help.c)

all: ssocr ssocr.1

ssocr: ssocr.o imgproc.o help.o charset.o

ssocr.o: ssocr.c ssocr.h defines.h imgproc.h help.h charset.h Makefile
imgproc.o: imgproc.c defines.h imgproc.h help.h Makefile
help.o: help.c defines.h imgproc.h help.h Makefile
charset.o: charset.c charset.h defines.h help.h Makefile

ssocr.1: ssocr.1.in Makefile defines.h help.c
	sed -e 's/@VERSION@/$(VERSION)/' \
	    -e "s/@DATE@/$(MANYEAR)/" \
	    -e 's/@CRYEARS@/$(CRYEARS)/' <$< >$@

ssocr-manpage.html: ssocr.1
	man -l -Thtml $< >$@

install: all
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR) $(DESTDIR)$(DOCDIR)
	install -s -m 0755 ssocr $(DESTDIR)$(BINDIR)/ssocr
	install -m 0644 ssocr.1 $(DESTDIR)$(MANDIR)/ssocr.1
	gzip -9 $(DESTDIR)$(MANDIR)/ssocr.1
	install -m 0644 $(DOCS) $(DESTDIR)$(DOCDIR)

ssocr-dir:
	install -d ssocr-$(VERSION)
	install -m 0644 Makefile $(DOCS) *.[ch] *.in ssocr-$(VERSION)
	install -d ssocr-$(VERSION)/notdebian
	install -m 0644 notdebian/* ssocr-$(VERSION)/notdebian
	chmod +x ssocr-$(VERSION)/notdebian/rules

notdebian/changelog:
	printf "ssocr ($(VERSION)-1) unstable; urgency=low\n\n  * self built package of current ssocr version in .deb format\n\n -- $(USER)  $(shell date -R)\n" >$@

selfdeb: notdebian/changelog notdebian/control notdebian/rules ssocr-dir
	(cd ssocr-$(VERSION); ln -sv notdebian debian; fakeroot debian/rules binary; fakeroot debian/rules clean; rm -f debian)

tar: ssocr-dir
	tar cvfj ssocr-$(VERSION).tar.bz2 ssocr-$(VERSION)

clean:
	$(RM) ssocr ssocr.1 *.o *~ testbild.png ssocr-manpage.html
	$(RM) notdebian/changelog
	$(RM) -r ssocr-$(VERSION) ssocr-?.?.? ssocr-?.??.?

distclean: clean
	$(RM) *.deb *.bz2

.PHONY: clean tar ssocr-dir install
