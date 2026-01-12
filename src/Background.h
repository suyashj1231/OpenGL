#pragma once

#include "Shader.h"
#include "config.h"
#include <string>
#include <vector>

struct BackgroundFrame {
  unsigned int textureID;
  int delay; // in milliseconds
};

class Background {
public:
  Background();
  ~Background();

  bool load(const std::string &path);
  void render(float deltaTime);

private:
  float currentTime;
  int currentFrameIndex;
  std::vector<BackgroundFrame> frames;

  unsigned int VAO, VBO;
  Shader *shader; // We'll manage a simple shader internally or pass it in?
  // Let's manage simple shader internally for self-containment

  void initRenderData();
  void createShader();
};
