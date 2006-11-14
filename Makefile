# go on and adjust here if you don't like those flags
CFLAGS=-Os -fomit-frame-pointer -s -pipe
#CFLAGS=-Wall -Os -fomit-frame-pointer -s -pipe -DDEBUG
CC=gcc
# likewise, if you want to change the destination prefix
DESTPREFIX=/usr/local
MANDIR=$(DESTPREFIX)/share/man/man8
DESTDIR=bin
GZIP=gzip -9
TARGET=schedtool
DOCS=README INSTALL SCHED_DESIGN TUNING
RELEASE=$(shell basename `pwd`)

all: affinity

clean:
	rm -f *.o $(TARGET)

distclean: clean unzipman
	rm -f *~ *.s

install: all install-doc zipman
	install -d $(DESTPREFIX)/$(DESTDIR)
	install -c $(TARGET) $(DESTPREFIX)/$(DESTDIR)
	install -d $(DESTPREFIX)/man/man8
	install -c schedtool.8.gz $(MANDIR)

install-doc:
	install -d $(DESTPREFIX)/share/doc/schedtool
	install -c $(DOCS) $(DESTPREFIX)/share/doc/schedtool

zipman:
	test -f schedtool.8 && $(GZIP) schedtool.8 || exit 0

unzipman:
	test -f schedtool.8.gz && $(GZIP) -d schedtool.8.gz || exit 0

affinity:
	$(MAKE) CFLAGS="$(CFLAGS) -DHAVE_AFFINITY" $(TARGET)

affinity_hack: clean
	$(MAKE) CFLAGS="$(CFLAGS) -DHAVE_AFFINITY -DHAVE_AFFINITY_HACK" $(TARGET)

no_affinity: clean
	$(MAKE) CFLAGS="$(CFLAGS)" $(TARGET)

release: distclean release_gz release_bz2
	@echo --- $(RELEASE) released ---

release_gz: distclean
	@echo Building tar.gz
	( cd .. ; tar czf $(RELEASE).tar.gz $(RELEASE) )

release_bz2: distclean
	@echo Building tar.bz2
	( cd .. ; tar cjf $(RELEASE).tar.bz2 $(RELEASE) )


schedtool: schedtool.o error.o
schedtool.o: schedtool.c syscall_magic.h error.h util.h
error.o: error.c error.h

