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
  for (char c : output) {
    if (c == '\n') {
      lines.push_back("");
    } else if (c == '\r') {
      // Carriage return: usually move cursor to start of line.
      // For simple rendering, we might ignore or reset logic.
      // For now, ignore it to avoid double newlines if \r\n
    } else if (c == '\b') {
      if (!lines.empty() && !lines.back().empty()) {
        lines.back().pop_back();
      }
    } else if (c == 7) { // Bell
                         // ding
    } else {
      // Filter non-printable checks if needed, but lets try blindly adding
      // Simple ANSI stripper could go here
      if (c >= 32 || c == 9) { // 32 is space, 9 is tab
        if (lines.empty())
          lines.push_back("");
        lines.back() += c;
      }
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
