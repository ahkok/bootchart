
VERSION = 0.1

CC := gcc

all: bootchart

install: bootchart
	mkdir -p $(DESTDIR)/sbin
	install bootchart $(DESTDIR)/sbin/

OBJS := log.o svg.o bootchart.o

CFLAGS += -Wall -W -Os -g -fstack-protector -D_FORTIFY_SOURCE=2 -Wformat -fno-common \
	 -Wimplicit-function-declaration  -Wimplicit-int -fstack-protector

LDADD  +=

%.o: %.c Makefile bootchart.h
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

bootchart: $(OBJS) Makefile
	@echo "  LD  $@"
	@$(CC) -o $@ $(OBJS) $(LDADD) $(LDFLAGS)

clean:
	rm -rf *.o *~ bootchart

dist:
	git tag v$(VERSION)
	git archive --format=tar --prefix="bootchart-$(VERSION)/" v$(VERSION) | \
		gzip > bootchart-$(VERSION).tar.gz

