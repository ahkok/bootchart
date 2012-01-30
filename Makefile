
VERSION = 1.13

CC := gcc

all: bootchartd

install: bootchartd
	mkdir -p $(DESTDIR)/sbin
	mkdir -p $(DESTDIR)/etc
	mkdir -p $(DESTDIR)/usr/share/doc/bootchart/
	install -m0755 bootchartd $(DESTDIR)/sbin/
	install -m0644 bootchartd.conf.example $(DESTDIR)/usr/share/doc/bootchart/
	install -m0644 README COPYING $(DESTDIR)/usr/share/doc/bootchart/

OBJS := log.o svg.o bootchart.o

CFLAGS += -Wall -W -Os -g -fstack-protector -D_FORTIFY_SOURCE=2 -Wformat -fno-common \
	 -Wimplicit-function-declaration  -Wimplicit-int -fstack-protector

LDADD  += -lrt

%.o: %.c Makefile bootchart.h
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

bootchartd: $(OBJS) Makefile
	@echo "  LD  $@"
	@$(CC) -o $@ $(OBJS) $(LDADD) $(LDFLAGS)

clean:
	rm -rf *.o *~ bootchartd

dist:
	git tag v$(VERSION)
	git archive --format=tar --prefix="bootchart-$(VERSION)/" v$(VERSION) | \
		gzip > bootchart-$(VERSION).tar.gz

