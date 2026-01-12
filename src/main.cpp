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

    // FPS Calculation
    frameCount++;
    fpsTimer += deltaTime;
    if (fpsTimer >= 1.0f) {
      fpsText = "FPS: " + std::to_string(frameCount);
      frameCount = 0;
      fpsTimer = 0.0f;
      // std::cout << fpsText << std::endl;
    }

    // Poll PTY
    std::string output = pty.readOutput();
    if (!output.empty()) {
      terminal.processOutput(output);
    }

    // Render
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    background.render(deltaTime);
    terminal.render(renderer, fontManager, deltaTime);

    // Render FPS (Top Right, Green)
    renderer.drawText(fontManager, fpsText, 800.0f - 100.0f, 600.0f - 30.0f,
                      1.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
  if (globalTerminal) {
    globalTerminal->setSize(width, height);
  }
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

void char_callback(GLFWwindow *window, unsigned int codepoint) {
  if (globalPTY) {
    // Simple unicode to char conversion (ascii only for now)
    char c = (char)codepoint;
    std::cout << "Key typed: " << c << " (" << codepoint << ")" << std::endl;
    globalPTY->writeInput(&c, 1);
  }
}