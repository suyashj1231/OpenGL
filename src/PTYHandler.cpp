#include "PTYHandler.h"
#include <signal.h>
#include <sys/ioctl.h>

PTYHandler::PTYHandler() : masterFd(-1), pid(-1) {}

PTYHandler::~PTYHandler() {
  if (masterFd != -1) {
    close(masterFd);
  }
  if (pid != -1) {
    kill(pid, SIGKILL); // Brutal but effective for now
  }
}

bool PTYHandler::spawnShell() {
  struct winsize win = {24, 80, 0, 0}; // Default size

  pid = forkpty(&masterFd, NULL, NULL, &win);

  if (pid < 0) {
    perror("forkpty");
    return false;
  } else if (pid == 0) {
    // Child process
    // Execute the shell
    // Set TERM for color support
    // Set TERM for color support
    setenv("TERM", "xterm-256color", 1);

    // Custom Prompt: Cyan Arrow -> Gold User -> Reset
    // Note: We use ANSI codes directly.
    // \e[1;36m = Bright Cyan
    // \e[1;33m = Bright Box (Gold-ish)
    // \u = User
    // \w = Working Directory
    setenv("PS1",
           "\\[\\e[1;36m\\]➜ \\[\\e[1;33m\\]\\u@opengl \\[\\e[1;34m\\]\\W "
           "\\[\\e[0m\\]% ",
           1);

    const char *shell = getenv("SHELL");
    if (!shell)
      shell = "/bin/sh";

    // Prepare args
    char *args[] = {(char *)shell, NULL};

    execvp(shell, args);

    // If we get here, exec failed
    perror("execvp");
    exit(1);
  }

  // Parent process
  // Set master fd to non-blocking
  int flags = fcntl(masterFd, F_GETFL, 0);
  fcntl(masterFd, F_SETFL, flags | O_NONBLOCK);

  return true;
}

std::string PTYHandler::readOutput() {
  char buffer[1024];
  std::string output = "";

  while (true) {
    ssize_t bytesRead = read(masterFd, buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
      buffer[bytesRead] = '\0';
      output += buffer;
    } else {
      // Error or EAGAIN (no more data)
      break;
    }
  }
  return output;
}

void PTYHandler::writeInput(const char *input, size_t size) {
  if (masterFd != -1) {
    write(masterFd, input, size);
  }
}

void PTYHandler::writeInput(std::string input) {
  writeInput(input.c_str(), input.length());
}

void PTYHandler::setWindowSize(int rows, int cols) {
  if (masterFd != -1) {
    struct winsize win = {(unsigned short)rows, (unsigned short)cols, 0, 0};
    ioctl(masterFd, TIOCSWINSZ, &win);
  }
}
