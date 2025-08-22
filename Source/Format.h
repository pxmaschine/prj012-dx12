/*
 * Format.h - fmt wrappers
 * Copyright (c) 2024 Johannes Przybilla. All Rights Reserved.
 */

#pragma once

#include <Platform/PlatformContext.h>

#if ZV_COMPILER_CL
#pragma warning(disable: 4541)  // dynamic_cast on polymorphic type with /GR-;
#endif

#include <ThirdParty/fmt/include/fmt/color.h>
#include <ThirdParty/fmt/include/fmt/core.h>

#include <CoreDefs.h>

namespace ZV
{
  using FormatContext = fmt::format_context;
  using FormatArgs = fmt::format_args;
  using FormatColor = fmt::color;

  template<typename Context, typename ...Args>
  using FormatArgStorage = fmt::format_arg_store<Context, fmt::remove_cvref_t<Args>...>;

  template<typename ...Args>
  FormatArgStorage<FormatContext, fmt::remove_cvref_t<Args>...> make_format_args(Args&&... args)
  {
    return fmt::make_format_args(args...);
  }

  inline String vformat(StringView fmt, FormatArgs args)
  {
    return fmt::vformat(fmt, args);
  }

  template <typename... Args>
  String format(StringView fmt_str, Args&&... args) 
  {
    return String(fmt::format(fmt_str.data(), std::forward<Args>(args)...));
  }

  inline void print_colored(FormatColor color, const String& str)
  {
    fmt::print(fmt::fg(color), "{}", str);
  }

  inline void print(const String& str)
  {
    print_colored(FormatColor::light_gray, str);
  }
}
