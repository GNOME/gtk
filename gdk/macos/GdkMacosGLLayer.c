/* GdkMacosGLLayer.c
 *
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/* Based on Chromium image_transport_surface_calayer_mac.mm
 * See the BSD-style license above.
 */

#include "config.h"

#include <OpenGL/gl.h>

#import "GdkMacosGLLayer.h"

@implementation GdkMacosGLLayer

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

-(id)initWithContext:(NSOpenGLContext *)shared
{
  [super init];
  _shared = [shared retain];
  return self;
}

-(void)dealloc
{
  [_shared release];
  _shared = nil;

  [super dealloc];
}

-(void)setContentsRect:(NSRect)bounds
{
  _pixelSize = bounds.size;
  [super setContentsRect:bounds];
}

-(CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
  return CGLRetainPixelFormat ([[_shared pixelFormat] CGLPixelFormatObj]);
}

-(CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
  CGLContextObj context = NULL;
  CGLCreateContext (pixelFormat, [_shared CGLContextObj], &context);
  return context;
}

-(BOOL)canDrawInCGLContext:(CGLContextObj)glContext
               pixelFormat:(CGLPixelFormatObj)pixelFormat
              forLayerTime:(CFTimeInterval)timeInterval
               displayTime:(const CVTimeStamp*)timeStamp
{
  return YES;
}

-(void)drawInCGLContext:(CGLContextObj)glContext
            pixelFormat:(CGLPixelFormatObj)pixelFormat
           forLayerTime:(CFTimeInterval)timeInterval
            displayTime:(const CVTimeStamp*)timeStamp
{
  if (_texture == 0)
    return;

  glClearColor (1, 0, 1, 1);
  glClear (GL_COLOR_BUFFER_BIT);
  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv (GL_VIEWPORT, viewport);
  NSSize viewportSize = NSMakeSize (viewport[2], viewport[3]);

  /* Set the coordinate system to be one-to-one with pixels. */
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, viewportSize.width, 0, viewportSize.height, -1, 1);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  /* Draw a fullscreen quad. */
  glColor4f (1, 1, 1, 1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, _texture);
  glBegin (GL_QUADS);
  {
    glTexCoord2f (0, 0);
    glVertex2f (0, 0);
    glTexCoord2f (0, _pixelSize.height);
    glVertex2f (0, _pixelSize.height);
    glTexCoord2f (_pixelSize.width, _pixelSize.height);
    glVertex2f (_pixelSize.width, _pixelSize.height);
    glTexCoord2f (_pixelSize.width, 0);
    glVertex2f (_pixelSize.width, 0);
  }
  glEnd ();
  glBindTexture (0, _texture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);
  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

-(void)setTexture:(GLuint)texture
{
  _texture = texture;
  [self setNeedsDisplay];
}

G_GNUC_END_IGNORE_DEPRECATIONS

@end
