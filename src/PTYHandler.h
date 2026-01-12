#pragma once

#include "config.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

class PTYHandler {
public:
  PTYHandler();
  ~PTYHandler();

  bool spawnShell();
  std::string readOutput();
  void writeInput(const char *input, size_t size);
  void writeInput(std::string input);

  // Set terminal size for the PTY
  void setWindowSize(int rows, int cols);

private:
  int masterFd;
  pid_t pid;
};
