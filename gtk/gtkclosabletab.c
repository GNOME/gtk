/* gtkclosabletab.c
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

#include "gtkclosabletab.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkenums.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkbutton.h"

struct _GtkClosableTab
{
  GtkTab parent;

  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *button;
};

typedef struct _GtkClosableTabClass GtkClosableTabClass;

struct _GtkClosableTabClass
{
  GtkTabClass parent_class;
};

G_DEFINE_TYPE (GtkClosableTab, gtk_closable_tab, GTK_TYPE_TAB)

static void
close_tab (GtkClosableTab *tab)
{
  GtkWidget *widget;
  GtkWidget *stack;

  widget = gtk_tab_get_widget (GTK_TAB (tab));
  stack = gtk_widget_get_parent (widget);
  gtk_container_remove (GTK_CONTAINER (stack), widget);
}

static void
gtk_closable_tab_init (GtkClosableTab *self)
{
  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_show (self->box);
  gtk_tab_set_child (GTK_TAB (self), self->box);

  self->label = gtk_label_new ("");
  gtk_widget_show (self->label);
  gtk_box_set_center_widget (GTK_BOX (self->box), self->label);
  g_object_bind_property (self, "title", self->label, "label", G_BINDING_DEFAULT);

  self->button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (self->button), GTK_RELIEF_NONE);
  gtk_box_pack_end (GTK_BOX (self->box), self->button, FALSE, FALSE, 0);

  g_signal_connect_swapped (self->button, "clicked", G_CALLBACK (close_tab), self);
}

static void
gtk_closable_tab_state_flags_changed (GtkWidget     *widget,
                                      GtkStateFlags  old_state)
{
  GtkClosableTab *tab = GTK_CLOSABLE_TAB (widget);
  gboolean checked;

  checked = (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_CHECKED) != 0;

  gtk_widget_set_visible (tab->button, checked);
}

static void
gtk_closable_tab_class_init (GtkClosableTabClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->state_flags_changed = gtk_closable_tab_state_flags_changed;
}

