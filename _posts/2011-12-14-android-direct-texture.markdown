---
layout: post
title: "Using direct textures on Android"
date: 2011-12-16 15 12:30:00
---

I've been working at [Mozilla](http://mozilla.com) on [Firefox Mobile](http://www.mozilla.org/en-US/mobile/) for a few months now. One of the goals of the new [native UI](http://lucasr.org/2011/11/15/native-ui-for-firefox-on-android/) is
to have liquid smooth scrolling and panning at all times. Unsurprisingly, we do this by drawing into an OpenGL texture and moving it around on the screen. This is pretty fast until you run out of content
in the texture and need to update it. Gecko runs in a separate thread and can draw to a buffer there without blocking us, but uploading that data into the texture is where problems arise. Right now
we use just one very large texture (usually 2048x2048), and glTexSubImage2D can take anywhere from 25ms to 60ms. Given that our target is 60fps, we have about 16ms to draw a frame. This means we're guaranteed to miss at least one frame every time we upload, but likely more than that. What we need is a
way of uploading texture data asynchronously (and preferably quicker). This is where direct textures can help.

If you haven't read Dianne Hackborn's recent posts on the Android graphics stack, you're missing out ([part 1](https://plus.google.com/105051985738280261832/posts/2FXDCz8x93s), [part 2](https://plus.google.com/u/0/105051985738280261832/posts/XAZ4CeVP6DC)). The window compositing system she describes (called SurfaceFlinger) is particularly interesting because it is close to the problem we have in Firefox. One of the pieces Android uses to to draw windows is the gralloc module. As you may have guessed, gralloc is short for 'graphics alloc'. You can see the short and simple API for it [here](https://github.com/android/platform_hardware_libhardware/blob/master/include/hardware/gralloc.h). Android has a wrapper class that encapsulates access to this called `GraphicBuffer`. It has an even nicer API, found [here](https://github.com/android/platform_frameworks_base/blob/master/include/ui/GraphicBuffer.h).  Usage is very straightforward. Simply create the `GraphicBuffer` with whatever size and pixel format you need, lock it, write your bits, and unlock. One of the major wins here is that you can use the `GraphicBuffer` instance from any thread. So not only does this reduce a copy of your image, but it also means you can upload it without blocking the rendering loop!

To get it on the screen using OpenGL, you can create an `EGLImageKHR` from the `GraphicBuffer` and bind it to a texture:
{% highlight c++ %}
#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#define EGL_IMAGE_PRESERVED_KHR   0x30D2

GraphicBuffer* buffer = new GraphicBuffer(1024, 1024, PIXEL_FORMAT_RGB_565,
                                          GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                                          GraphicBuffer::USAGE_HW_TEXTURE);

unsigned char* bits = NULL;
buffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, (void**)&bits);

// Write bitmap data into 'bits' here

buffer->unlock();

// Create the EGLImageKHR from the native buffer
EGLint eglImgAttrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
EGLImageKHR img = eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                                    EGL_NATIVE_BUFFER_ANDROID,
                                    (EGLClientBuffer)buffer->getNativeBuffer(),
                                    eglImgAttrs);

// Create GL texture, bind to GL_TEXTURE_2D, etc.

// Attach the EGLImage to whatever texture is bound to GL_TEXTURE_2D
glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img);
{% endhighlight %}

The resulting texture can be used as a regular one, with one caveat. Whenever you manipulate pixel data, the changes will be reflected on the screen immediately after `unlock`. You probably want
to double buffer in order to avoid problems here.

If you've ever used the Android NDK, it won't be surprising that `GraphicBuffer` (or anything similar) doesn't exist there. In order to use any of this in your app you'll need to resort to
`dlopen` hacks. It's a pretty depressing situation. Google uses this all over the OS, but doesn't seem to think that apps need a high performance API. But wait, it gets worse. Even after jumping
through these hoops, some gralloc drivers don't allow regular apps to play ball. So far, testing indicates that this is the case on Adreno and Mali GPUs. Thankfully, PowerVR and Tegra allow it, which covers a fair number of devices.

With any luck, I'll land the patches that use this in Firefox Mobile today. The result should be a much smoother panning and zooming experience on devices where gralloc is allowed to work.
