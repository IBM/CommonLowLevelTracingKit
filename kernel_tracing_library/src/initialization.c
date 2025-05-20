// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"

#include "abstraction/memory.h"
#include <c-vector/vec.h>

#include <linux/elf.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

typedef struct {
	const char *name;
	_clltk_tracebuffer_handler_t **tracebuffers;
	char *body;
	uint32_t body_size;
	uint32_t used;
} tmp_elf_section_t;

static const uint64_t tb_prefix = 0x62746b746c6c635f; // _clltktb as uint64
static const uint64_t tp_prefix = 0x70746b746c6c635f; // _clltktp as uint64

static bool section_matcher(const tmp_elf_section_t *const *const section, const char *const name)
{
	return name != NULL && 0 == strcmp((*section)->name, name);
}

static tmp_elf_section_t *get_elf_section(tmp_elf_section_t ***sectiontable, const char *const name)
{
	const vector_entry_match_t match = vector_find(*sectiontable, section_matcher, name);
	if (match.found) {
		return *(tmp_elf_section_t **)match.entry;
	} else {
		tmp_elf_section_t *const section = kzalloc(sizeof(tmp_elf_section_t), GFP_KERNEL);
		section->name = name;
		section->tracebuffers = vector_create();
		section->body = kzalloc(10 * 4096, GFP_KERNEL);
		section->body_size = 10 * 4096;
		section->used = 0;
		vector_add(sectiontable, section);
		return section;
	}
}

static void free_elf_section(tmp_elf_section_t *section)
{
	vector_free(&section->tracebuffers);
	kfree(section->body);
	kfree(section);
}

static void add_tracebuffer_to_elf_section(tmp_elf_section_t *const section,
										   _clltk_tracebuffer_handler_t *const tracebuffer)
{
	vector_add(&section->tracebuffers, tracebuffer);
}

static uint32_t add_to_elf_section(tmp_elf_section_t *const section, const void *const ptr,
								   const uint32_t size)
{
	const uint32_t offset = section->used;
	const uint32_t new_used = (offset + size);
	const bool to_big_for_current = new_used > section->body_size;
	if (to_big_for_current) {
		const uint32_t new_size = max(new_used, section->body_size * 2);
		section->body = memory_heap_realloc(section->body, new_size);
	}
	memcpy(&section->body[offset], ptr, size);
	section->used += size;
	return offset;
}

static void use_elf_section(const tmp_elf_section_t *const section)
{
	for (int tb_index = 0; tb_index < vector_size(section->tracebuffers); tb_index++) {
		_clltk_tracebuffer_handler_t *const tb = section->tracebuffers[tb_index];
		if (tb->tracebuffer == NULL && tb->meta.start == NULL && tb->meta.stop == NULL) {
			tb->meta.start = section->body;
			tb->meta.stop = &section->body[section->used];
			_clltk_tracebuffer_init_handler(tb);
		}
	}
}

void _clltk_init_tracing_for_this_module(const struct mod_kallsyms *const allsyms)
{
	tmp_elf_section_t **sectionstable = vector_create();

	// search throuh all symbols to get all tracebuffers handlers and tracepoint meta data
	for (unsigned int i = 0; i < allsyms->num_symtab; i++) {
		const Elf_Sym *const ksym = &allsyms->symtab[i];
		const char *const name = &allsyms->strtab[ksym->st_name];
		void *const ptr = (void *)ksym->st_value;

		uint64_t nameU64 = 0;
		memcpy(&nameU64, name, sizeof(nameU64));
		if (nameU64 == tb_prefix) {
			// store each tracebuffer which uses this elf section
			_clltk_tracebuffer_handler_t *const tb = (_clltk_tracebuffer_handler_t *)ptr;
			if (tb->meta.file_offset == 0) {
				tmp_elf_section_t *const elf = get_elf_section(&sectionstable, tb->definition.name);
				add_tracebuffer_to_elf_section(elf, tb);
			}

		} else if (nameU64 == tp_prefix) {
			// add tracepoint meta data to temp elf section and set in section offset
			_clltk_kernel_meta_proxy_t *const tp = (_clltk_kernel_meta_proxy_t *)ptr;
			if (tp->added_to_elf == false) {
				const _clltk_tracebuffer_handler_t *const tb = tp->tracebuffer;
				tmp_elf_section_t *const elf = get_elf_section(&sectionstable, tb->definition.name);
				tp->in_section_offset = add_to_elf_section(elf, tp->meta.ptr, tp->meta.size);
				tp->added_to_elf = true;
			}
		}
	}

	// not add elf sections to tracebuffer and init tracebuffers
	for (size_t index = 0; index < vector_size(sectionstable); index++) {
		tmp_elf_section_t *const elf = sectionstable[index];
		use_elf_section(elf);
		free_elf_section(elf);
	}
	vector_free(&sectionstable);
}
EXPORT_SYMBOL(_clltk_init_tracing_for_this_module);

void _clltk_deinit_tracing_for_this_module(const struct mod_kallsyms *const allsyms)
{
	// deinit all tracebuffers
	for (unsigned int i = 0; i < allsyms->num_symtab; i++) {
		const Elf_Sym *const ksym = &allsyms->symtab[i];
		const char *const name = &allsyms->strtab[ksym->st_name];
		void *const ptr = (void *)ksym->st_value;
		uint64_t nameU64 = 0;
		memcpy(&nameU64, name, sizeof(nameU64));
		if (nameU64 == tb_prefix) {
			_clltk_tracebuffer_handler_t *const tb = (_clltk_tracebuffer_handler_t *)ptr;
			_clltk_tracebuffer_reset_handler(tb);
		}
	}
}
EXPORT_SYMBOL(_clltk_deinit_tracing_for_this_module);