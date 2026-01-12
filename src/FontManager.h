#pragma once

#include "config.h"

struct Character {
  unsigned int TextureID; // ID handle of the glyph texture
  glm::ivec2 Size;        // Size of glyph
  glm::ivec2 Bearing;     // Offset from baseline to left/top of glyph
  unsigned int Advance;   // Offset to advance to next glyph
};

class FontManager {
public:
  FontManager();
  ~FontManager();

  bool loadFont(std::string fontPath, unsigned int fontSize);
  Character getCharacter(char c);

private:
  FT_Library ft;
  FT_Face face;
  std::map<char, Character> Characters;
};
