#include "Background.h"
#include <fstream>
#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "../dependencies/stb/stb_image.h"

Background::Background()
    : currentFrameIndex(0), currentTime(0.0f), shader(nullptr) {
  initRenderData();
  createShader();
}

Background::~Background() {
  for (auto &f : frames) {
    glDeleteTextures(1, &f.textureID);
  }
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  if (shader)
    delete shader;
}

void Background::initRenderData() {
  // Full screen quad
  float vertices[] = {// positions   // texCoords
                      -1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, -1.0f,
                      0.0f,  0.0f, 1.0f, -1.0f, 1.0f,  0.0f,

                      -1.0f, 1.0f, 0.0f, 1.0f,  1.0f,  -1.0f,
                      1.0f,  0.0f, 1.0f, 1.0f,  1.0f,  1.0f};

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Position attribute
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  // TexCoord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void Background::createShader() {
  const char *vs = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoords;
        out vec2 TexCoords;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoords = aTexCoords;
        }
    )";

  const char *fs = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoords;
        uniform sampler2D bgTexture;
        void main() {
            FragColor = texture(bgTexture, TexCoords);
            // Optional: dim the background to make text readable
            FragColor = vec4(FragColor.rgb * 0.3, 1.0); 
        }
    )";

  shader = new Shader(vs, fs, true);
}

bool Background::load(const std::string &path) {
  // Clear old
  for (auto &f : frames)
    glDeleteTextures(1, &f.textureID);
  frames.clear();

  // Read file into memory
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << path << std::endl;
    return false;
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<char> buffer(size);
  if (!file.read(buffer.data(), size)) {
    std::cerr << "Failed to read file: " << path << std::endl;
    return false;
  }

  int x, y, channels, frameCount;
  int *delays = nullptr;
  stbi_uc *data =
      stbi_load_gif_from_memory((stbi_uc *)buffer.data(), size, &delays, &x, &y,
                                &frameCount, &channels, 4);

  if (!data) {
    // Fallback to regular load if not gif (or single frame)
    // stbi_load_gif returns null if not GIF? Or maybe we should use stbi_load
    // if GIF fails. Let's try standard stbi_load for static images
    data = stbi_load_from_memory((stbi_uc *)buffer.data(), size, &x, &y,
                                 &channels, 4);
    if (!data) {
      std::cerr << "STB failed to load image" << std::endl;
      return false;
    }
    frameCount = 1;
    delays = nullptr;
  }

  // Process frames
  // data is one big buffer: frame0, frame1...
  // size of one frame = x * y * 4
  size_t frameSize = x * y * 4;

  for (int i = 0; i < frameCount; i++) {
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data + (i * frameSize));
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    BackgroundFrame frame;
    frame.textureID = texture;
    if (delays) {
      frame.delay = delays[i];
      if (frame.delay <= 0)
        frame.delay = 100; // Default 100ms
    } else {
      frame.delay = 0; // Static
    }
    frames.push_back(frame);
  }

  stbi_image_free(data);
  if (delays)
    stbi_image_free(delays);

  std::cout << "Loaded background: " << path << " (" << frameCount << " frames)"
            << std::endl;
  return true;
}

void Background::render(float deltaTime) {
  if (frames.empty())
    return;
  if (!shader)
    return;

  if (frames.size() > 1) {
    currentTime += deltaTime * 1000.0f; // to ms
    if (currentTime >= frames[currentFrameIndex].delay) {
      currentTime -= frames[currentFrameIndex].delay;
      currentFrameIndex = (currentFrameIndex + 1) % frames.size();
    }
  }

  shader->use();
  shader->setInt("bgTexture", 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, frames[currentFrameIndex].textureID);

  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}
