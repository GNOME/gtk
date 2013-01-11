/* GTK - The GIMP Toolkit
 * Copyright © 2013 Carlos Garnacho <carlosg@gnome.org>
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

/**
 * SECTION:gtkselectionwindow
 * @Short_description: Bubble window for content edition
 * @Title: GtkSelectionWindow
 *
 * GtkSelection widget is a small helper object to implement
 * touch-friendly content edition. #GtkEntry and #GtkTextView
 * use this internally to allow text selection edition and
 * manipulation.
 */

#include "config.h"
#include "gtkselectionwindow.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#define TOOLBAR_UI                               \
  "<ui>"                                         \
  "  <toolbar>"                                  \
  "    <toolitem name='cut' action='Cut' />"     \
  "    <toolitem name='copy' action='Copy' />"   \
  "    <toolitem name='paste' action='Paste' />" \
  "    <separator />"                            \
  "  </toolbar>"                                 \
  "</ui>"

static void _action_cut_cb   (GtkAction          *action,
                              GtkSelectionWindow *window);
static void _action_copy_cb  (GtkAction          *action,
                              GtkSelectionWindow *window);
static void _action_paste_cb (GtkAction          *action,
                              GtkSelectionWindow *window);

static GtkActionEntry entries[] = {
  { "Cut", GTK_STOCK_CUT, NULL, NULL, NULL, G_CALLBACK (_action_cut_cb) },
  { "Copy", GTK_STOCK_COPY, NULL, NULL, NULL, G_CALLBACK (_action_copy_cb) },
  { "Paste", GTK_STOCK_PASTE, NULL, NULL, NULL, G_CALLBACK (_action_paste_cb) }
};

enum {
  PROP_EDITABLE = 1,
  PROP_HAS_SELECTION
};

enum {
  CUT,
  COPY,
  PASTE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

struct _GtkSelectionWindowPrivate
{
  GtkUIManager *ui_manager;
  GtkWidget *toolbar;
  guint editable      : 1;
  guint has_selection : 1;
};

G_DEFINE_TYPE (GtkSelectionWindow, gtk_selection_window, GTK_TYPE_BUBBLE_WINDOW)

static void
_action_cut_cb (GtkAction          *action,
                GtkSelectionWindow *window)
{
  g_signal_emit (window, signals[CUT], 0);
}

static void
_action_copy_cb (GtkAction          *action,
                 GtkSelectionWindow *window)
{
  g_signal_emit (window, signals[COPY], 0);
}

static void
_action_paste_cb (GtkAction          *action,
                  GtkSelectionWindow *window)
{
  g_signal_emit (window, signals[PASTE], 0);
}

static void
_gtk_selection_window_update_state (GtkSelectionWindow *window)
{
  GtkSelectionWindowPrivate *priv;
  GtkClipboard *clipboard;
  gboolean text_available;
  GtkAction *action;

  priv = window->_priv;
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window),
                                        GDK_SELECTION_CLIPBOARD);
  text_available = gtk_clipboard_wait_is_text_available (clipboard);

  action = gtk_ui_manager_get_action (priv->ui_manager, "/toolbar/cut");
  gtk_action_set_sensitive (action, priv->editable && priv->has_selection);

  action = gtk_ui_manager_get_action (priv->ui_manager, "/toolbar/copy");
  gtk_action_set_sensitive (action, priv->has_selection);

  action = gtk_ui_manager_get_action (priv->ui_manager, "/toolbar/paste");
  gtk_action_set_sensitive (action, text_available && priv->editable);
}

static void
gtk_selection_window_map (GtkWidget *widget)
{
  _gtk_selection_window_update_state (GTK_SELECTION_WINDOW (widget));
  GTK_WIDGET_CLASS (gtk_selection_window_parent_class)->map (widget);
}

static void
gtk_selection_window_finalize (GObject *object)
{
  GtkSelectionWindowPrivate *priv;

  priv = GTK_SELECTION_WINDOW (object)->_priv;
  g_object_unref (priv->ui_manager);

  G_OBJECT_CLASS (gtk_selection_window_parent_class)->finalize (object);
}

static void
gtk_selection_window_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_EDITABLE:
      gtk_selection_window_set_editable (GTK_SELECTION_WINDOW (object),
                                         g_value_get_boolean (value));
      break;
    case PROP_HAS_SELECTION:
      gtk_selection_window_set_has_selection (GTK_SELECTION_WINDOW (object),
                                              g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_selection_window_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkSelectionWindowPrivate *priv;

  priv = GTK_SELECTION_WINDOW (object)->_priv;

  switch (prop_id)
    {
    case PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, priv->has_selection);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_selection_window_class_init (GtkSelectionWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_selection_window_finalize;
  object_class->set_property = gtk_selection_window_set_property;
  object_class->get_property = gtk_selection_window_get_property;

  widget_class->map = gtk_selection_window_map;

  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
                                                         P_("Editable"),
                                                         P_("Whether content is editable"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_HAS_SELECTION,
                                   g_param_spec_boolean ("has-selection",
                                                         P_("Has selection"),
                                                         P_("Whether there is any content currently selected"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT));
  signals[CUT] =
    g_signal_new (I_("cut"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  signals[COPY] =
    g_signal_new (I_("copy"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  signals[PASTE] =
    g_signal_new (I_("paste"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (GtkSelectionWindowPrivate));
}

static void
gtk_selection_window_init (GtkSelectionWindow *window)
{
  GtkSelectionWindowPrivate *priv;
  GtkActionGroup *group;

  window->_priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (window,
                                                      GTK_TYPE_SELECTION_WINDOW,
                                                      GtkSelectionWindowPrivate);

  group = gtk_action_group_new ("SelectionToolbar");
  gtk_action_group_add_actions (group, entries, G_N_ELEMENTS (entries), window);

  priv->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (priv->ui_manager, group, 0);
  gtk_ui_manager_add_ui_from_string (priv->ui_manager, TOOLBAR_UI, -1, NULL);

  priv->toolbar = gtk_ui_manager_get_widget (priv->ui_manager, "/toolbar");
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->toolbar), FALSE);
  gtk_widget_show_all (priv->toolbar);

  gtk_container_add (GTK_CONTAINER (window), priv->toolbar);
}

/**
 * gtk_selection_window_new:
 *
 * Creates a new #GtkSelectionWindow
 *
 * Returns: a newly created #GtkSelectionWindow
 **/
GtkWidget *
gtk_selection_window_new (void)
{
  return g_object_new (GTK_TYPE_SELECTION_WINDOW, NULL);
}

/**
 * gtk_selection_window_set_editable:
 * @window: a #GtkSelectionWindow
 * @editable: whether the current selection is editable
 *
 * Sets whether the current selection is editable. Toolbar options'
 * sensitivity will change according to this.
 *
 * Since: 3.8
 **/
void
gtk_selection_window_set_editable (GtkSelectionWindow *window,
				   gboolean            editable)
{
  GtkSelectionWindowPrivate *priv;
  gboolean need_update;

  g_return_if_fail (GTK_IS_SELECTION_WINDOW (window));

  priv = window->_priv;
  need_update = priv->editable != editable &&
    gtk_widget_get_visible (GTK_WIDGET (window));
  priv->editable = editable;

  if (need_update)
    _gtk_selection_window_update_state (window);

  g_object_notify (G_OBJECT (window), "editable");
}

/**
 * gtk_selection_window_get_editable:
 * @window: a #GtkSelectionWindow
 *
 * Returns whether the contents are editable according to @window
 *
 * Returns: whether the contents are editable
 *
 * Since: 3.8
 **/
gboolean
gtk_selection_window_get_editable (GtkSelectionWindow *window)
{
  GtkSelectionWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_SELECTION_WINDOW (window), FALSE);

  priv = window->_priv;

  return priv->editable;
}

/**
 * gtk_selection_window_set_has_selection:
 * @window: a #GtkSelectionWindow
 * @has_selection: whether there is currently a selection
 *
 * Sets whether there is any selected content. Toolbar options'
 * sensitivity will change according to this.
 *
 * Since: 3.8
 **/
void
gtk_selection_window_set_has_selection (GtkSelectionWindow *window,
                                        gboolean            has_selection)
{
  GtkSelectionWindowPrivate *priv;
  gboolean need_update;

  g_return_if_fail (GTK_IS_SELECTION_WINDOW (window));

  priv = window->_priv;
  need_update = priv->has_selection != has_selection &&
    gtk_widget_get_visible (GTK_WIDGET (window));
  priv->has_selection = has_selection;

  if (need_update)
    _gtk_selection_window_update_state (window);

  g_object_notify (G_OBJECT (window), "has-selection");
}

/**
 * gtk_selection_window_get_has_selection:
 * @window: a #GtkSelectionWindow
 *
 * Returns whether there any content is selected according to @window
 *
 * Returns: whether there is selected content
 *
 * Since: 3.8
 **/
gboolean
gtk_selection_window_get_has_selection (GtkSelectionWindow *window)
{
  GtkSelectionWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_SELECTION_WINDOW (window), FALSE);

  priv = window->_priv;

  return priv->has_selection;
}

/**
 * gtk_selection_window_get_toolbar:
 * @window: a #GtkSelectionWindow
 *
 * Returns the toolbar contained by @window so e.g. new elements
 * can be added.
 *
 * Returns: the internal toolbar
 *
 * Since: 3.8
 **/
GtkWidget *
gtk_selection_window_get_toolbar (GtkSelectionWindow *window)
{
  GtkSelectionWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_SELECTION_WINDOW (window), FALSE);

  priv = window->_priv;

  return priv->toolbar;
}
