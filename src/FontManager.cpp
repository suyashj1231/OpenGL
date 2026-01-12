#include "FontManager.h"

FontManager::FontManager() {
  if (FT_Init_FreeType(&ft)) {
    std::cout << "ERROR::FREETYPE: Could not init FreeType Library"
              << std::endl;
  }
}

FontManager::~FontManager() {
  FT_Done_Face(face);
  FT_Done_FreeType(ft);
}

bool FontManager::loadFont(std::string fontPath, unsigned int fontSize) {
  if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
    std::cout << "ERROR::FREETYPE: Failed to load font: " << fontPath
              << std::endl;
    return false;
  }

  FT_Set_Pixel_Sizes(face, 0, fontSize);

  // Disable byte-alignment restriction
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // Initialize Atlas Texture
  glGenTextures(1, &atlasTextureID);
  glBindTexture(GL_TEXTURE_2D, atlasTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED,
               GL_UNSIGNED_BYTE, NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Reserve space for white pixel at (0,0) for solid rects
  // We can upload it now: max value
  unsigned char white = 255;
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE,
                  &white);

  // Move cursor to avoid overwriting white pixel
  atlasX = 2;
  atlasY = 0;
  atlasRowHeight = 0;

  return true;
}

Character FontManager::getCharacter(unsigned int codepoint) {
  if (Characters.find(codepoint) != Characters.end()) {
    return Characters[codepoint];
  }

  if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
    std::cout << "ERROR::FREETYTPE: Failed to load Glyph for codepoint: "
              << codepoint << std::endl;
    // Return empty with atlasID to prevent crashes
    return Character{atlasTextureID, {0, 0}, {0, 0}, 0, 0, 0, 0, 0};
  }

  // Calculate glyph dimensions
  int w = face->glyph->bitmap.width;
  int h = face->glyph->bitmap.rows;

  // Simple packing: Move to next line if full
  // Add 1px padding
  if (atlasX + w + 1 >= atlasWidth) {
    atlasX = 0;
    atlasY += atlasRowHeight + 1; // Move down
    atlasRowHeight = 0;
  }

  // Update row height
  if (h > atlasRowHeight) {
    atlasRowHeight = h;
  }

  // Check if atlas is full (naive)
  if (atlasY + h >= atlasHeight) {
    std::cout << "ERROR: Texture Atlas Full!" << std::endl;
    // Ideally we would flush or make new atlas, but for now just fail
    // gracefully
    return Character{atlasTextureID, {0, 0}, {0, 0}, 0, 0, 0, 0, 0};
  }

  // Upload to Atlas
  glBindTexture(GL_TEXTURE_2D, atlasTextureID);
  glTexSubImage2D(GL_TEXTURE_2D, 0, atlasX, atlasY, w, h, GL_RED,
                  GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

  // Calculate UVs
  float tx = (float)atlasX / (float)atlasWidth;
  float ty = (float)atlasY / (float)atlasHeight;
  float tw = (float)w / (float)atlasWidth;
  float th = (float)h / (float)atlasHeight;

  Character character = {
      atlasTextureID,
      glm::ivec2(w, h),
      glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
      static_cast<unsigned int>(face->glyph->advance.x),
      tx,
      ty,
      tw,
      th};

  Characters.insert(std::pair<unsigned int, Character>(codepoint, character));

  // Advance cursor
  atlasX += w + 1;
  return character;
}
