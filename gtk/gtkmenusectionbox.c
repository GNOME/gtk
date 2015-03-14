/*
 * Copyright © 2014 Canonical Limited
 * Copyright © 2013 Carlos Garnacho
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmenusectionbox.h"

#include "gtkwidgetprivate.h"
#include "gtklabel.h"
#include "gtkmenutracker.h"
#include "gtkmodelbutton.h"
#include "gtkseparator.h"
#include "gtksizegroup.h"
#include "gtkstack.h"
#include "gtkstylecontext.h"
#include "gtkpopover.h"
#include "gtkorientable.h"

typedef GtkBoxClass GtkMenuSectionBoxClass;

struct _GtkMenuSectionBox
{
  GtkBox             parent_instance;

  GtkMenuSectionBox *toplevel;
  GtkMenuTracker    *tracker;
  GtkBox            *item_box;
  GtkWidget         *separator;
  guint              separator_sync_idle;
  gboolean           iconic;
  gint               depth;
};

typedef struct
{
  gint     n_items;
  gboolean previous_is_iconic;
} MenuData;

G_DEFINE_TYPE (GtkMenuSectionBox, gtk_menu_section_box, GTK_TYPE_BOX)

static void        gtk_menu_section_box_sync_separators (GtkMenuSectionBox  *box,
                                                         MenuData           *data);
static void        gtk_menu_section_box_new_submenu     (GtkMenuTrackerItem *item,
                                                         GtkMenuSectionBox  *toplevel,
                                                         GtkWidget          *focus,
                                                         const gchar        *name);
static GtkWidget * gtk_menu_section_box_new_section     (GtkMenuTrackerItem *item,
                                                         GtkMenuSectionBox  *parent);

static void
gtk_menu_section_box_sync_item (GtkWidget *widget,
                                gpointer   user_data)
{
  MenuData *data = (MenuData *)user_data;

  if (GTK_IS_MENU_SECTION_BOX (widget))
    gtk_menu_section_box_sync_separators (GTK_MENU_SECTION_BOX (widget), data);
  else
    data->n_items++;
}

/* We are trying to implement the following rules here:
 *
 * rule 1: never ever show separators for empty sections
 * rule 2: always show a separator if there is a label
 * rule 3: don't show a separator for the first section
 * rule 4: don't show a separator for the following sections if there are
 *         no items before it
 * rule 5: never show separators directly above or below an iconic box
 * (rule 6: these rules don't apply exactly the same way for subsections)
 */
static void
gtk_menu_section_box_sync_separators (GtkMenuSectionBox *box,
                                      MenuData          *data)
{
  gboolean previous_section_is_iconic;
  gboolean should_have_separator;
  gboolean should_have_top_margin = FALSE;
  gboolean is_not_empty_item;
  gboolean has_separator;
  gboolean has_label;
  gboolean separator_condition;
  gint n_items_before;

  n_items_before =  data->n_items;
  previous_section_is_iconic = data->previous_is_iconic;

  gtk_container_foreach (GTK_CONTAINER (box->item_box), gtk_menu_section_box_sync_item, data);

  is_not_empty_item = (data->n_items > n_items_before);

  if (is_not_empty_item)
    data->previous_is_iconic = box->iconic;

  if (box->separator == NULL)
    return;

  has_separator = gtk_widget_get_parent (box->separator) != NULL;
  has_label = !GTK_IS_SEPARATOR (box->separator);

  separator_condition = has_label ? TRUE : n_items_before > 0 &&
                                           box->depth <= 1 &&
                                           !previous_section_is_iconic &&
                                           !box->iconic;

  should_have_separator = separator_condition && is_not_empty_item;

  should_have_top_margin = !should_have_separator &&
                           (box->depth <= 1 || box->iconic) &&
                           n_items_before > 0 &&
                           is_not_empty_item;

  gtk_widget_set_margin_top (GTK_WIDGET (box->item_box), should_have_top_margin ? 10 : 0);

  if (should_have_separator == has_separator)
    return;

  if (should_have_separator)
    gtk_box_pack_start (GTK_BOX (box), box->separator, FALSE, FALSE, 0);
  else
    gtk_container_remove (GTK_CONTAINER (box), box->separator);
}

static gboolean
gtk_menu_section_box_handle_sync_separators (gpointer user_data)
{
  GtkMenuSectionBox *box = user_data;
  MenuData data;

  data.n_items = 0;
  data.previous_is_iconic = FALSE;
  gtk_menu_section_box_sync_separators (box, &data);

  box->separator_sync_idle = 0;

  return G_SOURCE_REMOVE;
}

static void
gtk_menu_section_box_schedule_separator_sync (GtkMenuSectionBox *box)
{
  box = box->toplevel;

  if (!box->separator_sync_idle)
    box->separator_sync_idle = gdk_threads_add_idle_full (G_PRIORITY_HIGH_IDLE, /* before resize... */
                                                          gtk_menu_section_box_handle_sync_separators,
                                                          box, NULL);
}

static void
gtk_popover_item_activate (GtkWidget *button,
                           gpointer   user_data)
{
  GtkMenuTrackerItem *item = user_data;
  GtkWidget *popover = NULL;

  if (gtk_menu_tracker_item_get_role (item) == GTK_MENU_TRACKER_ITEM_ROLE_NORMAL)
    {
      /* Activating the item could cause the popover
       * to be free'd, for example if it is a Quit item
       */
      popover = g_object_ref (gtk_widget_get_ancestor (button,
                                                       GTK_TYPE_POPOVER));
    }

  gtk_menu_tracker_item_activated (item);

  if (popover != NULL)
    {
      gtk_widget_hide (popover);
      g_object_unref (popover);
    }
}

static void
gtk_menu_section_box_remove_func (gint     position,
                                  gpointer user_data)
{
  GtkMenuSectionBox *box = user_data;
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (box->item_box));
  gtk_widget_destroy (g_list_nth_data (children, position));
  g_list_free (children);

  gtk_menu_section_box_schedule_separator_sync (box);
}

static gboolean
get_ancestors (GtkWidget  *widget,
               GType       widget_type,
               GtkWidget **ancestor,
               GtkWidget **below)
{
  GtkWidget *a, *b;

  a = NULL;
  b = widget;
  while (b != NULL)
    {
      a = gtk_widget_get_parent (b);
      if (!a)
        return FALSE;
      if (g_type_is_a (G_OBJECT_TYPE (a), widget_type))
        break;
      b = a;
    }

  *below = b;
  *ancestor = a;

  return TRUE;
}

static void
close_submenu (GtkWidget *button,
               gpointer   data)
{
  GtkMenuTrackerItem *item = data;
  GtkWidget *focus;

  if (gtk_menu_tracker_item_get_should_request_show (item))
    gtk_menu_tracker_item_request_submenu_shown (item, FALSE);

  focus = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "focus"));
  gtk_widget_grab_focus (focus);
}

static void
open_submenu (GtkWidget *button,
              gpointer   data)
{
  GtkMenuTrackerItem *item = data;
  GtkWidget *focus;

  if (gtk_menu_tracker_item_get_should_request_show (item))
    gtk_menu_tracker_item_request_submenu_shown (item, TRUE);

  focus = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "focus"));
  gtk_widget_grab_focus (focus);
}

static void
gtk_menu_section_box_insert_func (GtkMenuTrackerItem *item,
                                  gint                position,
                                  gpointer            user_data)
{
  GtkMenuSectionBox *box = user_data;
  GtkWidget *widget;

  if (gtk_menu_tracker_item_get_is_separator (item))
    {
      widget = gtk_menu_section_box_new_section (item, box);
    }
  else if (gtk_menu_tracker_item_get_has_link (item, G_MENU_LINK_SUBMENU))
    {
      GtkWidget *stack = NULL;
      GtkWidget *parent = NULL;
      gchar *name;

      widget = g_object_new (GTK_TYPE_MODEL_BUTTON,
                             "menu-name", gtk_menu_tracker_item_get_label (item),
                             NULL);
      g_object_bind_property (item, "label", widget, "text", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "icon", widget, "icon", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "sensitive", widget, "sensitive", G_BINDING_SYNC_CREATE);

      get_ancestors (GTK_WIDGET (box->toplevel), GTK_TYPE_STACK, &stack, &parent);
      gtk_container_child_get (GTK_CONTAINER (stack), parent, "name", &name, NULL);
      gtk_menu_section_box_new_submenu (item, box->toplevel, widget, name);
      g_free (name);
    }
  else
    {
      widget = gtk_model_button_new ();
      g_object_bind_property (item, "label", widget, "text", G_BINDING_SYNC_CREATE);

      if (box->iconic)
        {
          g_object_bind_property (item, "verb-icon", widget, "icon", G_BINDING_SYNC_CREATE);
          g_object_set (widget, "iconic", TRUE, "centered", TRUE, NULL);
        }
      else
        g_object_bind_property (item, "icon", widget, "icon", G_BINDING_SYNC_CREATE);

      g_object_bind_property (item, "sensitive", widget, "sensitive", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "role", widget, "role", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "toggled", widget, "active", G_BINDING_SYNC_CREATE);
      g_signal_connect (widget, "clicked", G_CALLBACK (gtk_popover_item_activate), item);
    }

  gtk_widget_show (widget);

  g_object_set_data_full (G_OBJECT (widget), "GtkMenuTrackerItem", g_object_ref (item), g_object_unref);

  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  if (box->iconic)
    gtk_box_pack_start (GTK_BOX (box->item_box), widget, TRUE, TRUE, 0);
  else
    gtk_container_add (GTK_CONTAINER (box->item_box), widget);
  gtk_box_reorder_child (GTK_BOX (box->item_box), widget, position);

  gtk_menu_section_box_schedule_separator_sync (box);
}

static void
gtk_menu_section_box_init (GtkMenuSectionBox *box)
{
  GtkWidget *item_box;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);

  box->toplevel = box;

  item_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  box->item_box = GTK_BOX (item_box);
  gtk_box_pack_end (GTK_BOX (box), item_box, FALSE, FALSE, 0);
  gtk_widget_set_halign (GTK_WIDGET (item_box), GTK_ALIGN_FILL);
  gtk_widget_show (item_box);

  gtk_widget_set_halign (GTK_WIDGET (box), GTK_ALIGN_FILL);
  g_object_set (box, "margin", 0, NULL);

}

static void
gtk_menu_section_box_dispose (GObject *object)
{
  GtkMenuSectionBox *box = GTK_MENU_SECTION_BOX (object);

  if (box->separator_sync_idle)
    {
      g_source_remove (box->separator_sync_idle);
      box->separator_sync_idle = 0;
    }

  g_clear_object (&box->separator);

  if (box->tracker)
    {
      gtk_menu_tracker_free (box->tracker);
      box->tracker = NULL;
    }

  G_OBJECT_CLASS (gtk_menu_section_box_parent_class)->dispose (object);
}

static void
gtk_menu_section_box_class_init (GtkMenuSectionBoxClass *class)
{
  G_OBJECT_CLASS (class)->dispose = gtk_menu_section_box_dispose;
}

static void
update_popover_position_cb (GObject    *source,
                            GParamSpec *spec,
                            gpointer   *user_data)
{
  GtkPopover *popover = GTK_POPOVER (source);
  GtkMenuSectionBox *box = GTK_MENU_SECTION_BOX (user_data);

  GtkPositionType new_pos = gtk_popover_get_position (popover);

  GList *children = gtk_container_get_children (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (box))));
  GList *l;

  for (l = children;
       l != NULL;
       l = l->next)
    {
      GtkWidget *w = l->data;

      if (new_pos == GTK_POS_BOTTOM)
        gtk_widget_set_valign (w, GTK_ALIGN_START);
      else if (new_pos == GTK_POS_TOP)
        gtk_widget_set_valign (w, GTK_ALIGN_END);
      else
        gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
    }
}

void
gtk_menu_section_box_new_toplevel (GtkStack    *stack,
                                   GMenuModel  *model,
                                   const gchar *action_namespace,
                                   GtkPopover  *popover)
{
  GtkMenuSectionBox *box;

  box = g_object_new (GTK_TYPE_MENU_SECTION_BOX,
                      "margin-top", 12,
                      "margin-bottom", 12,
                      NULL);
  gtk_stack_add_named (stack, GTK_WIDGET (box), "main");

  box->tracker = gtk_menu_tracker_new (GTK_ACTION_OBSERVABLE (_gtk_widget_get_action_muxer (GTK_WIDGET (box), TRUE)),
                                       model, TRUE, FALSE, FALSE, action_namespace,
                                       gtk_menu_section_box_insert_func,
                                       gtk_menu_section_box_remove_func, box);

  g_signal_connect (G_OBJECT (popover), "notify::position", G_CALLBACK (update_popover_position_cb), box);


  gtk_widget_show (GTK_WIDGET (box));
}

static void
gtk_menu_section_box_new_submenu (GtkMenuTrackerItem *item,
                                  GtkMenuSectionBox  *toplevel,
                                  GtkWidget          *focus,
                                  const gchar        *name)
{
  GtkMenuSectionBox *box;
  GtkWidget *button;

  box = g_object_new (GTK_TYPE_MENU_SECTION_BOX,
                      "margin-top", 12,
                      "margin-bottom", 12,
                      NULL);

  button = g_object_new (GTK_TYPE_MODEL_BUTTON,
                         "menu-name", name,
                         "inverted", TRUE,
                         "centered", TRUE,
                         NULL);

  g_object_bind_property (item, "label", button, "text", G_BINDING_SYNC_CREATE);
  g_object_bind_property (item, "icon", button, "icon", G_BINDING_SYNC_CREATE);

  g_object_set_data (G_OBJECT (button), "focus", focus);
  g_object_set_data (G_OBJECT (focus), "focus", button);

  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (focus, "clicked", G_CALLBACK (open_submenu), item);
  g_signal_connect (button, "clicked", G_CALLBACK (close_submenu), item);

  gtk_stack_add_named (GTK_STACK (gtk_widget_get_ancestor (GTK_WIDGET (toplevel), GTK_TYPE_STACK)),
                       GTK_WIDGET (box), gtk_menu_tracker_item_get_label (item));
  gtk_widget_show (GTK_WIDGET (box));

  box->tracker = gtk_menu_tracker_new_for_item_link (item, G_MENU_LINK_SUBMENU, FALSE, FALSE,
                                                     gtk_menu_section_box_insert_func,
                                                     gtk_menu_section_box_remove_func,
                                                     box);
}

static GtkWidget *
gtk_menu_section_box_new_section (GtkMenuTrackerItem *item,
                                  GtkMenuSectionBox  *parent)
{
  GtkMenuSectionBox *box;
  GtkWidget *separator;
  const gchar *label;
  const gchar *hint;

  box = g_object_new (GTK_TYPE_MENU_SECTION_BOX, NULL);
  box->toplevel = parent->toplevel;
  box->depth = parent->depth + 1;

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  label = gtk_menu_tracker_item_get_label (item);
  hint = gtk_menu_tracker_item_get_display_hint (item);

  if (hint && g_str_equal (hint, "horizontal-buttons"))
    {
      gtk_orientable_set_orientation (GTK_ORIENTABLE (box->item_box), GTK_ORIENTATION_HORIZONTAL);
      gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (box->item_box)), GTK_STYLE_CLASS_LINKED);
      box->iconic = TRUE;
      gtk_widget_set_margin_start (GTK_WIDGET (box->item_box), 12);
      gtk_widget_set_margin_end (GTK_WIDGET (box->item_box), 12);
    }

  if (label != NULL)
    {
      GtkWidget *title;

      title = gtk_label_new (label);
      g_object_bind_property (item, "label", title, "label", G_BINDING_SYNC_CREATE);
      gtk_style_context_add_class (gtk_widget_get_style_context (title), GTK_STYLE_CLASS_SEPARATOR);
      gtk_widget_set_halign (title, GTK_ALIGN_START);

      box->separator = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      g_object_ref_sink (box->separator);

      gtk_container_add (GTK_CONTAINER (box->separator), title);
      gtk_container_add (GTK_CONTAINER (box->separator), separator);
      gtk_widget_show_all (box->separator);
    }
  else
    {
      box->separator = separator;
      g_object_ref_sink (box->separator);

      gtk_widget_show (box->separator);
    }
    g_object_set (box->separator,
                  "margin-top", 4,
                  "margin-bottom", 4,
                  NULL);


  box->tracker = gtk_menu_tracker_new_for_item_link (item, G_MENU_LINK_SECTION, FALSE, FALSE,
                                                     gtk_menu_section_box_insert_func,
                                                     gtk_menu_section_box_remove_func,
                                                     box);

  return GTK_WIDGET (box);
}
