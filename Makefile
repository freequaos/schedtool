# go on and adjust here if you don't like those flags
CFLAGS=-Os -fomit-frame-pointer -s -pipe
#CFLAGS=-Wall -Os -fomit-frame-pointer -s -pipe -DDEBUG
CC=gcc
# likewise, if you want to change the destination prefix
DESTDIR=
DESTPREFIX=/usr/local
MANDIR=$(DESTPREFIX)/share/man/man8
GZIP=gzip -9
TARGET=schedtool
DOCS=LICENSE README INSTALL SCHED_DESIGN
RELEASE=$(shell basename `pwd`)

all: $(TARGET)

clean:
	rm -f *.o $(TARGET)

distclean: clean unzipman
	rm -f *~ *.s

install: all install-doc zipman
	install -d $(DESTDIR)$(DESTPREFIX)/bin
	install -p -c $(TARGET) $(DESTDIR)$(DESTPREFIX)/bin
	install -d $(DESTDIR)$(MANDIR)
	install -p -c schedtool.8.gz $(DESTDIR)$(MANDIR)

install-doc:
	install -d $(DESTDIR)$(DESTPREFIX)/share/doc/$(RELEASE)
	install -p -c $(DOCS) $(DESTDIR)$(DESTPREFIX)/share/doc/$(RELEASE)

zipman:
	test -f schedtool.8 && $(GZIP) schedtool.8 || exit 0

unzipman:
	test -f schedtool.8.gz && $(GZIP) -d schedtool.8.gz || exit 0

affinity_hack: clean
	$(MAKE) CFLAGS="$(CFLAGS) -DHAVE_AFFINITY_HACK" $(TARGET)

release: distclean release_gz release_bz2
	@echo --- $(RELEASE) released ---

release_gz: distclean
	@echo Building tar.gz
	( cd .. ; tar czf $(RELEASE).tar.gz $(RELEASE) )

release_bz2: distclean
	@echo Building tar.bz2
	( cd .. ; tar cjf $(RELEASE).tar.bz2 $(RELEASE) )


schedtool: schedtool.o error.o
schedtool.o: schedtool.c error.h util.h
error.o: error.c error.h

