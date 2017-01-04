#include "gl-utils.h"

GLuint CreateTexture(int size_x, int size_y, uint8_t *buffer = nullptr)
{
  uint8_t * source_buf = buffer;

  if (not source_buf)
    {
      LOGI("Creating dummy texture");
      source_buf = new uint8_t[size_x * size_y * 4];
      for (int x = 0; x < size_x; ++x)
	for (int y = 0; y < size_y; ++y)
	  {
	    unsigned int pixel_index = (x * size_y + y);
	    uint8_t *pixel = &source_buf[pixel_index * 4];
	    pixel[0] = static_cast<uint8_t>(0xff);
	    pixel[1] = static_cast<uint8_t>((x + y) & 0xff);
	    pixel[2] = static_cast<uint8_t>((x + y) & 0xff);
	    pixel[3] = static_cast<uint8_t>(0xff);
	  }
      LOGI("Texture data initialized");
    }

  GLuint tex_id;
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_x, size_y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, source_buf);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);
  CHECK(glGetError() == GL_NO_ERROR);
  if (not framebuffer)
    delete[] source_buf;
  LOGI("Creating texture done");
  return tex_id;
}
