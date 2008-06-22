/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <gtk/gtk.h>
#include "gailseparator.h"

static void         gail_separator_class_init            (GailSeparatorClass  *klass);
static void         gail_separator_init                  (GailSeparator       *accessible);
static void         gail_separator_initialize            (AtkObject           *accessible,
                                                          gpointer             data);
static AtkStateSet* gail_separator_ref_state_set	 (AtkObject	      *accessible);

G_DEFINE_TYPE (GailSeparator, gail_separator, GAIL_TYPE_WIDGET)

static void
gail_separator_class_init (GailSeparatorClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_separator_initialize;
  class->ref_state_set = gail_separator_ref_state_set;
}

static void
gail_separator_init (GailSeparator *accessible)
{
}

static void
gail_separator_initialize (AtkObject *accessible,
                           gpointer  data)
{
  ATK_OBJECT_CLASS (gail_separator_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_SEPARATOR;
}

static AtkStateSet*
gail_separator_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gail_separator_parent_class)->ref_state_set (accessible);
  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (widget == NULL)
    return state_set;

  if (GTK_IS_VSEPARATOR (widget))
    atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);
  else if (GTK_IS_HSEPARATOR (widget))
    atk_state_set_add_state (state_set, ATK_STATE_HORIZONTAL);

  return state_set;
}
