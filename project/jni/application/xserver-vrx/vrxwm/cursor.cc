#include "cursor.h"

#include <GLES2/gl2.h>

#include "common.h"
#include "gl-utils.h"
#include "vrx_renderer.h"

unsigned int VRXCursor::texId = 0;
const float VRXCursor::cursorsize = 5.0;
const float VRXCursor::distance = 500.0;
const char *VRXCursor::vertexShader =
  "uniform mat4 u_MVP;\n"
  "attribute vec2 a_Texcoord;\n"
  "attribute vec4 a_Position;\n"
  "varying vec2 v_Texcoord;\n"
  "void main() {\n"
  "  v_Texcoord = a_Texcoord;\n"
  "  gl_Position = u_MVP * a_Position;\n"
  "}\n";

const char *VRXCursor::fragmentShader =
  "precision mediump float;\n"
  "uniform sampler2D u_Texture;\n"
  "varying vec2 v_Texcoord;\n"
  "void main() {\n"
  "  gl_FragColor = texture2D(u_Texture, v_Texcoord);\n"
  "}\n";

std::array<float, 18> VRXCursor::coords;
std::array<float, 12> VRXCursor::texcoords;
gvr::Mat4f VRXCursor::modelview;
unsigned int VRXCursor::program;
int VRXCursor::MVP_param;
int VRXCursor::tex_param;
int VRXCursor::pos_param;

bool VRXCursor::InitGL()
{
  unsigned char texture[texsize * texsize * 4];

  for (unsigned int x = 0; x < texsize; ++x)
    for (unsigned int y = 0; y < texsize; ++y)
      {
	unsigned char *pixel = &texture[(y * texsize + x) * 4];
	unsigned char invalpha = std::min(255u, (POW2(x - texsize / 2) + POW2(y - texsize /2)) * 4);
	unsigned char alpha = 255 - invalpha;
	pixel[0] = 0x0;
	pixel[1] = 0xff;
	pixel[2] = 0x0;
	pixel[3] = alpha;
      }
  texId = CreateTexture(texsize, texsize, texture);

  if (not texId)
    return false;

  modelview = {1.0f, 0.0f, 0.0f, 0.0f,
	       0.0f, 1.0f, 0.0f, 0.0f,
	       0.0f, 0.0f, 1.0f, -distance,
	       0.0f, 0.0f, 0.0f, 1.0f};
  coords = {
    -cursorsize,  cursorsize,  0.0f,
    -cursorsize, -cursorsize,  0.0f,
     cursorsize,  cursorsize,  0.0f,
    -cursorsize, -cursorsize,  0.0f,
     cursorsize, -cursorsize,  0.0f,
     cursorsize,  cursorsize,  0.0f,
  };

  texcoords = {
    0.0f, 0.0f, // v0
    0.0f, 1.0f, // v1
    1.0f, 0.0f, // v2
    0.0f, 1.0f, // v1
    1.0f, 1.0f, // v3
    1.0f, 0.0f, // v2
  };

  int vshader = LoadGLShader(GL_VERTEX_SHADER, &vertexShader);
  CheckGLError("Cursor vertex shader");

  int fshader = LoadGLShader(GL_FRAGMENT_SHADER, &fragmentShader);
  CheckGLError("Cursor fragment shader");

  program = glCreateProgram();
  CheckGLError("Cursor program create");

  glAttachShader(program, vshader);
  CheckGLError("Cursor program attach vertex shader");

  glAttachShader(program, fshader);

  CheckGLError("Cursor program attach fragment shader");

  glLinkProgram(program);
  CheckGLError("Cursor program link");

  pos_param = glGetAttribLocation(program, "a_Position");
  tex_param = glGetAttribLocation(program, "a_Texcoord");
  MVP_param = glGetUniformLocation(program, "u_MVP");

  return true;
}

void VRXCursor::SetCursorMatrix(const gvr::Mat4f &newmat)
{
  modelview = newmat;
}

void VRXCursor::SetCursorPosition(const Vec4f &newpos)
{
  modelview.m[0][3] = newpos.x();
  modelview.m[1][3] = newpos.y();
  modelview.m[2][3] = newpos.z();
}

void VRXCursor::Draw(const gvr::Mat4f &mvp)
{
  glUseProgram(program);

  // Set the position of the window
  glVertexAttribPointer(pos_param, 3, GL_FLOAT, false, 0,
                        coords.data());
  glEnableVertexAttribArray(pos_param);

  CheckGLError("Drawing cursor: position");

  // Set texture
  glVertexAttribPointer(tex_param, 2, GL_FLOAT, false, 0,
			texcoords.data());
  glEnableVertexAttribArray(tex_param);
  glActiveTexture(GL_TEXTURE0);

  CheckGLError("Drawing cursor: texture");

  glBindTexture(GL_TEXTURE_2D, texId);

  CheckGLError("Drawing cursor: texture bind");

  gvr::Mat4f wmat = MatrixMul(mvp, modelview);
  glUniformMatrix4fv(MVP_param, 1, GL_FALSE,
                     MatrixToGLArray(wmat).data());

  CheckGLError("Drawing cursor: view matrix");

  glEnable(GL_BLEND);

  glDrawArrays(GL_TRIANGLES, 0, 6);
  CheckGLError("Drawing cursor");

  glDisable(GL_BLEND);
}

bool VRXCursor::IntersectWindow(const WmWindow *window, const Vec4f &direction, Vec4f &isect)
{
  Planef wplane({0.0, 0.0, 1.0}, {0.0, 0.0, static_cast<float>(-WmWindow::DEFAULT_DISTANCE)});
  isect = wplane.intersectLine(direction);

  if ((isect.x() <= window->getHalfWidth() and isect.x() > -window->getHalfWidth())
      and (isect.y() <= window->getHalfHeight() and isect.y() > -window->getHalfHeight()))
    return true;
  return false;
}

bool VRXCursor::IntersectWindow(unsigned int halfWidth, unsigned int halfHeight,
                                const Vec4f &direction, Vec4f &isect)
{
  Planef wplane({0.0, 0.0, 1.0}, {0.0, 0.0, static_cast<float>(-WmWindow::DEFAULT_DISTANCE)});
  isect = wplane.intersectLine(direction);

  if ((isect.x() <= halfWidth and isect.x() > -halfWidth)
      and (isect.y() <= halfHeight and isect.y() > -halfHeight))
    return true;
  return false;
}
