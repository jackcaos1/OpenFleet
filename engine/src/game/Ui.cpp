#include "game/Ui.h"

#include <algorithm>
#include <cmath>

namespace fleet {

namespace {
const int kCellW = 11, kCellH = 22;  // wie core/FontData.h
}

int UiMessageScale(int width, int height) {
  const int by_height = std::max(1, height / 500);
  const int by_width = std::max(1, width / (kCellW * 40));
  return std::max(1, std::min(by_height, by_width));
}

UiLayout BuildUiLayout(int width, int height, const UiState& st) {
  UiLayout L;
  std::vector<UiButton> b;
  auto add = [&](UiAction a, const char* label, bool active = false) {
    UiButton x;
    x.action = a;
    x.label = label;
    x.active = active;
    b.push_back(x);
  };
  if (!st.map_mode) {
    add(UiAction::PrevModel, "< Modell");
    add(UiAction::NextModel, "Modell >");
    add(UiAction::Variant, "Ansicht");
    add(UiAction::Map, "Karte");
    add(UiAction::Info, "Daten");
    add(UiAction::Hardpoints, "Hardpt.", st.hardpoints);
    add(UiAction::Fire, "Feuer", st.firing);
    add(UiAction::Mute, st.muted ? "Ton aus" : "Ton an", st.muted);
    add(UiAction::Folder, "Ordner");
    add(UiAction::Team, "Farbe");
    add(UiAction::Alpha, "Alpha");
  } else {
    add(UiAction::Menu, "Men\xC3\xBC");
    add(UiAction::Spawn, "Objekt");
    add(UiAction::MapMenu, "Karte");
    add(UiAction::Stop, "Stopp");
    add(UiAction::Delete, "Weg");
    add(UiAction::MapInfo, "Info");
    add(UiAction::Team, "Team");
    add(UiAction::Mute, st.muted ? "Ton aus" : "Ton an", st.muted);
    add(UiAction::Folder, "Ordner");
  }
  const bool portrait = height > width;
  const int cols = portrait ? 4 : 6;
  const int rows = (static_cast<int>(b.size()) + cols - 1) / cols;
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float btn_h = std::max(48.0f, std::min(170.0f, std::min(w, h) * 0.12f));
  const float btn_w = w / static_cast<float>(cols);
  size_t longest = 1;
  for (const UiButton& x : b) longest = std::max(longest, x.label.size());
  int scale = static_cast<int>(0.86f * btn_w / static_cast<float>(kCellW * static_cast<int>(longest)));
  scale = std::min(scale, static_cast<int>(0.55f * btn_h / static_cast<float>(kCellH)));
  L.text_scale = std::max(1, scale);
  L.bar_height = btn_h * static_cast<float>(rows);
  L.bar_y = h - L.bar_height;
  const float gap = std::max(2.0f, btn_h * 0.04f);
  for (size_t i = 0; i < b.size(); ++i) {
    const int r = static_cast<int>(i) / cols, c = static_cast<int>(i) % cols;
    b[i].rect.x = static_cast<float>(c) * btn_w + gap;
    b[i].rect.y = L.bar_y + static_cast<float>(r) * btn_h + gap;
    b[i].rect.w = btn_w - 2 * gap;
    b[i].rect.h = btn_h - 2 * gap;
  }
  L.buttons = std::move(b);
  return L;
}

UiAction UiHit(const UiLayout& L, float x, float y) {
  if (y < L.bar_y) return UiAction::None;
  for (const UiButton& b : L.buttons)
    if (b.rect.Contains(x, y)) return b.action;
  return UiAction::None;
}

}  // namespace fleet
