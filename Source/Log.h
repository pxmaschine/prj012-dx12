/*
 * Log.h - contains all logging functionality
 * Copyright (c) 2025 Johannes Przybilla. All Rights Reserved.
 */

#pragma once

#include <BitFlags.h>
#include <CoreDefs.h>
#include <Format.h>
#include <Platform/PlatformContext.h>

//------------------------------------------------------------------------------------------------------------------------------------
// Logging macros
//------------------------------------------------------------------------------------------------------------------------------------

// Fatal Errors are fatal and are always presented to the user.
#define zv_fatal(msg, ...)                                                                                     \
  do                                                                                                           \
  {                                                                                                            \
    ZV::Log::Internal::error(msg, ZV::make_format_args(__VA_ARGS__), true, __FUNCTION__, __FILE__, __LINE__);  \
  } while (0)

#if ZV_DEBUG

// Errors are bad and potentially fatal.  They are presented as a dialog with Abort, Retry, and Ignore.  Abort will
// break into the debugger, retry will continue the game, and ignore will continue the game and ignore every subsequent
// call to this specific error.  They are ignored completely in release mode.
#define zv_error(msg, ...)                                                                                      \
  do                                                                                                            \
  {                                                                                                             \
    ZV::Log::Internal::error(msg, ZV::make_format_args(__VA_ARGS__), false, __FUNCTION__, __FILE__, __LINE__);  \
  } while (0)

// Warnings are recoverable.  They are just logs with the "WARNING" tag that displays calling information.  The flags
// are initially set to WARNINGFLAG_DEFAULT (defined in debugger.cpp), but they can be overridden normally.
#define zv_warning(msg, ...)                                                                                                                    \
  do                                                                                                                                            \
  {                                                                                                                                             \
    ZV::Log::Internal::log(ZV::Log::Internal::LogSeverity::Warning, msg, ZV::make_format_args(__VA_ARGS__), __FUNCTION__, __FILE__, __LINE__);  \
  } while (0)

// This is just a convenient macro for logging if you don't feel like dealing with tags.  It calls Log() with a tag
// of "INFO".  The flags are initially set to LOGFLAG_DEFAULT (defined in debugger.cpp), but they can be overridden
// normally.
#define zv_info(msg, ...)                                                                                                 \
  do                                                                                                                      \
  {                                                                                                                       \
    ZV::Log::Internal::log(ZV::Log::Internal::LogSeverity::Info, msg, ZV::make_format_args(__VA_ARGS__), NULL, NULL, 0);  \
  } while (0)

// This macro is used for logging and should be the preferred method of "printf debugging".  You can use any tag
// string you want, just make sure to enabled the ones you want somewhere in your initialization.
#define zv_log(msg, ...)                                                                                                 \
  do                                                                                                                     \
  {                                                                                                                      \
    ZV::Log::Internal::log(ZV::Log::Internal::LogSeverity::Log, msg, ZV::make_format_args(__VA_ARGS__), NULL, NULL, 0);  \
  } while (0)

#define zv_assert_msg(expr, msg, ...)                                                                             \
  do                                                                                                              \
  {                                                                                                               \
    if (!(expr))                                                                                                  \
    {                                                                                                             \
      ZV::Log::Internal::error(msg, ZV::make_format_args(__VA_ARGS__), false, __FUNCTION__, __FILE__, __LINE__);  \
    }                                                                                                             \
  } while (0)

#define zv_assert(expr) zv_assert_msg(expr, #expr)

#else

#define zv_error(...) (void)(0)
#define zv_warning(...) (void)(0)
#define zv_info(...) (void)(0)
#define zv_log(...) (void)(0)
#define zv_assert_msg(...) (void)(0)
#define zv_assert(...) (void)(0)

#endif // ZV_LOG_ENABLED

//------------------------------------------------------------------------------------------------------------------------------------
// Debugger macros
//------------------------------------------------------------------------------------------------------------------------------------

#if ZV_OS_WINDOWS
extern "C" __declspec(dllimport) void __stdcall DebugBreak(void);
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);

# define zv_debug_break() (::DebugBreak())

# define zv_is_debugger_attached() (::IsDebuggerPresent())

# define zv_breakpoint()           \
  do                               \
  {                                \
    if(zv_is_debugger_attached())  \
    {                              \
      zv_debug_break();            \
    }                              \
  } while(0)
#endif

//------------------------------------------------------------------------------------------------------------------------------------
// Logging initialization
//------------------------------------------------------------------------------------------------------------------------------------

namespace ZV
{
  namespace Log
  {
    void initialize();
    void shutdown();
  }
}

//------------------------------------------------------------------------------------------------------------------------------------
// Logging internal
//------------------------------------------------------------------------------------------------------------------------------------

namespace ZV
{
  namespace Log
  {
    namespace Internal
    {
      enum class LogFlag : u8
      {
        WriteToLogFile =  1 << 0,
        WriteToDebugger = 1 << 1,
        WriteToConsole =  1 << 2,
      };
      DEFINE_BITMASK_OPERATORS(LogFlag);
      using LogBitFlags = BitFlags<LogFlag>;

      enum class LogSeverity : u8
      {
        Fatal = 0,
        Error = 1,
        Warning = 2,
        Info = 3,
        Log = 4,
      };

      void log(LogSeverity severity, const String& message, Optional<FormatArgs> args, const char* func_name, const char* src_file, u32 line_num);
      void error(const String& error_message, Optional<FormatArgs> args, bool is_fatal, const char* func_name, const char* src_file, u32 line_num);
    }
  }
}
