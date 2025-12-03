// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/**
 * @brief Source type for trace origin identification
 *
 * Encoding uses 2 bits:
 *   00 = Unknown (legacy files or unspecified)
 *   01 = Userspace
 *   10 = Kernel
 *   11 = TTY (special case: kernel trace where buffer name is "TTY")
 */
typedef enum {
	DEFINITION_SOURCE_UNKNOWN = 0x00,
	DEFINITION_SOURCE_USERSPACE = 0x01,
	DEFINITION_SOURCE_KERNEL = 0x02,
	DEFINITION_SOURCE_TTY = 0x03, // Kernel trace with TTY buffer name
} definition_source_type_t;

/**
 * @brief Definition section header structure (V2 with CRC protection)
 *
 * Layout in file:
 *   [0-7]   body_size      - Total size of body (name + flags + crc)
 *   [8-15]  flags          - Source type in bits 0-1, rest reserved
 *   [16-N]  name           - Tracebuffer name (null-terminated)
 *   [N+1]   crc8           - CRC8 over flags + name (excluding body_size)
 *
 * Backwards compatibility:
 *   - Old decoders read body_size, then interpret entire body as name
 *   - They will see: [flags bytes as garbage][actual name][crc byte]
 *   - Since name is null-terminated, they stop at first null in flags (if any)
 *     or read garbage prefix before actual name
 *
 * Better approach for backwards compatibility:
 *   [0-7]   body_size      - Total size of body
 *   [8-N]   name           - Tracebuffer name (null-terminated)
 *   [N+1]   extended_magic - "CLLTK_EX" (8 bytes) to identify V2
 *   [N+9]   flags          - Source type in bits 0-1
 *   [N+10]  reserved       - 6 bytes reserved
 *   [N+17]  crc8           - CRC8 over entire body (name + magic + flags + reserved)
 *
 * This way old decoders see valid name, ignore rest after null terminator.
 */

#define DEFINITION_EXTENDED_MAGIC "CLLTK_EX"
#define DEFINITION_EXTENDED_MAGIC_SIZE 8
#define DEFINITION_VERSION 2

/**
 * @brief Definition section header (fixed part at start of section)
 */
struct definition_header_t;
typedef struct definition_header_t definition_header_t;
struct __attribute__((packed, aligned(8))) definition_header_t {
	uint64_t body_size; // Size of everything after this field
};

/**
 * @brief Extended definition fields (V2, placed after null-terminated name)
 */
struct definition_extended_t;
typedef struct definition_extended_t definition_extended_t;
struct __attribute__((packed)) definition_extended_t {
	char magic[DEFINITION_EXTENDED_MAGIC_SIZE]; // "CLLTK_EX"
	uint8_t version;							// Extended format version (2)
	uint8_t source_type;						// definition_source_type_t
	uint8_t _reserved[5];						// Reserved for future use
	uint8_t crc8;								// CRC8 over body (from name start to reserved end)
};

/**
 * @brief Calculate the total size needed for definition section
 *
 * @param name_length Length of name string (excluding null terminator)
 * @return size_t Total section size including header
 */
size_t definition_calculate_size(size_t name_length);

/**
 * @brief Calculate just the body size for definition section
 *
 * @param name_length Length of name string (excluding null terminator)
 * @return size_t Body size (what goes into body_size field)
 */
size_t definition_calculate_body_size(size_t name_length);

/**
 * @brief Initialize definition section in memory
 *
 * @param destination Pointer to memory where definition will be written
 * @param name Tracebuffer name (will be copied)
 * @param name_length Length of name (excluding null terminator)
 * @param source_type Source type (userspace, kernel, etc.)
 * @return true on success, false on failure
 */
bool definition_init(void *destination, const char *name, size_t name_length,
					 definition_source_type_t source_type);

/**
 * @brief Read source type from definition section
 *
 * @param definition Pointer to definition section in memory
 * @return definition_source_type_t Source type, UNKNOWN if not present or invalid
 */
definition_source_type_t definition_get_source_type(const void *definition);

/**
 * @brief Get name from definition section
 *
 * @param definition Pointer to definition section in memory
 * @return const char* Pointer to name string (null-terminated)
 */
const char *definition_get_name(const void *definition);

/**
 * @brief Validate definition section CRC (V2 only)
 *
 * @param definition Pointer to definition section in memory
 * @return true if valid or V1 format (no CRC), false if V2 with invalid CRC
 */
bool definition_validate_crc(const void *definition);

/**
 * @brief Check if definition section has extended format (V2)
 *
 * @param definition Pointer to definition section in memory
 * @return true if V2 format with extended fields
 */
bool definition_has_extended(const void *definition);

#ifdef __cplusplus
}
#endif
