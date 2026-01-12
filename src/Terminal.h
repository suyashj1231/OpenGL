#pragma once

#include "config.h"

// Forward declarations
class Renderer;
class FontManager;
class PTYHandler;

class Terminal {
public:
  Terminal(float width, float height);

  // PTY Integration
  void processOutput(std::string output);
  void handleInput(int key, int action, int mods, PTYHandler &pty);

  // Rendering
  void render(Renderer &renderer, FontManager &fontManager);
  void write(std::string text); // Deprecated utility

  // Resize handling
  void setSize(float width, float height);

private:
  float screenWidth;
  float screenHeight;
  float lineHeight;
  float scale;
  glm::vec3 textColor;

  // Color State
  struct TerminalGlyph {
    char character;
    glm::vec3 color;
  };
  glm::vec3 currentColor{0.0f, 1.0f, 0.0f}; // Default Green
  std::vector<std::vector<TerminalGlyph>> lines;

  // Cursor State
  int cursorX = 0;

  // Internal helper
  void appendText(std::string text);

  // ANSI Parser State
  enum class ParserState {
    Normal,
    Esc,
    Csi // Control Sequence Introducer like \x1b[
  };
  ParserState parserState = ParserState::Normal;
  std::string csiParams;

  void handleCsi(char finalByte);
};
