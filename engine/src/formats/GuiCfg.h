#pragma once
// Layout-Konfiguration der Spieloberflaeche (misc/gui_*.cfg): "schluessel = wert wert ..." mit "//"-Kommentaren und #include.
// Werte sind Zahlenlisten (Rechtecke "x y w h", Farben) oder ein Text in Anfuehrungszeichen. Schluessel ohne Beachtung der Schreibweise.
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fleet {

class GuiCfg {
 public:
  // Liefert zu einem #include-Namen den Dateitext; false = nicht lesbar (Zeile wird uebersprungen).
  using IncludeResolver = std::function<bool(const std::string& name, std::string* text)>;

  void AddText(const std::string& text, const IncludeResolver& include, int depth = 0);

  bool Has(const std::string& key) const;
  // Erste n Zahlen des Eintrags; false, wenn der Eintrag fehlt oder weniger Zahlen hat.
  bool Numbers(const std::string& key, int n, float* out) const;
  bool Rect(const std::string& key, float out[4]) const { return Numbers(key, 4, out); }
  std::string Str(const std::string& key) const;  // Text in Anfuehrungszeichen, sonst leer
  int Int(const std::string& key, int def) const;

  size_t size() const { return entries_.size(); }

 private:
  struct Entry {
    std::vector<float> nums;
    std::string str;
  };
  std::unordered_map<std::string, Entry> entries_;
};

}  // namespace fleet
