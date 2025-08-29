/*
 * Log.cpp - contains all logging functionality
 * Copyright (c) 2025 Johannes Przybilla. All Rights Reserved.
 */
#include <Log.h>

#include <chrono>

#include <ThirdParty/fmt/include/fmt/chrono.h>

#include <Utility.h>

#if ZV_OS_WINDOWS
#include <windows.h>
#elif ZV_OS_MAC
#include <cstdio>
#include <CoreFoundation/CoreFoundation.h>
#endif

//------------------------------------------------------------------------------------------------------------------------------------
// Some constants
//------------------------------------------------------------------------------------------------------------------------------------

// the log filename
static const char *k_log_filename = "stdout";

// default display flags
#ifdef ZV_DEBUG
const ZV::Log::Internal::LogBitFlags k_fatalflag_default{ZV::Log::Internal::LogFlag::WriteToDebugger | ZV::Log::Internal::LogFlag::WriteToLogFile | ZV::Log::Internal::LogFlag::WriteToConsole};
const ZV::Log::Internal::LogBitFlags k_errorflag_default{ZV::Log::Internal::LogFlag::WriteToDebugger | ZV::Log::Internal::LogFlag::WriteToLogFile | ZV::Log::Internal::LogFlag::WriteToConsole};
const ZV::Log::Internal::LogBitFlags k_warningflag_default{ZV::Log::Internal::LogFlag::WriteToDebugger | ZV::Log::Internal::LogFlag::WriteToLogFile | ZV::Log::Internal::LogFlag::WriteToConsole};
const ZV::Log::Internal::LogBitFlags k_infoflag_default{ZV::Log::Internal::LogFlag::WriteToDebugger | ZV::Log::Internal::LogFlag::WriteToLogFile | ZV::Log::Internal::LogFlag::WriteToConsole};
const ZV::Log::Internal::LogBitFlags k_debugflag_default{ZV::Log::Internal::LogFlag::WriteToDebugger | ZV::Log::Internal::LogFlag::WriteToLogFile | ZV::Log::Internal::LogFlag::WriteToConsole};
// const ZV::Log::Internal::LogBitFlags k_externflag_default{ZV::Log::Internal::LogFlag::WriteToDebugger | ZV::Log::Internal::LogFlag::WriteToLogFile | ZV::Log::Internal::LogFlag::WriteToConsole};
#else
const ZV::Log::Internal::LogBitFlags k_fatalflag_default{0};
const ZV::Log::Internal::LogBitFlags k_errorflag_default{0};
const ZV::Log::Internal::LogBitFlags k_warningflag_default{0};
const ZV::Log::Internal::LogBitFlags k_infoflag_default{0};
const ZV::Log::Internal::LogBitFlags k_debugflag_default{0};
// const ZV::Log::Internal::LogBitFlags k_externflag_default{0};
#endif

namespace { class LogManager; }
static UniquePtr<LogManager> s_log_manager{ nullptr };

//------------------------------------------------------------------------------------------------------------------------------------
// Logging internal
//------------------------------------------------------------------------------------------------------------------------------------

namespace
{
  class LogManager : public Singleton<LogManager>
  {
  public:
    using LogSeverity = ZV::Log::Internal::LogSeverity;
    using LogBitFlags = ZV::Log::Internal::LogBitFlags;
    
    enum class ErrorDialogResult : u8
    {
      Abort,
      Retry,
      Ignore
    };

    struct Tag
    {
      LogBitFlags flags;
      ZV::FormatColor color = ZV::FormatColor::light_gray;
    };

  private:
    HashMap<LogSeverity, Tag> m_tags;

    // thread safety
    Mutex m_tag_mutex;

  public:
    LogManager();
    // void create(const char* logging_config_filename);

    // logs
    void log(LogSeverity severity, const String& message, Optional<ZV::FormatArgs> args, const char* func_name, const char* src_file, u32 line_num);
    ErrorDialogResult error(const String& error_message, Optional<ZV::FormatArgs> args, bool is_fatal, const char* func_name, const char* src_file, u32 line_num);
    void set_tag_config(LogSeverity severity, LogBitFlags flags, ZV::FormatColor color = ZV::FormatColor::light_gray);

  private:
    // log helpers
    void enable_virtual_terminal_processing();
    void attach_to_console();
    void output_final_buffer_to_logs(const String& final_buffer, LogBitFlags flags, ZV::FormatColor color);
    void write_to_log_file(const String& data) const;
    void get_output_buffer(String& out_output_buffer, LogSeverity severity, const String& message, Optional<ZV::FormatArgs> args, const char* func_name, const char* src_file, u32 line_num);
    constexpr StringView log_severity_to_string(LogSeverity severity);
  };
}

LogManager::LogManager()
  : BaseType(this)
{
#if ZV_DEBUG
  enable_virtual_terminal_processing();
#endif

  using LogSeverity = ZV::Log::Internal::LogSeverity;
  set_tag_config(LogSeverity::Fatal,   k_fatalflag_default,   ZV::FormatColor::orange_red);
  set_tag_config(LogSeverity::Error,   k_errorflag_default,   ZV::FormatColor::red);
  set_tag_config(LogSeverity::Warning, k_warningflag_default, ZV::FormatColor::yellow);
  set_tag_config(LogSeverity::Info,    k_infoflag_default,    ZV::FormatColor::light_gray);
  set_tag_config(LogSeverity::Log,     k_debugflag_default,   ZV::FormatColor::gray);

  ZV::print("\n");
}

/*
 * This function builds up the log string and outputs it to various places based on the display flags (m_displayFlags).
 */
void LogManager::log(LogSeverity severity, const String& message, Optional<ZV::FormatArgs> args, const char* func_name, const char* src_file, u32 line_num)
{
  m_tag_mutex.lock();

	auto find_it = m_tags.find(severity);
	if (find_it != m_tags.end())
	{
		String buffer;
		get_output_buffer(buffer, severity, message, args, func_name, src_file, line_num);
		output_final_buffer_to_logs(buffer, find_it->second.flags, find_it->second.color);

    m_tag_mutex.unlock();
	}
	else
	{
    m_tag_mutex.unlock();
	}
}

LogManager::ErrorDialogResult LogManager::error(const String& error_message, Optional<ZV::FormatArgs> args, bool is_fatal, const char* func_name, const char* src_file, u32 line_num)
{
	// buffer for our final output string
	String buffer;
	get_output_buffer(buffer, is_fatal ? LogSeverity::Fatal : LogSeverity::Error, error_message, args, func_name, src_file, line_num);

	// write the final buffer to all the various logs
	m_tag_mutex.lock();
	auto find_it = m_tags.find(is_fatal ? LogSeverity::Fatal : LogSeverity::Error);
	if (find_it != m_tags.end())
  {
		output_final_buffer_to_logs(buffer, find_it->second.flags, find_it->second.color);
  }
  m_tag_mutex.unlock();

#if ZV_OS_WINDOWS
  // show the dialog box
  int result = ::MessageBoxA(NULL, buffer.c_str(), is_fatal ? "FATAL" : "ERROR", MB_ABORTRETRYIGNORE|MB_ICONERROR|MB_DEFBUTTON3);

  // TODO: This is broken when not attached to a debugger!!!

	// act upon the choice
	switch (result)
	{
		case IDIGNORE : return ErrorDialogResult::Ignore;
		case IDABORT  : zv_debug_break(); return ErrorDialogResult::Retry;  // assembly language instruction to break into the debugger
		case IDRETRY :	return ErrorDialogResult::Retry;
		default :       return ErrorDialogResult::Retry;
	}
#endif
}

/*
 * Sets one or more display flags
 */
void LogManager::set_tag_config(LogSeverity severity, LogBitFlags flags, ZV::FormatColor color)
{
  m_tag_mutex.lock();
	if (flags != 0)
	{
		auto find_it = m_tags.find(severity);
		if (find_it == m_tags.end())
    {
			m_tags.emplace(severity, Tag{flags, color});
    }
		else
    {
			find_it->second = Tag{flags, color};
    }
	}
	else
	{
		m_tags.erase(severity);
	}
  m_tag_mutex.unlock();
}

constexpr StringView LogManager::log_severity_to_string(LogSeverity severity)
{
  switch (severity)
  {
    case LogSeverity::Fatal:   return "[Fatal]";
    case LogSeverity::Error:   return "[Error]";
    case LogSeverity::Warning: return "[Warn]";
    case LogSeverity::Info:    return "[Info]";
    case LogSeverity::Log:     return "";
    default:                   return "[Unknown]";
  }
}

/*
 * Fills out_output_buffer with the find error string.
 */
void LogManager::get_output_buffer(String& out_output_buffer, LogSeverity severity, const String& message, Optional<ZV::FormatArgs> args, const char* func_name, const char* src_file, u32 line_num)
{
  std::chrono::time_point now = std::chrono::system_clock::now();
  auto now_in_seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(now - now_in_seconds);
  const String timestamp = ZV::format("{}.{:03}", now_in_seconds, msec.count());

  if (args.has_value())
  {
    const String formatted = ZV::vformat(message, args.value());
    out_output_buffer = "[" + timestamp + "]" + String(log_severity_to_string(severity)) + " " + formatted;
  }
  else
  {
		out_output_buffer = "[" + timestamp + "]" + String(log_severity_to_string(severity)) + " " + message;
  }

	if (func_name != NULL)
	{
		out_output_buffer += "\nFunction: ";
		out_output_buffer += func_name;
	}

	if (src_file != NULL)
	{
		out_output_buffer += "\n";
		out_output_buffer += src_file;
	}

	if (line_num != 0)
	{
		out_output_buffer += "\nLine: ";
    char line_num_buffer[11];
    memset(line_num_buffer, 0, sizeof(char));
    if (_itoa_s(line_num, line_num_buffer, sizeof(line_num_buffer), 10) == 0)
    {
      out_output_buffer += line_num_buffer;
    }
	}

  out_output_buffer += "\n";
}

void LogManager::write_to_log_file(const String &data) const
{
  FILE *pLogFile = NULL;
  fopen_s(&pLogFile, k_log_filename, "a+");
  if (!pLogFile)
  {
    return; // can't write to the log file for some reason
  }

  fprintf_s(pLogFile, data.c_str());

  fclose(pLogFile);
}

/*
 * This is a helper function that writes the data string to the log file.
 *
 * IMPORTANT: The two places this function is called from wrap the code in the tag critical section (m_pTagCriticalSection), 
 * so that makes this call thread safe.  If you call this from anywhere else, make sure you wrap it in that critical section.
 */
void LogManager::output_final_buffer_to_logs(const String& final_buffer, LogBitFlags flags, ZV::FormatColor color)
{
	// Write the log to each display based on the display flags
	if (flags.is_set(ZV::Log::Internal::LogFlag::WriteToLogFile)) // log file
  {
		write_to_log_file(final_buffer);
  }
	if (flags.is_set(ZV::Log::Internal::LogFlag::WriteToDebugger))  // debugger output window
  {
#if ZV_OS_WINDOWS
    ::OutputDebugStringA(final_buffer.c_str());
#endif
  }
  if (flags.is_set(ZV::Log::Internal::LogFlag::WriteToConsole)) // console output
  {
    ZV::print_colored(color, final_buffer);
  }
}

void LogManager::enable_virtual_terminal_processing()
{
#if ZV_OS_WINDOWS
  if (!zv_is_debugger_attached())
  {
    attach_to_console();
  }

  HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE)
  {
      return;
  }

  DWORD dwMode = 0;
  if (!::GetConsoleMode(hOut, &dwMode))
  {
      return;
  }

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  ::SetConsoleMode(hOut, dwMode);
#endif
}

void LogManager::attach_to_console()
{
#if ZV_OS_WINDOWS
  if (AttachConsole(ATTACH_PARENT_PROCESS))
  {
    // Redirect standard output and error to the parent console
    FILE *dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
  }
  else
  {
    // Optional: fallback to a new console window if attaching fails
    AllocConsole();
    FILE *dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
  }
#endif
}

void ZV::Log::Internal::log(ZV::Log::Internal::LogSeverity severity, const String& message, Optional<FormatArgs> args, const char* func_name, const char* src_file, u32 line_num)
{
  LogManager::get().log(severity, message, args, func_name, src_file, line_num);
}

void ZV::Log::Internal::error(const String& error_message, Optional<FormatArgs> args, bool is_fatal, const char* func_name, const char* src_file, u32 line_num)
{
  static bool show_error = true;

  if (show_error)
  {
    if (LogManager::get().error(error_message, args, is_fatal, func_name, src_file, line_num) == LogManager::ErrorDialogResult::Ignore)
    {
      show_error = false;
    }
  }
}

//------------------------------------------------------------------------------------------------------------------------------------
// Logging initialization
//------------------------------------------------------------------------------------------------------------------------------------

void ZV::Log::initialize()
{
  zv_assert_msg(s_log_manager == nullptr, "Log manager already initialized!");
  s_log_manager = make_unique_ptr<LogManager>();
}

void ZV::Log::shutdown()
{
  s_log_manager = nullptr;
}
