#pragma once

#include "Shader.h"
#include "config.h"

// Forward declaration if possible, but FontManager is needed in drawText header
class FontManager;

class Renderer {
public:
  Renderer(Shader &shader);
  ~Renderer();

  void drawText(FontManager &fontManager, std::string text, float x, float y,
                float scale, glm::vec3 color);

private:
  Shader &shader;
  unsigned int VAO, VBO;

  void initRenderData();
};
