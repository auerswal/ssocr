CFLAGS  := -D_FORTIFY_SOURCE=2 -Wall -W -Wextra -pedantic -Werror -pedantic-errors -fstack-protector-all $(shell imlib2-config --cflags) -O3
LDLIBS  := -lm $(shell imlib2-config --libs)
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin
MANDIR  := $(PREFIX)/share/man/man1
DOCDIR  := $(PREFIX)/share/doc/ssocr
DOCS    := AUTHORS COPYING INSTALL README THANKS
VERSION := $(shell sed -n 's/^.*VERSION.*\(".*"\).*/\1/p' defines.h)

all: ssocr ssocr.1

ssocr: ssocr.o imgproc.o help.o charset.o

ssocr.o: ssocr.c ssocr.h defines.h imgproc.h help.h charset.h Makefile
imgproc.o: imgproc.c defines.h imgproc.h help.h Makefile
help.o: help.c defines.h imgproc.h help.h Makefile
charset.o: charset.c charset.h defines.h help.h Makefile

ssocr.1: ssocr.1.in Makefile
	sed -e "s/@VERSION@/$(VERSION)/" \
	    -e "s/@DATE@/$(shell date +%Y-%m-%d)/" <$< >$@

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
