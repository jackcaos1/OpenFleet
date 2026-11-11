#include "io/Inflate.h"

namespace fleet {

namespace {

struct Bits {
  const uint8_t* in;
  size_t n, pos = 0;
  uint32_t buf = 0;
  int cnt = 0;
  bool err = false;
  int Get(int need) {
    while (cnt < need) {
      if (pos >= n) {
        err = true;
        return 0;
      }
      buf |= static_cast<uint32_t>(in[pos++]) << cnt;
      cnt += 8;
    }
    const int v = static_cast<int>(buf & ((1u << need) - 1u));
    buf >>= need;
    cnt -= need;
    return v;
  }
};

struct Huff {
  uint16_t count[16];
  uint16_t symbol[288];
};

// Baut die Tabelle (Verfahren wie in zlibs "puff"). Rueckgabe: unbesetzte Codes (< 0 = ueberzeichnet).
int Build(Huff* h, const uint8_t* length, int n) {
  for (int i = 0; i < 16; ++i) h->count[i] = 0;
  for (int i = 0; i < n; ++i) ++h->count[length[i]];
  if (h->count[0] == n) return 0;
  int left = 1;
  for (int len = 1; len <= 15; ++len) {
    left <<= 1;
    left -= h->count[len];
    if (left < 0) return left;
  }
  uint16_t offs[16];
  offs[1] = 0;
  for (int len = 1; len < 15; ++len) offs[len + 1] = static_cast<uint16_t>(offs[len] + h->count[len]);
  for (int i = 0; i < n; ++i)
    if (length[i] != 0) h->symbol[offs[length[i]]++] = static_cast<uint16_t>(i);
  return left;
}

int Decode(Bits* b, const Huff& h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len <= 15; ++len) {
    code |= b->Get(1);
    if (b->err) return -1;
    const int count = h.count[len];
    if (code - count < first) return h.symbol[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  return -1;
}

const uint16_t kLBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint16_t kLExt[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kDBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const uint16_t kDExt[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool Codes(Bits* b, uint8_t* out, size_t out_size, size_t* op, const Huff& lc, const Huff& dc) {
  for (;;) {
    int sym = Decode(b, lc);
    if (sym < 0) return false;
    if (sym < 256) {
      if (*op >= out_size) return false;
      out[(*op)++] = static_cast<uint8_t>(sym);
    } else if (sym == 256) {
      return true;
    } else {
      sym -= 257;
      if (sym >= 29) return false;
      const size_t len = static_cast<size_t>(kLBase[sym] + b->Get(kLExt[sym]));
      const int ds = Decode(b, dc);
      if (ds < 0 || ds >= 30 || b->err) return false;
      const size_t dist = static_cast<size_t>(kDBase[ds] + b->Get(kDExt[ds]));
      if (b->err || dist > *op || *op + len > out_size) return false;
      for (size_t i = 0; i < len; ++i, ++*op) out[*op] = out[*op - dist];
    }
  }
}

}  // namespace

bool InflateRaw(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_size) {
  Bits b;
  b.in = in;
  b.n = in_size;
  size_t op = 0;
  int last;
  do {
    last = b.Get(1);
    const int type = b.Get(2);
    if (b.err) return false;
    if (type == 0) {
      b.buf = 0;
      b.cnt = 0;
      if (b.pos + 4 > b.n) return false;
      const unsigned len = in[b.pos] | (in[b.pos + 1] << 8), nlen = in[b.pos + 2] | (in[b.pos + 3] << 8);
      b.pos += 4;
      if (len != (~nlen & 0xFFFFu) || b.pos + len > b.n || op + len > out_size) return false;
      for (unsigned i = 0; i < len; ++i) out[op++] = in[b.pos++];
    } else if (type == 1) {
      uint8_t l[288];
      for (int i = 0; i < 144; ++i) l[i] = 8;
      for (int i = 144; i < 256; ++i) l[i] = 9;
      for (int i = 256; i < 280; ++i) l[i] = 7;
      for (int i = 280; i < 288; ++i) l[i] = 8;
      Huff lc, dc;
      Build(&lc, l, 288);
      uint8_t d[30];
      for (int i = 0; i < 30; ++i) d[i] = 5;
      Build(&dc, d, 30);
      if (!Codes(&b, out, out_size, &op, lc, dc)) return false;
    } else if (type == 2) {
      static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      const int nlen = b.Get(5) + 257, ndist = b.Get(5) + 1, ncode = b.Get(4) + 4;
      if (b.err || nlen > 286 || ndist > 30) return false;
      uint8_t lengths[320] = {0};
      for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<uint8_t>(b.Get(3));
      Huff lencode, lc, dc;
      if (b.err || Build(&lencode, lengths, 19) != 0) return false;
      int index = 0;
      while (index < nlen + ndist) {
        const int sym = Decode(&b, lencode);
        if (sym < 0) return false;
        if (sym < 16) {
          lengths[index++] = static_cast<uint8_t>(sym);
        } else {
          int len = 0, rep;
          if (sym == 16) {
            if (index == 0) return false;
            len = lengths[index - 1];
            rep = 3 + b.Get(2);
          } else if (sym == 17) {
            rep = 3 + b.Get(3);
          } else {
            rep = 11 + b.Get(7);
          }
          if (b.err || index + rep > nlen + ndist) return false;
          while (rep--) lengths[index++] = static_cast<uint8_t>(len);
        }
      }
      if (lengths[256] == 0) return false;
      if (Build(&lc, lengths, nlen) < 0 || Build(&dc, lengths + nlen, ndist) < 0) return false;
      if (!Codes(&b, out, out_size, &op, lc, dc)) return false;
    } else {
      return false;
    }
  } while (!last);
  return op == out_size;
}

}  // namespace fleet
