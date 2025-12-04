// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "definition.h"
#include "crc8/crc8.h"

#if defined(__KERNEL__)
#include <linux/string.h>
#else
#include <string.h>
#endif

/**
 * @brief Find the length of the name in a definition body.
 * @param header Pointer to the definition header
 * @param body Pointer to the definition body (after header)
 * @return Length of the name (excluding null terminator)
 */
static size_t find_name_length(const definition_header_t *header, const char *body)
{
	size_t name_length = 0;
	while (name_length < header->body_size && body[name_length] != '\0') {
		name_length++;
	}
	return name_length;
}

size_t definition_calculate_body_size(size_t name_length)
{
	// body = name (with null terminator) + extended fields
	return (name_length + 1) + sizeof(definition_extended_t);
}

size_t definition_calculate_size(size_t name_length)
{
	// header + body
	return sizeof(definition_header_t) + definition_calculate_body_size(name_length);
}

bool definition_init(void *destination, const char *name, size_t name_length,
					 definition_source_type_t source_type)
{
	if (destination == NULL || name == NULL || name_length == 0) {
		return false;
	}

	uint8_t *ptr = (uint8_t *)destination;

	// Write header (body_size field)
	uint64_t body_size = definition_calculate_body_size(name_length);
	memcpy(ptr, &body_size, sizeof(body_size));
	ptr += sizeof(definition_header_t);

	// Write name (null-terminated)
	const uint8_t *body_start = ptr; // For CRC calculation
	memcpy(ptr, name, name_length);
	ptr[name_length] = '\0';
	ptr += name_length + 1;

	// Write extended fields
	definition_extended_t extended = {0};
	memcpy(extended.magic, DEFINITION_EXTENDED_MAGIC, DEFINITION_EXTENDED_MAGIC_SIZE);
	extended.version = DEFINITION_VERSION;
	extended.source_type = (uint8_t)source_type;
	// extended._reserved is already zero from initialization

	// Calculate CRC over body (name + magic + version + source_type + reserved)
	// Exclude the crc8 field itself
	extended.crc8 = crc8_continue(0, body_start, (name_length + 1));
	extended.crc8 =
		crc8_continue(extended.crc8, (const uint8_t *)&extended, sizeof(definition_extended_t) - 1);

	// Write extended struct to destination
	memcpy(ptr, &extended, sizeof(extended));

	return true;
}

bool definition_has_extended(const void *definition)
{
	if (definition == NULL) {
		return false;
	}

	const definition_header_t *header = (const definition_header_t *)definition;
	const char *body = (const char *)definition + sizeof(definition_header_t);

	// Find end of name (null terminator)
	size_t name_length = find_name_length(header, body);

	// Check if there's enough space after name for extended fields
	const size_t remaining = header->body_size - (name_length + 1);
	if (remaining < sizeof(definition_extended_t)) {
		return false;
	}

	// Check for extended magic
	const char *extended_ptr = body + name_length + 1;
	return memcmp(extended_ptr, DEFINITION_EXTENDED_MAGIC, DEFINITION_EXTENDED_MAGIC_SIZE) == 0;
}

definition_source_type_t definition_get_source_type(const void *definition)
{
	if (!definition_has_extended(definition)) {
		return DEFINITION_SOURCE_UNKNOWN;
	}

	const definition_header_t *header = (const definition_header_t *)definition;
	const char *body = (const char *)definition + sizeof(definition_header_t);

	// Find end of name
	size_t name_length = find_name_length(header, body);

	const definition_extended_t *extended = (const definition_extended_t *)(body + name_length + 1);

	// Validate source type value
	uint8_t source = extended->source_type;
	if (source > DEFINITION_SOURCE_TTY) {
		return DEFINITION_SOURCE_UNKNOWN;
	}

	return (definition_source_type_t)source;
}

const char *definition_get_name(const void *definition)
{
	if (definition == NULL) {
		return NULL;
	}

	return (const char *)definition + sizeof(definition_header_t);
}

bool definition_validate_crc(const void *definition)
{
	if (!definition_has_extended(definition)) {
		// V1 format has no CRC, consider valid for backwards compatibility
		return true;
	}

	const definition_header_t *header = (const definition_header_t *)definition;
	const uint8_t *body = (const uint8_t *)definition + sizeof(definition_header_t);

	// Find end of name
	size_t name_length = find_name_length(header, (const char *)body);

	const definition_extended_t *extended = (const definition_extended_t *)(body + name_length + 1);

	// Calculate CRC over body (name + extended fields excluding crc8)
	uint8_t calculated_crc = crc8_continue(0, body, name_length + 1);
	calculated_crc =
		crc8_continue(calculated_crc, (const uint8_t *)extended, sizeof(definition_extended_t) - 1);

	return calculated_crc == extended->crc8;
}
