K=kernel
U=obj/usr/bin
D=obj/dyn/bin
I=include

OBJS = \
  $K/entry.o \
  $K/start.o \
  $K/console.o \
  $K/printf.o \
  $K/uart.o \
  $K/kalloc.o \
  $K/spinlock.o \
  $K/string.o \
  $K/main.o \
  $K/vm.o \
  $K/proc.o \
  $K/swtch.o \
  $K/trampoline.o \
  $K/trap.o \
  $K/syscall.o \
  $K/sysproc.o \
  $K/bio.o \
  $K/fs.o \
  $K/log.o \
  $K/sleeplock.o \
  $K/file.o \
  $K/pipe.o \
  $K/exec.o \
  $K/sysfile.o \
  $K/kernelvec.o \
  $K/plic.o \
  $K/sbi.o \
  $K/mmc.o \
  $K/sdhci.o \
  $K/sdhci-cv181x.o \
  $K/riscv-cache.o \
  $K/sd.o \
  $K/buddy.o \
  $K/slab.o \
  $K/page.o \
  $K/rtc.o \
  $K/signal.o \
  $K/clock.o \
  $K/mmap.o \
  $K/kmalloc.o

$K/ramdisk_data.o: fs.img

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#GCCPATH = /home/vagrant/riscv-gnu-toolchain/bin
#TOOLPREFIX = $(GCCPATH)/riscv64-unknown-elf-
#TOOLPREFIX = riscv64-linux-gnu-
#TOOLPREFIX = /home/vagrant/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-musl-x86_64/bin/riscv64-unknown-linux-musl-
TOOLPREFIX = /home/vagrant/duo-buildroot-sdk/host-tools/gcc/riscv64-elf-x86_64/bin/riscv64-unknown-elf-

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)as
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# marchを指定しないとCCのデフォルト(rv64imafdcv_xtheadc ?)が使用される
#CFLAGS = -Wall -Werror -Os -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS = -march=rv64gc -Wall -Werror -Os -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax -Wno-unused-function
CFLAGS += -I$(I) -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096 -z noexecstack #--no-warn-rwx-segments
ASFLAGS = $(CFLAGS)

$K/kernel: $(OBJS) $K/kernel.ld
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS)
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$K/kernel.ld: $K/kernel.ld.S
	$(CC) $(CFLAGS) -Wp,-MD,$K/kernel.ld.d -E -P -C -o $@ $<
$K/kernel.bin: $K/kernel
	$(OBJCOPY) -O binary -R .note -R .comment -S $< $(@)

tags: $(OBJS) _init
	etags *.S *.c

mkfs/mkfs: mkfs/mkfs.c
	gcc -Werror -Wall -Wno-unused-but-set-variable -Imkfs -o mkfs/mkfs mkfs/mkfs.c

$U/sh: obj/usr/bin/sh
# $U/busybox: obj/usr/bin/busybox
$D/myadd: obj/dyn/bin/myadd

UPROGS=\
	$U/cat\
	$U/date\
	$U/echo\
	$U/grep\
	$U/init\
	$U/kill\
	$U/ln\
	$U/ls\
	$U/mkdir\
	$U/rm\
	$U/mysh\
	$U/wc\
	$U/zombie \
	$U/sigtest\
	$U/sigtest2\
	$U/sigtest3 \
	$U/bigtest \
	$U/mmaptest \
	$U/mmaptest3 \
	$U/forktest \
	$U/sh \
	$U/login \
	$U/passwd \
	$U/getty \
	$U/su \
	$U/mprotecttest \
	$U/busybox
DPROGS= \
	$D/hello_dyn \
	$D/myadd

$(DPROGS):
	(cd dyn; make)

fs.img: mkfs/mkfs test2.txt $(UPROGS) $(DPROGS)
	mkfs/mkfs fs.img test2.txt $(UPROGS) $(DPROGS)

-include kernel/*.d

clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	$U/initcode $U/initcode.out $K/kernel $K/kernel.bin $K/kernel.ld fs.img \
	mkfs/mkfs .gdbinit
#	*/*.o */*.d */*.asm */*.sym \
# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -kernel $K/kernel.bin -m 128M -smp $(CPUS) -nographic
#QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
#QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu: $K/kernel.bin fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $K/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)
