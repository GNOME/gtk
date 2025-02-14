/* gtk-im-context-preview.h
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gtkimcontext.h"

G_BEGIN_DECLS

#define GTK_TYPE_IM_CONTEXT_PREVIEW (gtk_im_context_preview_get_type())

G_DECLARE_FINAL_TYPE (GtkIMContextPreview, gtk_im_context_preview, GTK, IM_CONTEXT_PREVIEW, GtkIMContext)

GtkIMContext *gtk_im_context_preview_new        (GtkIMContext        *im_context);
void          gtk_im_context_preview_set_suffix (GtkIMContextPreview *self,
                                                 const char          *suffix);

G_END_DECLS
