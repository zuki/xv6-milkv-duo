ARCH := riscv64
#CROSS := /home/vagrant/duo-buildroot-sdk/host-tools/gcc/riscv64-elf-x86_64/bin/riscv64-unknown-elf-
#CROSS := /usr/bin/riscv64-linux-gnu-
CROSS := /home/vagrant/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-musl-x86_64/bin/riscv64-unknown-linux-musl-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy
STRIP := $(CROSS)strip

#MUSL = /home/vagrant/musl-1.2.2/musl
MUSL = /home/vagrant/musl_current
LIBC_LIBS = $(wildcard /home/vagrant/musl_current/lib*)
LIBC_INCS = $(wildcard /home/vagrant/musl_current/include/**/*)
LIBC_DEPS = $(LIBC_LIBS) $(LIBC_INCS)

OBJ = ../obj/dyn
USR_CC := $(MUSL)/bin/musl-gcc

CFLAGS = -std=gnu99 -O3 -z max-page-size=4096 -fpie

BIN := $(OBJ)/bin
SRC := src

USER_DIRS := $(shell find $(SRC) -maxdepth 1 -mindepth 1 -type d)
USER_BINS := $(USER_DIRS:$(SRC)/%=$(BIN)/%)

all:
	mkdir -p $(OBJ)
	$(MAKE) $(USER_BINS)

$(OBJ)/%.c.o: %.c
	mkdir -p $(dir $@)
#	$(USR_CC) $(CFLAGS) -c -o $@ $<
	$(CC) $(CFLAGS) -c -o $@ $<

# Same as `bin/NAME: $(OBJ)/NAME/*.c.o $(LIBC_A)`
.SECONDEXPANSION:
$(BIN)/%: $$(addsuffix .o, $$(addprefix $(OBJ)/, $$(shell find $(SRC)/% -name "*.c")))
	mkdir -p $(dir $@)
#	$(USR_CC) $(CFLAGS) -pie -o $@ $^
	$(CC) $(CFLAGS) -pie -o $@ $^
	$(OBJDUMP) -S -d $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).asm
	$(OBJDUMP) -x $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).hdr
	$(STRIP) --strip-debug $@

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
