// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _clltk_cmd_interface_HEADER__
#define _clltk_cmd_interface_HEADER__

#include "CLI/App.hpp"		  // IWYU pragma: export
#include "CLI/Config.hpp"	  // IWYU pragma: export
#include "CLI/Formatter.hpp"  // IWYU pragma: export
#include "CLI/Validators.hpp" // IWYU pragma: export
#if __has_include("CLI/ExtraValidators.hpp")
#include "CLI/ExtraValidators.hpp" // IWYU pragma: export
#endif
#include <CLI/Error.hpp>  // IWYU pragma: export
#include <CLI/Option.hpp> // IWYU pragma: export

#include <mutex>

typedef void (*init_fn)(void);
#define COMMAND_INIT(func) \
	__attribute__((retain, used, section("clltk_cmdinit"))) static init_fn _init_##func##_ptr = func

namespace CommonLowLevelTracingKit::cmd::interface
{
using MainAppHandle = std::pair<CLI::App &, std::unique_lock<std::mutex>>;
MainAppHandle acquireMainApp(void);

namespace validator
{
struct TracebufferName : public CLI::Validator {
	TracebufferName(void);
};

} // namespace validator

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif