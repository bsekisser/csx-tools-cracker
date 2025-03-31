#pragma once

/* **** */

#include "cracker.h"

#include "symbol.h"

/* **** */

#include <stdint.h>

/* **** */

symbol_ptr cracker_data(cracker_ref cj, uint32_t pat, size_t size, size_t len);
uint32_t cracker_data_ptr_read(cracker_ref cj, uint32_t pat, size_t size);
uint32_t cracker_data_read(cracker_ref cj, uint32_t pat, size_t size);
int cracker_data_read_if(cracker_ref cj, uint32_t pat, size_t size, uint32_t* data);
symbol_ptr cracker_data_string(cracker_ref cj, uint32_t pat);
symbol_ptr cracker_data_string_rel(cracker_ref cj, uint32_t pat);
