/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkdialogprivate.h"
#include "gtkheaderbar.h"
#include "gtkheaderbarprivate.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkbox.h"
#include "gtkcontainerprivate.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"
#include "gtksizegroup.h"
#include "gtkstylecontext.h"

/**
 * SECTION:gtkdialog
 * @Short_description: Create popup windows
 * @Title: GtkDialog
 * @See_also: #GtkBox, #GtkWindow, #GtkButton
 *
 * Dialog boxes are a convenient way to prompt the user for a small amount
 * of input, e.g. to display a message, ask a question, or anything else
 * that does not require extensive effort on the user’s part.
 *
 * GTK+ treats a dialog as a window split vertically. The top section is a
 * #GtkBox, and is where widgets such as a #GtkLabel or a #GtkEntry should
 * be packed. The bottom area is known as the
 * “action area”. This is generally used for
 * packing buttons into the dialog which may perform functions such as
 * cancel, ok, or apply.
 *
 * #GtkDialog boxes are created with a call to gtk_dialog_new() or
 * gtk_dialog_new_with_buttons(). gtk_dialog_new_with_buttons() is
 * recommended; it allows you to set the dialog title, some convenient
 * flags, and add simple buttons.
 *
 * A “modal” dialog (that is, one which freezes the rest of the application
 * from user input), can be created by calling gtk_window_set_modal() on the
 * dialog. Use the GTK_WINDOW() macro to cast the widget returned from
 * gtk_dialog_new() into a #GtkWindow. When using gtk_dialog_new_with_buttons()
 * you can also pass the #GTK_DIALOG_MODAL flag to make a dialog modal.
 *
 * If you add buttons to #GtkDialog using gtk_dialog_new_with_buttons(),
 * gtk_dialog_add_button(), gtk_dialog_add_buttons(), or
 * gtk_dialog_add_action_widget(), clicking the button will emit a signal
 * called #GtkDialog::response with a response ID that you specified. GTK+
 * will never assign a meaning to positive response IDs; these are entirely
 * user-defined. But for convenience, you can use the response IDs in the
 * #GtkResponseType enumeration (these all have values less than zero). If
 * a dialog receives a delete event, the #GtkDialog::response signal will
 * be emitted with a response ID of #GTK_RESPONSE_DELETE_EVENT.
 *
 * If you want to block waiting for a dialog to return before returning
 * control flow to your code, you can call gtk_dialog_run(). This function
 * enters a recursive main loop and waits for the user to respond to the
 * dialog, returning the response ID corresponding to the button the user
 * clicked.
 *
 * For the simple dialog in the following example, in reality you’d probably
 * use #GtkMessageDialog to save yourself some effort. But you’d need to
 * create the dialog contents manually if you had more than a simple message
 * in the dialog.
 *
 * An example for simple GtkDialog usage:
 * |[<!-- language="C" -->
 * // Function to open a dialog box with a message
 * void
 * quick_message (GtkWindow *parent, gchar *message)
 * {
 *  GtkWidget *dialog, *label, *content_area;
 *  GtkDialogFlags flags;
 *
 *  // Create the widgets
 *  flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 *  dialog = gtk_dialog_new_with_buttons ("Message",
 *                                        parent,
 *                                        flags,
 *                                        _("_OK"),
 *                                        GTK_RESPONSE_NONE,
 *                                        NULL);
 *  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
 *  label = gtk_label_new (message);
 *
 *  // Ensure that the dialog box is destroyed when the user responds
 *
 *  g_signal_connect_swapped (dialog,
 *                            "response",
 *                            G_CALLBACK (gtk_widget_destroy),
 *                            dialog);
 *
 *  // Add the label, and show everything we’ve added
 *
 *  gtk_container_add (GTK_CONTAINER (content_area), label);
 *  gtk_widget_show (dialog);
 * }
 * ]|
 *
 * # GtkDialog as GtkBuildable
 *
 * The GtkDialog implementation of the #GtkBuildable interface exposes the
 * @content_area and @action_area as internal children with the names
 * “content_area” and “action_area”.
 *
 * GtkDialog supports a custom <action-widgets> element, which can contain
 * multiple <action-widget> elements. The “response” attribute specifies a
 * numeric response, and the content of the element is the id of widget
 * (which should be a child of the dialogs @action_area). To mark a response
 * as default, set the “default“ attribute of the <action-widget> element
 * to true.
 *
 * GtkDialog supports adding action widgets by specifying “action“ as
 * the “type“ attribute of a <child> element. The widget will be added
 * either to the action area or the headerbar of the dialog, depending
 * on the “use-header-bar“ property. The response id has to be associated
 * with the action widget using the <action-widgets> element.
 *
 * An example of a #GtkDialog UI definition fragment:
 * |[
 * <object class="GtkDialog" id="dialog1">
 *   <child type="action">
 *     <object class="GtkButton" id="button_cancel"/>
 *   </child>
 *   <child type="action">
 *     <object class="GtkButton" id="button_ok">
 *     </object>
 *   </child>
 *   <action-widgets>
 *     <action-widget response="cancel">button_cancel</action-widget>
 *     <action-widget response="ok" default="true">button_ok</action-widget>
 *   </action-widgets>
 * </object>
 * ]|
 */

typedef struct
{
  GtkWidget *headerbar;
  GtkWidget *action_area;
  GtkWidget *content_area;
  GtkWidget *action_box;
  GtkSizeGroup *size_group;

  gint use_header_bar;
  gboolean constructed;
} GtkDialogPrivate;

typedef struct _ResponseData ResponseData;

struct _ResponseData
{
  gint response_id;
};

static void      gtk_dialog_add_buttons_valist   (GtkDialog    *dialog,
                                                  const gchar  *first_button_text,
                                                  va_list       args);

static gboolean  gtk_dialog_close_request        (GtkWindow    *window);
static void      gtk_dialog_map                  (GtkWidget    *widget);

static void      gtk_dialog_close                (GtkDialog    *dialog);

static ResponseData * get_response_data          (GtkWidget    *widget,
                                                  gboolean      create);

static void     gtk_dialog_buildable_interface_init   (GtkBuildableIface  *iface);
static gboolean gtk_dialog_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                       GtkBuilder         *builder,
                                                       GObject            *child,
                                                       const gchar        *tagname,
                                                       GtkBuildableParser *parser,
                                                       gpointer           *data);
static void     gtk_dialog_buildable_custom_finished  (GtkBuildable       *buildable,
                                                       GtkBuilder         *builder,
                                                       GObject            *child,
                                                       const gchar        *tagname,
                                                       gpointer            user_data);
static void     gtk_dialog_buildable_add_child        (GtkBuildable       *buildable,
                                                       GtkBuilder         *builder,
                                                       GObject            *child,
                                                       const gchar        *type);


enum {
  PROP_0,
  PROP_USE_HEADER_BAR
};

enum {
  RESPONSE,
  CLOSE,
  LAST_SIGNAL
};

static guint dialog_signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (GtkDialog, gtk_dialog, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (GtkDialog)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_dialog_buildable_interface_init))

static void
set_use_header_bar (GtkDialog *dialog,
                    gint       use_header_bar)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  if (use_header_bar == -1)
    return;

  priv->use_header_bar = use_header_bar;
}

/* A convenience helper for built-in dialogs */
void
gtk_dialog_set_use_header_bar_from_setting (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_assert (!priv->constructed);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (dialog)),
                "gtk-dialogs-use-header", &priv->use_header_bar,
                NULL);
}

static void
gtk_dialog_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkDialog *dialog = GTK_DIALOG (object);

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      set_use_header_bar (dialog, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_dialog_get_property (GObject      *object,
                         guint         prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  GtkDialog *dialog = GTK_DIALOG (object);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      g_value_set_int (value, priv->use_header_bar);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
action_widget_activated (GtkWidget *widget, GtkDialog *dialog)
{
  gint response_id;

  response_id = gtk_dialog_get_response_for_widget (dialog, widget);

  gtk_dialog_response (dialog, response_id);
}

typedef struct {
  GtkWidget *child;
  gint       response_id;
} ActionWidgetData;

static void
add_response_data (GtkDialog *dialog,
                   GtkWidget *child,
                   gint       response_id)
{
  ResponseData *ad;
  guint signal_id;

  ad = get_response_data (child, TRUE);
  ad->response_id = response_id;

  if (GTK_IS_BUTTON (child))
    signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
  else
    signal_id = GTK_WIDGET_GET_CLASS (child)->activate_signal;

  if (signal_id)
    {
      GClosure *closure;

      closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
                                       G_OBJECT (dialog));
      g_signal_connect_closure_by_id (child, signal_id, 0, closure, FALSE);
    }
  else
    g_warning ("Only 'activatable' widgets can be packed into the action area of a GtkDialog");
}

static void
add_to_header_bar (GtkDialog *dialog,
                   GtkWidget *child,
                   gint       response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_HELP)
    gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->headerbar), child);
  else
    gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->headerbar), child);

  gtk_size_group_add_widget (priv->size_group, child);

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_CLOSE)
    gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (priv->headerbar), FALSE);
}

static void
apply_response_for_action_area (GtkDialog *dialog,
                                GtkWidget *child,
                                gint       response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_assert (gtk_widget_get_parent (child) == priv->action_area);
}

static void
add_to_action_area (GtkDialog *dialog,
                    GtkWidget *child,
                    gint       response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE);
  gtk_container_add (GTK_CONTAINER (priv->action_area), child);
  apply_response_for_action_area (dialog, child, response_id);
}

static void
update_suggested_action (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  if (priv->use_header_bar)
    {
      GList *children, *l;

      children = gtk_container_get_children (GTK_CONTAINER (priv->headerbar));
      for (l = children; l != NULL; l = l->next)
        {
          GtkWidget *child = l->data;
	  GtkStyleContext *context = gtk_widget_get_style_context (child);

          if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_DEFAULT))
            gtk_style_context_add_class (context, GTK_STYLE_CLASS_SUGGESTED_ACTION);
          else
            gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SUGGESTED_ACTION);
        }
      g_list_free (children);
    }
}

static void
add_cb (GtkContainer *container,
        GtkWidget    *widget,
        GtkDialog    *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  if (priv->use_header_bar)
    g_warning ("Content added to the action area of a dialog using header bars");

  gtk_widget_set_visible (priv->action_box, TRUE);
}

static void
gtk_dialog_constructed (GObject *object)
{
  GtkDialog *dialog = GTK_DIALOG (object);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  G_OBJECT_CLASS (gtk_dialog_parent_class)->constructed (object);

  priv->constructed = TRUE;
  if (priv->use_header_bar == -1)
    priv->use_header_bar = FALSE;

  if (priv->use_header_bar)
    {
      GList *children, *l;

      children = gtk_container_get_children (GTK_CONTAINER (priv->action_area));
      for (l = children; l != NULL; l = l->next)
        {
          GtkWidget *child = l->data;
          gboolean has_default;
          ResponseData *rd;
          gint response_id;

          has_default = gtk_widget_has_default (child);
          rd = get_response_data (child, FALSE);
          response_id = rd ? rd->response_id : GTK_RESPONSE_NONE;

          g_object_ref (child);
          gtk_container_remove (GTK_CONTAINER (priv->action_area), child);
          add_to_header_bar (dialog, child, response_id);
          g_object_unref (child);

          if (has_default)
            gtk_window_set_default_widget (GTK_WINDOW (dialog), child);
        }
      g_list_free (children);

      update_suggested_action (dialog);
      _gtk_header_bar_track_default_decoration (GTK_HEADER_BAR (priv->headerbar));

      g_signal_connect (priv->action_area, "add", G_CALLBACK (add_cb), dialog);
    }
  else
    {
      gtk_window_set_titlebar (GTK_WINDOW (dialog), NULL);
      priv->headerbar = NULL;
    }

  gtk_widget_set_visible (priv->action_box, !priv->use_header_bar);
}

static void
gtk_dialog_finalize (GObject *obj)
{
  GtkDialog *dialog = GTK_DIALOG (obj);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_object_unref (priv->size_group);

  G_OBJECT_CLASS (gtk_dialog_parent_class)->finalize (obj);
}

static void
gtk_dialog_class_init (GtkDialogClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkWindowClass *window_class;
  GtkBindingSet *binding_set;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  window_class = GTK_WINDOW_CLASS (class);

  gobject_class->constructed  = gtk_dialog_constructed;
  gobject_class->set_property = gtk_dialog_set_property;
  gobject_class->get_property = gtk_dialog_get_property;
  gobject_class->finalize = gtk_dialog_finalize;

  widget_class->map = gtk_dialog_map;

  window_class->close_request = gtk_dialog_close_request;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_DIALOG);

  class->close = gtk_dialog_close;

  /**
   * GtkDialog::response:
   * @dialog: the object on which the signal is emitted
   * @response_id: the response ID
   *
   * Emitted when an action widget is clicked, the dialog receives a
   * delete event, or the application programmer calls gtk_dialog_response().
   * On a delete event, the response ID is #GTK_RESPONSE_DELETE_EVENT.
   * Otherwise, it depends on which action widget was clicked.
   */
  dialog_signals[RESPONSE] =
    g_signal_new (I_("response"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkDialogClass, response),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_INT);

  /**
   * GtkDialog::close:
   *
   * The ::close signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user uses a keybinding to close
   * the dialog.
   *
   * The default binding for this signal is the Escape key.
   */
  dialog_signals[CLOSE] =
    g_signal_new (I_("close"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkDialogClass, close),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkDialog:use-header-bar:
   *
   * %TRUE if the dialog uses a #GtkHeaderBar for action buttons
   * instead of the action-area.
   *
   * For technical reasons, this property is declared as an integer
   * property, but you should only set it to %TRUE or %FALSE.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_HEADER_BAR,
                                   g_param_spec_int ("use-header-bar",
                                                     P_("Use Header Bar"),
                                                     P_("Use Header Bar for actions."),
                                                     -1, 1, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkdialog.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, headerbar);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, action_area);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, content_area);
  gtk_widget_class_bind_template_child_private (widget_class, GtkDialog, action_box);

  gtk_widget_class_set_css_name (widget_class, I_("dialog"));
}

static void
gtk_dialog_init (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  priv = gtk_dialog_get_instance_private (dialog);

  priv->use_header_bar = -1;
  priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_widget_init_template (GTK_WIDGET (dialog));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->custom_tag_start = gtk_dialog_buildable_custom_tag_start;
  iface->custom_finished = gtk_dialog_buildable_custom_finished;
  iface->add_child = gtk_dialog_buildable_add_child;
}

static gboolean
gtk_dialog_close_request (GtkWindow *window)
{
  /* emit response signal, this will shut down the loop if we are in gtk_dialog_run */
  gtk_dialog_response (GTK_DIALOG (window), GTK_RESPONSE_DELETE_EVENT);

  return GTK_WINDOW_CLASS (gtk_dialog_parent_class)->close_request (window);
}

static GList *
get_action_children (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  GList *children;

  if (priv->constructed && priv->use_header_bar)
    children = gtk_container_get_children (GTK_CONTAINER (priv->headerbar));
  else
    children = gtk_container_get_children (GTK_CONTAINER (priv->action_area));

  return children;
}

/* A far too tricky heuristic for getting the right initial
 * focus widget if none was set. What we do is we focus the first
 * widget in the tab chain, but if this results in the focus
 * ending up on one of the response widgets _other_ than the
 * default response, we focus the default response instead.
 *
 * Additionally, skip selectable labels when looking for the
 * right initial focus widget.
 */
static void
gtk_dialog_map (GtkWidget *widget)
{
  GtkWidget *default_widget, *focus;
  GtkWindow *window = GTK_WINDOW (widget);
  GtkDialog *dialog = GTK_DIALOG (widget);

  if (gtk_window_get_transient_for (window) == NULL)
    g_message ("GtkDialog mapped without a transient parent. This is discouraged.");

  GTK_WIDGET_CLASS (gtk_dialog_parent_class)->map (widget);

  focus = gtk_window_get_focus (window);
  if (!focus)
    {
      GList *children, *tmp_list;
      GtkWidget *first_focus = NULL;

      do
        {
          g_signal_emit_by_name (window, "move_focus", GTK_DIR_TAB_FORWARD);

          focus = gtk_window_get_focus (window);
          if (GTK_IS_LABEL (focus) &&
              !gtk_label_get_current_uri (GTK_LABEL (focus)))
            gtk_label_select_region (GTK_LABEL (focus), 0, 0);

          if (first_focus == NULL)
            first_focus = focus;
          else if (first_focus == focus)
            break;

          if (!GTK_IS_LABEL (focus))
            break;
        }
      while (TRUE);

      tmp_list = children = get_action_children (dialog);

      while (tmp_list)
	{
	  GtkWidget *child = tmp_list->data;

          default_widget = gtk_window_get_default_widget (window);
	  if ((focus == NULL || child == focus) &&
	      child != default_widget &&
	      default_widget)
	    {
	      gtk_widget_grab_focus (default_widget);
	      break;
	    }

	  tmp_list = tmp_list->next;
	}

      g_list_free (children);
    }
}

static void
gtk_dialog_close (GtkDialog *dialog)
{
  gtk_window_close (GTK_WINDOW (dialog));
}

/**
 * gtk_dialog_new:
 *
 * Creates a new dialog box.
 *
 * Widgets should not be packed into this #GtkWindow
 * directly, but into the @content_area and @action_area, as described above.
 *
 * Returns: the new dialog as a #GtkWidget
 */
GtkWidget*
gtk_dialog_new (void)
{
  return g_object_new (GTK_TYPE_DIALOG, NULL);
}

static GtkWidget*
gtk_dialog_new_empty (const gchar     *title,
                      GtkWindow       *parent,
                      GtkDialogFlags   flags)
{
  GtkDialog *dialog;

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", (flags & GTK_DIALOG_USE_HEADER_BAR) != 0,
                         NULL);

  if (title)
    gtk_window_set_title (GTK_WINDOW (dialog), title);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  return GTK_WIDGET (dialog);
}

/**
 * gtk_dialog_new_with_buttons:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @flags: from #GtkDialogFlags
 * @first_button_text: (allow-none): text to go in first button, or %NULL
 * @...: response ID for first button, then additional buttons, ending with %NULL
 *
 * Creates a new #GtkDialog with title @title (or %NULL for the default
 * title; see gtk_window_set_title()) and transient parent @parent (or
 * %NULL for none; see gtk_window_set_transient_for()). The @flags
 * argument can be used to make the dialog modal (#GTK_DIALOG_MODAL)
 * and/or to have it destroyed along with its transient parent
 * (#GTK_DIALOG_DESTROY_WITH_PARENT). After @flags, button
 * text/response ID pairs should be listed, with a %NULL pointer ending
 * the list. Button text can be arbitrary text. A response ID can be
 * any positive number, or one of the values in the #GtkResponseType
 * enumeration. If the user clicks one of these dialog buttons,
 * #GtkDialog will emit the #GtkDialog::response signal with the corresponding
 * response ID. If a #GtkDialog receives a delete event,
 * it will emit ::response with a response ID of #GTK_RESPONSE_DELETE_EVENT.
 * However, destroying a dialog does not emit the ::response signal;
 * so be careful relying on ::response when using the
 * #GTK_DIALOG_DESTROY_WITH_PARENT flag. Buttons are from left to right,
 * so the first button in the list will be the leftmost button in the dialog.
 *
 * Here’s a simple example:
 * |[<!-- language="C" -->
 *  GtkWidget *main_app_window; // Window the dialog should show up on
 *  GtkWidget *dialog;
 *  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
 *  dialog = gtk_dialog_new_with_buttons ("My dialog",
 *                                        main_app_window,
 *                                        flags,
 *                                        _("_OK"),
 *                                        GTK_RESPONSE_ACCEPT,
 *                                        _("_Cancel"),
 *                                        GTK_RESPONSE_REJECT,
 *                                        NULL);
 * ]|
 *
 * Returns: a new #GtkDialog
 */
GtkWidget*
gtk_dialog_new_with_buttons (const gchar    *title,
                             GtkWindow      *parent,
                             GtkDialogFlags  flags,
                             const gchar    *first_button_text,
                             ...)
{
  GtkDialog *dialog;
  va_list args;

  dialog = GTK_DIALOG (gtk_dialog_new_empty (title, parent, flags));

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
                                 args);

  va_end (args);

  return GTK_WIDGET (dialog);
}

static void
response_data_free (gpointer data)
{
  g_slice_free (ResponseData, data);
}

static ResponseData *
get_response_data (GtkWidget *widget,
		   gboolean   create)
{
  ResponseData *ad = g_object_get_data (G_OBJECT (widget),
                                        "gtk-dialog-response-data");

  if (ad == NULL && create)
    {
      ad = g_slice_new (ResponseData);

      g_object_set_data_full (G_OBJECT (widget),
                              I_("gtk-dialog-response-data"),
                              ad,
			      response_data_free);
    }

  return ad;
}

/**
 * gtk_dialog_add_action_widget:
 * @dialog: a #GtkDialog
 * @child: an activatable widget
 * @response_id: response ID for @child
 *
 * Adds an activatable widget to the action area of a #GtkDialog,
 * connecting a signal handler that will emit the #GtkDialog::response
 * signal on the dialog when the widget is activated. The widget is
 * appended to the end of the dialog’s action area. If you want to add a
 * non-activatable widget, simply pack it into the @action_area field
 * of the #GtkDialog struct.
 **/
void
gtk_dialog_add_action_widget (GtkDialog *dialog,
                              GtkWidget *child,
                              gint       response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (GTK_IS_WIDGET (child));

  add_response_data (dialog, child, response_id);

  if (priv->constructed && priv->use_header_bar)
    {
      add_to_header_bar (dialog, child, response_id);

      if (gtk_widget_has_default (child))
        {
          gtk_window_set_default_widget (GTK_WINDOW (dialog), child);
          update_suggested_action (dialog);
        }
    }
  else
    add_to_action_area (dialog, child, response_id);
}

/**
 * gtk_dialog_add_button:
 * @dialog: a #GtkDialog
 * @button_text: text of button
 * @response_id: response ID for the button
 *
 * Adds a button with the given text and sets things up so that
 * clicking the button will emit the #GtkDialog::response signal with
 * the given @response_id. The button is appended to the end of the
 * dialog’s action area. The button widget is returned, but usually
 * you don’t need it.
 *
 * Returns: (transfer none): the #GtkButton widget that was added
 **/
GtkWidget*
gtk_dialog_add_button (GtkDialog   *dialog,
                       const gchar *button_text,
                       gint         response_id)
{
  GtkWidget *button;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);
  g_return_val_if_fail (button_text != NULL, NULL);

  button = gtk_button_new_with_label (button_text);
  gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);

  gtk_widget_show (button);

  gtk_dialog_add_action_widget (dialog, button, response_id);

  return button;
}

static void
gtk_dialog_add_buttons_valist (GtkDialog      *dialog,
                               const gchar    *first_button_text,
                               va_list         args)
{
  const gchar* text;
  gint response_id;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  if (first_button_text == NULL)
    return;

  text = first_button_text;
  response_id = va_arg (args, gint);

  while (text != NULL)
    {
      gtk_dialog_add_button (dialog, text, response_id);

      text = va_arg (args, gchar*);
      if (text == NULL)
        break;
      response_id = va_arg (args, int);
    }
}

/**
 * gtk_dialog_add_buttons:
 * @dialog: a #GtkDialog
 * @first_button_text: button text
 * @...: response ID for first button, then more text-response_id pairs
 *
 * Adds more buttons, same as calling gtk_dialog_add_button()
 * repeatedly.  The variable argument list should be %NULL-terminated
 * as with gtk_dialog_new_with_buttons(). Each button must have both
 * text and response ID.
 */
void
gtk_dialog_add_buttons (GtkDialog   *dialog,
                        const gchar *first_button_text,
                        ...)
{
  va_list args;

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
                                 args);

  va_end (args);
}

/**
 * gtk_dialog_set_response_sensitive:
 * @dialog: a #GtkDialog
 * @response_id: a response ID
 * @setting: %TRUE for sensitive
 *
 * Calls `gtk_widget_set_sensitive (widget, @setting)`
 * for each widget in the dialog’s action area with the given @response_id.
 * A convenient way to sensitize/desensitize dialog buttons.
 **/
void
gtk_dialog_set_response_sensitive (GtkDialog *dialog,
                                   gint       response_id,
                                   gboolean   setting)
{
  GList *children;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  children = get_action_children (dialog);

  tmp_list = children;
  while (tmp_list != NULL)
    {
      GtkWidget *widget = tmp_list->data;
      ResponseData *rd = get_response_data (widget, FALSE);

      if (rd && rd->response_id == response_id)
        gtk_widget_set_sensitive (widget, setting);

      tmp_list = tmp_list->next;
    }

  g_list_free (children);
}

/**
 * gtk_dialog_set_default_response:
 * @dialog: a #GtkDialog
 * @response_id: a response ID
 *
 * Sets the last widget in the dialog’s action area with the given @response_id
 * as the default widget for the dialog. Pressing “Enter” normally activates
 * the default widget.
 **/
void
gtk_dialog_set_default_response (GtkDialog *dialog,
                                 gint       response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  GList *children;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  children = get_action_children (dialog);

  tmp_list = children;
  while (tmp_list != NULL)
    {
      GtkWidget *widget = tmp_list->data;
      ResponseData *rd = get_response_data (widget, FALSE);

      if (rd && rd->response_id == response_id)
        gtk_window_set_default_widget (GTK_WINDOW (dialog), widget);

      tmp_list = tmp_list->next;
    }

  g_list_free (children);

  if (priv->use_header_bar)
    update_suggested_action (dialog);
}

/**
 * gtk_dialog_response:
 * @dialog: a #GtkDialog
 * @response_id: response ID
 *
 * Emits the #GtkDialog::response signal with the given response ID.
 * Used to indicate that the user has responded to the dialog in some way;
 * typically either you or gtk_dialog_run() will be monitoring the
 * ::response signal and take appropriate action.
 **/
void
gtk_dialog_response (GtkDialog *dialog,
                     gint       response_id)
{
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  g_signal_emit (dialog,
		 dialog_signals[RESPONSE],
		 0,
		 response_id);
}

typedef struct
{
  GtkDialog *dialog;
  gint response_id;
  GMainLoop *loop;
  gboolean destroyed;
} RunInfo;

static void
shutdown_loop (RunInfo *ri)
{
  if (g_main_loop_is_running (ri->loop))
    g_main_loop_quit (ri->loop);
}

static void
run_unmap_handler (GtkDialog *dialog, gpointer data)
{
  RunInfo *ri = data;

  shutdown_loop (ri);
}

static void
run_response_handler (GtkDialog *dialog,
                      gint response_id,
                      gpointer data)
{
  RunInfo *ri;

  ri = data;

  ri->response_id = response_id;

  shutdown_loop (ri);
}

static void
run_destroy_handler (GtkDialog *dialog, gpointer data)
{
  RunInfo *ri = data;

  /* shutdown_loop will be called by run_unmap_handler */

  ri->destroyed = TRUE;
}

/**
 * gtk_dialog_run:
 * @dialog: a #GtkDialog
 *
 * Blocks in a recursive main loop until the @dialog either emits the
 * #GtkDialog::response signal, or is destroyed. If the dialog is
 * destroyed during the call to gtk_dialog_run(), gtk_dialog_run() returns
 * #GTK_RESPONSE_NONE. Otherwise, it returns the response ID from the
 * ::response signal emission.
 *
 * Before entering the recursive main loop, gtk_dialog_run() calls
 * gtk_widget_show() on the dialog for you. Note that you still
 * need to show any children of the dialog yourself.
 *
 * During gtk_dialog_run(), the default behavior of delete events
 * is disabled; if the dialog receives a delete event, it will not be
 * destroyed as windows usually are, and gtk_dialog_run() will return
 * #GTK_RESPONSE_DELETE_EVENT. Also, during gtk_dialog_run() the dialog
 * will be modal. You can force gtk_dialog_run() to return at any time by
 * calling gtk_dialog_response() to emit the ::response signal. Destroying
 * the dialog during gtk_dialog_run() is a very bad idea, because your
 * post-run code won’t know whether the dialog was destroyed or not.
 *
 * After gtk_dialog_run() returns, you are responsible for hiding or
 * destroying the dialog if you wish to do so.
 *
 * Typical usage of this function might be:
 * |[<!-- language="C" -->
 *   GtkWidget *dialog = gtk_dialog_new ();
 *   // Set up dialog...
 *
 *   int result = gtk_dialog_run (GTK_DIALOG (dialog));
 *   switch (result)
 *     {
 *       case GTK_RESPONSE_ACCEPT:
 *          // do_application_specific_something ();
 *          break;
 *       default:
 *          // do_nothing_since_dialog_was_cancelled ();
 *          break;
 *     }
 *   gtk_widget_destroy (dialog);
 * ]|
 *
 * Note that even though the recursive main loop gives the effect of a
 * modal dialog (it prevents the user from interacting with other
 * windows in the same window group while the dialog is run), callbacks
 * such as timeouts, IO channel watches, DND drops, etc, will
 * be triggered during a gtk_dialog_run() call.
 *
 * Returns: response ID
 **/
gint
gtk_dialog_run (GtkDialog *dialog)
{
  RunInfo ri = { NULL, GTK_RESPONSE_NONE, NULL, FALSE };
  gboolean was_modal;
  gboolean was_hide_on_close;
  gulong response_handler;
  gulong unmap_handler;
  gulong destroy_handler;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), -1);

  g_object_ref (dialog);

  was_modal = gtk_window_get_modal (GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  was_hide_on_close = gtk_window_get_hide_on_close (GTK_WINDOW (dialog));
  gtk_window_set_hide_on_close (GTK_WINDOW (dialog), TRUE);

  if (!gtk_widget_get_visible (GTK_WIDGET (dialog)))
    gtk_widget_show (GTK_WIDGET (dialog));

  response_handler =
    g_signal_connect (dialog,
                      "response",
                      G_CALLBACK (run_response_handler),
                      &ri);

  unmap_handler =
    g_signal_connect (dialog,
                      "unmap",
                      G_CALLBACK (run_unmap_handler),
                      &ri);

  destroy_handler =
    g_signal_connect (dialog,
                      "destroy",
                      G_CALLBACK (run_destroy_handler),
                      &ri);

  ri.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (ri.loop);
  g_main_loop_unref (ri.loop);

  ri.loop = NULL;

  if (!ri.destroyed)
    {
      gtk_window_set_modal (GTK_WINDOW (dialog), was_modal);
      gtk_window_set_hide_on_close (GTK_WINDOW (dialog), was_hide_on_close);

      g_signal_handler_disconnect (dialog, response_handler);
      g_signal_handler_disconnect (dialog, unmap_handler);
      g_signal_handler_disconnect (dialog, destroy_handler);
    }

  g_object_unref (dialog);

  return ri.response_id;
}

/**
 * gtk_dialog_get_widget_for_response:
 * @dialog: a #GtkDialog
 * @response_id: the response ID used by the @dialog widget
 *
 * Gets the widget button that uses the given response ID in the action area
 * of a dialog.
 *
 * Returns: (nullable) (transfer none): the @widget button that uses the given
 *     @response_id, or %NULL.
 */
GtkWidget*
gtk_dialog_get_widget_for_response (GtkDialog *dialog,
				    gint       response_id)
{
  GList *children;
  GList *tmp_list;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  children = get_action_children (dialog);

  tmp_list = children;
  while (tmp_list != NULL)
    {
      GtkWidget *widget = tmp_list->data;
      ResponseData *rd = get_response_data (widget, FALSE);

      if (rd && rd->response_id == response_id)
        {
          g_list_free (children);
          return widget;
        }

      tmp_list = tmp_list->next;
    }

  g_list_free (children);

  return NULL;
}

/**
 * gtk_dialog_get_response_for_widget:
 * @dialog: a #GtkDialog
 * @widget: a widget in the action area of @dialog
 *
 * Gets the response id of a widget in the action area
 * of a dialog.
 *
 * Returns: the response id of @widget, or %GTK_RESPONSE_NONE
 *  if @widget doesn’t have a response id set.
 */
gint
gtk_dialog_get_response_for_widget (GtkDialog *dialog,
				    GtkWidget *widget)
{
  ResponseData *rd;

  rd = get_response_data (widget, FALSE);
  if (!rd)
    return GTK_RESPONSE_NONE;
  else
    return rd->response_id;
}

typedef struct {
  gchar *widget_name;
  gint response_id;
  gboolean is_default;
  gint line;
  gint col;
} ActionWidgetInfo;

typedef struct {
  GtkDialog *dialog;
  GtkBuilder *builder;
  GSList *items;
  gint response_id;
  gboolean is_default;
  gboolean is_text;
  GString *string;
  gboolean in_action_widgets;
  gint line;
  gint col;
} SubParserData;

static void
free_action_widget_info (gpointer data)
{
  ActionWidgetInfo *item = data;

  g_free (item->widget_name);
  g_free (item);
}

static void
parser_start_element (GtkBuildableParseContext *context,
                      const gchar              *element_name,
                      const gchar             **names,
                      const gchar             **values,
                      gpointer                  user_data,
                      GError                  **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (strcmp (element_name, "action-widget") == 0)
    {
      const gchar *response;
      gboolean is_default = FALSE;
      GValue gvalue = G_VALUE_INIT;

      if (!_gtk_builder_check_parent (data->builder, context, "action-widgets", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "response", &response,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "default", &is_default,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!gtk_builder_value_from_string_type (data->builder, GTK_TYPE_RESPONSE_TYPE, response, &gvalue, error))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->response_id = g_value_get_enum (&gvalue);
      data->is_default = is_default;
      data->is_text = TRUE;
      g_string_set_size (data->string, 0);
      gtk_buildable_parse_context_get_position (context, &data->line, &data->col);
    }
  else if (strcmp (element_name, "action-widgets") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);

      data->in_action_widgets = TRUE;
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkDialog", element_name,
                                        error);
    }
}

static void
parser_text_element (GtkBuildableParseContext *context,
                     const gchar              *text,
                     gsize                     text_len,
                     gpointer                  user_data,
                     GError                  **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
parser_end_element (GtkBuildableParseContext  *context,
                    const gchar               *element_name,
                    gpointer                   user_data,
                    GError                   **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->is_text)
    {
      ActionWidgetInfo *item;

      item = g_new (ActionWidgetInfo, 1);
      item->widget_name = g_strdup (data->string->str);
      item->response_id = data->response_id;
      item->is_default = data->is_default;
      item->line = data->line;
      item->col = data->col;

      data->items = g_slist_prepend (data->items, item);
      data->is_default = FALSE;
      data->is_text = FALSE;
    }
}

static const GtkBuildableParser sub_parser =
  {
    parser_start_element,
    parser_end_element,
    parser_text_element,
  };

static gboolean
gtk_dialog_buildable_custom_tag_start (GtkBuildable       *buildable,
                                       GtkBuilder         *builder,
                                       GObject            *child,
                                       const gchar        *tagname,
                                       GtkBuildableParser *parser,
                                       gpointer           *parser_data)
{
  SubParserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "action-widgets") == 0)
    {
      data = g_slice_new0 (SubParserData);
      data->dialog = GTK_DIALOG (buildable);
      data->builder = builder;
      data->string = g_string_new ("");
      data->items = NULL;
      data->in_action_widgets = FALSE;

      *parser = sub_parser;
      *parser_data = data;
      return TRUE;
    }

  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
						   tagname, parser, parser_data);
}

static void
gtk_dialog_buildable_custom_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder,
				      GObject      *child,
				      const gchar  *tagname,
				      gpointer      user_data)
{
  GtkDialog *dialog = GTK_DIALOG (buildable);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  GSList *l;
  SubParserData *data;
  GObject *object;
  ResponseData *ad;
  guint signal_id;

  if (strcmp (tagname, "action-widgets") != 0)
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
      return;
    }

  data = (SubParserData*)user_data;
  data->items = g_slist_reverse (data->items);

  for (l = data->items; l; l = l->next)
    {
      ActionWidgetInfo *item = l->data;
      gboolean is_action;

      object = _gtk_builder_lookup_object (builder, item->widget_name, item->line, item->col);
      if (!object)
        continue;

      /* If the widget already has reponse data at this point, it
       * was either added by gtk_dialog_add_action_widget(), or via
       * <child type="action"> or by moving an action area child
       * to the header bar. In these cases, apply placement heuristics
       * based on the response id.
       */
      is_action = get_response_data (GTK_WIDGET (object), FALSE) != NULL;

      ad = get_response_data (GTK_WIDGET (object), TRUE);
      ad->response_id = item->response_id;

      if (GTK_IS_BUTTON (object))
	signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
      else
	signal_id = GTK_WIDGET_GET_CLASS (object)->activate_signal;

      if (signal_id && !is_action)
	{
	  GClosure *closure;

	  closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
					   G_OBJECT (dialog));
	  g_signal_connect_closure_by_id (object, signal_id, 0, closure, FALSE);
	}

      if (gtk_widget_get_parent (GTK_WIDGET (object)) == priv->action_area)
        {
          apply_response_for_action_area (dialog, GTK_WIDGET (object), ad->response_id);
        }
      else if (gtk_widget_get_parent (GTK_WIDGET (object)) == priv->headerbar)
        {
          if (is_action)
            {
              g_object_ref (object);
              gtk_container_remove (GTK_CONTAINER (priv->headerbar), GTK_WIDGET (object));
              add_to_header_bar (dialog, GTK_WIDGET (object), ad->response_id);
              g_object_unref (object);
            }
        }

      if (item->is_default)
        gtk_window_set_default_widget (GTK_WINDOW (dialog), GTK_WIDGET (object));
    }

  g_slist_free_full (data->items, free_action_widget_info);
  g_string_free (data->string, TRUE);
  g_slice_free (SubParserData, data);

  update_suggested_action (dialog);
}

static void
gtk_dialog_buildable_add_child (GtkBuildable  *buildable,
                                GtkBuilder    *builder,
                                GObject       *child,
                                const gchar   *type)
{
  GtkDialog *dialog = GTK_DIALOG (buildable);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  if (type == NULL)
    parent_buildable_iface->add_child (buildable, builder, child, type);
  else if (g_str_equal (type, "titlebar"))
    {
      priv->headerbar = GTK_WIDGET (child);
      _gtk_header_bar_track_default_decoration (GTK_HEADER_BAR (priv->headerbar));
      gtk_window_set_titlebar (GTK_WINDOW (buildable), priv->headerbar);
    }
  else if (g_str_equal (type, "action"))
    gtk_dialog_add_action_widget (GTK_DIALOG (buildable), GTK_WIDGET (child), GTK_RESPONSE_NONE);
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

GtkWidget *
gtk_dialog_get_action_area (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return priv->action_area;
}

/**
 * gtk_dialog_get_header_bar:
 * @dialog: a #GtkDialog
 *
 * Returns the header bar of @dialog. Note that the
 * headerbar is only used by the dialog if the
 * #GtkDialog:use-header-bar property is %TRUE.
 *
 * Returns: (transfer none): the header bar
 */
GtkWidget *
gtk_dialog_get_header_bar (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return priv->headerbar;
}

/**
 * gtk_dialog_get_content_area:
 * @dialog: a #GtkDialog
 *
 * Returns the content area of @dialog.
 *
 * Returns: (type Gtk.Box) (transfer none): the content area #GtkBox.
 **/
GtkWidget *
gtk_dialog_get_content_area (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return priv->content_area;
}
