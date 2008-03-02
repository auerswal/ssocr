CFLAGS := -Wall -W -pedantic -Werror -pedantic-errors $(shell imlib2-config --cflags) -O6
LDFLAGS := $(shell imlib2-config --libs) -lm
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin

all: ssocr

ssocr: ssocr.o

ssocr.o: ssocr.c ssocr.h

install:
	install -s -m 0755 ssocr $(BINDIR)/ssocr

clean:
	$(RM) ssocr *.o *~ testbild.png
