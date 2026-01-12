#include "../src/PTYHandler.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

void test_shell_interaction() {
  std::cout << "Starting PTY Shell Test..." << std::endl;

  PTYHandler pty;
  if (!pty.spawnShell()) {
    std::cerr << "Failed to spawn shell!" << std::endl;
    exit(1);
  }
  std::cout << "Shell spawned." << std::endl;

  // Give it a moment to initialize prompt
  std::cout << "Waiting for prompt..." << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  std::string initial = pty.readOutput();
  std::cout << "Initial output (" << initial.length() << " bytes)."
            << std::endl;
  // Print safely
  for (char c : initial) {
    if (c >= 32 && c <= 126)
      std::cout << c;
    else
      std::cout << ".";
  }
  std::cout << std::endl;

  // Send a command
  std::string cmd = "echo 'Hello OpenGL Terminal'\n";
  pty.writeInput(cmd);
  std::cout << "Sent command: " << cmd << std::endl;

  // Read response loop
  std::string accumulated_output = "";
  std::cout << "Listening for response..." << std::endl;
  for (int i = 0; i < 20; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string chunk = pty.readOutput();
    if (!chunk.empty()) {
      std::cout << "Chunk: '" << chunk << "'" << std::endl;
      accumulated_output += chunk;
    }
    if (accumulated_output.find("Hello OpenGL Terminal") != std::string::npos) {
      std::cout << "Found expected output!" << std::endl;
      break;
    }
  }

  std::cout << "Accumulated Output:\n" << accumulated_output << std::endl;

  if (accumulated_output.find("Hello OpenGL Terminal") != std::string::npos) {
    std::cout << "TEST PASSED" << std::endl;
  } else {
    std::cout << "TEST FAILED: Did not find expected echo." << std::endl;
    exit(1);
  }
}

int main() {
  setbuf(stdout, NULL);
  test_shell_interaction();
  return 0;
}
