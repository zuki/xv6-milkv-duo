CROSS := /usr/bin/riscv64-linux-gnu-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy
STRIP := $(CROSS)strip

MUSL = /usr/local/musl_riscv
USR_CC := $(MUSL)/bin/musl-gcc

CFLAGS = -std=gnu99 -O3 -z max-page-size=4096

all: myadd

myadd: main.o libmyadd.so
	$(USR_CC) -pie -o myadd -L./ main.o -lmyadd
#	$(CC) -pie -o myadd -L./ main.o -lmyadd

libmyadd.so: myadd.c
	$(USR_CC) $(CFLAGS) -shared -o $@ -c myadd.c
#	$(CC) $(CFLAGS) -shared -o $@ -c myadd.c

%.o: %.c myadd.h
	$(USR_CC) $(CFLAGS) -fpie -o $@ -c main.c
#	$(CC) $(CFLAGS) -fpie -o $@ -c main.c

install: myadd
	cp myadd myadd.org
	$(STRIP) -o myadd myadd.org
	cp myadd $(HOME)/xv6-milkv-musl/obj/dyn/bin/

clean:
	rm -f myadd libmyadd.so *.o *.org
