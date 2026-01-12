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
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
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

void Renderer::drawText(FontManager &fontManager, std::string text, float x,
                        float y, float scale, glm::vec3 color) {
  // Activate corresponding render state
  shader.use();
  shader.setInt("text", 0); // Explicitly set texture unit 0
  shader.setVec3("textColor", color.x, color.y, color.z);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(VAO);

  // Iterate through all characters
  std::string::const_iterator c;
  for (c = text.begin(); c != text.end(); c++) {
    Character ch = fontManager.getCharacter(*c);

    float xpos = x + ch.Bearing.x * scale;
    float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

    float w = ch.Size.x * scale;
    float h = ch.Size.y * scale;

    // Skip empty glyphs (like space)
    if (w > 0 && h > 0) {
      // Update VBO for each character
      float vertices[6][4] = {
          {xpos, ypos + h, 0.0f, 0.0f},    {xpos, ypos, 0.0f, 1.0f},
          {xpos + w, ypos, 1.0f, 1.0f},

          {xpos, ypos + h, 0.0f, 0.0f},    {xpos + w, ypos, 1.0f, 1.0f},
          {xpos + w, ypos + h, 1.0f, 0.0f}};
      // Render glyph texture over quad
      glBindTexture(GL_TEXTURE_2D, ch.TextureID);
      // Update content of VBO memory
      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      // Render quad
      glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Now advance cursors for next glyph (note that advance is number of 1/64
    // pixels)
    x += (ch.Advance >> 6) *
         scale; // Bitshift by 6 to get value in pixels (2^6 = 64)
  }
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}
