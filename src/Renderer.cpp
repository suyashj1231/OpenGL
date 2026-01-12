#include "Renderer.h"
#include "FontManager.h" // Full definition needed here

Renderer::Renderer(Shader &shader) : shader(shader) { initRenderData(); }

Renderer::~Renderer() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteTextures(1, &whiteTexture);
}

void Renderer::initRenderData() {
  // Configure VAO/VBO for texture quads
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // Allocate buffer for MAX_QUADS
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * MAX_QUADS, NULL,
               GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Create a 1x1 white texture for solid rectangles
  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  unsigned char whitePixel[] = {255, 255, 255, 255}; // GL_RGBA
  // Note: Shader uses .r for alpha, so 255 is full alpha
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               whitePixel);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Renderer::drawRect(float x, float y, float w, float h, glm::vec3 color) {
  shader.use();
  shader.setInt("text", 0);
  shader.setVec3("textColor", color.x, color.y, color.z);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(VAO);

  float vertices[6][4] = {{x, y + h, 0.0f, 0.0f},    {x, y, 0.0f, 1.0f},
                          {x + w, y, 1.0f, 1.0f},

                          {x, y + h, 0.0f, 0.0f},    {x + w, y, 1.0f, 1.0f},
                          {x + w, y + h, 1.0f, 0.0f}};

  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::begin() { vertices.clear(); }

void Renderer::end() {
  if (vertices.empty())
    return;

  shader.use();
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(VAO);

  // Upload ALL vertices
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float),
                  vertices.data());

  // Draw
  glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::drawCodepoint(FontManager &fontManager, unsigned int codepoint,
                             float x, float y, float scale, glm::vec3 color) {
  // Assuming strict batching for text.
  // Color handling: If color changes, we technically need to flush or use
  // vertex colors. Our shader uses `uniform vec3 textColor`. This is a problem
  // for batching multicolored text! To support multicolored batching (syntax
  // highlighting), we should move color to vertex attributes. For now, let's
  // assume one color or flush on color change. BUT the terminal has mostly one
  // color? No, syntax highlighting. Ideally: Add Color (r,g,b) to vertex
  // attributes. (vec4 pos+uv, vec3 color). That's a bigger refactor. "Fast Fix"
  // for 40fps Frame Drop: The text is mostly White/Gold. If we flush on color
  // change, it's still way better than flush on every char. Let's implement
  // flush-on-color-change or flush-on-texture-change logic? Wait, `end()`
  // doesn't take params. Let's store `currentBatchTexture` and
  // `currentBatchColor` in Renderer class? Updating Renderer.h is hard
  // mid-stream.

  // CURRENT STRATEGY:
  // Since we already modified `FontManager` to use Atlas, `TextureID` is
  // constant! Good. Challenge: `color`. If we don't change `color`, batch
  // works. If we change `color`, we must flush.

  // We need to track current color.
  static glm::vec3 lastColor = glm::vec3(-1.0f);
  static unsigned int lastTexture = 0;

  Character ch = fontManager.getCharacter(codepoint);

  // Check Flush Conditions
  if (ch.TextureID != lastTexture || color != lastColor) {
    if (!vertices.empty()) {
      // Flush logic inline here or call end/begin?
      // We can't verify 'end' binds the right texture if we don't store it.
      // Let's rely on immediate flush for state change.

      shader.use();
      shader.setVec3("textColor", lastColor.x, lastColor.y, lastColor.z);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, lastTexture);

      glBindVertexArray(VAO);
      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float),
                      vertices.data());
      glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4);

      vertices.clear();
    }
    lastTexture = ch.TextureID;
    lastColor = color;
  }

  // Add vertices
  float xpos = x + ch.Bearing.x * scale;
  float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
  float w = ch.Size.x * scale;
  float h = ch.Size.y * scale;

  // UVs
  float u = ch.tx;
  float v = ch.ty;
  float tw = ch.tw; // Width in texture space
  float th = ch.th; // Height in texture space
  // We need (u, v) = top left.
  // ch.Size is bitmap size.
  // In `FontManager.cpp`: tx/ty are top-left? Yes.

  if (w > 0 && h > 0) {
    // Top Left
    vertices.push_back(xpos);
    vertices.push_back(ypos + h);
    vertices.push_back(u);
    vertices.push_back(v);

    // Bottom Left
    vertices.push_back(xpos);
    vertices.push_back(ypos);
    vertices.push_back(u);
    vertices.push_back(v + th);

    // Bottom Right
    vertices.push_back(xpos + w);
    vertices.push_back(ypos);
    vertices.push_back(u + tw);
    vertices.push_back(v + th);

    // Top Left
    vertices.push_back(xpos);
    vertices.push_back(ypos + h);
    vertices.push_back(u);
    vertices.push_back(v);

    // Bottom Right
    vertices.push_back(xpos + w);
    vertices.push_back(ypos);
    vertices.push_back(u + tw);
    vertices.push_back(v + th);

    // Top Right
    vertices.push_back(xpos + w);
    vertices.push_back(ypos + h);
    vertices.push_back(u + tw);
    vertices.push_back(v);
  }
}

void Renderer::drawText(FontManager &fontManager, std::string text, float x,
                        float y, float scale, glm::vec3 color) {
  for (char c : text) {
    if (fontManager.Characters.find(c) == fontManager.Characters.end()) {
      fontManager.getCharacter(c); // Ensure loaded
    }
    // drawCodepoint handles batching now
    drawCodepoint(fontManager, c, x, y, scale, color);

    Character ch = fontManager.getCharacter(c);
    x += (ch.Advance >> 6) * scale;
  }
}
