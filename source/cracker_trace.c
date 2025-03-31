#include "cracker_trace.h"

#include "cracker_strings.h"
#include "cracker.h"

/* **** */

#include "libarm/include/arm_ir.h"

/* **** */

#include <stdarg.h>
#include <stdio.h>

/* **** */

static
int __trace_comment_start(cracker_ref cj)
{
	if(!cj->core.trace.enabled) return(0);

	if(!cj->core.trace.comment) {
		printf("; /* ");

		cj->core.trace.comment = 1;
	}

	return(1);
}

static
void __trace_comment_end(cracker_ref cj)
{
	if(!cj->core.trace.enabled) return;
	if(!cj->core.trace.comment) return;

	printf(" */");

	cj->core.trace.comment = 0;
}

/* **** */

static
void __trace_end(cracker_ref cj)
{
	if(!cj->core.trace.started) return;

	__trace_comment_end(cj);

	printf(");\n");

	cj->core.trace.started = 0;
}

static
int __trace_start(cracker_ref cj)
{
	if(!cj->core.trace.enabled) return(0);

	if(!cj->core.trace.started) {
		const unsigned thumb = IP & 1;

		printf("%c(0x%08x, 0x%08x, %s, ",
			thumb ? 'T' : 'A', IP & ~(3 >> thumb), IR, CCs(1));

		cj->core.trace.started = 1;
	}

	return(1);
}

/* **** */

void _itrace(cracker_ref cj, const char* format, ...)
{
	if(!cj->core.trace.enabled) return;
	if(!cj->core.trace.started) return;

	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

void _itrace_comment(cracker_ref cj, const char* format, ...)
{
	if(!cj->core.trace.enabled) return;

	__trace_comment_start(cj);

	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	__trace_comment_end(cj);
}

void _itrace_comment_end(cracker_ref cj, const char* format, ...)
{
	if(!cj->core.trace.enabled) return;
	if(!cj->core.trace.comment) return;

	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	__trace_comment_end(cj);
}

int _itrace_comment_start(cracker_ref cj, const char* format, ...)
{
	if(!cj->core.trace.enabled) return(0);

	__trace_comment_start(cj);

	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	return(1);
}

void _itrace_end(cracker_ref cj, const char* format, ...)
{
	if(!cj->core.trace.enabled) return;
	if(!cj->core.trace.started) return;

	if(format) {
		va_list ap;
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);
	}

	__trace_end(cj);
}

void _itrace_end_with_comment(cracker_ref cj, const char* format, ...)
{
	if(!cj->core.trace.enabled) return;
	if(!cj->core.trace.started) return;

	__trace_comment_start(cj);

	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	__trace_end(cj);
}

void itrace(cracker_ref cj, const char* format, ...)
{
	if(!__trace_start(cj))
		return;

	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	__trace_end(cj);
}

int itrace_start(cracker_ref cj, const char* format, ...)
{
	if(!__trace_start(cj))
		return(0);

	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	return(1);
}
