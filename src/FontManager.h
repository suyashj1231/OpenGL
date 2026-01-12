#pragma once

#include "config.h"

struct Character {
  unsigned int TextureID; // ID handle of the glyph texture
  glm::ivec2 Size;        // Size of glyph
  glm::ivec2 Bearing;     // Offset from baseline to left/top of glyph
  unsigned int Advance;   // Offset to advance to next glyph
  // Atlas Coordinates (0.0 - 1.0)
  float tx, ty; // Top-left
  float tw, th; // Width/Height in texture space
};

class FontManager {
public:
  // Atlas State
  unsigned int atlasTextureID;
  int atlasWidth = 1024;
  int atlasHeight = 1024;
  int atlasX = 0;
  int atlasY = 0;
  int atlasRowHeight = 0;

  FontManager();
  ~FontManager();

  bool loadFont(std::string fontPath, unsigned int fontSize);
  Character getCharacter(unsigned int c);

private:
  FT_Library ft;
  FT_Face face;
  std::map<unsigned int, Character> Characters;
};
