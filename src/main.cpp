#include "config.h"

#include "FontManager.h"
#include "PTYHandler.h"
#include "Renderer.h"
#include "Shader.h"
#include "Terminal.h"

// Global state for callbacks
Terminal *globalTerminal = nullptr;
PTYHandler *globalPTY = nullptr;

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods);
void char_callback(GLFWwindow *window, unsigned int codepoint);

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

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Initialize Systems
  Shader shader("src/text.vs", "src/text.fs");
  glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
  shader.use();
  shader.setMat4("projection", &projection[0][0]);

  Renderer renderer(shader);
  FontManager fontManager;
  if (!fontManager.loadFont("/System/Library/Fonts/Monaco.ttf", 48)) {
    if (!fontManager.loadFont("/Library/Fonts/Arial.ttf", 48)) {
      std::cout << "Failed to load font" << std::endl;
    }
  }

  Terminal terminal(800.0f, 600.0f);
  globalTerminal = &terminal;

  PTYHandler pty;
  globalPTY = &pty;

  if (pty.spawnShell()) {
    std::cout << "Shell spawned successfully" << std::endl;
  } else {
    std::cout << "Failed to spawn shell" << std::endl;
  }

  pty.setWindowSize(15, 80); // Approximate

  while (!glfwWindowShouldClose(window)) {
    // Poll PTY
    std::string output = pty.readOutput();
    if (!output.empty()) {
      terminal.processOutput(output);
    }

    // Render
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    terminal.render(renderer, fontManager);

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
    globalTerminal->handleInput(key, action, mods, *globalPTY);
  }
}

void char_callback(GLFWwindow *window, unsigned int codepoint) {
  if (globalPTY) {
    // Simple unicode to char conversion (ascii only for now)
    char c = (char)codepoint;
    globalPTY->writeInput(&c, 1);
  }
}