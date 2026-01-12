#include "config.h" // Ignore this error
int main() {
  GLFWwindow *window;
  if (!glfwInit()) {
    std::cout << "GLFW couldn't start" << std::endl;
    return -1;
  }
  window = glfwCreateWindow(640, 480, "My Window", NULL, NULL);
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }
  glfwTerminate();
  return 0;
}