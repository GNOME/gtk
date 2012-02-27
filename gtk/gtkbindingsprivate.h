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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_BINDINGS_PRIVATE_H__
#define __GTK_BINDINGS_PRIVATE_H__

#include "gtkbindings.h"

G_BEGIN_DECLS

guint _gtk_binding_parse_binding     (GScanner        *scanner);
void  _gtk_binding_reset_parsed      (void);
void  _gtk_binding_entry_add_signall (GtkBindingSet   *binding_set,
                                      guint            keyval,
                                      GdkModifierType  modifiers,
                                      const gchar     *signal_name,
                                      GSList          *binding_args);

G_END_DECLS

#endif /* __GTK_BINDINGS_PRIVATE_H__ */
