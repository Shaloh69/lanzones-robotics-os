// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzTransfer.h"

#include "LzStore.h"
#include "LzUi.h"

LzTransferScreen Transfer;

void LzTransferScreen::openExport(const char *tag, uint16_t version,
                                  const void *data, uint16_t size) {
  tag_ = tag;
  ver_ = version;
  size_ = size;
  src_ = (const uint8_t *)data;
  pos_ = 0;
  crc_ = LzStore::crc32(data, size);
  st_ = EX_RUN;
  char hdr[48];
  snprintf(hdr, sizeof(hdr), "LZCFG %s v%u sz=%u", tag_, ver_, size_);
  Serial.println(hdr);
  OS.push(this);
}

void LzTransferScreen::openImport(const char *tag, uint16_t version,
                                  void *staging, uint16_t size,
                                  void (*onImported)()) {
  tag_ = tag;
  ver_ = version;
  size_ = size;
  dst_ = (uint8_t *)staging;
  cb_ = onImported;
  pos_ = 0;
  lineLen_ = 0;
  err_ = "";
  st_ = IM_WAIT;
  while (Serial.available()) Serial.read();  // drop stale bytes
  OS.push(this);
}

bool LzTransferScreen::onEvent(const LzEvent &ev) {
  if (ev.btn == BTN_BACK && ev.type == EV_PRESS) {
    OS.pop();  // cancel/close; staging buffer is simply abandoned
    return true;
  }
  return ev.type == EV_PRESS;  // swallow other keys
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

void LzTransferScreen::feedLine() {
  line_[lineLen_] = 0;
  if (lineLen_ == 0) return;
  if (st_ == IM_WAIT) {
    unsigned v = 0, sz = 0;
    char tag[16];
    if (sscanf(line_, "LZCFG %15s v%u sz=%u", tag, &v, &sz) == 3) {
      if (strcmp(tag, tag_) != 0) {
        st_ = IM_ERR;
        err_ = "Wrong OS tag";
      } else if (v != ver_) {
        st_ = IM_ERR;
        err_ = "Version mismatch";
      } else if (sz != size_) {
        st_ = IM_ERR;
        err_ = "Size mismatch";
      } else {
        st_ = IM_DATA;
        pos_ = 0;
      }
    }
    return;
  }
  if (st_ != IM_DATA) return;
  if (strncmp(line_, "END", 3) == 0) {
    unsigned rxCrc = 0;
    if (sscanf(line_, "END %x", &rxCrc) != 1 || pos_ != size_ ||
        LzStore::crc32(dst_, size_) != (uint32_t)rxCrc) {
      st_ = IM_ERR;
      err_ = (pos_ != size_) ? "Truncated data" : "CRC mismatch";
    } else {
      st_ = IM_DONE;
      Buzzer.play(SND_CONFIRM);
      if (cb_) cb_();
    }
    return;
  }
  // hex payload line
  for (uint8_t i = 0; i + 1 < lineLen_; i += 2) {
    int h = hexVal(line_[i]), l = hexVal(line_[i + 1]);
    if (h < 0 || l < 0) {
      st_ = IM_ERR;
      err_ = "Bad hex";
      return;
    }
    if (pos_ >= size_) {  // bounds check (spec 6.2)
      st_ = IM_ERR;
      err_ = "Too much data";
      return;
    }
    dst_[pos_++] = (uint8_t)((h << 4) | l);
  }
}

void LzTransferScreen::onTick(uint32_t now) {
  (void)now;
  if (st_ == EX_RUN) {
    // stream while the TX buffer has room — never blocks the loop
    while (pos_ < size_ && Serial.availableForWrite() > 40) {
      char out[36];
      uint8_t n = 0;
      for (uint8_t i = 0; i < 16 && pos_ < size_; i++, pos_++) {
        snprintf(out + n, sizeof(out) - n, "%02X", src_[pos_]);
        n += 2;
      }
      Serial.println(out);
      OS.requestRedraw();
    }
    if (pos_ >= size_ && Serial.availableForWrite() > 20) {
      char end[20];
      snprintf(end, sizeof(end), "END %08lX", (unsigned long)crc_);
      Serial.println(end);
      st_ = EX_DONE;
      Buzzer.play(SND_CONFIRM);
      OS.requestRedraw();
    }
    return;
  }
  if (st_ == IM_WAIT || st_ == IM_DATA) {
    uint8_t budget = 128;  // bounded work per tick
    while (Serial.available() && budget--) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        feedLine();
        lineLen_ = 0;
        OS.requestRedraw();
        if (st_ == IM_DONE || st_ == IM_ERR) break;
      } else if (lineLen_ < sizeof(line_) - 1) {
        line_[lineLen_++] = c;
      } else {
        lineLen_ = 0;  // oversized line: discard defensively
      }
    }
  }
}

void LzTransferScreen::draw(U8G2 &g) {
  (void)g;
  char b[26];
  switch (st_) {
    case EX_RUN:
      Display.bodyRow(0, "Exporting config", false);
      snprintf(b, sizeof(b), "over serial: %lu%%",
               (unsigned long)(pos_ * 100 / (size_ ? size_ : 1)));
      Display.bodyRow(1, b, false);
      Display.bodyRow(3, "115200 on PA9/10", false);
      break;
    case EX_DONE:
      Display.bodyRow(0, "Export complete.", false);
      Display.bodyRow(1, "Capture the text", false);
      Display.bodyRow(2, "incl. LZCFG+END.", false);
      break;
    case IM_WAIT:
      Display.bodyRow(0, "Waiting for LZCFG", false);
      Display.bodyRow(1, "header on serial", false);
      Display.bodyRow(2, "(115200, PA9/10)", false);
      Display.bodyRow(3, "Paste export now.", false);
      break;
    case IM_DATA:
      snprintf(b, sizeof(b), "Receiving: %lu%%",
               (unsigned long)(pos_ * 100 / (size_ ? size_ : 1)));
      Display.bodyRow(1, b, false);
      break;
    case IM_DONE:
      Display.bodyRow(0, "Import OK (RAM).", false);
      Display.bodyRow(1, "Use Save All to", false);
      Display.bodyRow(2, "persist to flash.", false);
      break;
    default:
      Display.bodyRow(0, "Import FAILED:", false);
      Display.bodyRow(1, err_, false);
      Display.bodyRow(2, "BK, then retry.", false);
      break;
  }
}
