// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "unique_stack.h"

#include "crc8/crc8.h"
#include "md5/md5.h"

#if defined(__KERNEL__)
#include <linux/string.h>
#include <linux/types.h>
#else
#include <stddef.h>
#include <string.h>
#endif

#define RETURN_IF_INVALID(_FH_, ...) \
	if (!unique_stack_valid(_FH_))   \
	return __VA_ARGS__

static inline uint64_t body_offset(unique_stack_handler_t *fh)
{
	return fh->file_offset + sizeof(unique_stack_header_t);
}

bool unique_stack_valid(const unique_stack_handler_t *handler)
{
	return (handler != NULL) && (handler->valid);
}

static unique_stack_handler_t _unique_stack_open(file_t *fh, uint64_t file_offset, bool init)
{
	unique_stack_handler_t handler = {0};
	if (fh == NULL)
		return handler;

	handler.file = fh;
	handler.file_offset = file_offset;
	if (init) {
		unique_stack_header_t stack_header = {.version = 1};
		file_pwrite(fh, &stack_header, sizeof(stack_header), file_offset);
	}

	handler.valid = true;
	return handler;
}

unique_stack_handler_t unique_stack_init(file_t *fh, uint64_t file_offset)
{
	return _unique_stack_open(fh, file_offset, true);
}
unique_stack_handler_t unique_stack_open(file_t *fh, uint64_t file_offset)
{
	return _unique_stack_open(fh, file_offset, false);
}

void unique_stack_close(unique_stack_handler_t *handler)
{
	RETURN_IF_INVALID(handler);

	handler->valid = 0;
	handler->file = NULL;
	handler->file_offset = 0;
}

static uint64_t check_if_already_in_file(unique_stack_handler_t *handler, __uint128_t md5_hash)
{
	uint64_t offset_in_stack_body = 0;
	unique_stack_header_t header = {0};
	file_pread(handler->file, &header, sizeof(header), handler->file_offset);
	while (offset_in_stack_body < header.body_size) {
		// get next entry head
		entry_head_t head = {0};
		file_pread(handler->file, (void *)&head, sizeof(head),
				   body_offset(handler) + offset_in_stack_body);

		// check if the same
		if (head.md5_hash == md5_hash)
			return body_offset(handler) + offset_in_stack_body + sizeof(head);

		// increment to next entry
		offset_in_stack_body += sizeof(head) + head.body_size;
		continue;
	}

	return 0; // if searched to whole stack without any finding
}

static __uint128_t hash_function(const void *body, uint32_t size)
{
	MD5Context context = {0};
	md5Init(&context);
	md5Update(&context, (uint8_t *)&size, sizeof(size));
	md5Update(&context, body, size);
	md5Finalize(&context);
	__uint128_t md5_hash = 0;
	memcpy(&md5_hash, context.digest, sizeof(md5_hash));
	return md5_hash;
}

uint64_t unique_stack_add(unique_stack_handler_t *handler, const void *body, uint32_t size)
{
	RETURN_IF_INVALID(handler, 0);

	const __uint128_t md5_hash = hash_function(body, size);
	uint64_t file_offset = check_if_already_in_file(handler, md5_hash);
	if (file_offset > 0) {
		// already in stack
		return file_offset;
	} else {
		// not in stack, so add this
		entry_head_t entry_head = {
			.md5_hash = md5_hash,
			.body_size = size,
		};
		entry_head.crc = crc8_continue(0, (const uint8_t *)&entry_head, sizeof(entry_head) - 1);
		// add entry
		uint64_t stack_body_size = 0;
		const uint64_t stack_body_size_offset =
			handler->file_offset + offsetof(unique_stack_header_t, body_size);
		file_pread(handler->file, &stack_body_size, sizeof(stack_body_size),
				   stack_body_size_offset);

		const size_t base_offset = body_offset(handler) + stack_body_size;
		const size_t entry_head_offset = base_offset;
		const size_t body_offset = entry_head_offset + sizeof(entry_head);

		file_pwrite(handler->file, body, size, body_offset);
		file_pwrite(handler->file, &entry_head, sizeof(entry_head), entry_head_offset);

		stack_body_size += sizeof(entry_head) + size;
		file_pwrite(handler->file, &stack_body_size, sizeof(stack_body_size),
					stack_body_size_offset);
		// return offset of data in file
		return base_offset + sizeof(entry_head);
	}
}