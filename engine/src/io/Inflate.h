#pragma once
// Deflate-Entpacker (RFC 1951), ohne Fremdbibliothek, damit der Kern auf jeder Plattform gleich laeuft.
#include <cstddef>
#include <cstdint>

namespace fleet {

// Entpackt einen rohen Deflate-Strom nach out (genau out_size Bytes erwartet). false bei Fehlern oder abweichender Laenge.
bool InflateRaw(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_size);

}  // namespace fleet
