// Android-Plattformschicht (NativeActivity): besitzt EGL-Kontext und Eingabe, ruft den Spielkern auf.
// Der Kern selbst kennt kein Android.

#include <cstdio>
#include <EGL/egl.h>
#include <jni.h>
#include <aaudio/AAudio.h>
#include <android/input.h>
#include <android/native_activity.h>
#include <android/window.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <time.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <cstring>
#include <vector>
#include <string>

#include "core/Log.h"
#include "SafSource.h"
#include "io/OverlaySource.h"
#include "io/ZipSource.h"
#include <sys/stat.h>
#include <unistd.h>
#include "game/Game.h"

namespace {

void AndroidLogSink(fleet::LogLevel level, const char* message) {
  int prio = ANDROID_LOG_INFO;
  switch (level) {
    case fleet::LogLevel::Debug: prio = ANDROID_LOG_DEBUG; break;
    case fleet::LogLevel::Info: prio = ANDROID_LOG_INFO; break;
    case fleet::LogLevel::Warn: prio = ANDROID_LOG_WARN; break;
    case fleet::LogLevel::Error: prio = ANDROID_LOG_ERROR; break;
  }
  __android_log_print(prio, "fleetclient", "%s", message);
}

double NowSeconds() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

// Brueckt GameHost zu den Java-Methoden der MainActivity.
class AndroidHost : public fleet::GameHost {
 public:
  AndroidHost(JNIEnv* env, jobject activity) : env_(env), activity_(activity) {
    jclass cls = env->GetObjectClass(activity);
    show_message_ = env->GetMethodID(cls, "showMessage", "(Ljava/lang/String;)V");
    pick_folder_ = env->GetMethodID(cls, "pickFolder", "()V");
    generation_ = env->GetMethodID(cls, "dataGeneration", "()I");
    has_folder_ = env->GetMethodID(cls, "hasFolder", "()Z");
    zip_count_ = env->GetMethodID(cls, "zipCount", "()I");
    open_zip_fd_ = env->GetMethodID(cls, "openZipFd", "(I)I");
    env->DeleteLocalRef(cls);
  }
  void ShowMessage(const std::string& text) override {
    jstring js = env_->NewStringUTF(text.c_str());
    env_->CallVoidMethod(activity_, show_message_, js);
    env_->DeleteLocalRef(js);
    Clear();
  }
  void RequestQuit() override { quit_requested = true; }
  void SetKeepScreenOn(bool on) override {
    if (native_) ANativeActivity_setWindowFlags(native_, on ? AWINDOW_FLAG_KEEP_SCREEN_ON : 0, AWINDOW_FLAG_KEEP_SCREEN_ON);
  }
  ANativeActivity* native_ = nullptr;
  bool quit_requested = false;
  void RequestDataFolder() override {
    env_->CallVoidMethod(activity_, pick_folder_);
    Clear();
  }
  int Generation() {
    const int g = env_->CallIntMethod(activity_, generation_);
    Clear();
    return g;
  }
  int ZipCount() {
    const int n = env_->CallIntMethod(activity_, zip_count_);
    Clear();
    return n;
  }
  int OpenZipFd(int i) {
    const int fd = env_->CallIntMethod(activity_, open_zip_fd_, i);
    Clear();
    return fd;
  }
  bool HasFolder() {
    const bool b = env_->CallBooleanMethod(activity_, has_folder_) == JNI_TRUE;
    Clear();
    return b;
  }

 private:
  void Clear() {
    if (env_->ExceptionCheck()) env_->ExceptionClear();
  }
  JNIEnv* env_;
  jobject activity_;
  jmethodID show_message_, pick_folder_, generation_, has_folder_, zip_count_, open_zip_fd_;
};

// Dateideskriptor einer ZIP (gehoert danach dem Quellobjekt). Gelesen wird mit pread; geht das nicht (Pipe o. ae.), wird die
// Datei einmal komplett in den Speicher geladen.
struct FdHolder {
  int fd;
  explicit FdHolder(int f) : fd(f) {}
  ~FdHolder() { if (fd >= 0) close(fd); }
};

std::shared_ptr<fleet::FileSource> OpenZipFromFd(int fd) {
  if (fd < 0) return nullptr;
  auto holder = std::make_shared<FdHolder>(fd);
  struct stat st;
  if (fstat(fd, &st) != 0 || st.st_size <= 0) return nullptr;
  const uint64_t size = static_cast<uint64_t>(st.st_size);
  std::string err;
  auto z = fleet::ZipSource::Open([holder](uint64_t off, void* buf, size_t n) {
    size_t done = 0;
    while (done < n) {
      const ssize_t r = pread(holder->fd, static_cast<char*>(buf) + done, n - done, static_cast<off_t>(off + done));
      if (r <= 0) return false;
      done += static_cast<size_t>(r);
    }
    return true;
  }, size, &err);
  if (z) return z;
  if (size > (1ull << 30)) return nullptr;
  // Rueckfall: komplett laden
  auto data = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(size));
  lseek(fd, 0, SEEK_SET);
  size_t done = 0;
  while (done < data->size()) {
    const ssize_t r = read(fd, data->data() + done, data->size() - done);
    if (r <= 0) break;
    done += static_cast<size_t>(r);
  }
  if (done != data->size()) return nullptr;
  return fleet::ZipSource::Open([data](uint64_t off, void* buf, size_t n) {
    if (off + n > data->size()) return false;
    std::memcpy(buf, data->data() + off, n);
    return true;
  }, data->size(), &err);
}

struct App {
  android_app* app = nullptr;
  AndroidHost* host = nullptr;
  std::shared_ptr<fleet::FileSource> source;
  int seen_generation = -1;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  EGLConfig config = nullptr;
  std::unique_ptr<fleet::Game> game;
  int width = 0;
  int height = 0;
  bool focused = false;
  double last_time = 0.0;

  bool HasSurface() const { return surface != EGL_NO_SURFACE; }

  bool InitEgl() {
    if (context != EGL_NO_CONTEXT && display != EGL_NO_DISPLAY) {
      // Wieder im Vordergrund: Kontext (und damit alle GL-Objekte und der Spielstand) bleibt, nur die Flaeche ist neu.
      surface = eglCreateWindowSurface(display, config, app->window, nullptr);
      if (surface == EGL_NO_SURFACE || !eglMakeCurrent(display, surface, surface, context)) {
        FLEET_LOGE("EGL: neue Flaeche konnte nicht erstellt werden");
        return false;
      }
      return true;
    }
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
      FLEET_LOGE("EGL: Display/Initialisierung fehlgeschlagen");
      return false;
    }
    const EGLint config_attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                     EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 0,
                                     EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLint num_configs = 0;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs) || num_configs < 1) {
      FLEET_LOGE("EGL: keine passende Konfiguration (OpenGL ES 3)");
      return false;
    }
    surface = eglCreateWindowSurface(display, config, app->window, nullptr);
    const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
      FLEET_LOGE("EGL: Surface/Kontext konnten nicht erstellt werden");
      return false;
    }
    return true;
  }

  void TermEgl() {
    if (display != EGL_NO_DISPLAY) {
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
      if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
      eglTerminate(display);
    }
    display = EGL_NO_DISPLAY;
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
  }

  // Ton ueber AAudio: Der Mischer des Spielkerns wird aus dem Audio-Thread aufgerufen.
  AAudioStream* audio = nullptr;
  static aaudio_data_callback_result_t AudioCallback(AAudioStream*, void* user, void* data, int32_t frames) {
    static_cast<fleet::Game*>(user)->MixAudio(static_cast<int16_t*>(data), frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }
  void StartAudio() {
    if (audio || !game) return;
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setSampleRate(b, game->audio_rate());
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_POWER_SAVING);
    AAudioStreamBuilder_setDataCallback(b, &App::AudioCallback, game.get());
    if (AAudioStreamBuilder_openStream(b, &audio) == AAUDIO_OK) AAudioStream_requestStart(audio);
    else FLEET_LOGW("AAudio: Stream konnte nicht geoeffnet werden");
    AAudioStreamBuilder_delete(b);
  }
  void StopAudio() {
    if (!audio) return;
    AAudioStream_requestStop(audio);
    AAudioStream_close(audio);
    audio = nullptr;
  }

  // Spielstand beim Verlassen der App (state.bzn, Originalformat): wird beim naechsten Start geladen und geloescht.
  std::string StatePath() const { return std::string(app->activity->internalDataPath) + "/state.bzn"; }

  void SaveStateFile() {
    if (!game) return;
    std::vector<uint8_t> bytes;
    if (!game->SaveState(&bytes)) {
      std::remove(StatePath().c_str());
      return;
    }
    const std::string tmp = StatePath() + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    std::fclose(f);
    if (ok) std::rename(tmp.c_str(), StatePath().c_str());
    else std::remove(tmp.c_str());
  }

  void LoadStateFile() {
    FILE* f = std::fopen(StatePath().c_str(), "rb");
    if (!f) return;
    std::vector<uint8_t> bytes;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    std::remove(StatePath().c_str());  // nach dem Laden wird die Datei geloescht
    if (!bytes.empty()) game->LoadStateLater(std::move(bytes));
  }

  void OnWindowCreated() {
    const bool resume = game != nullptr && context != EGL_NO_CONTEXT;
    if (!InitEgl()) {
      FullShutdown();
      return;
    }
    if (!resume) {
      game = std::make_unique<fleet::Game>();
      game->SetHost(host);
      std::remove((std::string(app->activity->internalDataPath) + "/state.txt").c_str());  // Rest des alten Autosaves
      LoadStateFile();
      seen_generation = -1;  // Datenquelle im naechsten Durchlauf setzen
      if (!game->OnSurfaceCreated()) FLEET_LOGE("Spielkern: Grafik-Initialisierung fehlgeschlagen");
    }
    width = height = 0;
    last_time = NowSeconds();
    StartAudio();
  }

  // App geht in den Hintergrund: nur die Bildflaeche wird abgebaut, Kontext und Spiel bleiben erhalten.
  void OnWindowDestroyed() {
    StopAudio();
    if (display != EGL_NO_DISPLAY) {
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    }
    surface = EGL_NO_SURFACE;
  }

  // Ende der Activity (oder verlorener Kontext): alles freigeben.
  void FullShutdown() {
    StopAudio();
    if (game && context != EGL_NO_CONTEXT && display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) eglMakeCurrent(display, surface, surface, context);
    if (game) game->OnSurfaceDestroyed();
    game.reset();
    TermEgl();
  }

  // Prueft, ob der Nutzer einen (neuen) Spielordner gewaehlt hat.
  void SyncDataSource() {
    if (!game || !host) return;
    const int g = host->Generation();
    if (g == seen_generation) return;
    seen_generation = g;
    // Quellen: ZIPs in der gewaehlten Reihenfolge (oben gewinnt), danach der Ordner
    auto overlay = std::make_shared<fleet::OverlaySource>();
    int layers = 0;
    const int zc = host->ZipCount();
    for (int i = 0; i < zc; ++i) {
      auto z = OpenZipFromFd(host->OpenZipFd(i));
      if (z) {
        overlay->Add(z);
        ++layers;
      } else {
        FLEET_LOGW("ZIP %d nicht lesbar", i + 1);
      }
    }
    if (host->HasFolder() && source) {
      overlay->Add(source);
      ++layers;
    }
    if (layers > 0) game->SetDataSource(overlay);
  }

  void Frame() {
    SyncDataSource();
    EGLint w = 0, h = 0;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    if (w != width || h != height) {
      width = w;
      height = h;
      game->OnResize(w, h);
    }
    const double now = NowSeconds();
    game->OnFrame(now - last_time);
    last_time = now;
    if (!eglSwapBuffers(display, surface) && eglGetError() == EGL_CONTEXT_LOST) {
      // Kontext vom System entzogen (selten): Spielstand ist nicht zu retten, neu aufbauen
      FullShutdown();
      if (app->window) OnWindowCreated();
    }
  }
};

void OnAppCmd(android_app* app, int32_t cmd) {
  auto* self = static_cast<App*>(app->userData);
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      if (app->window) self->OnWindowCreated();
      break;
    case APP_CMD_TERM_WINDOW:
      self->OnWindowDestroyed();
      break;
    case APP_CMD_GAINED_FOCUS:
      self->focused = true;
      if (self->audio) AAudioStream_requestStart(self->audio);
      self->last_time = NowSeconds();
      break;
    case APP_CMD_PAUSE:  // App verlaesst den Vordergrund: Spiel steht (keine Bilder mehr), Stand sichern
      self->SaveStateFile();
      break;
    case APP_CMD_RESUME:  // Prozess lebt noch: das laufende Spiel geht weiter, die Datei wird nicht gebraucht
      std::remove(self->StatePath().c_str());
      self->last_time = NowSeconds();
      break;
    case APP_CMD_STOP:
      break;
    case APP_CMD_LOST_FOCUS:
      self->focused = false;
      if (self->audio) AAudioStream_requestPause(self->audio);
      break;
    default:
      break;
  }
}

int32_t OnInput(android_app* app, AInputEvent* event) {
  auto* self = static_cast<App*>(app->userData);
  if (!self->game || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
  const int32_t action = AMotionEvent_getAction(event);
  const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
  const size_t index = static_cast<size_t>((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                           AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
  fleet::PointerEvent p;
  if (masked == AMOTION_EVENT_ACTION_MOVE) {  // eine Bewegung betrifft alle aufliegenden Finger
    p.action = fleet::PointerEvent::Action::Move;
    const size_t count = AMotionEvent_getPointerCount(event);
    for (size_t i = 0; i < count; ++i) {
      p.x = AMotionEvent_getX(event, i);
      p.y = AMotionEvent_getY(event, i);
      p.id = AMotionEvent_getPointerId(event, i);
      self->game->OnPointer(p);
    }
    return 1;
  }
  switch (masked) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      p.action = fleet::PointerEvent::Action::Down;
      break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
    case AMOTION_EVENT_ACTION_CANCEL:
      p.action = fleet::PointerEvent::Action::Up;
      break;
    default:
      return 0;
  }
  p.x = AMotionEvent_getX(event, index);
  p.y = AMotionEvent_getY(event, index);
  p.id = AMotionEvent_getPointerId(event, index);
  self->game->OnPointer(p);
  return 1;
}

}  // namespace

void android_main(android_app* app) {
  fleet::SetLogSink(AndroidLogSink);
  FLEET_LOGI("fleetclient M0 gestartet");
  JNIEnv* env = nullptr;
  app->activity->vm->AttachCurrentThread(&env, nullptr);
  AndroidHost host(env, app->activity->clazz);  // clazz ist hier die Activity-Instanz
  host.native_ = app->activity;
  App state;
  state.host = &host;
  state.source = std::shared_ptr<fleet::FileSource>(fleet::OpenSafFolder(app->activity->vm, app->activity->clazz));
  state.app = app;
  app->userData = &state;
  app->onAppCmd = OnAppCmd;
  app->onInputEvent = OnInput;

  while (true) {
    int events = 0;
    android_poll_source* source = nullptr;
    // Timeout bei jedem Durchlauf neu bestimmen: ohne Surface/Fokus warten, sonst nicht blockieren.
    for (;;) {
      const bool animating = state.HasSurface() && state.focused && state.game;
      if (ALooper_pollOnce(animating ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&source)) < 0) break;
      if (source) source->process(app, source);
      if (app->destroyRequested) {
        state.FullShutdown();
        app->activity->vm->DetachCurrentThread();
        return;
      }
    }
    if (state.HasSurface() && state.focused && state.game) state.Frame();
    if (host.quit_requested) {
      host.quit_requested = false;
      ANativeActivity_finish(app->activity);
    }
  }
}
