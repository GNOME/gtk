/* gtkcombobox.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtkcombobox.h"
#include "gtkcelllayout.h"
#include "gtkcellview.h"
#include "gtkcellviewmenuitem.h"

#include "gtktreeselection.h"
#include "gtkframe.h"
#include "gtktogglebutton.h"
#include "gtkvseparator.h"
#include "gtkarrow.h"
#include "gtkmenu.h"
#include "gtkmain.h"
#include "gtkeventbox.h"
#include "gtkcellrenderertext.h"
#include "gtkbindings.h"
#include "gtkliststore.h"
#include "gtkwindow.h"

#include <gdk/gdkkeysyms.h>

#include <gobject/gvaluecollector.h>

#include <string.h>
#include <stdarg.h>

#include "gtkmarshalers.h"
#include "gtkintl.h"


/* WELCOME, to THE house of evil code */


typedef struct _ComboCellInfo ComboCellInfo;
struct _ComboCellInfo
{
  GtkCellRenderer *cell;
  GSList *attributes;

  GtkCellLayoutDataFunc func;
  gpointer func_data;
  GDestroyNotify destroy;

  guint expand : 1;
  guint pack : 1;
};

#define GTK_COMBO_BOX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_COMBO_BOX, GtkComboBoxPrivate))

struct _GtkComboBoxPrivate
{
  GtkTreeModel *model;

  gint col_column;
  gint row_column;

  gint wrap_width;

  gint active_item;

  GtkWidget *tree_view;
  GtkTreeViewColumn *column;

  GtkWidget *cell_view;

  GtkWidget *hbox;
  GtkWidget *cell_view_frame;

  GtkWidget *button;
  GtkWidget *arrow;
  GtkWidget *separator;

  GtkWidget *popup_widget;
  GtkWidget *popup_window;
  GtkWidget *popup_frame;

  guint inserted_id;
  guint deleted_id;

  gint width;
  GSList *cells;

  guint changed_id;

  guint popup_in_progress : 1;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_WRAP_WIDTH,
  PROP_ROW_SPAN_COLUMN,
  PROP_COLUMN_SPAN_COLUMN,
  PROP_ACTIVE
};

static GtkBinClass *parent_class = NULL;
static guint combo_box_signals[LAST_SIGNAL] = {0,};

#define BONUS_PADDING 4


/* common */
static void     gtk_combo_box_class_init           (GtkComboBoxClass *klass);
static void     gtk_combo_box_cell_layout_init     (GtkCellLayoutIface *iface);
static void     gtk_combo_box_init                 (GtkComboBox      *combo_box);

static void     gtk_combo_box_set_property         (GObject         *object,
                                                    guint            prop_id,
                                                    const GValue    *value,
                                                    GParamSpec      *spec);
static void     gtk_combo_box_get_property         (GObject         *object,
                                                    guint            prop_id,
                                                    GValue          *value,
                                                    GParamSpec      *spec);

static void     gtk_combo_box_style_set            (GtkWidget       *widget,
                                                    GtkStyle        *previous_style,
                                                    gpointer         data);
static void     gtk_combo_box_button_toggled       (GtkWidget       *widget,
                                                    gpointer         data);
static void     gtk_combo_box_add                  (GtkContainer    *container,
                                                    GtkWidget       *widget);

static ComboCellInfo *gtk_combo_box_get_cell_info  (GtkComboBox      *combo_box,
                                                    GtkCellRenderer  *cell);

static void     gtk_combo_box_menu_show            (GtkWidget        *menu,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_hide            (GtkWidget        *menu,
                                                    gpointer          user_data);

static void     gtk_combo_box_set_popup_widget     (GtkComboBox      *combo_box,
                                                    GtkWidget        *popup);
static void     gtk_combo_box_menu_position        (GtkMenu          *menu,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *push_in,
                                                    gpointer          user_data);
static void     gtk_combo_box_popup                (GtkComboBox      *combo_box);
static void     gtk_combo_box_popdown              (GtkComboBox      *combo_box);

static gint     gtk_combo_box_calc_requested_width (GtkComboBox      *combo_box,
                                                    GtkTreePath      *path);
static void     gtk_combo_box_remeasure            (GtkComboBox      *combo_box);

static void     gtk_combo_box_unset_model          (GtkComboBox      *combo_box);
static void     gtk_combo_box_set_model_internal   (GtkComboBox      *combo_box);

static void     gtk_combo_box_size_request         (GtkWidget        *widget,
                                                    GtkRequisition   *requisition);
static void     gtk_combo_box_size_allocate        (GtkWidget        *widget,
                                                    GtkAllocation    *allocation);
static void     gtk_combo_box_forall               (GtkContainer     *container,
                                                    gboolean          include_internals,
                                                    GtkCallback       callback,
                                                    gpointer          callback_data);
static gboolean gtk_combo_box_expose_event         (GtkWidget        *widget,
                                                    GdkEventExpose   *event);
static gboolean gtk_combo_box_scroll_event         (GtkWidget        *widget,
                                                    GdkEventScroll   *event);

/* list */
static void     gtk_combo_box_list_setup           (GtkComboBox      *combo_box);
static void     gtk_combo_box_list_destroy         (GtkComboBox      *combo_box);

static gboolean gtk_combo_box_list_button_released (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_key_press       (GtkWidget        *widget,
                                                    GdkEventKey      *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_button_pressed  (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);

static void     gtk_combo_box_list_row_changed     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);

/* menu */
static void     gtk_combo_box_menu_setup           (GtkComboBox      *combo_box,
                                                    gboolean          add_childs);
static void     gtk_combo_box_menu_fill            (GtkComboBox      *combo_box);
static void     gtk_combo_box_menu_destroy         (GtkComboBox      *combo_box);

static void     gtk_combo_box_item_get_size        (GtkComboBox      *combo_box,
                                                    gint              index,
                                                    gint             *cols,
                                                    gint             *rows);
static void     gtk_combo_box_relayout_item        (GtkComboBox      *combo_box,
                                                    gint              index);
static void     gtk_combo_box_relayout             (GtkComboBox      *combo_box);

static gboolean gtk_combo_box_menu_button_press    (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_item_activate   (GtkWidget        *item,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_row_inserted    (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_row_deleted     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_row_changed     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);

/* cell layout */
static void     gtk_combo_box_cell_layout_pack_start         (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              gboolean               expand);
static void     gtk_combo_box_cell_layout_pack_end           (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              gboolean               expand);
static void     gtk_combo_box_cell_layout_clear              (GtkCellLayout         *layout);
static void     gtk_combo_box_cell_layout_add_attribute      (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              const gchar           *attribute,
                                                              gint                   column);
static void     gtk_combo_box_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              GtkCellLayoutDataFunc  func,
                                                              gpointer               func_data,
                                                              GDestroyNotify         destroy);
static void     gtk_combo_box_cell_layout_clear_attributes   (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell);
static void     gtk_combo_box_cell_layout_reorder            (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              gint                   position);
static gboolean gtk_combo_box_mnemonic_activate              (GtkWidget    *widget,
							      gboolean      group_cycling);


GType
gtk_combo_box_get_type (void)
{
  static GType combo_box_type = 0;

  if (!combo_box_type)
    {
      static const GTypeInfo combo_box_info =
        {
          sizeof (GtkComboBoxClass),
          NULL, /* base_init */
          NULL, /* base_finalize */
          (GClassInitFunc) gtk_combo_box_class_init,
          NULL, /* class_finalize */
          NULL, /* class_data */
          sizeof (GtkComboBox),
          0,
          (GInstanceInitFunc) gtk_combo_box_init
        };

      static const GInterfaceInfo cell_layout_info =
        {
          (GInterfaceInitFunc) gtk_combo_box_cell_layout_init,
          NULL,
          NULL
        };

      combo_box_type = g_type_register_static (GTK_TYPE_BIN,
                                               "GtkComboBox",
                                               &combo_box_info,
                                               0);

      g_type_add_interface_static (combo_box_type,
                                   GTK_TYPE_CELL_LAYOUT,
                                   &cell_layout_info);
    }

  return combo_box_type;
}

/* common */
static void
gtk_combo_box_class_init (GtkComboBoxClass *klass)
{
  GObjectClass *object_class;
  GtkBindingSet *binding_set;
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;

  binding_set = gtk_binding_set_by_class (klass);

  container_class = (GtkContainerClass *)klass;
  container_class->forall = gtk_combo_box_forall;
  container_class->add = gtk_combo_box_add;

  widget_class = (GtkWidgetClass *)klass;
  widget_class->size_allocate = gtk_combo_box_size_allocate;
  widget_class->size_request = gtk_combo_box_size_request;
  widget_class->expose_event = gtk_combo_box_expose_event;
  widget_class->scroll_event = gtk_combo_box_scroll_event;
  widget_class->mnemonic_activate = gtk_combo_box_mnemonic_activate;

  object_class = (GObjectClass *)klass;
  object_class->set_property = gtk_combo_box_set_property;
  object_class->get_property = gtk_combo_box_get_property;

  parent_class = g_type_class_peek_parent (klass);

  /* signals */
  combo_box_signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkComboBoxClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /* properties */
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("ComboBox model"),
                                                        P_("The model for the combo box"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_WRAP_WIDTH,
                                   g_param_spec_int ("wrap_width",
                                                     P_("Wrap width"),
                                                     P_("Wrap width for layouting the items in a grid"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ROW_SPAN_COLUMN,
                                   g_param_spec_int ("row_span_column",
                                                     P_("Row span column"),
                                                     P_("TreeModel column containing the row span values"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_COLUMN_SPAN_COLUMN,
                                   g_param_spec_int ("column_span_column",
                                                     P_("Column span column"),
                                                     P_("TreeModel column containing the column span values"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_int ("active",
                                                     P_("Active item"),
                                                     P_("The item which is currently active"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("appearance",
                                                                 P_("ComboBox appareance"),
                                                                 P_("ComboBox appearance, where TRUE means Windows-style."),
                                                                 FALSE,
                                                                 G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkComboBoxPrivate));
}

static void
gtk_combo_box_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = gtk_combo_box_cell_layout_pack_start;
  iface->pack_end = gtk_combo_box_cell_layout_pack_end;
  iface->clear = gtk_combo_box_cell_layout_clear;
  iface->add_attribute = gtk_combo_box_cell_layout_add_attribute;
  iface->set_cell_data_func = gtk_combo_box_cell_layout_set_cell_data_func;
  iface->clear_attributes = gtk_combo_box_cell_layout_clear_attributes;
  iface->reorder = gtk_combo_box_cell_layout_reorder;
}

static void
gtk_combo_box_init (GtkComboBox *combo_box)
{
  combo_box->priv = GTK_COMBO_BOX_GET_PRIVATE (combo_box);

  g_signal_connect (combo_box, "style_set",
                    G_CALLBACK (gtk_combo_box_style_set), NULL);

  combo_box->priv->cell_view = gtk_cell_view_new ();
  gtk_container_add (GTK_CONTAINER (combo_box), combo_box->priv->cell_view);
  gtk_widget_show (combo_box->priv->cell_view);

  combo_box->priv->width = 0;
  combo_box->priv->wrap_width = 0;

  combo_box->priv->active_item = -1;
  combo_box->priv->col_column = -1;
  combo_box->priv->row_column = -1;
}

static void
gtk_combo_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  switch (prop_id)
    {
      case PROP_MODEL:
        gtk_combo_box_set_model (combo_box, g_value_get_object (value));
        break;

      case PROP_WRAP_WIDTH:
        gtk_combo_box_set_wrap_width (combo_box, g_value_get_int (value));
        break;

      case PROP_ROW_SPAN_COLUMN:
        gtk_combo_box_set_row_span_column (combo_box, g_value_get_int (value));
        break;

      case PROP_COLUMN_SPAN_COLUMN:
        gtk_combo_box_set_column_span_column (combo_box, g_value_get_int (value));
        break;

      case PROP_ACTIVE:
        gtk_combo_box_set_active (combo_box, g_value_get_int (value));
        break;

      default:
        break;
    }
}

static void
gtk_combo_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value, combo_box->priv->model);
        break;

      case PROP_WRAP_WIDTH:
        g_value_set_int (value, combo_box->priv->wrap_width);
        break;

      case PROP_ROW_SPAN_COLUMN:
        g_value_set_int (value, combo_box->priv->row_column);
        break;

      case PROP_COLUMN_SPAN_COLUMN:
        g_value_set_int (value, combo_box->priv->col_column);
        break;

      case PROP_ACTIVE:
        g_value_set_int (value, gtk_combo_box_get_active (combo_box));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_combo_box_style_set (GtkWidget *widget,
                         GtkStyle  *previous_style,
                         gpointer   data)
{
  gboolean appearance;
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  gtk_widget_queue_resize (widget);

  /* if wrap_width > 0, then we are in grid-mode and forced to use
   * unix style
   */
  if (combo_box->priv->wrap_width)
    return;

  gtk_widget_style_get (widget,
                        "appearance", &appearance,
                        NULL);

  /* TRUE is windows style */
  if (appearance)
    {
      if (GTK_IS_MENU (combo_box->priv->popup_widget))
        gtk_combo_box_menu_destroy (combo_box);
      gtk_combo_box_list_setup (combo_box);
    }
  else
    {
      if (GTK_IS_TREE_VIEW (combo_box->priv->tree_view))
        gtk_combo_box_list_destroy (combo_box);
      gtk_combo_box_menu_setup (combo_box, TRUE);
    }
}

static void
gtk_combo_box_button_toggled (GtkWidget *widget,
                              gpointer   data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      if (!combo_box->priv->popup_in_progress)
        gtk_combo_box_popup (combo_box);
    }
  else
    gtk_combo_box_popdown (combo_box);
}

static void
gtk_combo_box_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);

  if (combo_box->priv->cell_view && combo_box->priv->cell_view->parent)
    {
      gtk_container_remove (container, combo_box->priv->cell_view);
      (* GTK_CONTAINER_CLASS (parent_class)->add) (container, widget);
    }
  else
    {
      (* GTK_CONTAINER_CLASS (parent_class)->add) (container, widget);
    }

  if (combo_box->priv->cell_view &&
      widget != combo_box->priv->cell_view)
    {
      /* since the cell_view was unparented, it's gone now */
      combo_box->priv->cell_view = NULL;

      if (!combo_box->priv->tree_view && combo_box->priv->separator)
        {
          gtk_widget_unparent (combo_box->priv->separator);

          g_object_ref (G_OBJECT (combo_box->priv->arrow));
          gtk_widget_unparent (combo_box->priv->arrow);
          gtk_container_add (GTK_CONTAINER (combo_box->priv->button),
                             combo_box->priv->arrow);
          g_object_unref (G_OBJECT (combo_box->priv->arrow));

          gtk_widget_queue_resize (GTK_WIDGET (container));
        }
      else if (combo_box->priv->cell_view_frame)
        {
          gtk_widget_unparent (combo_box->priv->cell_view_frame);
          combo_box->priv->cell_view_frame = NULL;
        }
    }
}

static ComboCellInfo *
gtk_combo_box_get_cell_info (GtkComboBox     *combo_box,
                             GtkCellRenderer *cell)
{
  GSList *i;

  for (i = combo_box->priv->cells; i; i = i->next)
    {
      ComboCellInfo *info = (ComboCellInfo *)i->data;

      if (info->cell == cell)
        return info;
    }

  return NULL;
}

static void
gtk_combo_box_menu_show (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                TRUE);
  combo_box->priv->popup_in_progress = FALSE;
}

static void
gtk_combo_box_menu_hide (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                FALSE);
}

static void
gtk_combo_box_set_popup_widget (GtkComboBox *combo_box,
                                GtkWidget   *popup)
{
  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    combo_box->priv->popup_widget = NULL;
  else if (combo_box->priv->popup_widget)
    {
      gtk_container_remove (GTK_CONTAINER (combo_box->priv->popup_frame),
                            combo_box->priv->popup_widget);
      g_object_unref (G_OBJECT (combo_box->priv->popup_widget));
      combo_box->priv->popup_widget = NULL;
    }

  if (GTK_IS_MENU (popup))
    {
      if (combo_box->priv->popup_window)
        {
          gtk_widget_destroy (combo_box->priv->popup_window);
          combo_box->priv->popup_window = combo_box->priv->popup_frame = NULL;
        }

      combo_box->priv->popup_widget = popup;

      g_signal_connect (popup, "show",
                        G_CALLBACK (gtk_combo_box_menu_show), combo_box);
      g_signal_connect (popup, "hide",
                        G_CALLBACK (gtk_combo_box_menu_hide), combo_box);

      /* FIXME: need to attach to widget? */
    }
  else
    {
      if (!combo_box->priv->popup_window)
        {
          combo_box->priv->popup_window = gtk_window_new (GTK_WINDOW_POPUP);

          combo_box->priv->popup_frame = gtk_frame_new (NULL);
          gtk_frame_set_shadow_type (GTK_FRAME (combo_box->priv->popup_frame),
                                     GTK_SHADOW_NONE);
          gtk_container_add (GTK_CONTAINER (combo_box->priv->popup_window),
                             combo_box->priv->popup_frame);
          gtk_widget_show (combo_box->priv->popup_frame);
        }

      gtk_container_add (GTK_CONTAINER (combo_box->priv->popup_frame),
                         popup);
      gtk_widget_show (popup);
      g_object_ref (G_OBJECT (popup));
      combo_box->priv->popup_widget = popup;
    }
}

static void
gtk_combo_box_menu_position (GtkMenu  *menu,
                             gint     *x,
                             gint     *y,
                             gint     *push_in,
                             gpointer  user_data)
{
  gint sx, sy;
  GtkWidget *child;
  GtkRequisition req;
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  /* FIXME: is using the size request here broken? */
  child = GTK_BIN (combo_box)->child;

  gdk_window_get_origin (child->window, &sx, &sy);

  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  if (gtk_widget_get_direction (GTK_WIDGET (combo_box)) == GTK_TEXT_DIR_RTL)
    *x = sx;
  else
    *x = sx + child->allocation.width - req.width;
  *y = sy + child->allocation.height;

  if (GTK_WIDGET_NO_WINDOW (child))
    {
      *x += child->allocation.x;
      *y += child->allocation.y;
    }

  *push_in = TRUE;
}

static void
gtk_combo_box_popup (GtkComboBox *combo_box)
{
  gint x, y, width, height;
  GtkWidget *sample;

  if (GTK_WIDGET_MAPPED (combo_box->priv->popup_widget))
    return;

  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      if (combo_box->priv->active_item != -1)
        {
          GList *childs;

          childs = gtk_container_get_children (GTK_CONTAINER (combo_box->priv->popup_widget));
          gtk_menu_shell_select_item (GTK_MENU_SHELL (combo_box->priv->popup_widget),
                                      g_list_nth_data (childs, combo_box->priv->active_item));
          g_list_free (childs);
        }

      gtk_menu_popup (GTK_MENU (combo_box->priv->popup_widget),
                      NULL, NULL,
                      gtk_combo_box_menu_position, combo_box,
                      0, 0);
      return;
    }

  /* size it */
  sample = GTK_BIN (combo_box)->child;

  width = sample->allocation.width;
  height = sample->allocation.height;

  gdk_window_get_origin (sample->window,
                         &x, &y);

  if (combo_box->priv->cell_view_frame)
    {
       x -= GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness;
       width += 2 * (GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness);
    }

  gtk_widget_set_size_request (combo_box->priv->popup_window,
                               width, -1);

  if (GTK_WIDGET_NO_WINDOW (sample))
    {
      x += sample->allocation.x;
      y += sample->allocation.y;
    }

  gtk_window_move (GTK_WINDOW (combo_box->priv->popup_window),
                   x, y + height);

  /* popup */
  gtk_widget_show_all (combo_box->priv->popup_window);

  gtk_widget_grab_focus (combo_box->priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                TRUE);

  if (!GTK_WIDGET_HAS_FOCUS (combo_box->priv->tree_view))
    {
      gdk_keyboard_grab (combo_box->priv->popup_window->window,
                         FALSE, GDK_CURRENT_TIME);
      gtk_widget_grab_focus (combo_box->priv->tree_view);
    }
}

static void
gtk_combo_box_popdown (GtkComboBox *combo_box)
{
  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      gtk_menu_popdown (GTK_MENU (combo_box->priv->popup_widget));
      return;
    }

  gtk_widget_hide_all (combo_box->priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                FALSE);
}

static gint
gtk_combo_box_calc_requested_width (GtkComboBox *combo_box,
                                    GtkTreePath *path)
{
  gint padding;
  GtkRequisition req;

  if (combo_box->priv->cell_view)
    gtk_widget_style_get (combo_box->priv->cell_view,
                          "focus-line-width", &padding,
                          NULL);
  else
    padding = 0;

  /* add some pixels for good measure */
  padding += BONUS_PADDING;

  if (combo_box->priv->cell_view)
    gtk_cell_view_get_size_of_row (GTK_CELL_VIEW (combo_box->priv->cell_view),
                                   path, &req);
  else
    req.width = 0;

  return req.width + padding;
}

static void
gtk_combo_box_remeasure (GtkComboBox *combo_box)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  gint padding = 0;

  if (!gtk_tree_model_get_iter_first (combo_box->priv->model, &iter))
    return;

  combo_box->priv->width = 0;

  path = gtk_tree_path_new_from_indices (0, -1);

  if (combo_box->priv->cell_view)
    gtk_widget_style_get (combo_box->priv->cell_view,
                          "focus-line-width", &padding,
                          NULL);
  else
    padding = 0;

  /* add some pixels for good measure */
  padding += BONUS_PADDING;

  do
    {
      GtkRequisition req;

      if (combo_box->priv->cell_view)
        gtk_cell_view_get_size_of_row (GTK_CELL_VIEW (combo_box->priv->cell_view),
                                       path, &req);
      else
        req.width = 0;

      combo_box->priv->width = MAX (combo_box->priv->width,
                                    req.width + padding);

      gtk_tree_path_next (path);
    }
  while (gtk_tree_model_iter_next (combo_box->priv->model, &iter));

  gtk_tree_path_free (path);
}

static void
gtk_combo_box_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  gint width, height;
  GtkRequisition bin_req;

  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  /* common */
  gtk_widget_size_request (GTK_BIN (widget)->child, &bin_req);
  gtk_combo_box_remeasure (combo_box);
  bin_req.width = MAX (bin_req.width, combo_box->priv->width);

  if (!combo_box->priv->tree_view)
    {
      /* menu mode */

      if (combo_box->priv->cell_view)
        {
          GtkRequisition sep_req, arrow_req;
          gint border_width, xthickness, ythickness;

          border_width = GTK_CONTAINER (combo_box->priv->button)->border_width;
          xthickness = combo_box->priv->button->style->xthickness;
          ythickness = combo_box->priv->button->style->ythickness;

          bin_req.width = MAX (bin_req.width, combo_box->priv->width);

          gtk_widget_size_request (combo_box->priv->separator, &sep_req);
          gtk_widget_size_request (combo_box->priv->arrow, &arrow_req);

          height = MAX (sep_req.height, arrow_req.height);
          height = MAX (height, bin_req.height);

          width = bin_req.width + sep_req.width + arrow_req.width;

          height += border_width + 1 + xthickness * 2 + 4;
          width += border_width + 1 + ythickness * 2 + 4;

          requisition->width = width;
          requisition->height = height;
        }
      else
        {
          GtkRequisition but_req;

          gtk_widget_size_request (combo_box->priv->button, &but_req);

          requisition->width = bin_req.width + but_req.width;
          requisition->height = MAX (bin_req.height, but_req.height);
        }
    }
  else
    {
      /* list mode */
      GtkRequisition button_req;

      /* sample + frame */
      *requisition = bin_req;

      if (combo_box->priv->cell_view_frame)
        {
          requisition->width += 2 *
            (GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
             GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness);
          requisition->height += 2 *
            (GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
             GTK_WIDGET (combo_box->priv->cell_view_frame)->style->ythickness);
        }

      /* the button */
      gtk_widget_size_request (combo_box->priv->button, &button_req);

      requisition->height = MAX (requisition->height, button_req.height);
      requisition->width += button_req.width;
    }
}

static void
gtk_combo_box_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkAllocation child;
  GtkRequisition req;
  gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  widget->allocation = *allocation;

  if (!combo_box->priv->tree_view)
    {
      if (combo_box->priv->cell_view)
        {
          gint border_width, xthickness, ythickness;
          gint width;

          /* menu mode */
          gtk_widget_size_allocate (combo_box->priv->button, allocation);

          /* set some things ready */
          border_width = GTK_CONTAINER (combo_box->priv->button)->border_width;
          xthickness = combo_box->priv->button->style->xthickness;
          ythickness = combo_box->priv->button->style->ythickness;

          child.x = allocation->x + border_width + 1 + xthickness + 2;
          child.y = allocation->y + border_width + 1 + ythickness + 2;

          width = allocation->width - (border_width + 1 + xthickness * 2 + 4);

          /* handle the childs */
          gtk_widget_size_request (combo_box->priv->arrow, &req);
          child.width = req.width;
          child.height = allocation->height - 2 * (child.y - allocation->y);
          if (!is_rtl)
            child.x += width - req.width;
          gtk_widget_size_allocate (combo_box->priv->arrow, &child);
          if (is_rtl)
            child.x += req.width;
          gtk_widget_size_request (combo_box->priv->separator, &req);
          child.width = req.width;
          if (!is_rtl)
            child.x -= req.width;
          gtk_widget_size_allocate (combo_box->priv->separator, &child);

          if (is_rtl)
            {
              child.x += req.width;
              child.width = allocation->x + allocation->width 
                - (border_width + 1 + xthickness + 2) - child.x;
            }
          else 
            {
              child.width = child.x;
              child.x = allocation->x + border_width + 1 + xthickness + 2;
              child.width -= child.x;
            }

          gtk_widget_size_allocate (GTK_BIN (widget)->child, &child);
        }
      else
        {
          gtk_widget_size_request (combo_box->priv->button, &req);
          if (is_rtl)
            child.x = allocation->x;
          else
            child.x = allocation->x + allocation->width - req.width;
          child.y = allocation->y;
          child.width = req.width;
          child.height = allocation->height;
          gtk_widget_size_allocate (combo_box->priv->button, &child);

          if (is_rtl)
            child.x = allocation->x + req.width;
          else
            child.x = allocation->x;
          child.y = allocation->y;
          child.width = allocation->width - req.width;
          gtk_widget_size_allocate (GTK_BIN (widget)->child, &child);
        }
    }
  else
    {
      /* list mode */

      /* button */
      gtk_widget_size_request (combo_box->priv->button, &req);
      if (is_rtl)
        child.x = allocation->x;
      else
        child.x += allocation->x + allocation->width - req.width;
      child.y = allocation->y;
      child.width = req.width;
      child.height = allocation->height;
      gtk_widget_size_allocate (combo_box->priv->button, &child);

      /* frame */
      if (is_rtl)
        child.x = allocation->x + req.width;
      else
        child.x = allocation->x;
      child.y = allocation->y;
      child.width = allocation->width - req.width;
      child.height = allocation->height;

      if (combo_box->priv->cell_view_frame)
        {
          gtk_widget_size_allocate (combo_box->priv->cell_view_frame, &child);

          /* the sample */
          child.x +=
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness;
          child.y +=
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->ythickness;
          child.width -= 2 * (
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness);
          child.height -= 2 * (
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->ythickness);
        }

      gtk_widget_size_allocate (GTK_BIN (combo_box)->child, &child);
    }
}

static void
gtk_combo_box_unset_model (GtkComboBox *combo_box)
{
  if (!combo_box->priv->tree_view)
    {
      /* menu mode */
      g_signal_handler_disconnect (combo_box->priv->model,
                                   combo_box->priv->inserted_id);
      g_signal_handler_disconnect (combo_box->priv->model,
                                   combo_box->priv->deleted_id);
      g_signal_handler_disconnect (combo_box->priv->model,
                                   combo_box->priv->changed_id);

      combo_box->priv->inserted_id =
      combo_box->priv->deleted_id =
      combo_box->priv->changed_id = -1;

      if (combo_box->priv->popup_widget)
        gtk_container_foreach (GTK_CONTAINER (combo_box->priv->popup_widget),
                               (GtkCallback)gtk_widget_destroy, NULL);
    }
  else
    {
      /* list mode */
      g_signal_handler_disconnect (combo_box->priv->model,
                                   combo_box->priv->changed_id);
      combo_box->priv->changed_id = -1;
    }
}

static void
gtk_combo_box_set_model_internal (GtkComboBox *combo_box)
{
  if (!combo_box->priv->tree_view)
    {
      /* menu mode */
      combo_box->priv->inserted_id =
        g_signal_connect (combo_box->priv->model, "row_inserted",
                          G_CALLBACK (gtk_combo_box_menu_row_inserted),
                          combo_box);
      combo_box->priv->deleted_id =
        g_signal_connect (combo_box->priv->model, "row_deleted",
                          G_CALLBACK (gtk_combo_box_menu_row_deleted),
                          combo_box);
      combo_box->priv->changed_id =
        g_signal_connect (combo_box->priv->model, "row_changed",
                          G_CALLBACK (gtk_combo_box_menu_row_changed),
                          combo_box);
    }
  else
    {
      /* list mode */
      gtk_tree_view_set_model (GTK_TREE_VIEW (combo_box->priv->tree_view),
                               combo_box->priv->model);

      combo_box->priv->changed_id =
        g_signal_connect (combo_box->priv->model, "row_changed",
                          G_CALLBACK (gtk_combo_box_list_row_changed),
                          combo_box);
    }
}

static void
gtk_combo_box_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);

  if (include_internals)
    {
      if (!combo_box->priv->tree_view)
        {
          if (combo_box->priv->cell_view && combo_box->priv->button)
            {
              (* callback) (combo_box->priv->button, callback_data);
              (* callback) (combo_box->priv->separator, callback_data);
              (* callback) (combo_box->priv->arrow, callback_data);
            }
          else if (combo_box->priv->arrow)
            {
              (* callback) (combo_box->priv->button, callback_data);
              (* callback) (combo_box->priv->arrow, callback_data);
            }
        }
      else
        {
          (* callback) (combo_box->priv->button, callback_data);
          if (combo_box->priv->cell_view_frame)
            (* callback) (combo_box->priv->cell_view_frame, callback_data);
        }
    }

  if (GTK_BIN (container)->child)
    (* callback) (GTK_BIN (container)->child, callback_data);
}

static gboolean
gtk_combo_box_expose_event (GtkWidget      *widget,
                            GdkEventExpose *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (!combo_box->priv->tree_view)
    {
      gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                      combo_box->priv->button, event);

      if (combo_box->priv->separator)
        {
          gtk_container_propagate_expose (GTK_CONTAINER (combo_box->priv->button),
                                          combo_box->priv->separator, event);

          /* if not in this case, arrow gets its expose event from button */
          gtk_container_propagate_expose (GTK_CONTAINER (combo_box->priv->button),
                                          combo_box->priv->arrow, event);
        }
    }
  else
    {
      gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                      combo_box->priv->button, event);

      if (combo_box->priv->cell_view_frame)
        gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                        combo_box->priv->cell_view_frame, event);
    }

  gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                  GTK_BIN (widget)->child, event);

  return FALSE;
}

static gboolean
gtk_combo_box_scroll_event (GtkWidget          *widget,
                            GdkEventScroll     *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  gint index;
  gint items;
    
  index = gtk_combo_box_get_active (combo_box);

  if (index != -1)
    {
      items = gtk_tree_model_iter_n_children (combo_box->priv->model, NULL);
      
      if (event->direction == GDK_SCROLL_UP)
        index--;
      else 
        index++;

      gtk_combo_box_set_active (combo_box, CLAMP (index, 0, items - 1));
    }

  return TRUE;
}

/*
 * menu style
 */

static void
cell_view_sync_cells (GtkComboBox *combo_box,
                      GtkCellView *cell_view)
{
  GSList *k;

  for (k = combo_box->priv->cells; k; k = k->next)
    {
      GSList *j;
      ComboCellInfo *info = (ComboCellInfo *)k->data;

      if (info->pack == GTK_PACK_START)
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cell_view),
                                    info->cell, info->expand);
      else if (info->pack == GTK_PACK_END)
        gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (cell_view),
                                  info->cell, info->expand);

      gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (cell_view),
                                          info->cell,
                                          info->func, info->func_data, NULL);

      for (j = info->attributes; j; j = j->next->next)
        {
          gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (cell_view),
                                         info->cell,
                                         j->data,
                                         GPOINTER_TO_INT (j->next->data));
        }
    }
}

static void
gtk_combo_box_menu_setup (GtkComboBox *combo_box,
                          gboolean     add_childs)
{
  GtkWidget *box;

  if (combo_box->priv->cell_view)
    {
      combo_box->priv->button = gtk_toggle_button_new ();
      g_signal_connect (combo_box->priv->button, "toggled",
                        G_CALLBACK (gtk_combo_box_button_toggled), combo_box);
      gtk_widget_set_parent (combo_box->priv->button,
                             GTK_BIN (combo_box)->child->parent);

      combo_box->priv->separator = gtk_vseparator_new ();
      gtk_widget_set_parent (combo_box->priv->separator,
                             combo_box->priv->button);
      gtk_widget_show (combo_box->priv->separator);

      combo_box->priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      gtk_widget_set_parent (combo_box->priv->arrow, combo_box->priv->button);
      gtk_widget_show (combo_box->priv->arrow);

      gtk_widget_show_all (combo_box->priv->button);

      if (GTK_WIDGET_MAPPED (GTK_BIN (combo_box)->child))
        {
          /* I have no clue why, but we need to manually map in this case. */
          gtk_widget_map (combo_box->priv->separator);
          gtk_widget_map (combo_box->priv->arrow);
        }
    }
  else
    {
      combo_box->priv->button = gtk_toggle_button_new ();
      g_signal_connect (combo_box->priv->button, "toggled",
                        G_CALLBACK (gtk_combo_box_button_toggled), combo_box);
      gtk_widget_set_parent (combo_box->priv->button,
                             GTK_BIN (combo_box)->child->parent);

      combo_box->priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      gtk_container_add (GTK_CONTAINER (combo_box->priv->button),
                         combo_box->priv->arrow);
      gtk_widget_show_all (combo_box->priv->button);
    }

  g_signal_connect (combo_box->priv->button, "button_press_event",
                    G_CALLBACK (gtk_combo_box_menu_button_press),
                    combo_box);

  /* create our funky menu */
  box = gtk_menu_new ();
  gtk_combo_box_set_popup_widget (combo_box, box);

  /* add items */
  if (add_childs)
    gtk_combo_box_menu_fill (combo_box);
}

static void
gtk_combo_box_menu_fill (GtkComboBox *combo_box)
{
  gint i, items;
  GtkWidget *menu;
  GtkWidget *tmp;

  if (!combo_box->priv->model)
    return;

  items = gtk_tree_model_iter_n_children (combo_box->priv->model, NULL);
  menu = combo_box->priv->popup_widget;

  for (i = 0; i < items; i++)
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_from_indices (i, -1);
      tmp = gtk_cell_view_menu_item_new_from_model (combo_box->priv->model,
                                                    path);
      g_signal_connect (tmp, "activate",
                        G_CALLBACK (gtk_combo_box_menu_item_activate),
                        combo_box);

      cell_view_sync_cells (combo_box,
                            GTK_CELL_VIEW (GTK_BIN (tmp)->child));

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), tmp);
      gtk_widget_show (tmp);

      gtk_tree_path_free (path);
    }
}

static void
gtk_combo_box_menu_destroy (GtkComboBox *combo_box)
{
  /* disconnect signal handlers */
  gtk_combo_box_unset_model (combo_box);

  g_signal_handlers_disconnect_matched (combo_box->priv->button,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_menu_button_press, NULL);

  /* unparent will remove our latest ref */
  if (combo_box->priv->cell_view)
    {
      gtk_widget_unparent (combo_box->priv->arrow);
      gtk_widget_unparent (combo_box->priv->separator);
      gtk_widget_unparent (combo_box->priv->button);
    }
  else
    {
      /* will destroy the arrow too */
      gtk_widget_unparent (combo_box->priv->button);
    }

  /* changing the popup window will unref the menu and the childs */
}

/*
 * grid
 */

static void
gtk_combo_box_item_get_size (GtkComboBox *combo_box,
                             gint         index,
                             gint        *cols,
                             gint        *rows)
{
  GtkTreeIter iter;

  gtk_tree_model_iter_nth_child (combo_box->priv->model, &iter, NULL, index);

  if (cols)
    {
      if (combo_box->priv->col_column == -1)
        *cols = 1;
      else
        gtk_tree_model_get (combo_box->priv->model, &iter,
                            combo_box->priv->col_column, cols,
                            -1);
    }

  if (rows)
    {
      if (combo_box->priv->row_column == -1)
        *rows = 1;
      else
        gtk_tree_model_get (combo_box->priv->model, &iter,
                            combo_box->priv->row_column, rows,
                            -1);
    }
}

static gboolean
menu_occupied (GtkMenu *menu,
               guint    left_attach,
               guint    right_attach,
               guint    top_attach,
               guint    bottom_attach)
{
  GList *i;

  g_return_val_if_fail (GTK_IS_MENU (menu), TRUE);
  g_return_val_if_fail (left_attach < right_attach, TRUE);
  g_return_val_if_fail (top_attach < bottom_attach, TRUE);

  for (i = GTK_MENU_SHELL (menu)->children; i; i = i->next)
    {
      guint l, r, b, t;
      gboolean h_intersect = FALSE;
      gboolean v_intersect = FALSE;

      gtk_container_child_get (GTK_CONTAINER (menu), i->data,
                               "left_attach", &l,
                               "right_attach", &r,
                               "bottom_attach", &b,
                               "top_attach", &t,
                               NULL);

      /* look if this item intersects with the given coordinates */
      h_intersect  = left_attach <= l && l <= right_attach;
      h_intersect &= left_attach <= r && r <= right_attach;

      v_intersect  = top_attach <= t && t <= bottom_attach;
      v_intersect &= top_attach <= b && b <= bottom_attach;

      if (h_intersect && v_intersect)
        return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_relayout_item (GtkComboBox *combo_box,
                             gint         index)
{
  gint current_col = 0, current_row = 0;
  gint rows, cols;
  GList *list;
  GtkWidget *item;
  GtkWidget *menu;

  menu = combo_box->priv->popup_widget;
  if (!GTK_IS_MENU_SHELL (menu))
    return;

  list = gtk_container_get_children (GTK_CONTAINER (menu));
  item = g_list_nth_data (list, index);
  g_list_free (list);

  gtk_combo_box_item_get_size (combo_box, index, &cols, &rows);

  /* look for a good spot */
  while (1)
    {
      if (current_col + cols > combo_box->priv->wrap_width)
        {
          current_col = 0;
          current_row++;
        }

      if (!menu_occupied (GTK_MENU (menu),
                          current_col, current_col + cols,
                          current_row, current_row + rows))
        break;

      current_col++;
    }

  /* set attach props */
  gtk_menu_attach (GTK_MENU (menu), item,
                   current_col, current_col + cols,
                   current_row, current_row + rows);
}

static void
gtk_combo_box_relayout (GtkComboBox *combo_box)
{
  gint i, items;
  GList *list, *j;
  GtkWidget *menu;

  /* ensure we are in menu style */
  if (combo_box->priv->tree_view)
    gtk_combo_box_list_destroy (combo_box);

  menu = combo_box->priv->popup_widget;

  if (!GTK_IS_MENU_SHELL (menu))
    {
      gtk_combo_box_menu_setup (combo_box, FALSE);
      menu = combo_box->priv->popup_widget;
    }

  /* get rid of all children */
  g_return_if_fail (GTK_IS_MENU_SHELL (menu));

  list = gtk_container_get_children (GTK_CONTAINER (menu));

  for (j = g_list_last (list); j; j = j->prev)
    gtk_container_remove (GTK_CONTAINER (menu), j->data);

  g_list_free (list);

  /* and relayout */
  items = gtk_tree_model_iter_n_children (combo_box->priv->model, NULL);

  for (i = 0; i < items; i++)
    {
      GtkWidget *tmp;
      GtkTreePath *path;

      path = gtk_tree_path_new_from_indices (i, -1);
      tmp = gtk_cell_view_menu_item_new_from_model (combo_box->priv->model,
                                                    path);

      g_signal_connect (tmp, "activate",
                        G_CALLBACK (gtk_combo_box_menu_item_activate),
                        combo_box);

      cell_view_sync_cells (combo_box, GTK_CELL_VIEW (GTK_BIN (tmp)->child));

      gtk_menu_shell_insert (GTK_MENU_SHELL (menu), tmp, i);

      if (combo_box->priv->wrap_width)
        gtk_combo_box_relayout_item (combo_box, i);

      gtk_widget_show (tmp);

      gtk_tree_path_free (path);
    }
}

/* callbacks */
static gboolean
gtk_combo_box_menu_button_press (GtkWidget      *widget,
                                 GdkEventButton *event,
                                 gpointer        user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (! GTK_IS_MENU (combo_box->priv->popup_widget))
    return FALSE;

  if (event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
      combo_box->priv->popup_in_progress = TRUE;
      gtk_menu_popup (GTK_MENU (combo_box->priv->popup_widget),
                      NULL, NULL,
                      gtk_combo_box_menu_position, combo_box,
                      event->button, event->time);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_menu_item_activate (GtkWidget *item,
                                  gpointer   user_data)
{
  gint index;
  GtkWidget *menu;
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  menu = combo_box->priv->popup_widget;
  g_return_if_fail (GTK_IS_MENU (menu));

  index = g_list_index (GTK_MENU_SHELL (menu)->children, item);

  gtk_combo_box_set_active (combo_box, index);
}

static void
gtk_combo_box_menu_row_inserted (GtkTreeModel *model,
                                 GtkTreePath  *path,
                                 GtkTreeIter  *iter,
                                 gpointer      user_data)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (!combo_box->priv->popup_widget)
    return;

  menu = combo_box->priv->popup_widget;
  g_return_if_fail (GTK_IS_MENU (menu));

  item = gtk_cell_view_menu_item_new_from_model (model, path);
  g_signal_connect (item, "activate",
                    G_CALLBACK (gtk_combo_box_menu_item_activate),
                    combo_box);

  cell_view_sync_cells (combo_box, GTK_CELL_VIEW (GTK_BIN (item)->child));

  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item,
                         gtk_tree_path_get_indices (path)[0]);
  gtk_widget_show (item);
}

static void
gtk_combo_box_menu_row_deleted (GtkTreeModel *model,
                                GtkTreePath  *path,
                                gpointer      user_data)
{
  gint index, items;
  GtkWidget *menu;
  GtkWidget *item;
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (!combo_box->priv->popup_widget)
    return;

  index = gtk_tree_path_get_indices (path)[0];
  items = gtk_tree_model_iter_n_children (model, NULL);

  if (gtk_combo_box_get_active (combo_box) == index)
    gtk_combo_box_set_active (combo_box, index + 1 % items);

  menu = combo_box->priv->popup_widget;
  g_return_if_fail (GTK_IS_MENU (menu));

  item = g_list_nth_data (GTK_MENU_SHELL (menu)->children, index);
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  gtk_container_remove (GTK_CONTAINER (menu), item);
}

static void
gtk_combo_box_menu_row_changed (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  gint width;

  if (!combo_box->priv->popup_widget)
    return;

  if (combo_box->priv->wrap_width)
    gtk_combo_box_relayout_item (combo_box,
                                 gtk_tree_path_get_indices (path)[0]);

  width = gtk_combo_box_calc_requested_width (combo_box, path);

  if (width > combo_box->priv->width)
    {
      gtk_widget_set_size_request (combo_box->priv->cell_view, width, -1);
      gtk_widget_queue_resize (combo_box->priv->cell_view);
      combo_box->priv->width = width;
    }
}

/*
 * list style
 */

static void
gtk_combo_box_list_setup (GtkComboBox *combo_box)
{
  GSList *i;
  GtkTreeSelection *sel;

  combo_box->priv->button = gtk_toggle_button_new ();
  gtk_widget_set_parent (combo_box->priv->button,
                         GTK_BIN (combo_box)->child->parent);
  g_signal_connect (combo_box->priv->button, "button_press_event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed), combo_box);
  g_signal_connect (combo_box->priv->button, "toggled",
                    G_CALLBACK (gtk_combo_box_button_toggled), combo_box);

  combo_box->priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (combo_box->priv->button),
                     combo_box->priv->arrow);
  gtk_widget_show_all (combo_box->priv->button);

  if (combo_box->priv->cell_view)
    {
      combo_box->priv->cell_view_frame = gtk_frame_new (NULL);
      gtk_widget_set_parent (combo_box->priv->cell_view_frame,
                             GTK_BIN (combo_box)->child->parent);
      gtk_frame_set_shadow_type (GTK_FRAME (combo_box->priv->cell_view_frame),
                                 GTK_SHADOW_IN);

      g_object_set (G_OBJECT (combo_box->priv->cell_view),
                    "background", "white",
                    "background_set", TRUE,
                    NULL);

      gtk_widget_show (combo_box->priv->cell_view_frame);
    }

  combo_box->priv->tree_view = gtk_tree_view_new ();
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (combo_box->priv->tree_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (combo_box->priv->tree_view),
                                     FALSE);

  g_signal_connect (combo_box->priv->tree_view, "button_press_event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed),
                    combo_box);
  g_signal_connect (combo_box->priv->tree_view, "button_release_event",
                    G_CALLBACK (gtk_combo_box_list_button_released),
                    combo_box);
  g_signal_connect (combo_box->priv->tree_view, "key_press_event",
                    G_CALLBACK (gtk_combo_box_list_key_press),
                    combo_box);

  combo_box->priv->column = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (combo_box->priv->tree_view),
                               combo_box->priv->column);

  /* set the models */
  gtk_combo_box_set_model_internal (combo_box);

  /* sync up */
  for (i = combo_box->priv->cells; i; i = i->next)
    {
      GSList *j;
      ComboCellInfo *info = (ComboCellInfo *)i->data;

      if (info->pack == GTK_PACK_START)
        gtk_tree_view_column_pack_start (combo_box->priv->column,
                                         info->cell, info->expand);
      else if (info->pack == GTK_PACK_END)
        gtk_tree_view_column_pack_end (combo_box->priv->column,
                                       info->cell, info->expand);

      for (j = info->attributes; j; j = j->next->next)
        {
          gtk_tree_view_column_add_attribute (combo_box->priv->column,
                                              info->cell,
                                              j->data,
                                              GPOINTER_TO_INT (j->next->data));
        }
    }

  if (combo_box->priv->active_item != -1)
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_from_indices (combo_box->priv->active_item, -1);
      if (path)
        {
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (combo_box->priv->tree_view),
                                    path, NULL, FALSE);
          gtk_tree_path_free (path);
        }
    }

  /* set sample/popup widgets */
  gtk_combo_box_set_popup_widget (GTK_COMBO_BOX (combo_box),
                                  combo_box->priv->tree_view);

  gtk_widget_show (combo_box->priv->tree_view);
}

static void
gtk_combo_box_list_destroy (GtkComboBox *combo_box)
{
  /* disconnect signals */
  gtk_combo_box_unset_model (combo_box);

  g_signal_handlers_disconnect_matched (combo_box->priv->tree_view,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL, NULL, combo_box);
  g_signal_handlers_disconnect_matched (combo_box->priv->button,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_list_button_pressed,
                                        NULL);

  /* destroy things (unparent will kill the latest ref from us)
   * last unref on button will destroy the arrow
   */
  gtk_widget_unparent (combo_box->priv->button);

  if (combo_box->priv->cell_view)
    {
      g_object_set (G_OBJECT (combo_box->priv->cell_view),
                    "background_set", FALSE,
                    NULL);

      gtk_widget_unparent (combo_box->priv->cell_view_frame);
    }

  gtk_widget_destroy (combo_box->priv->tree_view);
  combo_box->priv->tree_view = NULL;
  combo_box->priv->popup_widget = NULL;
}

/* callbacks */
static void
gtk_combo_box_list_remove_grabs (GtkComboBox *combo_box)
{
  if (GTK_WIDGET_HAS_GRAB (combo_box->priv->tree_view))
    gtk_grab_remove (combo_box->priv->tree_view);

  if (GTK_WIDGET_HAS_GRAB (combo_box->priv->popup_window))
    {
      gtk_grab_remove (combo_box->priv->popup_window);
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
    }
}

static gboolean
gtk_combo_box_list_button_pressed (GtkWidget      *widget,
                                   GdkEventButton *event,
                                   gpointer        data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (ewidget == combo_box->priv->tree_view)
    return TRUE;

  if ((ewidget != combo_box->priv->button) ||
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (combo_box->priv->button)))
    return FALSE;

  gtk_combo_box_popup (combo_box);

  gtk_grab_add (combo_box->priv->popup_window);
  gdk_pointer_grab (combo_box->priv->popup_window->window, TRUE,
                    GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK |
                    GDK_POINTER_MOTION_MASK,
                    NULL, NULL, GDK_CURRENT_TIME);

  gtk_grab_add (combo_box->priv->tree_view);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                TRUE);

  combo_box->priv->popup_in_progress = TRUE;

  return TRUE;
}

static gboolean
gtk_combo_box_list_button_released (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    gpointer        data)
{
  gboolean ret;
  GtkTreePath *path = NULL;

  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  gboolean popup_in_progress = FALSE;

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (combo_box->priv->popup_in_progress)
    {
      popup_in_progress = TRUE;
      combo_box->priv->popup_in_progress = FALSE;
    }

  if (ewidget != combo_box->priv->tree_view)
    {
      if (ewidget == combo_box->priv->button &&
          !popup_in_progress &&
          gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (combo_box->priv->button)))
        {
          gtk_combo_box_list_remove_grabs (combo_box);
          gtk_combo_box_popdown (combo_box);
          return TRUE;
        }

      /* released outside treeview */
      if (ewidget != combo_box->priv->button)
        {
          gtk_combo_box_list_remove_grabs (combo_box);
          gtk_combo_box_popdown (combo_box);

          return TRUE;
        }

      return FALSE;
    }

  /* drop grabs */
  gtk_combo_box_list_remove_grabs (combo_box);

  /* select something cool */
  ret = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                       event->x, event->y,
                                       &path,
                                       NULL, NULL, NULL);

  if (!ret)
    return TRUE; /* clicked outside window? */

  gtk_combo_box_set_active (combo_box, gtk_tree_path_get_indices (path)[0]);
  gtk_combo_box_popdown (combo_box);

  gtk_tree_path_free (path);

  return TRUE;
}

static gboolean
gtk_combo_box_list_key_press (GtkWidget   *widget,
                              GdkEventKey *event,
                              gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if ((event->keyval == GDK_Return || event->keyval == GDK_KP_Enter ||
       event->keyval == GDK_space || event->keyval == GDK_KP_Space) ||
      event->keyval == GDK_Escape)
    {
      if (event->keyval != GDK_Escape)
        {
          gboolean ret;
          GtkTreeIter iter;
          GtkTreeModel *model = NULL;
          GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (combo_box->priv->tree_view));

          ret = gtk_tree_selection_get_selected (sel, &model, &iter);
          if (ret)
            {
              GtkTreePath *path;

              path = gtk_tree_model_get_path (model, &iter);
              if (path)
                {
                  gtk_combo_box_set_active (combo_box, gtk_tree_path_get_indices (path)[0]);
                  gtk_tree_path_free (path);
                }
            }
        }
      else
        /* reset active item -- this is incredibly lame and ugly */
        gtk_combo_box_set_active (combo_box,
                                  gtk_combo_box_get_active (combo_box));

      gtk_combo_box_list_remove_grabs (combo_box);
      gtk_combo_box_popdown (combo_box);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_list_row_changed (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  gint width;

  width = gtk_combo_box_calc_requested_width (combo_box, path);

  if (width > combo_box->priv->width)
    {
      gtk_widget_set_size_request (combo_box->priv->cell_view, width, -1);
      gtk_widget_queue_resize (combo_box->priv->cell_view);
      combo_box->priv->width = width;
    }
}

/*
 * GtkCellLayout implementation
 */
static void
gtk_combo_box_cell_layout_pack_start (GtkCellLayout   *layout,
                                      GtkCellRenderer *cell,
                                      gboolean         expand)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box = GTK_COMBO_BOX (layout);
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  info = g_new0 (ComboCellInfo, 1);
  info->cell = cell;
  info->expand = expand;
  info->pack = GTK_PACK_START;

  combo_box->priv->cells = g_slist_append (combo_box->priv->cells, info);

  if (combo_box->priv->cell_view)
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                                cell, expand);

  if (combo_box->priv->column)
    gtk_tree_view_column_pack_start (combo_box->priv->column, cell, expand);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), cell, expand);
        }
      g_list_free (list);
    }
}

static void
gtk_combo_box_cell_layout_pack_end (GtkCellLayout   *layout,
                                    GtkCellRenderer *cell,
                                    gboolean         expand)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box = GTK_COMBO_BOX (layout);
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  info = g_new0 (ComboCellInfo, 1);
  info->cell = cell;
  info->expand = expand;
  info->pack = GTK_PACK_END;

  if (combo_box->priv->cell_view)
    gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                              cell, expand);

  if (combo_box->priv->column)
    gtk_tree_view_column_pack_end (combo_box->priv->column, cell, expand);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (view), cell, expand);
        }
      g_list_free (list);
    }
}

static void
gtk_combo_box_cell_layout_clear (GtkCellLayout *layout)
{
  GtkWidget *menu;
  GtkComboBox *combo_box = GTK_COMBO_BOX (layout);

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (combo_box->priv->cell_view)
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box->priv->cell_view));

  if (combo_box->priv->column)
    gtk_tree_view_column_clear (combo_box->priv->column);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_clear (GTK_CELL_LAYOUT (view));
        }
      g_list_free (list);
    }
}

static void
gtk_combo_box_cell_layout_add_attribute (GtkCellLayout   *layout,
                                         GtkCellRenderer *cell,
                                         const gchar     *attribute,
                                         gint             column)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box = GTK_COMBO_BOX (layout);
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  info = gtk_combo_box_get_cell_info (combo_box, cell);

  info->attributes = g_slist_prepend (info->attributes,
                                      GINT_TO_POINTER (column));
  info->attributes = g_slist_prepend (info->attributes,
                                      g_strdup (attribute));

  if (combo_box->priv->cell_view)
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                                   cell, attribute, column);

  if (combo_box->priv->column)
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_box->priv->column),
                                   cell, attribute, column);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), cell,
                                         attribute, column);
        }
      g_list_free (list);
    }

  gtk_widget_queue_resize (GTK_WIDGET (combo_box));
}

static void
gtk_combo_box_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                              GtkCellRenderer       *cell,
                                              GtkCellLayoutDataFunc  func,
                                              gpointer               func_data,
                                              GDestroyNotify         destroy)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box = GTK_COMBO_BOX (layout);
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  info = gtk_combo_box_get_cell_info (combo_box, cell);
  g_return_if_fail (info != NULL);

  if (info->destroy)
    {
      GDestroyNotify d = info->destroy;

      info->destroy = NULL;
      d (info->func_data);
    }

  info->func = func;
  info->func_data = func_data;
  info->destroy = destroy;

  if (combo_box->priv->cell_view)
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo_box->priv->cell_view), cell, func, func_data, NULL);

  if (combo_box->priv->column)
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo_box->priv->column), cell, func, func_data, NULL);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (view), cell,
                                              func, func_data, NULL);
        }
      g_list_free (list);
    }

  gtk_widget_queue_resize (GTK_WIDGET (combo_box));
}

static void
gtk_combo_box_cell_layout_clear_attributes (GtkCellLayout   *layout,
                                            GtkCellRenderer *cell)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box = GTK_COMBO_BOX (layout);
  GtkWidget *menu;
  GSList *list;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  info = gtk_combo_box_get_cell_info (combo_box, cell);
  g_return_if_fail (info != NULL);

  list = info->attributes;
  while (list && list->next)
    {
      g_free (list->data);
      list = list->next->next;
    }
  g_slist_free (list);

  info->attributes = NULL;

  if (combo_box->priv->cell_view)
    gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (combo_box->priv->cell_view), cell);

  if (combo_box->priv->column)
    gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (combo_box->priv->column), cell);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (view), cell);
        }
      g_list_free (list);
    }

  gtk_widget_queue_resize (GTK_WIDGET (combo_box));
}

static void
gtk_combo_box_cell_layout_reorder (GtkCellLayout   *layout,
                                   GtkCellRenderer *cell,
                                   gint             position)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box = GTK_COMBO_BOX (layout);
  GtkWidget *menu;
  GSList *link;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  info = gtk_combo_box_get_cell_info (combo_box, cell);

  g_return_if_fail (info != NULL);
  g_return_if_fail (position >= 0);

  link = g_slist_find (combo_box->priv->cells, info);

  g_return_if_fail (link != NULL);

  combo_box->priv->cells = g_slist_remove_link (combo_box->priv->cells, link);
  combo_box->priv->cells = g_slist_insert (combo_box->priv->cells, info,
                                           position);

  if (combo_box->priv->cell_view)
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                             cell, position);

  if (combo_box->priv->column)
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo_box->priv->column),
                             cell, position);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_reorder (GTK_CELL_LAYOUT (view), cell, position);
        }
      g_list_free (list);
    }

  gtk_widget_queue_draw (GTK_WIDGET (combo_box));
}

/*
 * public API
 */

/**
 * gtk_combo_box_new:
 *
 * Creates a new empty #GtkComboBox.
 *
 * Return value: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new (void)
{
  return GTK_WIDGET (g_object_new (gtk_combo_box_get_type (), NULL));
}

/**
 * gtk_combo_box_new_with_model:
 * @model: A #GtkTreeModel.
 *
 * Creates a new #GtkComboBox with the model initialized to @model.
 *
 * Return value: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new_with_model (GtkTreeModel *model)
{
  GtkComboBox *combo_box;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

  combo_box = GTK_COMBO_BOX (g_object_new (gtk_combo_box_get_type (),
                                           "model", model,
                                           NULL));

  return GTK_WIDGET (combo_box);
}

/**
 * gtk_combo_box_set_wrap_width:
 * @combo_box: A #GtkComboBox.
 * @width: Preferred number of columns.
 *
 * Sets the wrap width of @combo_box to be @width. The wrap width is basically
 * the preferred number of columns when you want to the popup to be layed out
 * in a table.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_wrap_width (GtkComboBox *combo_box,
                              gint         width)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (width > 0);

  combo_box->priv->wrap_width = width;

  gtk_combo_box_relayout (combo_box);
}

/**
 * gtk_combo_box_set_row_span_column:
 * @combo_box: A #GtkComboBox.
 * @row_span: A column in the model passed during construction.
 *
 * Sets the column with row span information for @combo_box to be @row_span.
 * The row span column contains integers which indicate how many rows
 * an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_row_span_column (GtkComboBox *combo_box,
                                   gint         row_span)
{
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  col = gtk_tree_model_get_n_columns (combo_box->priv->model);
  g_return_if_fail (row_span >= 0 && row_span < col);

  combo_box->priv->row_column = row_span;

  gtk_combo_box_relayout (combo_box);
}

/**
 * gtk_combo_box_set_column_span_column:
 * @combo_box: A #GtkComboBox.
 * @column_span: A column in the model passed during construction.
 *
 * Sets the column with column span information for @combo_box to be
 * @column_span. The column span column contains integers which indicate
 * how many columns an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_column_span_column (GtkComboBox *combo_box,
                                      gint         column_span)
{
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  col = gtk_tree_model_get_n_columns (combo_box->priv->model);
  g_return_if_fail (column_span >= 0 && column_span < col);

  combo_box->priv->col_column = column_span;

  gtk_combo_box_relayout (combo_box);
}

/**
 * gtk_combo_box_get_active:
 * @combo_box: A #GtkComboBox.
 *
 * Returns the index of the currently active item, or -1 if there's no
 * active item.
 *
 * Return value: An integer which is the index of the currently active item, or
 * -1 if there's no active item.
 *
 * Since: 2.4
 */
gint
gtk_combo_box_get_active (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return combo_box->priv->active_item;
}

/**
 * gtk_combo_box_set_active:
 * @combo_box: A #GtkComboBox.
 * @index: An index in the model passed during construction, or -1 to have
 * no active item.
 *
 * Sets the active item of @combo_box to be the item at @index.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_active (GtkComboBox *combo_box,
                          gint         index)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  /* -1 means "no item selected" */
  g_return_if_fail (index >= -1);

  if (combo_box->priv->active_item == index)
    return;

  combo_box->priv->active_item = index;

  if (index == -1)
    {
      if (combo_box->priv->tree_view)
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (combo_box->priv->tree_view)));
      else
        {
          GtkMenu *menu = GTK_MENU (combo_box->priv->popup_widget);

          if (GTK_IS_MENU (menu))
            gtk_menu_set_active (menu, -1);
        }

      if (combo_box->priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (combo_box->priv->cell_view), NULL);
    }
  else
    {
      path = gtk_tree_path_new_from_indices (index, -1);

      if (combo_box->priv->tree_view)
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (combo_box->priv->tree_view), path, NULL, FALSE);
      else
        {
          GtkMenu *menu = GTK_MENU (combo_box->priv->popup_widget);

          if (GTK_IS_MENU (menu))
            gtk_menu_set_active (GTK_MENU (menu), index);
        }

      if (combo_box->priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (combo_box->priv->cell_view), path);

      gtk_tree_path_free (path);
    }

  g_signal_emit_by_name (combo_box, "changed", NULL, NULL);
}


/**
 * gtk_combo_box_get_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: The uninitialized #GtkTreeIter.
 * 
 * Set @iter to point to the current active item, if it exists.
 * 
 * Return value: %TRUE, if @iter was set
 *
 * Since: 2.4
 **/
gboolean
gtk_combo_box_get_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkTreePath *path;
  gint active;
  gboolean retval;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  active = gtk_combo_box_get_active (combo_box);
  if (active < 0)
    return FALSE;

  path = gtk_tree_path_new_from_indices (active, -1);
  retval = gtk_tree_model_get_iter (gtk_combo_box_get_model (combo_box),
                                    iter, path);
  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_combo_box_set_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: The #GtkTreeIter.
 * 
 * Sets the current active item to be the one referenced by @iter. 
 * @iter must correspond to a path of depth one.
 * 
 * Since: 2.4
 **/
void
gtk_combo_box_set_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  path = gtk_tree_model_get_path (gtk_combo_box_get_model (combo_box), iter);
  g_return_if_fail (path != NULL);
  g_return_if_fail (gtk_tree_path_get_depth (path) == 1);
  
  gtk_combo_box_set_active (combo_box, gtk_tree_path_get_indices (path)[0]);
  gtk_tree_path_free (path);
}

/**
 * gtk_combo_box_set_model:
 * @combo_box: A #GtkComboBox.
 * @model: A #GtkTreeModel.
 *
 * Sets the model used by @combo_box to be @model. Will unset a
 * previously set model (if applicable).
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_model (GtkComboBox  *combo_box,
                         GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  if (combo_box->priv->model)
    {
      gtk_combo_box_unset_model (combo_box);
      g_object_unref (G_OBJECT (combo_box->priv->model));
    }

  combo_box->priv->model = model;
  g_object_ref (G_OBJECT (combo_box->priv->model));

  gtk_combo_box_set_model_internal (combo_box);
  if (!combo_box->priv->tree_view && combo_box->priv->popup_widget)
    gtk_combo_box_menu_fill (combo_box);

  if (combo_box->priv->cell_view)
    gtk_cell_view_set_model (GTK_CELL_VIEW (combo_box->priv->cell_view),
                             combo_box->priv->model);
}

/**
 * gtk_combo_box_get_model
 * @combo_box: A #GtkComboBox.
 *
 * Returns the #GtkTreeModel which is acting as data source for @combo_box.
 *
 * Return value: A #GtkTreeModel which was passed during construction.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_combo_box_get_model (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->model;
}


/* convenience API for simple text combos */

/**
 * gtk_combo_box_new_text:
 *
 * Convenience function which constructs a new text combo box, which is a
 * #GtkComboBox just displaying strings. If you use this function to create
 * a text combo box, you only want to manipulate it's data source with the
 * following convenience functions: gtk_combo_box_append_text(),
 * gtk_combo_box_insert_text() and gtk_combo_box_prepend_text().
 *
 * Return value: A new text combo box.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new_text (void)
{
  GtkWidget *combo_box;
  GtkCellRenderer *cell;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_STRING);

  combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
                                  "text", 0,
                                  NULL);

  return combo_box;
}

/**
 * gtk_combo_box_append_text:
 * @combo_box: A #GtkComboBox constructed using gtk_combo_box_new_text().
 * @text: A string.
 *
 * Appends @string to the list of strings stored in @combo_box. Note that
 * you can only use this function with combo boxes constructed with
 * gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_append_text (GtkComboBox *combo_box,
                           const gchar *text)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (combo_box->priv->model);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, text, -1);
}

/**
 * gtk_combo_box_insert_text:
 * @combo_box: A #GtkComboBox constructed using gtk_combo_box_new_text().
 * @position: An index to insert @text.
 * @text: A string.
 *
 * Inserts @string at @position in the list of strings stored in @combo_box.
 * Note that you can only use this function with combo boxes constructed
 * with gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_insert_text (GtkComboBox *combo_box,
                           gint         position,
                           const gchar *text)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (position >= 0);
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (combo_box->priv->model);

  gtk_list_store_insert (store, &iter, position);
  gtk_list_store_set (store, &iter, 0, text, -1);
}

/**
 * gtk_combo_box_prepend_text:
 * @combo_box: A #GtkComboBox constructed with gtk_combo_box_new_text().
 * @text: A string.
 *
 * Prepends @string to the list of strings stored in @combo_box. Note that
 * you can only use this function with combo boxes constructed with
 * gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_prepend_text (GtkComboBox *combo_box,
                            const gchar *text)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (combo_box->priv->model);

  gtk_list_store_prepend (store, &iter);
  gtk_list_store_set (store, &iter, 0, text, -1);
}

/**
 * gtk_combo_box_remove_text:
 * @combo_box: A #GtkComboBox constructed with gtk_combo_box_new_text().
 * @position: Index of the item to remove.
 *
 * Removes the string at @position from @combo_box. Note that you can only use
 * this function with combo boxes constructed with gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_remove_text (GtkComboBox *combo_box,
                           gint         position)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (position >= 0);

  store = GTK_LIST_STORE (combo_box->priv->model);

  if (gtk_tree_model_iter_nth_child (combo_box->priv->model, &iter,
                                     NULL, position))
    gtk_list_store_remove (store, &iter);
}


static gboolean
gtk_combo_box_mnemonic_activate (GtkWidget *widget,
				 gboolean   group_cycling)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  gtk_widget_grab_focus (combo_box->priv->tree_view);

  return TRUE;
}

