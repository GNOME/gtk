/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010, Red Hat, Inc
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_DISPLAY_MANAGER_PRIVATE_H__
#define __GDK_DISPLAY_MANAGER_PRIVATE_H__

#include "gdkdisplaymanager.h"

G_BEGIN_DECLS

struct _GdkDisplayManager
{
  GObject parent_instance;
};

struct _GdkDisplayManagerClass
{
  GObjectClass parent_class;

  GSList *     (*list_displays)       (GdkDisplayManager *manager);
  GdkDisplay * (*get_default_display) (GdkDisplayManager *manager);
  void         (*set_default_display) (GdkDisplayManager *manager,
                                       GdkDisplay        *display);
  GdkDisplay * (*open_display)        (GdkDisplayManager *manager,
                                       const gchar       *name);

  /* FIXME the following should really be frontend-only, not vfuncs */
  GdkAtom      (*atom_intern)         (GdkDisplayManager *manager,
                                       const gchar       *atom_name,
                                       gboolean           copy_name);
  gchar *      (*get_atom_name)       (GdkDisplayManager *manager,
                                       GdkAtom            atom);
  guint        (*lookup_keyval)       (GdkDisplayManager *manager,
                                       const gchar       *name);
  gchar *      (*get_keyval_name)     (GdkDisplayManager *manager,
                                       guint              keyval);
  void         (*keyval_convert_case) (GdkDisplayManager *manager,
                                       guint              keyval,
                                       guint             *lower,
                                       guint             *upper);

  /* signals */
  void         (*display_opened)      (GdkDisplayManager *manager,
                                       GdkDisplay        *display);
};

G_END_DECLS

#endif
