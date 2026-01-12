#include "Terminal.h"
#include "FontManager.h"
#include "PTYHandler.h"
#include "Renderer.h"
#include <sstream>

Terminal::Terminal(float width, float height)
    : screenWidth(width), screenHeight(height), lineHeight(20.0f), scale(1.0f),
      textColor(glm::vec3(1.0f, 1.0f, 1.0f)) {
  // No initial prompt, the shell will provide it
  // Default to Gold
  currentColor = defaultColor;
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
        cursorX = 0;
        // Reset color to default on newline (assumes end of input/output line)
        currentColor = defaultColor;
        // Reset scroll when typing/outputting
        scrollToBottom();
      } else if (c == '\r') {
        // Carriage Return: Return to start of line
        cursorX = 0;
      } else if (c == '\b') {
        // Backspace: move back one
        if (cursorX > 0) {
          cursorX--;
        }
      } else if (c == 7) {
        // bell
      } else {
        if (c >= 32 || c == 9) { // Printable or Tab
          if (lines.empty()) {
            lines.push_back(std::vector<TerminalGlyph>());
            cursorX = 0;
          }

          // Ensure space exists up to cursorX
          if (lines.back().size() <= cursorX) {
            while (lines.back().size() <= cursorX) {
              TerminalGlyph g;
              g.character = ' ';
              g.color = currentColor;
              lines.back().push_back(g);
            }
          }

          // Overwrite at cursorX
          lines.back()[cursorX].character = c;
          lines.back()[cursorX].color = currentColor;
          cursorX++;
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
    // std::cout << "[CSI] SGR: " << csiParams << std::endl;
    if (csiParams.empty() || csiParams == "0") {
      currentColor = defaultColor; // Reset to Default
    } else {
      std::stringstream ss(csiParams);
      std::string segment;

      // Split by ';'
      // We will do a manual parse loop to handle multiple codes
      int currentVal = 0;
      bool hasVal = false;

      auto applyCode = [&](int code) {
        if (code == 0) {
          currentColor = defaultColor; // Reset to Default
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
    // If user presses a key, jump to bottom (unless it's shift keys used for
    // scrolling)
    if (key != GLFW_KEY_LEFT_SHIFT && key != GLFW_KEY_RIGHT_SHIFT) {
      scrollToBottom();
    }

    // User is typing -> Switch to Input Color (Gold)
    currentColor = inputColor;

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

void Terminal::scroll(int amount) {
  scrollOffset += amount;
  // Clamp
  int maxLines = (int)(screenHeight / lineHeight);
  int totalLines = lines.size();
  if (totalLines <= maxLines) {
    scrollOffset = 0;
    return;
  }

  int maxScroll = totalLines - maxLines;
  if (scrollOffset > maxScroll)
    scrollOffset = maxScroll;
  if (scrollOffset < 0)
    scrollOffset = 0;
}

void Terminal::scrollToBottom() { scrollOffset = 0; }

void Terminal::changeScale(float delta) {
  scale += delta;
  // Clamp
  if (scale < 0.5f)
    scale = 0.5f;
  if (scale > 3.0f)
    scale = 3.0f;

  // Update line height (Base 20.0f)
  lineHeight = 20.0f * scale;
}

void Terminal::render(Renderer &renderer, FontManager &fontManager,
                      float deltaTime) {
  float y = screenHeight - lineHeight; // Start from top
  int maxLines = (int)(screenHeight / lineHeight);

  // Calculate start line based on scrollOffset
  int totalLines = lines.size();
  int startLine = 0;

  if (totalLines > maxLines) {
    startLine = totalLines - maxLines - scrollOffset;
    if (startLine < 0)
      startLine = 0;
  }

  // We only draw up to maxLines
  int endLine = startLine + maxLines;
  if (endLine > totalLines)
    endLine = totalLines;

  // Cursor Blinking Logic
  cursorTimer += deltaTime;
  if (cursorTimer >= 0.5f) {
    cursorTimer = 0.0f;
    showCursor = !showCursor;
  }

  // Selection Rects
  Point p1 = selectionStart;
  Point p2 = selectionEnd;
  if (isSelecting && (p1.row != -1)) {
    if (p2 < p1)
      std::swap(p1, p2);

    // Draw Selection Highlights
    float selY = y;
    for (int i = startLine; i < endLine; i++) {
      // Check if row i is inside selection range (row-wise)
      if (i >= p1.row && i <= p2.row) {
        float x = 10.0f;
        float charW = 11.0f * scale; // Estimate

        // Define col range for this row
        int startCol = (i == p1.row) ? p1.col : 0;
        int endCol = (i == p2.row) ? p2.col : 99999;

        // Render a big rect for the selected range in this line?
        // Or char by char? Big rect is faster.

        // Clamp endCol to line size (plus 1 for potential newline selection)
        int lineLen = lines[i].size();
        int actualEndCol = (endCol > lineLen)
                               ? lineLen
                               : endCol; // Allow selecting slightly past text

        if (startCol <= actualEndCol) {
          float startX = 10.0f + startCol * charW;
          float width = (actualEndCol - startCol + 1) *
                        charW; // +1 to capture the char itself
          // If dragging backwards? No we swapped p1/p2.

          // If startCol > actualEndCol (empty line or weirdness), width might
          // be 0 or neg
          if (width > 0) {
            renderer.drawRect(startX, selY, width, lineHeight, selectionColor);
          }
        }
      }
      selY -= lineHeight;
    }
  }

  for (int i = startLine; i < endLine; i++) {
    float x = 10.0f; // Padding

    // Draw cursor if this is the last line (active line)
    // Note: cursorX is relative to the active line.
    // If lines.size() represents history, the active line is lines.back().
    bool isCurrentLine = (i == lines.size() - 1);

    // Calculate cursor position by traversing glyphs up to cursorX
    float cursorDrawX = x;

    for (int j = 0; j < lines[i].size(); j++) {
      const auto &glyph = lines[i][j];
      if (isCurrentLine && j == cursorX) {
        cursorDrawX = x;
      }

      std::string s(1, glyph.character);
      renderer.drawText(fontManager, s, x, y, scale, glyph.color);

      Character ch = fontManager.getCharacter(glyph.character);
      x += (ch.Advance >> 6) * scale;
    }

    // If cursor is at the end (appending)
    if (isCurrentLine && cursorX >= lines[i].size()) {
      cursorDrawX = x;
    }

    // Draw Cursor
    if (isCurrentLine && showCursor) {
      // height typically lineHeight, width typically 10-ish?
      // Let's assume width of a space or 'M'
      float w = 10.0f;
      // Try getting 'M' width
      // Character chM = fontManager.getCharacter('M');
      // w = (chM.Advance >> 6) * scale;

      // Draw solid block
      renderer.drawRect(cursorDrawX, y, w, lineHeight, cursorColor);
    }

    y -= lineHeight;
  }
}