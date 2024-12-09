
#CROSS := /home/vagrant/riscv-gnu-toolchain/bin/riscv64-unknown-elf-
CROSS := /usr/bin/riscv64-linux-gnu-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy
STRIP := $(CROSS)strip

MUSL := $(HOME)/musl
USR_CC := $(MUSL)/bin/musl-gcc

OBJ = ../obj/usr

# -z max-page-size: https://stackoverflow.com/questions/33005638/how-to-change-alignment-of-code-segment-in-elf
CFLAGS = -std=gnu99 -Os -MMD -MP -static -z max-page-size=4096
#  -fno-omit-frame-pointer

BIN := $(OBJ)/bin
SRC := src

USER_DIRS := $(shell find $(SRC) -maxdepth 1 -mindepth 1 -type d)
USER_BINS := $(USER_DIRS:$(SRC)/%=$(BIN)/%)

all:
	$(MAKE) $(USER_BINS)

$(OBJ)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(USR_CC) $(CFLAGS) -T user.ld -c -o $@ $<

# Same as `bin/NAME: $(OBJ)/NAME/*.c.o $(LIBC_A)`
# NOTE: Static library should be at last.
.SECONDEXPANSION:
$(BIN)/%: $$(addsuffix .o, $$(addprefix $(OBJ)/, $$(shell find $(SRC)/% -name "*.c")))
	mkdir -p $(dir $@)
	$(USR_CC) $(CFLAGS) -T user.ld -o $@ $^
#	$(USR_CC) $(CFLAGS) -o $@ $^
	$(OBJDUMP) -S -d $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).asm
	$(OBJDUMP) -x $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).hdr
	$(STRIP) --strip-debug $@

icode: initcode/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -c initcode/initcode.S -o initcode/initcode.o
#	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode/initcode.out initcode/initcode.o
	$(LD) $(LDFLAGS) -N -e start -T user.ld -o initcode/initcode.out initcode/initcode.o
#	$(LD) $(LDFLAGS) -N -e start -o initcode/initcode.out initcode/initcode.o
	$(OBJCOPY) -S -O binary initcode/initcode.out initcode/initcode
	$(OBJDUMP) -S initcode/initcode.out > initcode/initcode.asm
	xxd -i initcode/initcode initcode/initcode.inc
clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
