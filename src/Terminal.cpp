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

// Helper to get number of visible rows
int Terminal::getRows() { return (int)(screenHeight / lineHeight); }

void Terminal::processOutput(std::string output) {
  for (char c : output) {
    if (parserState == ParserState::Normal) {
      if (c == 27) { // ESC
        parserState = ParserState::Esc;
      } else if (c == '\n') {
        cursorY++;
        cursorX = 0;
        // If we moved past the end, add a new line
        if (cursorY >= lines.size()) {
          lines.push_back(std::vector<TerminalGlyph>());
          // Maintain scroll at bottom if we are outputting
          scrollToBottom();
        }
        // Reset color? Usually color persists until changed.
        // currentColor = defaultColor; // Terminals usually persist color
        // across newlines
      } else if (c == '\r') {
        cursorX = 0;
      } else if (c == '\b') {
        if (cursorX > 0)
          cursorX--;
      } else if (c == 7) {
        // bell
      } else {
        if (c >= 32 || c == 9) { // Printable or Tab
          // Ensure line exists
          while (lines.size() <= cursorY) {
            lines.push_back(std::vector<TerminalGlyph>());
          }

          // Ensure space exists up to cursorX in current line
          if (lines[cursorY].size() <= cursorX) {
            while (lines[cursorY].size() <= cursorX) {
              TerminalGlyph g;
              g.character = ' ';
              g.color = currentColor;
              lines[cursorY].push_back(g);
            }
          }

          // Overwrite at cursorX
          lines[cursorY][cursorX].character = c;
          lines[cursorY][cursorX].color = currentColor;
          cursorX++;
        }
      }
    } else if (parserState == ParserState::Esc) {
      if (c == '[') {
        parserState = ParserState::Csi;
        csiParams = "";
      } else {
        parserState = ParserState::Normal;
      }
    } else if (parserState == ParserState::Csi) {
      if (c >= '0' && c <= '9') {
        csiParams += c;
      } else if (c == ';') {
        csiParams += c;
      } else if (c >= 0x40 && c <= 0x7E) {
        handleCsi(c);
        parserState = ParserState::Normal;
      }
    }
  }
}

void Terminal::handleCsi(char finalByte) {
  // Parse params
  std::vector<int> args;
  std::stringstream ss(csiParams);
  std::string segment;
  while (std::getline(ss, segment, ';')) {
    if (!segment.empty())
      args.push_back(std::stoi(segment));
    else
      args.push_back(0); // Default encoding?
  }

  int arg1 = args.size() > 0 ? args[0] : 1; // Default 1
  if (arg1 == 0)
    arg1 = 1; // CSI 0 A means 1 A usually

  if (finalByte == 'A') { // Up
    cursorY -= arg1;
    if (cursorY < 0)
      cursorY = 0;
  } else if (finalByte == 'B') { // Down
    cursorY += arg1;
    // Don't go past end? Or add lines?
    // Standard: Cursor Down stops at bottom margin.
    // For now, clamp to lines.size()
    if (cursorY >= lines.size())
      cursorY = lines.size() - 1;
    if (lines.empty())
      cursorY = 0;
  } else if (finalByte == 'C') { // Right
    cursorX += arg1;
  } else if (finalByte == 'D') { // Left
    cursorX -= arg1;
    if (cursorX < 0)
      cursorX = 0;
  } else if (finalByte == 'H' || finalByte == 'f') { // Cup - Cursor Position
    // args[0] is row (1-based), args[1] is col (1-based)
    int r = (args.size() > 0 && args[0] > 0) ? args[0] : 1;
    int c = (args.size() > 1 && args[1] > 0) ? args[1] : 1;

    // Map visual row to absolute row
    // Visual Row 1 = Top of screen
    // Top of screen = lines.size() - getRows() ?
    // Wait, if we are scrolling...
    // Let's assume the screen "follows" the bottom.
    int termRows = getRows();
    int topRowIndex = (int)lines.size() - termRows;
    if (topRowIndex < 0)
      topRowIndex = 0;

    // Actually, if we clear screen, lines.size() might be 1.
    // If we use 'H', we expect to jump to top.
    // If we assume the viewport is always locked to "end of buffer",
    // then "Top" is relative to that.

    cursorY = topRowIndex + (r - 1);
    cursorX = c - 1;

    // Ensure we don't jump way past end?
    // If program asks to go to row 50 and we have 1 line, we should extend?
    // Usually terminals have fixed size buffer. We grow dynamically.
    while (lines.size() <= cursorY) {
      lines.push_back(std::vector<TerminalGlyph>());
    }
  } else if (finalByte == 'm') {
    // SGR - Select Graphic Rendition
    if (csiParams.empty() || csiParams == "0") {
      currentColor = defaultColor;
    } else {
      // Manual loop as before
      std::stringstream ss2(csiParams);
      int currentVal = 0;
      bool hasVal = false;
      auto applyCode = [&](int code) {
        if (code == 0) {
          currentColor = defaultColor;
        } else if (code >= 30 && code <= 37) {
          static const glm::vec3 colors[] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0},
                                             {1, 1, 0}, {0, 0, 1}, {1, 0, 1},
                                             {0, 1, 1}, {1, 1, 1}};
          currentColor = colors[code - 30];
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
  } else if (finalByte == 'J') { // Erase in Display
    if (csiParams == "2") {
      lines.clear();
      lines.push_back(std::vector<TerminalGlyph>());
      cursorX = 0;
      cursorY = 0;
    }
  } else if (finalByte == 'K') { // Erase in Line
    // 0: cursor to end, 1: start to cursor, 2: all
    // Default 0
    int mode = csiParams.empty() ? 0 : std::stoi(csiParams);
    if (lines.size() > cursorY) {
      if (mode == 0) {
        if (lines[cursorY].size() > cursorX) {
          lines[cursorY].resize(cursorX);
        }
      } else if (mode == 2) {
        lines[cursorY].clear();
      }
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

// Selection Implementation
Terminal::Point Terminal::screenToGrid(float x, float y) {
  // Estimations
  float charW = 11.0f * scale;

  float topY = screenHeight;
  float distFromTop = topY - y;
  int row = (int)(distFromTop / lineHeight); // Visual row 0..N

  int maxLines = (int)(screenHeight / lineHeight);
  int totalLines = lines.size();
  int startLine = 0;
  if (totalLines > maxLines) {
    startLine = totalLines - maxLines - scrollOffset;
    if (startLine < 0)
      startLine = 0;
  }

  int absoluteRow = startLine + row;

  float paddingX = 10.0f;
  int col = (int)((x - paddingX) / charW);

  if (col < 0)
    col = 0;
  // We allow col to go beyond line length to select empty space/newlines

  if (absoluteRow < 0)
    absoluteRow = 0;
  if (absoluteRow >= totalLines)
    absoluteRow = totalLines - 1;
  if (lines.empty())
    absoluteRow = 0;

  return {absoluteRow, col};
}

void Terminal::startSelection(float mouseX, float mouseY) {
  selectionStart = screenToGrid(mouseX, mouseY);
  selectionEnd = selectionStart;
  isSelecting = true;
}

void Terminal::updateSelection(float mouseX, float mouseY) {
  if (isSelecting) {
    selectionEnd = screenToGrid(mouseX, mouseY);
  }
}

void Terminal::clearSelection() {
  isSelecting = false;
  selectionStart = {-1, -1};
  selectionEnd = {-1, -1};
}

bool Terminal::hasSelection() const {
  return isSelecting && !(selectionStart == selectionEnd);
}

std::string Terminal::getSelectionText() {
  if (!hasSelection())
    return "";

  Point p1 = selectionStart;
  Point p2 = selectionEnd;

  // Order them
  if (p2 < p1)
    std::swap(p1, p2);

  std::string res = "";

  for (int r = p1.row; r <= p2.row; r++) {
    if (r < 0 || r >= lines.size())
      continue;

    const auto &line = lines[r];
    int startCol = (r == p1.row) ? p1.col : 0;
    int endCol = (r == p2.row) ? p2.col : 99999; // End of line

    for (int c = startCol; c <= endCol; c++) {
      if (c < line.size()) {
        res += line[c].character;
      } else if (c == line.size()) {
        // Determine if we should include a newline
        // Generally yes if we selected past the end
        if (r != p2.row) { // Don't add newline if it's the very last selected
                           // item unless we really selected it
          res += "\n";
        }
      }
    }
  }
  return res;
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

    // Draw cursor if this is the active line
    bool isCurrentLine = (i == cursorY);

    // Calculate cursor position by traversing glyphs up to cursorX
    float cursorDrawX = x;

    for (int j = 0; j < lines[i].size(); j++) {
      const auto &glyph = lines[i][j];
      if (isCurrentLine && j == cursorX) {
        cursorDrawX = x;
      }

      // Render single codepoint
      Character ch = fontManager.getCharacter(glyph.character);

      GLfloat xpos = x + ch.Bearing.x * scale;
      GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
      GLfloat w = ch.Size.x * scale;
      GLfloat h = ch.Size.y * scale;

      // Draw Glyph
      renderer.drawCodepoint(fontManager, glyph.character, x, y, scale,
                             glyph.color);

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