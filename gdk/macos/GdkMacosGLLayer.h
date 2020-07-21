/* GdkMacosGLLayer.h
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

#include <AppKit/AppKit.h>
#include <glib.h>

#define GDK_IS_MACOS_GL_LAYER(obj) ((obj) && [obj isKindOfClass:[GdkMacosGLLayer class]])

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

@interface GdkMacosGLLayer : CAOpenGLLayer
{
  NSOpenGLContext *_shared;
  GLuint _texture;
  NSSize _pixelSize;
}

-(id)initWithContext:(NSOpenGLContext *)shared;
-(void)setTexture:(GLuint)texture;

@end

G_GNUC_END_IGNORE_DEPRECATIONS
