// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "content/public/common/content_switches.h"
#include "ui/gl/gl_surface_egl.h"

namespace content {

// there may be multiple WebGL contexts created with the same ANativeWindow
// as an ANativeWindow can be bound with only one EGLSurface so we need to keep
// the same one onscreen GLSurface for all WebGL contexts.
static scoped_refptr<gfx::GLSurface> onscreen_surface_ = NULL;

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_EQ(handle.transport_type, gfx::NATIVE_DIRECT);

  scoped_refptr<gfx::GLSurface> native_surface;

  if (!stub->is_webgl_onscreen() || !onscreen_surface_.get()) {
    ANativeWindow* window =
        GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(handle.handle);
    if (!window) {
      LOG(WARNING) << "Failed to acquire native widget.";
      return scoped_refptr<gfx::GLSurface>();
    }
    scoped_refptr<gfx::GLSurface> surface =
        new gfx::NativeViewGLSurfaceEGL(window);
    bool initialize_success = surface->Initialize();
    ANativeWindow_release(window);
    if (!initialize_success)
      return scoped_refptr<gfx::GLSurface>();
    if (stub->is_webgl_onscreen()) {
      LOG(ERROR) << "WebGL onscreen save surface";
      onscreen_surface_ = make_scoped_refptr(surface.get());
    }
    native_surface = surface;
  } else {
    LOG(ERROR) << "WebGL onscreen reuse surface";
    native_surface = onscreen_surface_;
  }
  return scoped_refptr<gfx::GLSurface>(
      new PassThroughImageTransportSurface(manager, stub, native_surface.get()));
}

}  // namespace content
