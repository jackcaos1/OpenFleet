#pragma once
// Minimales Logging. Die Plattform (Desktop/Android) setzt eine Senke, der Kern kennt keine Plattform.

namespace fleet {

enum class LogLevel { Debug, Info, Warn, Error };

using LogSink = void (*)(LogLevel level, const char* message);

// Ohne gesetzte Senke schreibt der Kern nach stderr.
void SetLogSink(LogSink sink);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void Log(LogLevel level, const char* format, ...);

}  // namespace fleet

#define FLEET_LOGD(...) ::fleet::Log(::fleet::LogLevel::Debug, __VA_ARGS__)
#define FLEET_LOGI(...) ::fleet::Log(::fleet::LogLevel::Info, __VA_ARGS__)
#define FLEET_LOGW(...) ::fleet::Log(::fleet::LogLevel::Warn, __VA_ARGS__)
#define FLEET_LOGE(...) ::fleet::Log(::fleet::LogLevel::Error, __VA_ARGS__)
