// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Config Export/Import over serial (spec 1: clone a tuned setup onto a
// second board). USART1 on PA9/PA10 @115200 — the pins the spec reserves
// for serial debug. Text format, safe to copy/paste through a terminal:
//
//   LZCFG <TAG> v<version> sz=<size>
//   <hex, 16 bytes per line>
//   END <crc32-hex>
//
// Export streams non-blockingly (only writes when the TX buffer has room);
// import parses char-by-char into a caller-provided staging buffer and
// only hands it over after size + CRC verify — a torn transfer never
// touches the live config. Flash is untouched; the user persists via the
// normal explicit Save action afterwards (spec 6.1/6.2).
#pragma once
#include <Arduino.h>

#include "LzOS.h"

class LzTransferScreen : public LzScreen {
 public:
  LzTransferScreen() : LzScreen("SERIAL") {}
  void openExport(const char *tag, uint16_t version, const void *data,
                  uint16_t size);
  void openImport(const char *tag, uint16_t version, void *staging,
                  uint16_t size, void (*onImported)());
  bool onEvent(const LzEvent &ev) override;
  void onTick(uint32_t now) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "BK:Cancel/Close"; }

 private:
  void feedLine();  // import: process one complete received line
  enum St : uint8_t { EX_RUN, EX_DONE, IM_WAIT, IM_DATA, IM_DONE, IM_ERR };
  St st_ = EX_DONE;
  const char *tag_ = "";
  uint16_t ver_ = 0, size_ = 0;
  const uint8_t *src_ = nullptr;
  uint8_t *dst_ = nullptr;
  uint32_t pos_ = 0;
  uint32_t crc_ = 0;
  void (*cb_)() = nullptr;
  char line_[64];
  uint8_t lineLen_ = 0;
  const char *err_ = "";
};
extern LzTransferScreen Transfer;
