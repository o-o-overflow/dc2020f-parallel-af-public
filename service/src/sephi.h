#ifndef SEPHI_H
#define SEPHI_H

#include <stdbool.h>

#include "types.h"

#define MAGIC_BYTES "sephiALD"
#define REFERENCE_MAX_SIZE 256

typedef struct {
   uint8_t magic_bytes[8];
   // num_constant are those destinations that SHOULD remain constant (i.e., not updated)
   uint16_t num_constant;
   // num_to_fix are those destinations that SHOULD be updated (used for labels: literals in instructions)
   uint16_t num_to_fix;
   // num_external_ref are those destinations that should be updated to the external reference
   uint16_t num_external_ref;
   // num_exported are those symbols that are exported and usable by others
   uint16_t num_exported;
} sephi_header;

typedef union {
   struct {
	  bool is_first_destination:1;
	  bool is_second_destination:1;
	  bool is_first_literal:1;
	  bool is_second_literal:1;
   };
   uint8_t raw;
} flags;

typedef struct {
   uint32_t instruction_number;
   flags flags;
} destination_to_update;

typedef struct {
   destination_to_update destination;
   char name[REFERENCE_MAX_SIZE];
} external_reference;

typedef struct {
   destination_type local_destination;
   char name[REFERENCE_MAX_SIZE];
} export_symbol;

#endif /* SEPHI_H */
