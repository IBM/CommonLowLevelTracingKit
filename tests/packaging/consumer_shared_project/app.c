/* Copyright (c) 2024, International Business Machines
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Drives the consumer plugin shared library so cross-.so tracing is exercised
 * end to end against the installed package.
 */

extern void plugin_emit(int value);

int main(void)
{
	plugin_emit(42);
	return 0;
}
