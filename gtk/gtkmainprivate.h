/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GTK_MAIN_PRIVATE_H__
#define __GTK_MAIN_PRIVATE_H__

#include "gtkmain.h"

G_BEGIN_DECLS

gboolean _gtk_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                           GValue                *return_accu,
                                           const GValue          *handler_return,
                                           gpointer               dummy);

gchar *_gtk_get_lc_ctype (void);

gboolean _gtk_module_has_mixed_deps (GModule *module);

G_END_DECLS

#endif  /* __GTK_MAIN_PRIVATE_H__ */
