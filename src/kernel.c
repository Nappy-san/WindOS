#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "serial.h"
#include "timer.h"
#include "interrupts.h"
#include "gpio.h"
#include "debug.h"
#include "ext2.h"
#include "storage_driver.h"

#define GPIO_LED_PIN 47

// Arbitrarily high adress so it doesn't conflict with something else.
// = 8MB
uint32_t __ramdisk = 0x0800000;

int memory_read(uint32_t address, void* buffer, uint32_t size) {
	uint32_t base = 0x10000 + __ramdisk; // The FS is concatenated with the kernel image.
	//TODO: don't hardcode that, because in real hardware the offset is 0x8000
	memcpy(buffer, (void*) (intptr_t) (address + base), size);
//	kernel_info("kernel.c","Disk access at address");
//	print_hex(address,2);

	return 0;
}

int memory_write(uint32_t address, void* buffer, uint32_t size) {
	uint32_t base = 0x10000 + __ramdisk;
	memcpy((void*) (intptr_t) (address + base), buffer, size);
	return 0;
}

void tree(superblock_t* fs, int inode, int depth) {
	dir_list_t* result = ext2_lsdir(fs, inode);

	while(result != 0) {
		if (result->name[0] != '.') {
			for (int i=0;i<depth;i++) {
				serial_write("| ");
			}
			if (result->attr == EXT2_ENTRY_DIRECTORY )
				serial_write("|=");
			else
				serial_write("|-");
			serial_putc('>');
			serial_write(result->name);
			serial_newline();
			if (result->attr == EXT2_ENTRY_DIRECTORY ) {
				tree(fs, result->val, depth+1);
			}
		}
		result = result->next;
	}
}

void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags) {
	(void) r0;
	(void) r1;
	(void) atags;

  char* a = (char*)malloc(100*sizeof(char));
  a[10] = 4;
	for (int i=0;i<100;i++) {
		a[i] = 0x41+i;
	}
	a[20] = 0;
	serial_init();

	serial_write(a);
	serial_newline();

	kernel_printf("[INFO][SERIAL] Serial output is hopefully ON.\n");

	storage_driver memorydisk;
	memorydisk.read = memory_read;
	memorydisk.write = memory_write;


	superblock_t* fsroot = ext2fs_initialize(&memorydisk);
	if (fsroot != 0) {
		dir_list_t* result = ext2_lsdir(fsroot, fsroot->root);
		char* target = "fat";

		while(result != 0) {
			if (strcmp(result->name, target) == 0) {
				kernel_printf("[INFO][KERNEL] Watch out! I'm reading the file %s:\n\n", target);
				ext2_inode_t data = ext2_get_inode_descriptor(fsroot, result->val);
				char* buffer = (char*) malloc(data.size+1);
				buffer[data.size] = 0;
				ext2_fread(fsroot, result->val, buffer, 81, 2048);
				kernel_printf("%s\n", buffer);
			}


			result = result->next;
		}
		//tree(fsroot, fsroot->root, 0);
	}
	while(1) {}
}
