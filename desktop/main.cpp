// Desktop-Launcher: SDL2-Fenster mit OpenGL-ES-3-Kontext, ruft den plattformunabhaengigen Spielkern auf.
//
//   fleetclient_desktop --data <spielordner> [--model Bcruise1]    interaktiv (links/rechts: Modell, hoch/runter: Ansicht (Detailstufe, Quartiere, Borg ...), A: Alpha-Modus, T: Teamfarbe,
//        Maus ziehen: drehen, Mausrad: zoomen, Esc: beenden)
//   fleetclient_desktop --data <spielordner> --frames 30 --screenshot out.ppm [--size 800x480]
//        zeichnet N Frames, schreibt das letzte Bild als PPM und beendet sich (fuer Tests/CI)
// Ohne --data wird die Umgebungsvariable FLEET_GAME_DIR verwendet.

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/Log.h"
#include "game/Game.h"
#include "io/FileSource.h"
#include "render/Gl.h"

namespace {

bool WritePpm(const std::string& path, int w, int h) {
  std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = h - 1; y >= 0; --y) {  // GL liest von unten nach oben
    for (int x = 0; x < w; ++x) {
      const unsigned char* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
      std::fwrite(p, 1, 3, f);
    }
  }
  std::fclose(f);
  return true;
}

void SdlLogSink(fleet::LogLevel level, const char* message) {
  const char* tag = level == fleet::LogLevel::Error ? "E" : level == fleet::LogLevel::Warn ? "W" : "I";
  std::fprintf(stderr, "[%s] %s\n", tag, message);
}

class DesktopHost : public fleet::GameHost {
 public:
  explicit DesktopHost(SDL_Window* w) : window_(w) {}
  void ShowMessage(const std::string& text) override { {
    std::string t = text;
    for (char& c : t) if (c == '\n') c = ' ';
    SDL_SetWindowTitle(window_, ("fleetclient - " + t).c_str());
  } }
  void RequestQuit() override { quit = true; }
  bool quit = false;
  void RequestDataFolder() override { FLEET_LOGI("Spielordner bitte per --data <ordner> angeben."); }

 private:
  SDL_Window* window_;
};

}  // namespace

int main(int argc, char** argv) {
  std::string autostart;  // Entwicklungshilfe: Skirmish ohne Eingabe starten und simulieren (feste Schrittweite 0,05 s)
  double sim_seconds = 0;
  bool ai_vs_ai = false;
  std::string human_build;  // "<odf>:<index>"
  int max_frames = -1;
  std::string screenshot;
  std::string data_dir;
  std::string model_name;
  if (const char* env = std::getenv("FLEET_GAME_DIR")) data_dir = env;
  int width = 960, height = 540;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) {
      max_frames = std::atoi(argv[++i]);
    } else if (!std::strcmp(argv[i], "--screenshot") && i + 1 < argc) {
      screenshot = argv[++i];
    } else if (!std::strcmp(argv[i], "--autostart") && i + 1 < argc) {
      autostart = argv[++i];
    } else if (!std::strcmp(argv[i], "--sim-seconds") && i + 1 < argc) {
      sim_seconds = std::atof(argv[++i]);
    } else if (!std::strcmp(argv[i], "--human-build") && i + 1 < argc) {
      human_build = argv[++i];
    } else if (!std::strcmp(argv[i], "--ai-vs-ai")) {
      ai_vs_ai = true;
    } else if (!std::strcmp(argv[i], "--data") && i + 1 < argc) {
      data_dir = argv[++i];
    } else if (!std::strcmp(argv[i], "--model") && i + 1 < argc) {
      model_name = argv[++i];
    } else if (!std::strcmp(argv[i], "--size") && i + 1 < argc) {
      std::sscanf(argv[++i], "%dx%d", &width, &height);
    }
  }

  fleet::SetLogSink(SdlLogSink);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    FLEET_LOGE("SDL_Init: %s", SDL_GetError());
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  SDL_Window* window = SDL_CreateWindow("fleetclient (M0)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                                        height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
  if (!window) {
    FLEET_LOGE("Fenster: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_GLContext context = SDL_GL_CreateContext(window);
  if (!context) {
    FLEET_LOGE("OpenGL-ES-3-Kontext: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(1);

  int exit_code = 0;
  {
    fleet::Game game;
    DesktopHost host(window);
    game.SetHost(&host);
    if (!data_dir.empty()) {
      std::shared_ptr<fleet::FileSource> src = fleet::OpenDirectory(data_dir);
      game.SetDataSource(src);
      if (!model_name.empty()) game.SelectModel(model_name);
    } else {
      FLEET_LOGW("Kein Spielordner: --data <ordner> oder FLEET_GAME_DIR angeben.");
    }
    SDL_AudioDeviceID audio_dev = 0;
    {
      SDL_AudioSpec want;
      SDL_zero(want);
      want.freq = game.audio_rate();
      want.format = AUDIO_S16SYS;
      want.channels = 2;
      want.samples = 1024;
      want.userdata = &game;
      want.callback = [](void* ud, Uint8* stream, int len) { static_cast<fleet::Game*>(ud)->MixAudio(reinterpret_cast<int16_t*>(stream), len / 4); };
      audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);  // ohne Aenderung: SDL wandelt notfalls selbst
      if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
      else FLEET_LOGW("Kein Audioausgang: %s", SDL_GetError());
    }
    if (!game.OnSurfaceCreated()) {
      exit_code = 1;
    } else {
      SDL_GL_GetDrawableSize(window, &width, &height);
      game.OnResize(width, height);
      bool running = true;
      Uint64 last = SDL_GetPerformanceCounter();
      double sim_t = 0, next_report = 0;
      if (!autostart.empty()) {
        for (int k = 0; k < 3; ++k) game.OnFrame(0.01);  // Daten uebernehmen (ApplyPending)
        if (!game.DebugStartSkirmish(autostart, ai_vs_ai)) { FLEET_LOGE("Autostart: Karte %s nicht gestartet", autostart.c_str()); running = false; exit_code = 1; }
      }
      while (running) {
        if (!autostart.empty()) {
          game.OnFrame(0.05);
          sim_t += 0.05;
          if (!human_build.empty() && sim_t >= 1.0) {
            const size_t c = human_build.find(':');
            std::fprintf(stderr, "human build: %s\n", game.DebugHumanBuild(human_build.substr(0, c), c == std::string::npos ? 0 : std::atoi(human_build.c_str() + c + 1)).c_str());
            human_build.clear();
          }
          if (sim_t >= next_report) { std::fprintf(stderr, "--- t=%.0f s\n%s", sim_t, game.DebugReport().c_str()); next_report += 30.0; }
          if (sim_t >= sim_seconds) running = false;
          SDL_GL_SwapWindow(window);
          continue;
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_QUIT) running = false;
          if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
          if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_t) game.CycleTeamColor();
          if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_i) game.ShowInfo();
          if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_h) game.ToggleHardpoints();
          if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_f) game.ToggleFire();
          if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_s) game.ToggleMute();
          if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_g) game.ToggleGrid();
          if (e.type == SDL_MOUSEWHEEL) game.OnScroll(static_cast<float>(e.wheel.y));
          if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_LMASK)) {
            fleet::PointerEvent p;
            p.action = fleet::PointerEvent::Action::Move;
            p.x = static_cast<float>(e.motion.x);
            p.y = static_cast<float>(e.motion.y);
            p.id = SDL_BUTTON_LEFT;
            game.OnPointer(p);
          }
          if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            SDL_GL_GetDrawableSize(window, &width, &height);
            game.OnResize(width, height);
          }
          if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
            fleet::PointerEvent p;
            p.action = e.type == SDL_MOUSEBUTTONDOWN ? fleet::PointerEvent::Action::Down : fleet::PointerEvent::Action::Up;
            p.x = static_cast<float>(e.button.x);
            p.y = static_cast<float>(e.button.y);
            p.id = e.button.button;
            game.OnPointer(p);
          }
        }
        const Uint64 now = SDL_GetPerformanceCounter();
        const double dt = static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency());
        last = now;
        game.OnFrame(dt);
        if (host.quit) running = false;
        if (max_frames > 0 && game.frames_rendered() >= max_frames) {
          if (!screenshot.empty() && !WritePpm(screenshot, width, height)) {
            FLEET_LOGE("Screenshot konnte nicht geschrieben werden: %s", screenshot.c_str());
            exit_code = 1;
          }
          running = false;
        }
        SDL_GL_SwapWindow(window);
      }
      if (audio_dev) SDL_CloseAudioDevice(audio_dev);
      game.OnSurfaceDestroyed();
    }
  }
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return exit_code;
}
