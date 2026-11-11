#include "core/Log.h"

#include <cstdarg>
#include <cstdio>

namespace fleet {

namespace {
LogSink g_sink = nullptr;

const char* LevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return "D";
    case LogLevel::Info: return "I";
    case LogLevel::Warn: return "W";
    case LogLevel::Error: return "E";
  }
  return "?";
}
}  // namespace

void SetLogSink(LogSink sink) { g_sink = sink; }

void Log(LogLevel level, const char* format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (g_sink) {
    g_sink(level, buffer);
  } else {
    std::fprintf(stderr, "[%s] %s\n", LevelName(level), buffer);
  }
}

}  // namespace fleet
