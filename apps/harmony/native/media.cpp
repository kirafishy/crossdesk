#include "media.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <native_window/external_window.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <hilog/log.h>

namespace crossdesk::harmony {
Renderer::Renderer(Presented presented) : presented_(std::move(presented)), thread_([this] { Run(); }) {}
Renderer::~Renderer() {
  { std::lock_guard lock(mutex_); stop_ = true; ready_.notify_one(); }
  thread_.join();
}
void Renderer::Surface(uint64_t id) {
  std::lock_guard lock(mutex_);
  surface_ = id; changed_ = true; latest_.clear(); ready_.notify_one();
  rendered_ = 0;
  OH_LOG_Print(LOG_APP, LOG_INFO, 0, "CrossDesk", "Video surface attached=%{public}d", id != 0);
}
void Renderer::Clear() {
  std::lock_guard lock(mutex_);
  latest_.clear(); rendered_ = 0;
}
void Renderer::Video(const MiniRtcVideoFrame& frame, uint64_t generation) {
  if (!frame.data || !frame.width || !frame.height || (frame.width & 1) || (frame.height & 1) ||
      frame.width > 8192 || frame.height > 8192 ||
      frame.size < size_t(frame.width) * frame.height * 3 / 2) return;
  std::lock_guard lock(mutex_);
  if (!surface_ || stop_) return;
  width_ = frame.width; height_ = frame.height;
  captured_ = frame.captured_timestamp; received_ = frame.received_timestamp; generation_ = generation;
  latest_.assign(frame.data, frame.data + size_t(width_) * height_ * 3 / 2);
  ready_.notify_one();
}
void Renderer::Run() {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  OHNativeWindow* window = nullptr;
  GLuint program = 0, textures[2]{}, vertex_array = 0, vertex_buffer = 0;
  EGLConfig config{};
  auto destroy = [&] {
    if (display != EGL_NO_DISPLAY) {
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    }
    surface = EGL_NO_SURFACE;
    if (window) OH_NativeWindow_DestroyNativeWindow(window);
    window = nullptr;
  };
  if (display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr)) {
    eglBindAPI(EGL_OPENGL_ES_API);
    // Use one ES3 dispatch library/context/shader pipeline. The emulator's
    // GLESv2 entry points can return zero handles without a GL error.
    const EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLint count = 0;
    eglChooseConfig(display, attrs, &config, 1, &count);
    const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    if (count) context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx);
  }
  OH_LOG_Print(LOG_APP, LOG_INFO, 0, "CrossDesk", "EGL initialized: context=%{public}d error=%{public}x",
               context != EGL_NO_CONTEXT, eglGetError());
  bool reported_error = false;
  bool shader_failed = false;
  for (;;) {
    std::vector<uint8_t> pixels;
    int width, height; uint64_t id, captured, received, generation; bool changed;
    {
      std::unique_lock lock(mutex_);
      ready_.wait(lock, [&] { return stop_ || changed_ || !latest_.empty(); });
      if (stop_) break;
      pixels.swap(latest_); width = width_; height = height_;
      id = surface_; changed = changed_; changed_ = false;
      captured = captured_; received = received_; generation = generation_;
    }
    if (changed) {
      destroy();
      reported_error = false;
      shader_failed = false;
      int window_result = id ? OH_NativeWindow_CreateNativeWindowFromSurfaceId(id, &window) : -1;
      if (id && context != EGL_NO_CONTEXT && window_result == 0) {
        surface = eglCreateWindowSurface(display, config, reinterpret_cast<EGLNativeWindowType>(window), nullptr);
      }
      if (id) OH_LOG_Print(LOG_APP, LOG_INFO, 0, "CrossDesk", "EGL surface: window=%{public}d surface=%{public}d error=%{public}x",
                           window_result, surface != EGL_NO_SURFACE, eglGetError());
    }
    if (pixels.empty()) continue;
    if (surface == EGL_NO_SURFACE || !eglMakeCurrent(display, surface, surface, context)) {
      if (!reported_error) OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "CrossDesk", "Cannot bind video surface: %{public}x", eglGetError());
      reported_error = true; continue;
    }
    if (shader_failed) continue;
    if (!program) {
      OH_LOG_Print(LOG_APP, LOG_INFO, 0, "CrossDesk", "GL version: %{public}s", glGetString(GL_VERSION));
      const char* vs = "#version 300 es\nlayout(location=0) in vec2 position; layout(location=1) in vec2 uv; out vec2 coord; void main(){coord=uv;gl_Position=vec4(position,0.,1.);}";
      const char* fs = "#version 300 es\nprecision mediump float; in vec2 coord; uniform sampler2D yTex; uniform sampler2D uvTex; out vec4 color; void main(){float y=1.164383*(texture(yTex,coord).r-0.062745);vec2 c=texture(uvTex,coord).rg-vec2(0.5);color=vec4(y+1.596027*c.y,y-0.391762*c.x-0.812968*c.y,y+2.017232*c.x,1.);}";
      auto shader = [](GLenum type, const char* src) {
        GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
        GLint compiled = GL_FALSE; glGetShaderiv(s, GL_COMPILE_STATUS, &compiled);
        if (!s || !compiled) {
          char info[1024]{}; glGetShaderInfoLog(s, sizeof(info), nullptr, info);
          OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "CrossDesk", "Video shader compilation failed: id=%{public}u %{public}s", s, info);
          glDeleteShader(s); return GLuint(0);
        }
        return s;
      };
      GLuint v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs);
      if (!v || !f) {
        glDeleteShader(v); glDeleteShader(f); shader_failed = true; continue;
      }
      program = glCreateProgram(); glAttachShader(program, v); glAttachShader(program, f);
      glBindAttribLocation(program, 0, "position"); glBindAttribLocation(program, 1, "uv");
      glLinkProgram(program); glDeleteShader(v); glDeleteShader(f);
      GLint linked = GL_FALSE; glGetProgramiv(program, GL_LINK_STATUS, &linked);
      if (!program || !linked) {
        char info[1024]{}; glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "CrossDesk", "Video shader link failed: %{public}s", info);
        glDeleteProgram(program); program = 0; shader_failed = true; continue;
      }
      const GLfloat vertices[] = {-1,-1,0,1, 1,-1,1,1, -1,1,0,0, 1,1,1,0};
      glGenVertexArrays(1, &vertex_array); glBindVertexArray(vertex_array);
      glGenBuffers(1, &vertex_buffer); glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(2 * sizeof(float)));
      glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
      glGenTextures(2, textures);
      for (GLuint texture : textures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      }
    }
    EGLint sw = 0, sh = 0;
    eglQuerySurface(display, surface, EGL_WIDTH, &sw); eglQuerySurface(display, surface, EGL_HEIGHT, &sh);
    glViewport(0, 0, sw, sh); glClearColor(0.035f, 0.05f, 0.08f, 1); glClear(GL_COLOR_BUFFER_BIT);
    const float scale = std::min(float(sw) / width, float(sh) / height);
    const int rw = int(width * scale), rh = int(height * scale);
    glViewport((sw - rw) / 2, (sh - rh) / 2, rw, rh);
    glUseProgram(program); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < 2; ++i) {
      glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, textures[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, i ? GL_RG8 : GL_R8,
          i ? width / 2 : width, i ? height / 2 : height, 0,
          i ? GL_RG : GL_RED, GL_UNSIGNED_BYTE,
          pixels.data() + (i ? size_t(width) * height : 0));
    }
    glUniform1i(glGetUniformLocation(program, "yTex"), 0);
    glUniform1i(glGetUniformLocation(program, "uvTex"), 1);
    glBindVertexArray(vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    const GLenum draw_error = glGetError();
    const bool swapped = eglSwapBuffers(display, surface);
    if (draw_error == GL_NO_ERROR && swapped && sw > 0 && sh > 0) {
      if (presented_) presented_(pixels.data(), width, height, captured, received, generation);
      if (rendered_.fetch_add(1) == 0)
        OH_LOG_Print(LOG_APP, LOG_INFO, 0, "CrossDesk", "First video frame presented: %{public}dx%{public}d on %{public}dx%{public}d",
                     width, height, sw, sh);
    } else if (!reported_error) {
      OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "CrossDesk", "Video presentation failed: gl=%{public}x egl=%{public}x size=%{public}dx%{public}d",
                   draw_error, eglGetError(), sw, sh);
      reported_error = true;
    }
  }
  if (context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE) {
    eglMakeCurrent(display, surface, surface, context); glDeleteTextures(2, textures); glDeleteProgram(program);
    glDeleteBuffers(1, &vertex_buffer); glDeleteVertexArrays(1, &vertex_array);
  }
  destroy();
  if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
  // Keep the process-wide display alive for capture and other UI contexts.
  eglReleaseThread();
}

}
