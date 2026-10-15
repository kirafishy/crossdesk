// GLES presentation only; all UI and callbacks belong to GuiApplication.
struct Surface {
  DesktopWindow& owner;
  explicit Surface(DesktopWindow& window) : owner(window) {}
  ~Surface() { DetachSurface(); }
EGLDisplay display = EGL_NO_DISPLAY;
EGLContext context = EGL_NO_CONTEXT;
EGLSurface surface = EGL_NO_SURFACE;
OHNativeWindow* native_window = nullptr;
GLuint program = 0, texture = 0, vao = 0;
float density = 1;
std::vector<slint::Rgb8Pixel> pixels;
uint64_t frames = 0;
uint32_t rendered_width = 0, rendered_height = 0;
void DetachSurface() {
  if (display != EGL_NO_DISPLAY) {
    if (surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT) {
      eglMakeCurrent(display, surface, surface, context);
      glDeleteTextures(1, &texture); glDeleteProgram(program); glDeleteVertexArrays(1, &vao);
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
    // EGL display is shared by the application windows.
  }
  if (native_window) OH_NativeWindow_DestroyNativeWindow(native_window);
  display = EGL_NO_DISPLAY; context = EGL_NO_CONTEXT; surface = EGL_NO_SURFACE;
  native_window = nullptr; program = texture = vao = 0;
  // ReusedBuffer's retained contents belong to the Slint window, not the EGL
  // surface. Keep them when a hidden window gets a new XComponent; otherwise
  // Slint repaints only dirty regions over a cleared (black) buffer.
  frames = 0;
  rendered_width = rendered_height = 0;
}

GLuint Shader(GLenum type, const char* source) {
  GLuint value = glCreateShader(type);
  glShaderSource(value, 1, &source, nullptr); glCompileShader(value);
  GLint ok = 0; glGetShaderiv(value, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024]{}; glGetShaderInfoLog(value, sizeof(log), nullptr, log);
    OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "SlintDesktop", "Shader failed: %{public}s", log);
    glDeleteShader(value); return 0;
  }
  return value;
}

bool AttachSurface(uint64_t id, float scale) {
  DetachSurface();
  if (!id || !std::isfinite(scale) || scale <= 0) return false;
  owner.SetDensity(scale);
  density = scale;
  if (OH_NativeWindow_CreateNativeWindowFromSurfaceId(id, &native_window) != 0) return false;
  display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!eglInitialize(display, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_ES_API)) return false;
  const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
  EGLConfig config; EGLint count = 0;
  if (!eglChooseConfig(display, attributes, &config, 1, &count) || count == 0) return false;
  const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
  surface = eglCreateWindowSurface(display, config, reinterpret_cast<EGLNativeWindowType>(native_window), nullptr);
  if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE ||
      !eglMakeCurrent(display, surface, surface, context)) return false;
  const char* vertex = "#version 300 es\n"
      "out vec2 uv; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
      "uv=vec2(p.x,1.-p.y); gl_Position=vec4(p*2.-1.,0.,1.);}";
  const char* fragment = "#version 300 es\nprecision mediump float;"
      "in vec2 uv; uniform sampler2D frame; out vec4 color; void main(){color=texture(frame,uv);}";
  GLuint v = Shader(GL_VERTEX_SHADER, vertex), f = Shader(GL_FRAGMENT_SHADER, fragment);
  if (!v || !f) { glDeleteShader(v); glDeleteShader(f); return false; }
  program = glCreateProgram(); glAttachShader(program, v); glAttachShader(program, f);
  glLinkProgram(program); glDeleteShader(v); glDeleteShader(f);
  GLint linked = 0; glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) return false;
  glGenVertexArrays(1, &vao); glBindVertexArray(vao);
  glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  OH_LOG_Print(LOG_APP, LOG_INFO, 0, "SlintDesktop", "Surface attached scale=%{public}.3f", density);
  return true;
}

void Render() {
  if (surface == EGL_NO_SURFACE) return;
  EGLint width = 0, height = 0;
  eglQuerySurface(display, surface, EGL_WIDTH, &width);
  eglQuerySurface(display, surface, EGL_HEIGHT, &height);
  if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return;
  if (rendered_width != uint32_t(width) || rendered_height != uint32_t(height)) {
    rendered_width = width; rendered_height = height;
    owner.extent = slint::PhysicalSize({uint32_t(width), uint32_t(height)});
    owner.window().dispatch_resize_event(slint::LogicalSize({width / density, height / density}));
    pixels.resize(size_t(width) * height); owner.dirty = true;
    OH_LOG_Print(LOG_APP, LOG_INFO, 0, "SlintDesktop", "Window %{public}d resize %{public}dx%{public}d", owner.id, width, height);
  }
  if (!owner.dirty && !owner.window().has_active_animations()) return;
  if (pixels.size() != size_t(width) * height) pixels.resize(size_t(width) * height);
  const auto started = std::chrono::steady_clock::now();
  owner.dirty = false;
  owner.software.render(pixels, width);
  if (!eglMakeCurrent(display, surface, surface, context)) return;
  glViewport(0, 0, width, height); glUseProgram(program);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  glUniform1i(glGetUniformLocation(program, "frame"), 0);
  glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES, 0, 3);
  const GLenum error = glGetError();
  const bool swapped = eglSwapBuffers(display, surface);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (error || !swapped) {
    OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "SlintDesktop", "Present failed gl=%{public}x egl=%{public}x", error, eglGetError());
  } else if (++frames == 1 || frames % 120 == 0) {
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    OH_LOG_Print(LOG_APP, LOG_INFO, 0, "SlintDesktop", "Presented frames=%{public}llu frame_ms=%{public}.2f", (unsigned long long)frames, ms);
  }
}

};
