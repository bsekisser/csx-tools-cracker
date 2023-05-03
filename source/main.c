#include "cracker_arm_ir.h"
#include "cracker_strings.h"
#include "cracker_trace.h"
#include "cracker.h"
#include "garmin.h"

/* **** */

#include "bitfield.h"
#include "err_test.h"
#include "log.h"

/* **** */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* **** */

static void load_content(cracker_content_p content, const char* file_name)
{
	char out[256];
	snprintf(out, 254, "%s%s%s", LOCAL_RGNDIR, RGNFileName, file_name);

	LOG("opening %s", out);

	const int fd = open(out, O_RDONLY);
	ERR(fd);

	struct stat sb;
	ERR(fstat(fd, &sb));

	void *data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	ERR_NULL(data);

	close(fd);

	content->data = data;
	content->base = 0x10020000;
	content->size = sb.st_size;
	content->end = content->base + content->size;

	LOG("Start: 0x%08x, End: 0x%08x",
		content->base, content->end);
}

int main(void)
{
	cracker_p cj = calloc(1, sizeof(cracker_t));

//	load_content(&cj->content, LOADER_FileName);
	load_content(&cj->content, FIRMWARE_FileName);

	cracker_data(cj, cj->content.end, sizeof(uint32_t));
	cracker_text(cj, cj->content.base);

	uint32_t src = cj->content.end;
	do {
		if(0xffffa55a == _read(cj, src, sizeof(uint32_t)))
			cracker_data(cj, src, sizeof(uint32_t));

		src--;
	} while(src > cj->content.base);
	
	PC = cj->content.base;
	IR = _read(cj, PC, sizeof(uint32_t));
	PC += sizeof(uint32_t);
	switch(IR) {
		case 0xea000002:
			cracker_data_ptr_read(cj, PC, sizeof(uint32_t)); /* end */
			PC += sizeof(uint32_t);
			cracker_data_ptr_read(cj, PC, sizeof(uint16_t)); /* hwid */
			PC += sizeof(uint32_t);
			cracker_data_ptr_read(cj, PC, sizeof(uint16_t)); /* sw_version */
			break;
		default:
			exit(-1);
	}

	for(cj->symbol_pass = 1; cj->symbol_count.added; cj->symbol_pass++)
//	for(cj->symbol_pass = 1; cj->symbol_pass <= 3; cj->symbol_pass++)
		cracker_pass(cj, 0);

	cj->collect_refs = 1;
	cj->symbol_pass = 0;

	cracker_pass(cj, 0);

	CORE_TRACE("/* **** **** **** **** */");

	printf("\n\n/* **** **** **** **** */\n\n");

	cracker_clear(cj);
	cracker_symbol_queue_log(cj, cj->symbol_qhead);

	free(cj);
	return(0);
}
