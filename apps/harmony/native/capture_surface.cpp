#include "capture_surface.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <hilog/log.h>
#include <algorithm>
#include <vector>

namespace crossdesk::harmony {
OHNativeWindow* CaptureSurface::Start(int width, int height, Frame frame) {
  Stop();
  if (width <= 0 || height <= 0 || (width & 1) || (height & 1) || !frame) return nullptr;
  {
    std::lock_guard lock(mutex_);
    prepared_ = false; stopping_ = false; pending_ = 0; window_ = nullptr;
  }
  thread_ = std::thread([this, width, height, frame = std::move(frame)] { Run(width, height, frame); });
  std::unique_lock lock(mutex_);
  ready_.wait(lock, [this] { return prepared_; });
  auto* window = window_;
  lock.unlock();
  if (!window) Stop(); // Finish failed EGL cleanup before trying the CPU path.
  return window;
}
void CaptureSurface::Stop() {
  { std::lock_guard lock(mutex_); stopping_ = true; ready_.notify_one(); }
  if (thread_.joinable()) thread_.join();
}
void CaptureSurface::Run(int width, int height, Frame frame) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface target = EGL_NO_SURFACE;
  OH_NativeImage* image = nullptr;
  GLuint input = 0, output = 0, fbo = 0, program = 0, vao = 0;
  bool current = false;
  const auto prepare = [&]() -> bool {
    if (!eglInitialize(display, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_ES_API)) return false;
    const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig config{}; EGLint count = 0;
    if (!eglChooseConfig(display, attributes, &config, 1, &count) || count == 0) return false;
    const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    const EGLint pbuffer_attributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    target = eglCreatePbufferSurface(display, config, pbuffer_attributes);
    if (context == EGL_NO_CONTEXT || target == EGL_NO_SURFACE) return false;
    if (!(current = eglMakeCurrent(display, target, target, context))) return false;
    GLint limit = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    if (width > limit || height > limit) return false;
    const char* vertex = R"(#version 300 es
      uniform mat4 transform; out vec2 uv;
      void main() {
        vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
        uv = (transform * vec4(p, 0.0, 1.0)).xy;
        gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
      })";
    const char* fragment = R"(#version 300 es
      #extension GL_OES_EGL_image_external_essl3 : require
      precision mediump float;
      uniform samplerExternalOES source; in vec2 uv; out vec4 color;
      void main() { color = texture(source, uv); })";
    const auto shader = [](GLenum type, const char* source) {
      GLuint value = glCreateShader(type); glShaderSource(value, 1, &source, nullptr); glCompileShader(value);
      GLint ok = 0; glGetShaderiv(value, GL_COMPILE_STATUS, &ok);
      if (!ok) { glDeleteShader(value); return GLuint(0); } return value;
    };
    GLuint v = shader(GL_VERTEX_SHADER, vertex), f = shader(GL_FRAGMENT_SHADER, fragment);
    if (!v || !f) { glDeleteShader(v); glDeleteShader(f); return false; }
    program = glCreateProgram(); glAttachShader(program, v); glAttachShader(program, f); glLinkProgram(program);
    glDeleteShader(v); glDeleteShader(f);
    GLint linked = 0; glGetProgramiv(program, GL_LINK_STATUS, &linked); if (!linked) return false;
    glGenTextures(1, &input); glBindTexture(GL_TEXTURE_EXTERNAL_OES, input);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    image = OH_NativeImage_Create(input, GL_TEXTURE_EXTERNAL_OES);
    if (!image) return false;
    if (OH_ConsumerSurface_SetDefaultSize(image, width, height) != 0) return false;
    OH_OnFrameAvailableListener listener{this, [](void* context) {
      auto* self = static_cast<CaptureSurface*>(context);
      std::lock_guard lock(self->mutex_);
      self->pending_ = std::min(self->pending_ + 1, 64u); self->ready_.notify_one();
    }};
    if (OH_NativeImage_SetOnFrameAvailableListener(image, listener) != 0) return false;
    glGenTextures(1, &output); glBindTexture(GL_TEXTURE_2D, output);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    window_ = OH_NativeImage_AcquireNativeWindow(image);
    return window_ != nullptr;
  };
  const bool success = prepare();
  {
    std::lock_guard lock(mutex_);
    if (!success) window_ = nullptr;
    prepared_ = true; ready_.notify_one();
  }
  if (success) {
    OH_LOG_Print(LOG_APP, LOG_INFO, 0, "SlintDesktop", "Capture uses GLES surface readback");
    std::vector<uint8_t> pixels(size_t(width) * height * 4);
    const auto source_location = glGetUniformLocation(program, "source");
    const auto transform_location = glGetUniformLocation(program, "transform");
    bool reported_error = false;
    for (;;) {
      {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [this] { return stopping_ || pending_ > 0; });
        if (stopping_) break;
        --pending_;
      }
      if (OH_NativeImage_UpdateSurfaceImage(image) != 0) continue;
      float transform[16]{};
      if (OH_NativeImage_GetTransformMatrixV2(image, transform) != 0) continue;
      glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, width, height);
      glUseProgram(program); glBindVertexArray(vao);
      glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_EXTERNAL_OES, input);
      glUniform1i(source_location, 0);
      glUniformMatrix4fv(transform_location, 1, GL_FALSE, transform);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      const auto error = glGetError();
      if (error == GL_NO_ERROR) {
        // NativeImage's V2 matrix already accounts for the producer's buffer
        // orientation. Flipping glReadPixels rows again inverts the desktop.
        frame(pixels.data(), width, height, width * 4);
      } else if (!reported_error) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "SlintDesktop", "Capture GLES readback failed: %{public}u", error);
        reported_error = true;
      }
    }
  } else OH_LOG_Print(LOG_APP, LOG_WARN, 0, "SlintDesktop", "GLES capture surface unavailable");
  if (image) { OH_NativeImage_UnsetOnFrameAvailableListener(image); OH_NativeImage_Destroy(&image); }
  if (current) {
    glDeleteVertexArrays(1, &vao); glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &input); glDeleteTextures(1, &output); glDeleteProgram(program);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }
  if (target != EGL_NO_SURFACE) eglDestroySurface(display, target);
  if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
  // The default display is shared with Slint/ArkUI and other video windows.
  // Terminating it here would invalidate their contexts on disconnect.
  eglReleaseThread();
  { std::lock_guard lock(mutex_); window_ = nullptr; }
}
}
