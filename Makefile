CC=~/opt/cross/bin/i686-elf-gcc
LDSCRIPT=linker.ld
LDFLAGS=-ffreestanding -O2 -nostdlib
CCFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra
KERNEL_IMAGE=kry_kern
C_SRC=$(wildcard *.c)
ASM_SRC=$(wildcard *.asm)
OBJ=$(C_SRC:.c=.o) $(ASM_SRC:.asm=.o)
NASM_FLAGS=-felf

all: $(KERNEL_IMAGE)
	@echo Done.

$(KERNEL_IMAGE): $(OBJ)
	@echo Linking kernel image "$(KERNEL)"...
	@$(CC) -T $(LDSCRIPT) -o $@ $(LDFLAGS) $^ -lgcc
	
%.o: %.c
	@echo [CC] $<
	@$(CC) -c $< -o $@ $(CCFLAGS)

%.o: %.asm
	@echo [ASM] $<
	@nasm $(NASM_FLAGS) -g -o $@ $<
	
clean:
	@rm -rf *.o
	@echo Cleaned up.
