#pragma once

#include "Shader.h"
#include "config.h"

// Forward declaration if possible, but FontManager is needed in drawText header
class FontManager;

class Renderer {
public:
  Renderer(Shader &shader);
  ~Renderer();

  // Batching methods
  void begin();
  void end();

  void drawText(FontManager &fontManager, std::string text, float x, float y,
                float scale, glm::vec3 color);
  void drawCodepoint(FontManager &fontManager, unsigned int codepoint, float x,
                     float y, float scale, glm::vec3 color);
  void drawRect(float x, float y, float w, float h, glm::vec3 color);

private:
  Shader &shader;
  unsigned int VAO, VBO;
  unsigned int whiteTexture;

  // Batching
  std::vector<float> vertices;
  const unsigned int MAX_QUADS = 10000;

  // Batch State
  glm::vec3 batchColor = glm::vec3(-1.0f);
  unsigned int batchTexture = 0;

  void initRenderData();
};
