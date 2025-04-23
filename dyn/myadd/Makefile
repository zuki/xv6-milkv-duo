CROSS := /home/vagrant/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-musl-x86_64/bin/riscv64-unknown-linux-musl-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy
STRIP := $(CROSS)strip

CFLAGS = -std=gnu99 -O3 -z max-page-size=4096

all: myadd

myadd: main.o libmyadd.so
	$(CC) -pie -o myadd -L./ main.o -lmyadd

libmyadd.so: myadd.c
	$(CC) $(CFLAGS) -shared -o $@ -c myadd.c

%.o: %.c myadd.h
	$(CC) $(CFLAGS) -fpie -o $@ -c main.c

install: myadd
	cp myadd myadd.org
	$(STRIP) -o myadd myadd.org
	cp myadd $(HOME)/xv6-milkv-musl/obj/dyn/bin/

clean:
	rm -f myadd libmyadd.so *.o *.org
