
#CROSS := /home/vagrant/riscv-gnu-toolchain/bin/riscv64-unknown-elf-
#CROSS := /usr/bin/riscv64-linux-gnu-
CROSS := /home/vagrant/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-musl-x86_64/bin/riscv64-unknown-linux-musl-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy
STRIP := $(CROSS)strip

MUSL := $(HOME)/musl-1.2.2/musl
#LIBC_A = $(MUSL)/lib/libc.a
#LIBC_SPEC = $(MUSL)/lib/musl-gcc.specs
#LIBC_LIBS = $(wildcard $(MUSL)/lib/*)
#LIBC_INCS = $(wildcard $(MUSL)/include/**/*)
#LIBC_DEPS = $(LIBC_LIBS) $(LIBC_INCS)

USR_CC := $(MUSL)/bin/musl-gcc

OBJ = ../obj/usr

# -z max-page-size: https://stackoverflow.com/questions/33005638/how-to-change-alignment-of-code-segment-in-elf
# T-Head拡張を使用
#CFLAGS = -march=rv64gcxthead -std=gnu99 -Os -static -MMD -MP -z max-page-size=4096 -fmerge-all-constants
CFLAGS = -std=gnu99 -Os -fno-pie -MMD -MP -z max-page-size=4096 -fmerge-all-constants
CFLAGS += -fno-omit-frame-pointer

LDFLAGS = -static -z norelro

BIN := $(OBJ)/bin
SRC := src

USER_DIRS := $(shell find $(SRC) -maxdepth 1 -mindepth 1 -type d)
USER_BINS := $(USER_DIRS:$(SRC)/%=$(BIN)/%)

all:
	$(MAKE) $(USER_BINS)

$(OBJ)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
#	$(USR_CC) $(CFLAGS) -c -o $@ $<
# Same as `bin/NAME: $(OBJ)/NAME/*.c.o $(LIBC_A)`
# NOTE: Static library should be at last.
.SECONDEXPANSION:
$(BIN)/%: $$(addsuffix .o, $$(addprefix $(OBJ)/, $$(shell find $(SRC)/% -name "*.c")))
	mkdir -p $(dir $@)
#	$(USR_CC) $(LDFLAGS) -T user.ld -o $@ $^
#	$(USR_CC) $(LDFLAGS) -o $@ $^
	$(CC) $(LDFLAGS) -o $@ $^
	$(OBJDUMP) -S -d $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).asm
	$(OBJDUMP) -x $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).hdr
	$(STRIP) --strip-debug $@

icode: initcode/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -c initcode/initcode.S -o initcode/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode/initcode.out initcode/initcode.o
#	$(LD) $(LDFLAGS) -N -e start -T user.ld -o initcode/initcode.out initcode/initcode.o
#	$(LD) $(LDFLAGS) -N -e start -o initcode/initcode.out initcode/initcode.o
	$(STRIP) initcode/initcode.out
	$(OBJCOPY) -S -O binary initcode/initcode.out initcode/initcode
	$(OBJDUMP) -S initcode/initcode.out > initcode/initcode.asm
	xxd -i initcode/initcode initcode/initcode.inc
clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
