#include <stdio.h>

/* **** */

#include "cracker.h"

/* **** */

#include <stdint.h>

/* **** */

#if 0
	#define _CORE_TRACE_(_f, ...) \
		({ \
			if(cj->core.trace) \
				printf(_f, ##__VA_ARGS__); \
		})

	#define CORE_TRACE(_f, ...) \
		({ \
			CORE_TRACE_START(); \
			CORE_TRACE_END(_f, ##__VA_ARGS__); \
		})

	#define CORE_TRACE_START(_f, ...) \
		({ \
			int _thumb = IP & 1; \
			uint32_t _ip = IP & (_thumb ? ~1 : ~3); \
			_CORE_TRACE_("%c(0x%08x, 0x%08x, %s, " _f, _thumb ? 'T' : 'A', _ip, IR, CCs(1), ##__VA_ARGS__); \
		})

	#define CORE_TRACE_END(_f, ...) \
		_CORE_TRACE_(_f ");\n", ##__VA_ARGS__);
#endif

/* **** */

void _itrace(cracker_ref cj, const char* format, ...);
void _itrace_comment(cracker_ref cj, const char* format, ...);
void _itrace_comment_end(cracker_ref cj, const char* format, ...);
int _itrace_comment_start(cracker_ref cj, const char* format, ...);
void _itrace_end(cracker_ref cj, const char* format, ...);
void _itrace_end_with_comment(cracker_ref cj, const char* format, ...);
void itrace(cracker_ref cj, const char* format, ...);
int itrace_start(cracker_ref cj, const char* format, ...);
