/* gskngltypesprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GSK_NGL_TYPES_PRIVATE_H__
#define __GSK_NGL_TYPES_PRIVATE_H__

#include <epoxy/gl.h>
#include <graphene.h>
#include <gdk/gdk.h>
#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GSK_NGL_N_VERTICES 6

typedef struct _GskNglAttachmentState GskNglAttachmentState;
typedef struct _GskNglBuffer GskNglBuffer;
typedef struct _GskNglCommandQueue GskNglCommandQueue;
typedef struct _GskNglCompiler GskNglCompiler;
typedef struct _GskNglDrawVertex GskNglDrawVertex;
typedef struct _GskNglRenderTarget GskNglRenderTarget;
typedef struct _GskNglGlyphLibrary GskNglGlyphLibrary;
typedef struct _GskNglIconLibrary GskNglIconLibrary;
typedef struct _GskNglProgram GskNglProgram;
typedef struct _GskNglRenderJob GskNglRenderJob;
typedef struct _GskNglShadowLibrary GskNglShadowLibrary;
typedef struct _GskNglTexture GskNglTexture;
typedef struct _GskNglTextureSlice GskNglTextureSlice;
typedef struct _GskNglTextureAtlas GskNglTextureAtlas;
typedef struct _GskNglTextureLibrary GskNglTextureLibrary;
typedef struct _GskNglTextureNineSlice GskNglTextureNineSlice;
typedef struct _GskNglUniformInfo GskNglUniformInfo;
typedef struct _GskNglUniformProgram GskNglUniformProgram;
typedef struct _GskNglUniformState GskNglUniformState;
typedef struct _GskNglDriver GskNglDriver;

struct _GskNglDrawVertex
{
  float position[2];
  union {
    float uv[2];
    guint16 color2[4];
  };
  guint16 color[4];
};

G_END_DECLS

#endif /* __GSK_NGL_TYPES_PRIVATE_H__ */
