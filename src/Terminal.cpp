#include "Terminal.h"
#include "FontManager.h"
#include "PTYHandler.h"
#include "Renderer.h"

Terminal::Terminal(float width, float height)
    : screenWidth(width), screenHeight(height), lineHeight(40.0f), scale(1.0f),
      textColor(glm::vec3(1.0f, 1.0f, 1.0f)) {
  // No initial prompt, the shell will provide it
}

// Deprecated write method, replacing internal logic
void Terminal::write(std::string text) { appendText(text); }

void Terminal::processOutput(std::string output) {
  if (!output.empty()) {
    std::cout << "Terminal received " << output.length() << " bytes from PTY"
              << std::endl;
  }
  for (char c : output) {
    if (parserState == ParserState::Normal) {
      if (c == 27) { // ESC
        parserState = ParserState::Esc;
      } else if (c == '\n') {
        lines.push_back("");
      } else if (c == '\r') {
        // carriage return
      } else if (c == '\b') {
        if (!lines.empty() && !lines.back().empty()) {
          lines.back().pop_back();
        }
      } else if (c == 7) {
        // bell
      } else {
        if (c >= 32 || c == 9) {
          if (lines.empty())
            lines.push_back("");
          lines.back() += c;
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
        // Usually we just ignore intermediate bytes for now
      }
    }
  }
}

void Terminal::handleCsi(char finalByte) {
  // For now, we strip mostly.
  // 'm' is SGR (Select Graphic Rendition) -> Colors
  // 'K' is Erase in Line
  // 'J' is Erase in Display

  if (finalByte == 'm') {
    // Color handling would go here
    // csiParams contains "1;31" etc.
    // For now, doing nothing effectively strips it.
  } else if (finalByte == 'K') {
    // Erase line logic (clear from cursor to end)
  } else if (finalByte == 'J') {
    // Clear screen
    if (csiParams == "2") {
      lines.clear();
      lines.push_back("");
    }
  }
}

void Terminal::handleInput(int key, int action, int mods, PTYHandler &pty) {
  // We do NOT echo characters here. We send them to PTY.
  // The PTY/Shell will echo them back if needed.

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
    // Normal characters are handled in char_callback in main
  }
}

void Terminal::appendText(std::string text) { processOutput(text); }

void Terminal::setSize(float width, float height) {
  screenWidth = width;
  screenHeight = height;
}

void Terminal::render(Renderer &renderer, FontManager &fontManager) {
  float y = screenHeight - lineHeight; // Start from top

  // Simple render last N lines logic
  // We should probably start from the bottom if we want a scrolling terminal
  // feel, but for now, top-down and just show what's in buffer

  // If too many lines, show the last ones that fit
  int maxLines = (int)(screenHeight / lineHeight);
  int startLine = 0;
  if (lines.size() > maxLines) {
    startLine = lines.size() - maxLines;
  }

  for (int i = startLine; i < lines.size(); i++) {
    renderer.drawText(fontManager, lines[i], 10.0f, y, scale, textColor);
    y -= lineHeight;
  }
}
