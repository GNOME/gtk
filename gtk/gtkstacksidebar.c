/*
 * Copyright (c) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author:
 *      Ikey Doherty <michael.i.doherty@intel.com>
 */

#include "config.h"

#include "gtkstacksidebar.h"

#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtkscrolledwindow.h"
#include "gtkseparator.h"
#include "gtkstylecontext.h"
#include "gtksingleselection.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkstacksidebar
 * @Title: GtkStackSidebar
 * @Short_description: An automatic sidebar widget
 *
 * A GtkStackSidebar enables you to quickly and easily provide a
 * consistent "sidebar" object for your user interface.
 *
 * In order to use a GtkStackSidebar, you simply use a GtkStack to
 * organize your UI flow, and add the sidebar to your sidebar area. You
 * can use gtk_stack_sidebar_set_stack() to connect the #GtkStackSidebar
 * to the #GtkStack.
 *
 * # CSS nodes
 *
 * GtkStackSidebar has a single CSS node with name stacksidebar and
 * style class .sidebar.
 *
 * When circumstances require it, GtkStackSidebar adds the
 * .needs-attention style class to the widgets representing the stack
 * pages.
 */

struct _GtkStackSidebarPrivate
{
  GtkListBox *list;
  GtkStack *stack;
  GtkSelectionModel *pages;
  GHashTable *rows;
  gboolean in_child_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkStackSidebar, gtk_stack_sidebar, GTK_TYPE_BIN)

enum
{
  PROP_0,
  PROP_STACK,
  N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
gtk_stack_sidebar_set_property (GObject    *object,
                                guint       prop_id,
                                const       GValue *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_STACK:
      gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_sidebar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (GTK_STACK_SIDEBAR (object));

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, priv->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_header (GtkListBoxRow *row,
               GtkListBoxRow *before,
               gpointer       userdata)
{
  GtkWidget *ret = NULL;

  if (before && !gtk_list_box_row_get_header (row))
    {
      ret = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, ret);
    }
}

static gint
sort_list (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       userdata)
{
  gint left = 0; gint right = 0;


  if (row1)
    left = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row1), "child-index"));

  if (row2)
    right = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row2), "child-index"));

  if (left < right)
    return  -1;

  if (left == right)
    return 0;

  return 1;
}

static void
gtk_stack_sidebar_row_selected (GtkListBox    *box,
                                GtkListBoxRow *row,
                                gpointer       userdata)
{
  GtkStackSidebar *sidebar = GTK_STACK_SIDEBAR (userdata);
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  guint index;

  if (priv->in_child_changed)
    return;

  if (!row)
    return;

  index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "child-index"));
  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (priv->pages), index);
}

static void
gtk_stack_sidebar_init (GtkStackSidebar *sidebar)
{
  GtkStyleContext *style;
  GtkStackSidebarPrivate *priv;
  GtkWidget *sw;

  priv = gtk_stack_sidebar_get_instance_private (sidebar);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (sidebar), sw);

  priv->list = GTK_LIST_BOX (gtk_list_box_new ());

  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (priv->list));

  gtk_list_box_set_header_func (priv->list, update_header, sidebar, NULL);
  gtk_list_box_set_sort_func (priv->list, sort_list, sidebar, NULL);

  g_signal_connect (priv->list, "row-selected",
                    G_CALLBACK (gtk_stack_sidebar_row_selected), sidebar);

  style = gtk_widget_get_style_context (GTK_WIDGET (sidebar));
  gtk_style_context_add_class (style, "sidebar");

  priv->rows = g_hash_table_new (NULL, NULL);
}

static void
update_row (GtkStackSidebar *sidebar,
            GtkWidget       *widget,
            GtkWidget       *row)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  gchar *title;
  gboolean needs_attention;
  GtkStyleContext *context;

  g_object_get (gtk_stack_get_page (GTK_STACK (priv->stack), widget),
                "title", &title,
                "needs-attention", &needs_attention,
                NULL);

  item = gtk_bin_get_child (GTK_BIN (row));
  gtk_label_set_text (GTK_LABEL (item), title);

  gtk_widget_set_visible (row, gtk_widget_get_visible (widget) && title != NULL);

  context = gtk_widget_get_style_context (row);
  if (needs_attention)
     gtk_style_context_add_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);

  g_free (title);
}

static void
on_visible_updated (GtkWidget        *widget,
                    GParamSpec       *pspec,
                    GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *row;

  row = g_hash_table_lookup (priv->rows, widget);
  update_row (sidebar, widget, row);
}

static void
on_page_updated (GtkStackPage    *page,
                 GParamSpec      *pspec,
                 GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *widget;
  GtkWidget *row;

  widget = gtk_stack_page_get_child (page);
  row = g_hash_table_lookup (priv->rows, widget);
  update_row (sidebar, widget, row);
}

static void
add_child (GtkWidget       *widget,
           guint            position,
           GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  GtkWidget *row;
  GtkStackPage *page;

  /* Make a pretty item when we add kids */
  item = gtk_label_new ("");
  gtk_widget_set_halign (item, GTK_ALIGN_START);
  gtk_widget_set_valign (item, GTK_ALIGN_CENTER);
  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), item);
  gtk_widget_show (item);

  update_row (sidebar, widget, row);

  /* Hook up for events */
  page = gtk_stack_get_page (GTK_STACK (priv->stack), widget);
  g_signal_connect (widget, "notify::visible", G_CALLBACK (on_visible_updated), sidebar);
  g_signal_connect (page, "notify", G_CALLBACK (on_page_updated), sidebar);

  g_object_set_data (G_OBJECT (row), "child-index", GUINT_TO_POINTER (position));
  g_hash_table_insert (priv->rows, widget, row);
  gtk_container_add (GTK_CONTAINER (priv->list), row);
}

static void
populate_sidebar (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *widget, *row;
  guint i, selected;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (priv->pages)); i++)
    {
      widget = g_list_model_get_item (G_LIST_MODEL (priv->pages), i);
      add_child (widget, i, sidebar);
    }

  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (priv->pages));
  if (selected != GTK_INVALID_LIST_POSITION)
    {
      widget = g_list_model_get_item (G_LIST_MODEL (priv->pages), selected);
      row = g_hash_table_lookup (priv->rows, widget);
      priv->in_child_changed = TRUE;
      gtk_list_box_select_row (priv->list, GTK_LIST_BOX_ROW (row));
      priv->in_child_changed = FALSE;
    }
}

static void
clear_sidebar (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  GHashTableIter iter;
  GtkWidget *widget;
  GtkWidget *row;

  g_hash_table_iter_init (&iter, priv->rows);
  while (g_hash_table_iter_next (&iter, (gpointer *)&widget, (gpointer *)&row))
    {
      gtk_container_remove (GTK_CONTAINER (priv->list), row);
      g_hash_table_iter_remove (&iter);
      if (priv->stack)
        {
          GtkStackPage *page = gtk_stack_get_page (priv->stack, widget);
          if (page)
            g_signal_handlers_disconnect_by_func (page, on_page_updated, sidebar);
          g_signal_handlers_disconnect_by_func (widget, on_visible_updated, sidebar);
        }
    }
}

static void
items_changed_cb (GListModel       *model,
                  guint             position,
                  guint             removed,
                  guint             added,
                  GtkStackSidebar  *sidebar)
{
  /* FIXME: we can do better */
  clear_sidebar (sidebar);
  populate_sidebar (sidebar);
}

static void
selection_changed_cb (GtkSelectionModel *model,
                      GParamSpec        *pspec,
                      GtkStackSidebar   *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);
  guint selected;
  GtkWidget *child;
  GtkWidget *row;

  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));
  if (selected == GTK_INVALID_LIST_POSITION)
    return;

  child = g_list_model_get_item (G_LIST_MODEL (model), selected);
  row = g_hash_table_lookup (priv->rows, child);
  if (row != NULL)
    {
      priv->in_child_changed = TRUE;
      gtk_list_box_select_row (priv->list, GTK_LIST_BOX_ROW (row));
      priv->in_child_changed = FALSE;
    }
}

static void
disconnect_stack_signals (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);

  g_signal_handlers_disconnect_by_func (priv->pages, items_changed_cb, sidebar);
  g_signal_handlers_disconnect_by_func (priv->pages, selection_changed_cb, sidebar);
  g_signal_handlers_disconnect_by_func (priv->stack, disconnect_stack_signals, sidebar);
}

static void
connect_stack_signals (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);

  g_signal_connect (priv->pages, "items-changed", G_CALLBACK (items_changed_cb), sidebar);
  g_signal_connect (priv->pages, "notify::selected", G_CALLBACK (selection_changed_cb), sidebar);
  g_signal_connect_swapped (priv->stack, "destroy", G_CALLBACK (disconnect_stack_signals), sidebar);
}

static void
gtk_stack_sidebar_dispose (GObject *object)
{
  GtkStackSidebar *sidebar = GTK_STACK_SIDEBAR (object);

  gtk_stack_sidebar_set_stack (sidebar, NULL);

  G_OBJECT_CLASS (gtk_stack_sidebar_parent_class)->dispose (object);
}

static void
gtk_stack_sidebar_finalize (GObject *object)
{
  GtkStackSidebar *sidebar = GTK_STACK_SIDEBAR (object);
  GtkStackSidebarPrivate *priv = gtk_stack_sidebar_get_instance_private (sidebar);

  g_hash_table_destroy (priv->rows);

  G_OBJECT_CLASS (gtk_stack_sidebar_parent_class)->finalize (object);
}

static void
gtk_stack_sidebar_class_init (GtkStackSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_stack_sidebar_dispose;
  object_class->finalize = gtk_stack_sidebar_finalize;
  object_class->set_property = gtk_stack_sidebar_set_property;
  object_class->get_property = gtk_stack_sidebar_get_property;

  obj_properties[PROP_STACK] =
      g_param_spec_object (I_("stack"), P_("Stack"),
                           P_("Associated stack for this GtkStackSidebar"),
                           GTK_TYPE_STACK,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_css_name (widget_class, I_("stacksidebar"));
}

/**
 * gtk_stack_sidebar_new:
 *
 * Creates a new sidebar.
 *
 * Returns: the new #GtkStackSidebar
 */
GtkWidget *
gtk_stack_sidebar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_STACK_SIDEBAR, NULL));
}

/**
 * gtk_stack_sidebar_set_stack:
 * @sidebar: a #GtkStackSidebar
 * @stack: a #GtkStack
 *
 * Set the #GtkStack associated with this #GtkStackSidebar.
 *
 * The sidebar widget will automatically update according to the order
 * (packing) and items within the given #GtkStack.
 */
void
gtk_stack_sidebar_set_stack (GtkStackSidebar *sidebar,
                             GtkStack        *stack)
{
  GtkStackSidebarPrivate *priv;

  g_return_if_fail (GTK_IS_STACK_SIDEBAR (sidebar));
  g_return_if_fail (GTK_IS_STACK (stack) || stack == NULL);

  priv = gtk_stack_sidebar_get_instance_private (sidebar);

  if (priv->stack == stack)
    return;

  if (priv->stack)
    {
      disconnect_stack_signals (sidebar);
      clear_sidebar (sidebar);
      g_clear_object (&priv->stack);
      g_clear_object (&priv->pages);
    }
  if (stack)
    {
      priv->stack = g_object_ref (stack);
      priv->pages = gtk_stack_get_pages (stack);
      populate_sidebar (sidebar);
      connect_stack_signals (sidebar);
    }

  gtk_widget_queue_resize (GTK_WIDGET (sidebar));

  g_object_notify (G_OBJECT (sidebar), "stack");
}

/**
 * gtk_stack_sidebar_get_stack:
 * @sidebar: a #GtkStackSidebar
 *
 * Retrieves the stack.
 * See gtk_stack_sidebar_set_stack().
 *
 * Returns: (nullable) (transfer none): the associated #GtkStack or
 *     %NULL if none has been set explicitly
 */
GtkStack *
gtk_stack_sidebar_get_stack (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv;

  g_return_val_if_fail (GTK_IS_STACK_SIDEBAR (sidebar), NULL);

  priv = gtk_stack_sidebar_get_instance_private (sidebar);

  return GTK_STACK (priv->stack);
}
