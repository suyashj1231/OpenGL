#include "Terminal.h"
#include "FontManager.h"
#include "PTYHandler.h"
#include "Renderer.h"
#include <sstream>

Terminal::Terminal(float width, float height)
    : screenWidth(width), screenHeight(height), lineHeight(20.0f), scale(1.0f),
      textColor(glm::vec3(1.0f, 1.0f, 1.0f)) {
  // No initial prompt, the shell will provide it
}

// Deprecated write method, replacing internal logic
void Terminal::write(std::string text) { appendText(text); }

void Terminal::processOutput(std::string output) {
  for (char c : output) {
    if (parserState == ParserState::Normal) {
      if (c == 27) { // ESC
        parserState = ParserState::Esc;
      } else if (c == '\n') {
        lines.push_back(std::vector<TerminalGlyph>());
      } else if (c == '\r') {
        // Handle carriage return by clearing the current line to simulate
        // overwrite
        if (!lines.empty()) {
          lines.back().clear();
        }
      } else if (c == '\b') {
        if (!lines.empty() && !lines.back().empty()) {
          lines.back().pop_back();
        }
      } else if (c == 7) {
        // bell
      } else {
        if (c >= 32 || c == 9) {
          if (lines.empty())
            lines.push_back(std::vector<TerminalGlyph>());

          TerminalGlyph g;
          g.character = c;
          g.color = currentColor;
          lines.back().push_back(g);
        }
      }
    } else if (parserState == ParserState::Esc) {
      if (c == '[') {
        parserState = ParserState::Csi;
        csiParams = "";
      } else {
        // Unsupported sequence, reset
        parserState = ParserState::Normal;
      }
    } else if (parserState == ParserState::Csi) {
      if (c >= '0' && c <= '9') {
        csiParams += c;
      } else if (c == ';') {
        csiParams += c;
      } else if (c >= 0x40 && c <= 0x7E) {
        // Final byte
        handleCsi(c);
        parserState = ParserState::Normal;
      } else {
        // Unexpected char in CSI?
      }
    }
  }
}

void Terminal::handleCsi(char finalByte) {
  if (finalByte == 'm') {
    // SGR - Select Graphic Rendition
    if (csiParams.empty() || csiParams == "0") {
      currentColor = glm::vec3(1.0f, 1.0f, 1.0f); // Reset
    } else {
      std::stringstream ss(csiParams);
      std::string segment;

      // Split by ';'
      // We will do a manual parse loop to handle multiple codes
      int currentVal = 0;
      bool hasVal = false;

      auto applyCode = [&](int code) {
        if (code == 0) {
          currentColor = glm::vec3(1.0f, 1.0f, 1.0f);
        } else if (code >= 30 && code <= 37) {
          if (code == 30)
            currentColor = glm::vec3(0.0f, 0.0f, 0.0f); // Black
          else if (code == 31)
            currentColor = glm::vec3(1.0f, 0.0f, 0.0f); // Red
          else if (code == 32)
            currentColor = glm::vec3(0.0f, 1.0f, 0.0f); // Green
          else if (code == 33)
            currentColor = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow
          else if (code == 34)
            currentColor = glm::vec3(0.0f, 0.0f, 1.0f); // Blue
          else if (code == 35)
            currentColor = glm::vec3(1.0f, 0.0f, 1.0f); // Magenta
          else if (code == 36)
            currentColor = glm::vec3(0.0f, 1.0f, 1.0f); // Cyan
          else if (code == 37)
            currentColor = glm::vec3(1.0f, 1.0f, 1.0f); // White
        }
      };

      for (char c : csiParams) {
        if (c == ';') {
          if (hasVal)
            applyCode(currentVal);
          currentVal = 0;
          hasVal = false;
        } else if (isdigit(c)) {
          currentVal = currentVal * 10 + (c - '0');
          hasVal = true;
        }
      }
      if (hasVal)
        applyCode(currentVal);
    }
  } else if (finalByte == 'K') {
    // Erase in Line
  } else if (finalByte == 'J') {
    // Clear screen
    if (csiParams == "2") {
      lines.clear();
      lines.push_back(std::vector<TerminalGlyph>());
    }
  }
}

void Terminal::handleInput(int key, int action, int mods, PTYHandler &pty) {
  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    if (key == GLFW_KEY_ENTER) {
      pty.writeInput("\n");
    } else if (key == GLFW_KEY_BACKSPACE) {
      char backspace = 127; // ASCII del
      pty.writeInput(&backspace, 1);
    } else if (key == GLFW_KEY_UP) {
      pty.writeInput("\033[A");
    } else if (key == GLFW_KEY_DOWN) {
      pty.writeInput("\033[B");
    } else if (key == GLFW_KEY_LEFT) {
      pty.writeInput("\033[D");
    } else if (key == GLFW_KEY_RIGHT) {
      pty.writeInput("\033[C");
    }
  }
}

void Terminal::appendText(std::string text) { processOutput(text); }

void Terminal::setSize(float width, float height) {
  screenWidth = width;
  screenHeight = height;
}

void Terminal::render(Renderer &renderer, FontManager &fontManager) {
  float y = screenHeight - lineHeight; // Start from top
  int maxLines = (int)(screenHeight / lineHeight);
  int startLine = 0;
  if (lines.size() > maxLines) {
    startLine = lines.size() - maxLines;
  }

  for (int i = startLine; i < lines.size(); i++) {
    float x = 10.0f;
    for (const auto &glyph : lines[i]) {
      std::string s(1, glyph.character);
      // Draw one char at a time with its color
      // Note: this is inefficient but functional for now
      renderer.drawText(fontManager, s, x, y, scale, glyph.color);

      // Manually advance X because drawText doesn't return advanced position
      // We need to query the font manager for advance
      Character ch = fontManager.getCharacter(glyph.character);
      x += (ch.Advance >> 6) * scale;
    }
    y -= lineHeight;
  }
}