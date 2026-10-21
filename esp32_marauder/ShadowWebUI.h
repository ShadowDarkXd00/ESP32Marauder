#pragma once

#include "configs.h"

#if defined(SHADOW_CLIENT_ESP32)

#include <WebServer.h>

class ShadowWebUI {
 public:
  void start();
  void stop();
  void loop();
  bool active() const { return active_; }

 private:
  void drawText(const String& text);
  void drawReady();
  WebServer server_{80};
  bool active_ = false;
  String ssid_;
  String password_;
};

extern ShadowWebUI shadow_web_ui;
void shadowWebStart();
void shadowWebStop();
void shadowWebLoop();
bool shadowWebDisplayActive();

#endif
