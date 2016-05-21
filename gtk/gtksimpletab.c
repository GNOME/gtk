/* gtksimpletab.c
 *
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtksimpletab.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkenums.h"
#include "gtklabel.h"

struct _GtkSimpleTab
{
  GtkTab parent;

  GtkWidget *label;
};

typedef struct _GtkSimpleTabClass GtkSimpleTabClass;

struct _GtkSimpleTabClass
{
  GtkTabClass parent_class;
};

G_DEFINE_TYPE (GtkSimpleTab, gtk_simple_tab, GTK_TYPE_TAB)

static void
gtk_simple_tab_class_init (GtkSimpleTabClass *klass)
{
}

static void
gtk_simple_tab_init (GtkSimpleTab *self)
{
  self->label = gtk_label_new ("");
  gtk_widget_show (self->label);
  gtk_widget_set_halign (self->label, GTK_ALIGN_CENTER);

  gtk_tab_set_child (GTK_TAB (self), self->label);

  g_object_bind_property (self, "title", self->label, "label", G_BINDING_DEFAULT);
}
