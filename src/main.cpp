#include "config.h"

#include "Background.h"
#include "FontManager.h"
#include "PTYHandler.h"
#include "Renderer.h"
#include "Shader.h"
#include "Terminal.h"

// Global state
Terminal *globalTerminal = nullptr;
PTYHandler *globalPTY = nullptr;
bool vsyncEnabled = true;

void updatePTYSize(int width, int height);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods);
void char_callback(GLFWwindow *window, unsigned int codepoint);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window =
      glfwCreateWindow(800, 600, "OpenGL Terminal", NULL, NULL);
  if (window == NULL) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetCharCallback(window, char_callback);
  glfwSetScrollCallback(window, scroll_callback);

  // Register Mouse Callbacks
  void mouse_button_callback(GLFWwindow * window, int button, int action,
                             int mods);
  void cursor_position_callback(GLFWwindow * window, double xpos, double ypos);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetCursorPosCallback(window, cursor_position_callback);

  // Default VSync ON
  glfwSwapInterval(1);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Initialize Systems
  const char *vertexSource =
      "#version 330 core\n"
      "layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>\n"
      "out vec2 TexCoords;\n"
      "\n"
      "uniform mat4 projection;\n"
      "\n"
      "void main()\n"
      "{\n"
      "    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
      "    TexCoords = vertex.zw;\n"
      "}\0";

  const char *fragmentSource =
      "#version 330 core\n"
      "in vec2 TexCoords;\n"
      "out vec4 color;\n"
      "\n"
      "uniform sampler2D text;\n"
      "uniform vec3 textColor;\n"
      "\n"
      "void main()\n"
      "{    \n"
      "    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);\n"
      "    color = vec4(textColor, 1.0) * sampled;\n"
      "}\0";

  Shader shader(vertexSource, fragmentSource, true);
  glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
  shader.use();
  shader.setMat4("projection", &projection[0][0]);

  Renderer renderer(shader);
  FontManager fontManager;
  if (!fontManager.loadFont("/System/Library/Fonts/Monaco.ttf", 18)) {
    if (!fontManager.loadFont("/Library/Fonts/Arial.ttf", 18)) {
      std::cout << "Failed to load font" << std::endl;
    }
  }

  Terminal terminal(800.0f, 600.0f);
  globalTerminal = &terminal;

  PTYHandler pty;
  globalPTY = &pty;

  Background background;
  // Try to load a gif if it exists, otherwise warn
  if (!background.load("res/bg.gif")) {
    std::cout << "Usage: Place a 'bg.gif' in 'res/' folder to see it!"
              << std::endl;
  }

  if (pty.spawnShell()) {
    std::cout << "Shell spawned successfully" << std::endl;
    // Inject custom prompt command to override .zshrc/.bashrc
    // Zsh syntax: %F{color} for foreground
    // Changed %F{blue} to %F{green} for directory
    // Chain 'clear' to minimize flash of original prompt
    std::string cmd = "export PS1=\"%F{cyan}➜ %F{yellow}%n@opengl %F{green}%1d "
                      "%f%% \"; clear\n";
    pty.writeInput(cmd);
  } else {
    std::cout << "Failed to spawn shell" << std::endl;
  }

  pty.setWindowSize(15, 80); // Approximate

  // Timing
  float deltaTime = 0.0f;
  float lastFrame = 0.0f;

  // FPS
  int frameCount = 0;
  float fpsTimer = 0.0f;
  std::string fpsText = "FPS: 0";

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // GPU Query Result Handling
    GLuint64 startTime = 0, stopTime = 0;
    static GLuint queryID[2] = {0, 0};
    static bool queryInit = false;
    static int frontDiff = 0;
    static double gpuUsage = 0.0;

    if (!queryInit) {
      glGenQueries(2, queryID);
      queryInit = true;
    }

    // Retrieve result from previous frame to avoid stall
    GLuint64 timeElapsed = 0;
    GLint available = 0;
    // We actually just use GL_TIME_ELAPSED
    // Double buffer queries
    // query[0] = active this frame
    // query[1] = active next frame? No
    // Use simple swap

    static int queryBack = 0;
    static int queryFront = 1;

    // Check if result is available for the front buffer
    glGetQueryObjectiv(queryID[queryFront], GL_QUERY_RESULT_AVAILABLE,
                       &available);

    glBeginQuery(GL_TIME_ELAPSED, queryID[queryBack]);

    // FPS Calculation
    frameCount++;
    fpsTimer += deltaTime;

    // Accumulate GPU time for averaging
    static double accumulatedGpuTime = 0.0;
    static double accumulatedFrameTime = 0.0;

    if (available) {
      glGetQueryObjectui64v(queryID[queryFront], GL_QUERY_RESULT, &timeElapsed);
      double gpuSeconds = (double)timeElapsed / 1e9;
      accumulatedGpuTime += gpuSeconds;
      accumulatedFrameTime += deltaTime;
    }

    if (fpsTimer >= 1.0f) {
      fpsText = "FPS: " + std::to_string(frameCount) +
                (vsyncEnabled ? " (VSync)" : " (Uncapped)");

      // Calculate Average GPU Usage
      if (accumulatedFrameTime > 0.0) {
        gpuUsage = (accumulatedGpuTime / accumulatedFrameTime) * 100.0;
        if (gpuUsage > 100.0)
          gpuUsage = 100.0;
      } else {
        gpuUsage = 0.0;
      }

      // Reset
      frameCount = 0;
      fpsTimer = 0.0f;
      accumulatedGpuTime = 0.0;
      accumulatedFrameTime = 0.0;
    }

    // Poll PTY
    std::string output = pty.readOutput();
    if (!output.empty()) {
      terminal.processOutput(output);
    }

    // Update Projection (Handle Resize)
    int scrWidth, scrHeight;
    glfwGetFramebufferSize(window, &scrWidth, &scrHeight);

    // Check minimize
    if (scrWidth == 0 || scrHeight == 0) {
      glfwPollEvents();
      continue;
    }

    glm::mat4 projection =
        glm::ortho(0.0f, (float)scrWidth, 0.0f, (float)scrHeight);
    shader.use();
    shader.setMat4("projection", &projection[0][0]);

    // Update terminal size if needed (might be redundant if callback does it,
    // but safe)
    if (globalTerminal) {
      globalTerminal->setSize((float)scrWidth, (float)scrHeight);
    }

    // Render
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    background.render(deltaTime);
    terminal.render(renderer, fontManager, deltaTime);

    glEndQuery(GL_TIME_ELAPSED);
    std::swap(queryBack, queryFront);

    // Render FPS (Top Right, Green)
    // Align right: Width - EstimatedTextWidth
    // "FPS: 60 (Uncapped)" is approx 18 chars. * 10px = 180px.
    // Let's rely on a safe padding.
    float textX = (float)scrWidth - 220.0f;
    renderer.drawText(fontManager, fpsText, textX, (float)scrHeight - 30.0f,
                      1.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    std::string gpuText = "GPU: " + std::to_string((int)gpuUsage) + "%";
    renderer.drawText(fontManager, gpuText, textX, (float)scrHeight - 60.0f,
                      1.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}

void updatePTYSize(int width, int height) {
  if (globalPTY && globalTerminal) {
    float scale = globalTerminal->getScale();
    // Estimated char dimensions (Monaco 18 base)
    float charW = 11.0f * scale;
    float charH = 20.0f * scale;

    int cols = (int)(width / charW);
    int rows = (int)(height / charH);

    // Ensure at least 1x1
    if (cols < 1)
      cols = 1;
    if (rows < 1)
      rows = 1;

    globalPTY->setWindowSize(rows, cols);
  }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
  if (globalTerminal) {
    globalTerminal->setSize(width, height);
  }
  updatePTYSize(width, height);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  if (globalTerminal && globalPTY) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
      if (key == GLFW_KEY_PAGE_UP && (mods & GLFW_MOD_SHIFT)) {
        globalTerminal->scroll(10);
        return;
      }
      if (key == GLFW_KEY_PAGE_DOWN && (mods & GLFW_MOD_SHIFT)) {
        globalTerminal->scroll(-10);
        return;
      }

      // Paste: Cmd+V (Super+V)
      if (key == GLFW_KEY_V && (mods & GLFW_MOD_SUPER)) {
        const char *clipboard = glfwGetClipboardString(window);
        if (clipboard) {
          globalPTY->writeInput(clipboard);
        }
        if (clipboard) {
          globalPTY->writeInput(clipboard);
        }
        return;
      }

      // FPS Toggle (F3)
      if (key == GLFW_KEY_F3) {
        vsyncEnabled = !vsyncEnabled;
        glfwSwapInterval(vsyncEnabled ? 1 : 0);
        return;
      }

      // Zoom In (Cmd + Equal/Plus)
      if (key == GLFW_KEY_EQUAL && (mods & GLFW_MOD_SUPER)) {
        globalTerminal->changeScale(0.1f);
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        updatePTYSize(w, h);
        return;
      }

      // Zoom Out (Cmd + Minus) - Note: GLFW_KEY_MINUS
      if (key == GLFW_KEY_MINUS && (mods & GLFW_MOD_SUPER)) {
        globalTerminal->changeScale(-0.1f);
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        updatePTYSize(w, h);
        return;
      }

      // Copy: Cmd+C (Super+C)
      if (key == GLFW_KEY_C && (mods & GLFW_MOD_SUPER)) {
        if (globalTerminal && globalTerminal->hasSelection()) {
          std::string text = globalTerminal->getSelectionText();
          glfwSetClipboardString(window, text.c_str());
        }
        return;
      }
    }
    globalTerminal->handleInput(key, action, mods, *globalPTY);
  }
}

// Trackpad scroll accumulator
double scrollAccumulator = 0.0;

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  if (globalTerminal) {
    // Accumulate the scroll amount (yoffset is often small float for trackpads)
    scrollAccumulator += yoffset * 3.0; // Multiplier for speed

    // Only scroll if we have accumulated at least 1 line
    if (std::abs(scrollAccumulator) >= 1.0) {
      int lines = (int)scrollAccumulator;
      globalTerminal->scroll(lines);
      scrollAccumulator -= lines;
    }
  }
}

// Mouse Interactions
// Mouse Interactions
bool isDragging = false;
void mouse_button_callback(GLFWwindow *window, int button, int action,
                           int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      isDragging = true;
      double x, y;
      glfwGetCursorPos(window, &x, &y);

      // Calculate Content Scale (DPI support)
      int fw, fh;
      glfwGetFramebufferSize(window, &fw, &fh);

      int ww, wh;
      glfwGetWindowSize(window, &ww, &wh);

      float xScale = (float)fw / (float)ww;
      float yScale = (float)fh / (float)wh;

      // Apply scale
      float processedX = (float)x * xScale;
      float processedY = (float)y * yScale;

      // Flip Y for terminal
      // Terminal expects OpenGL Y (0 at bottom) relative to framebuffer height
      float glY = (float)fh - processedY;

      if (globalTerminal) {
        globalTerminal->startSelection(processedX, glY);
      }
    } else if (action == GLFW_RELEASE) {
      isDragging = false;
    }
  }
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
  if (isDragging && globalTerminal) {
    int fw, fh;
    glfwGetFramebufferSize(window, &fw, &fh);

    int ww, wh;
    glfwGetWindowSize(window, &ww, &wh);

    float xScale = (float)fw / (float)ww;
    float yScale = (float)fh / (float)wh;

    float processedX = (float)xpos * xScale;
    float processedY = (float)ypos * yScale;

    float glY = (float)fh - processedY; // Invert based on framebuffer height

    globalTerminal->updateSelection(processedX, glY);
  }
}

void char_callback(GLFWwindow *window, unsigned int codepoint) {
  if (globalPTY) {
    // Simple unicode to char conversion (ascii only for now)
    char c = (char)codepoint;
    std::cout << "Key typed: " << c << " (" << codepoint << ")" << std::endl;
    globalPTY->writeInput(&c, 1);
  }
}