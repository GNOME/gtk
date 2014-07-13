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
#include "gtkheaderbar.h"
#include "gtkbbox.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkbox.h"
#include "gtkboxprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"
#include "deprecated/gtkstock.h"
#include "gtksizegroup.h"

/**
 * SECTION:gtkdialog
 * @Short_description: Create popup windows
 * @Title: GtkDialog
 * @See_also: #GtkVBox, #GtkWindow, #GtkButton
 *
 * Dialog boxes are a convenient way to prompt the user for a small amount
 * of input, e.g. to display a message, ask a question, or anything else
 * that does not require extensive effort on the user’s part.
 *
 * GTK+ treats a dialog as a window split vertically. The top section is a
 * #GtkVBox, and is where widgets such as a #GtkLabel or a #GtkEntry should
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
 * If “dialog” is a newly created dialog, the two primary areas of the
 * window can be accessed through gtk_dialog_get_content_area() and
 * gtk_dialog_get_action_area(), as can be seen from the example below.
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
 *  gtk_widget_show_all (dialog);
 * }
 * ]|
 *
 * # GtkDialog as GtkBuildable
 *
 * The GtkDialog implementation of the #GtkBuildable interface exposes the
 * @vbox and @action_area as internal children with the names “vbox” and
 * “action_area”.
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
 *     <object class="GtkButton" id="button_ok"/>
 *   </child>
 *   <action-widgets>
 *     <action-widget response="cancel">button_cancel</action-widget>
 *     <action-widget response="ok" default="true">button_ok</action-widget>
 *   </action-widgets>
 * </object>
 * ]|
 */

struct _GtkDialogPrivate
{
  GtkWidget *vbox;
  GtkWidget *headerbar;
  GtkWidget *action_area;
  GtkWidget *action_box;
  GtkSizeGroup *size_group;

  gint use_header_bar;
  gboolean constructed;
};

typedef struct _ResponseData ResponseData;

struct _ResponseData
{
  gint response_id;
};

static void      gtk_dialog_add_buttons_valist   (GtkDialog    *dialog,
                                                  const gchar  *first_button_text,
                                                  va_list       args);

static gboolean  gtk_dialog_delete_event_handler (GtkWidget    *widget,
                                                  GdkEventAny  *event,
                                                  gpointer      user_data);
static void      gtk_dialog_style_updated        (GtkWidget    *widget);
static void      gtk_dialog_map                  (GtkWidget    *widget);

static void      gtk_dialog_close                (GtkDialog    *dialog);

static ResponseData * get_response_data          (GtkWidget    *widget,
                                                  gboolean      create);

static void      gtk_dialog_buildable_interface_init     (GtkBuildableIface *iface);
static gboolean  gtk_dialog_buildable_custom_tag_start   (GtkBuildable  *buildable,
                                                          GtkBuilder    *builder,
                                                          GObject       *child,
                                                          const gchar   *tagname,
                                                          GMarkupParser *parser,
                                                          gpointer      *data);
static void      gtk_dialog_buildable_custom_finished    (GtkBuildable  *buildable,
                                                          GtkBuilder    *builder,
                                                          GObject       *child,
                                                          const gchar   *tagname,
                                                          gpointer       user_data);
static void      gtk_dialog_buildable_add_child          (GtkBuildable  *buildable,
                                                          GtkBuilder    *builder,
                                                          GObject       *child,
                                                          const gchar   *type);


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
  GtkDialogPrivate *priv = dialog->priv;

  if (use_header_bar == -1)
    return;

  priv->use_header_bar = use_header_bar;
}

/* A convenience helper for built-in dialogs */
void
gtk_dialog_set_use_header_bar_from_setting (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = dialog->priv;

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
  GtkDialogPrivate *priv = dialog->priv;

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
apply_response_for_header_bar (GtkDialog *dialog,
                               GtkWidget *child,
                               gint       response_id)
{
  GtkDialogPrivate *priv = dialog->priv;
  GtkPackType pack;

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_HELP)
    pack = GTK_PACK_START;
  else
    pack = GTK_PACK_END;

  gtk_container_child_set (GTK_CONTAINER (priv->headerbar), child,
                           "pack-type", pack,
                           NULL);

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_CLOSE)
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->headerbar), FALSE);
}

static void
add_to_header_bar (GtkDialog *dialog,
                   GtkWidget *child,
                   gint       response_id)
{
  GtkDialogPrivate *priv = dialog->priv;

  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (priv->headerbar), child);
  gtk_size_group_add_widget (priv->size_group, child);
  apply_response_for_header_bar (dialog, child, response_id);

}

static void
apply_response_for_action_area (GtkDialog *dialog,
                                GtkWidget *child,
                                gint       response_id)
{
  GtkDialogPrivate *priv = dialog->priv;

  if (response_id == GTK_RESPONSE_HELP)
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (priv->action_area), child, TRUE);
}

static void
add_to_action_area (GtkDialog *dialog,
                    GtkWidget *child,
                    gint       response_id)
{
  GtkDialogPrivate *priv = dialog->priv;

  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE);
  gtk_container_add (GTK_CONTAINER (priv->action_area), child);
  apply_response_for_action_area (dialog, child, response_id);
}

static void
update_suggested_action (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = dialog->priv;

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
  GtkDialogPrivate *priv = dialog->priv;

  if (priv->use_header_bar)
    g_warning ("Content added to the action area of a dialog using header bars");

  gtk_widget_show (GTK_WIDGET (priv->action_box));
}

static void
gtk_dialog_constructed (GObject *object)
{
  GtkDialog *dialog = GTK_DIALOG (object);
  GtkDialogPrivate *priv = dialog->priv;

  G_OBJECT_CLASS (gtk_dialog_parent_class)->constructed (object);

  priv->constructed = TRUE;
  if (priv->use_header_bar == -1)
    priv->use_header_bar = FALSE;

  if (priv->use_header_bar)
    {
      GList *children, *l;

      gtk_window_set_titlebar (GTK_WINDOW (dialog), priv->headerbar);
      gtk_window_set_title (GTK_WINDOW (dialog), gtk_window_get_title (GTK_WINDOW (dialog)));

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
            gtk_widget_grab_default (child);
        }
      g_list_free (children);

      update_suggested_action (dialog);

      g_signal_connect (priv->action_area, "add", G_CALLBACK (add_cb), dialog);
    }

  gtk_widget_set_visible (priv->headerbar, priv->use_header_bar);
  gtk_widget_set_visible (priv->action_box, !priv->use_header_bar);
}

static void
gtk_dialog_finalize (GObject *obj)
{
  GtkDialog *dialog = GTK_DIALOG (obj);

  g_object_unref (dialog->priv->size_group);

  G_OBJECT_CLASS (gtk_dialog_parent_class)->finalize (obj);
}

static void
gtk_dialog_class_init (GtkDialogClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->constructed  = gtk_dialog_constructed;
  gobject_class->set_property = gtk_dialog_set_property;
  gobject_class->get_property = gtk_dialog_get_property;
  gobject_class->finalize = gtk_dialog_finalize;

  widget_class->map = gtk_dialog_map;
  widget_class->style_updated = gtk_dialog_style_updated;

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
		  _gtk_marshal_VOID__INT,
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
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkDialog:content-area-border:
   *
   * The default border width used around the
   * content area of the dialog, as returned by
   * gtk_dialog_get_content_area(), unless gtk_container_set_border_width()
   * was called on that widget directly.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("content-area-border",
                                                             P_("Content area border"),
                                                             P_("Width of border around the main dialog area"),
                                                             0,
                                                             G_MAXINT,
                                                             2,
                                                             GTK_PARAM_READABLE));
  /**
   * GtkDialog:content-area-spacing:
   *
   * The default spacing used between elements of the
   * content area of the dialog, as returned by
   * gtk_dialog_get_content_area(), unless gtk_box_set_spacing()
   * was called on that widget directly.
   *
   * Since: 2.16
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("content-area-spacing",
                                                             P_("Content area spacing"),
                                                             P_("Spacing between elements of the main dialog area"),
                                                             0,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("button-spacing",
                                                             P_("Button spacing"),
                                                             P_("Spacing between buttons"),
                                                             0,
                                                             G_MAXINT,
                                                             6,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkDialog:action-area-border:
   *
   * The default border width used around the
   * action area of the dialog, as returned by
   * gtk_dialog_get_action_area(), unless gtk_container_set_border_width()
   * was called on that widget directly.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("action-area-border",
                                                             P_("Action area border"),
                                                             P_("Width of border around the button area at the bottom of the dialog"),
                                                             0,
                                                             G_MAXINT,
                                                             5,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkDialog:use-header-bar:
   *
   * %TRUE if the dialog uses a #GtkHeaderBar for action buttons
   * instead of the action-area.
   *
   * For technical reasons, this property is declared as an integer
   * property, but you should only set it to %TRUE or %FALSE.
   *
   * Since: 3.12
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
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, vbox);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, headerbar);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, action_area);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, action_box);
  gtk_widget_class_bind_template_callback (widget_class, gtk_dialog_delete_event_handler);
}

static void
update_spacings (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = dialog->priv;
  gint content_area_border;
  gint content_area_spacing;
  gint button_spacing;
  gint action_area_border;

  gtk_widget_style_get (GTK_WIDGET (dialog),
                        "content-area-border", &content_area_border,
                        "content-area-spacing", &content_area_spacing,
                        "button-spacing", &button_spacing,
                        "action-area-border", &action_area_border,
                        NULL);

  if (!_gtk_container_get_border_width_set (GTK_CONTAINER (priv->vbox)))
    {
      gtk_container_set_border_width (GTK_CONTAINER (priv->vbox),
                                      content_area_border);
      _gtk_container_set_border_width_set (GTK_CONTAINER (priv->vbox), FALSE);
    }

  if (!_gtk_box_get_spacing_set (GTK_BOX (priv->vbox)))
    {
      gtk_box_set_spacing (GTK_BOX (priv->vbox), content_area_spacing);
      _gtk_box_set_spacing_set (GTK_BOX (priv->vbox), FALSE);
    }

  gtk_box_set_spacing (GTK_BOX (priv->action_area),
                       button_spacing);

  if (!_gtk_container_get_border_width_set (GTK_CONTAINER (priv->action_area)))
    {
      gtk_container_set_border_width (GTK_CONTAINER (priv->action_area),
                                      action_area_border);
      _gtk_container_set_border_width_set (GTK_CONTAINER (priv->action_area), FALSE);
    }
}

static void
gtk_dialog_init (GtkDialog *dialog)
{
  dialog->priv = gtk_dialog_get_instance_private (dialog);

  dialog->priv->use_header_bar = -1;
  dialog->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_widget_init_template (GTK_WIDGET (dialog));

  update_spacings (dialog);
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
gtk_dialog_delete_event_handler (GtkWidget   *widget,
                                 GdkEventAny *event,
                                 gpointer     user_data)
{
  /* emit response signal */
  gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_DELETE_EVENT);

  /* Do the destroy by default */
  return FALSE;
}

static GList *
get_action_children (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = dialog->priv;
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
gtk_dialog_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_dialog_parent_class)->style_updated (widget);

  update_spacings (GTK_DIALOG (widget));
}

static GtkWidget *
dialog_find_button (GtkDialog *dialog,
                   gint       response_id)
{
  GtkWidget *child = NULL;
  GList *children, *tmp_list;

  children = get_action_children (dialog);

  for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
    {
      ResponseData *rd = get_response_data (tmp_list->data, FALSE);

      if (rd && rd->response_id == response_id)
       {
         child = tmp_list->data;
         break;
       }
    }

  g_list_free (children);

  return child;
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
 * directly, but into the @vbox and @action_area, as described above.
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
 * response ID. If a #GtkDialog receives the #GtkWidget::delete-event signal,
 * it will emit ::response with a response ID of #GTK_RESPONSE_DELETE_EVENT.
 * However, destroying a dialog does not emit the ::response signal;
 * so be careful relying on ::response when using the
 * #GTK_DIALOG_DESTROY_WITH_PARENT flag. Buttons are from left to right,
 * so the first button in the list will be the leftmost button in the dialog.
 *
 * Here’s a simple example:
 * |[<!-- language="C" -->
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
  GtkDialogPrivate *priv = dialog->priv;

  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (GTK_IS_WIDGET (child));

  add_response_data (dialog, child, response_id);

  if (priv->constructed && priv->use_header_bar)
    {
      add_to_header_bar (dialog, child, response_id);

      if (gtk_widget_has_default (child))
        {
          gtk_widget_grab_default (child);
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

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (button_text)
    {
      GtkStockItem item;
      if (gtk_stock_lookup (button_text, &item))
        g_object_set (button, "use-stock", TRUE, NULL);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;

  gtk_style_context_add_class (gtk_widget_get_style_context (button), "text-button");
  gtk_widget_set_can_default (button, TRUE);

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

      tmp_list = g_list_next (tmp_list);
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
	gtk_widget_grab_default (widget);

      tmp_list = g_list_next (tmp_list);
    }

  g_list_free (children);

  if (dialog->priv->use_header_bar)
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

static gint
run_delete_handler (GtkDialog *dialog,
                    GdkEventAny *event,
                    gpointer data)
{
  RunInfo *ri = data;

  shutdown_loop (ri);

  return TRUE; /* Do not destroy */
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
 * During gtk_dialog_run(), the default behavior of #GtkWidget::delete-event
 * is disabled; if the dialog receives ::delete_event, it will not be
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
 *   gint result = gtk_dialog_run (GTK_DIALOG (dialog));
 *   switch (result)
 *     {
 *       case GTK_RESPONSE_ACCEPT:
 *          do_application_specific_something ();
 *          break;
 *       default:
 *          do_nothing_since_dialog_was_cancelled ();
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
  gulong response_handler;
  gulong unmap_handler;
  gulong destroy_handler;
  gulong delete_handler;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), -1);

  g_object_ref (dialog);

  was_modal = gtk_window_get_modal (GTK_WINDOW (dialog));
  if (!was_modal)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

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

  delete_handler =
    g_signal_connect (dialog,
                      "delete-event",
                      G_CALLBACK (run_delete_handler),
                      &ri);

  destroy_handler =
    g_signal_connect (dialog,
                      "destroy",
                      G_CALLBACK (run_destroy_handler),
                      &ri);

  ri.loop = g_main_loop_new (NULL, FALSE);

  gdk_threads_leave ();
  g_main_loop_run (ri.loop);
  gdk_threads_enter ();

  g_main_loop_unref (ri.loop);

  ri.loop = NULL;

  if (!ri.destroyed)
    {
      if (!was_modal)
        gtk_window_set_modal (GTK_WINDOW(dialog), FALSE);

      g_signal_handler_disconnect (dialog, response_handler);
      g_signal_handler_disconnect (dialog, unmap_handler);
      g_signal_handler_disconnect (dialog, delete_handler);
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
 * Returns: (transfer none): the @widget button that uses the given
 *     @response_id, or %NULL.
 *
 * Since: 2.20
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

      tmp_list = g_list_next (tmp_list);
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
 *
 * Since: 2.8
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

static gboolean
gtk_alt_dialog_button_order (void)
{
  gboolean result;
  g_object_get (gtk_settings_get_default (),
		"gtk-alternative-button-order", &result, NULL);
  return result;
}

/**
 * gtk_alternative_dialog_button_order:
 * @screen: (allow-none): a #GdkScreen, or %NULL to use the default screen
 *
 * Returns %TRUE if dialogs are expected to use an alternative
 * button order on the screen @screen. See
 * gtk_dialog_set_alternative_button_order() for more details
 * about alternative button order.
 *
 * If you need to use this function, you should probably connect
 * to the ::notify:gtk-alternative-button-order signal on the
 * #GtkSettings object associated to @screen, in order to be
 * notified if the button order setting changes.
 *
 * Returns: Whether the alternative button order should be used
 *
 * Since: 2.6
 * Deprecated: 3.10: Deprecated
 */
gboolean
gtk_alternative_dialog_button_order (GdkScreen *screen)
{
  return gtk_alt_dialog_button_order ();
}

static void
gtk_dialog_set_alternative_button_order_valist (GtkDialog *dialog,
                                                gint       first_response_id,
                                                va_list    args)
{
  GtkDialogPrivate *priv = dialog->priv;
  GtkWidget *child;
  gint response_id;
  gint position;

  response_id = first_response_id;
  position = 0;
  while (response_id != -1)
    {
      /* reorder child with response_id to position */
      child = dialog_find_button (dialog, response_id);
      if (child != NULL)
        gtk_box_reorder_child (GTK_BOX (priv->action_area), child, position);
      else
        g_warning ("%s : no child button with response id %d.", G_STRFUNC,
                   response_id);

      response_id = va_arg (args, gint);
      position++;
    }
}

/**
 * gtk_dialog_set_alternative_button_order:
 * @dialog: a #GtkDialog
 * @first_response_id: a response id used by one @dialog’s buttons
 * @...: a list of more response ids of @dialog’s buttons, terminated by -1
 *
 * Sets an alternative button order. If the
 * #GtkSettings:gtk-alternative-button-order setting is set to %TRUE,
 * the dialog buttons are reordered according to the order of the
 * response ids passed to this function.
 *
 * By default, GTK+ dialogs use the button order advocated by the
 * [GNOME Human Interface Guidelines](http://library.gnome.org/devel/hig-book/stable/)
 * with the affirmative button at the far
 * right, and the cancel button left of it. But the builtin GTK+ dialogs
 * and #GtkMessageDialogs do provide an alternative button order,
 * which is more suitable on some platforms, e.g. Windows.
 *
 * Use this function after adding all the buttons to your dialog, as the
 * following example shows:
 *
 * |[<!-- language="C" -->
 * cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
 *                                        _("_Cancel"),
 *                                        GTK_RESPONSE_CANCEL);
 *
 * ok_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
 *                                    _("_OK"),
 *                                    GTK_RESPONSE_OK);
 *
 * gtk_widget_grab_default (ok_button);
 *
 * help_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
 *                                      _("_Help"),
 *                                      GTK_RESPONSE_HELP);
 *
 * gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
 *                                          GTK_RESPONSE_OK,
 *                                          GTK_RESPONSE_CANCEL,
 *                                          GTK_RESPONSE_HELP,
 *                                          -1);
 * ]|
 *
 * Since: 2.6
 * Deprecated: 3.10: Deprecated
 */
void
gtk_dialog_set_alternative_button_order (GtkDialog *dialog,
					 gint       first_response_id,
					 ...)
{
  GtkDialogPrivate *priv = dialog->priv;
  va_list args;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  if (priv->constructed && priv->use_header_bar)
    return;

  if (!gtk_alt_dialog_button_order ())
    return;

  va_start (args, first_response_id);

  gtk_dialog_set_alternative_button_order_valist (dialog,
                                                 first_response_id,
                                                 args);
  va_end (args);
}
/**
 * gtk_dialog_set_alternative_button_order_from_array:
 * @dialog: a #GtkDialog
 * @n_params: the number of response ids in @new_order
 * @new_order: (array length=n_params): an array of response ids of
 *     @dialog’s buttons
 *
 * Sets an alternative button order. If the
 * #GtkSettings:gtk-alternative-button-order setting is set to %TRUE,
 * the dialog buttons are reordered according to the order of the
 * response ids in @new_order.
 *
 * See gtk_dialog_set_alternative_button_order() for more information.
 *
 * This function is for use by language bindings.
 *
 * Since: 2.6
 * Deprecated: 3.10: Deprecated
 */
void
gtk_dialog_set_alternative_button_order_from_array (GtkDialog *dialog,
                                                    gint       n_params,
                                                    gint      *new_order)
{
  GtkDialogPrivate *priv = dialog->priv;
  GtkWidget *child;
  gint position;

  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (new_order != NULL);

  if (dialog->priv->use_header_bar)
    return;

  if (!gtk_alt_dialog_button_order ())
    return;

  for (position = 0; position < n_params; position++)
  {
      /* reorder child with response_id to position */
      child = dialog_find_button (dialog, new_order[position]);
      if (child != NULL)
        gtk_box_reorder_child (GTK_BOX (priv->action_area), child, position);
      else
        g_warning ("%s : no child button with response id %d.", G_STRFUNC,
                   new_order[position]);
    }
}

typedef struct {
  gchar *widget_name;
  gint response_id;
  gboolean is_default;
} ActionWidgetInfo;

typedef struct {
  GtkDialog *dialog;
  GtkBuilder *builder;
  GSList *items;
  gchar *response;
  gboolean is_default;
} ActionWidgetsSubParserData;

static gint
parse_response_id (const gchar *response_attr)
{
  int response_id;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value;

  response_id = g_ascii_strtoll (response_attr, NULL, 10);
  if (response_id != 0)
    goto out;

  enum_class = g_type_class_ref (GTK_TYPE_RESPONSE_TYPE);
  enum_value = g_enum_get_value_by_nick (enum_class, response_attr);
  if (enum_value == NULL)
    goto out;

  response_id = enum_value->value;

 out:
  if (enum_class)
    g_type_class_unref (enum_class);

  return response_id;
}

static void
attributes_start_element (GMarkupParseContext *context,
			  const gchar         *element_name,
			  const gchar        **names,
			  const gchar        **values,
			  gpointer             user_data,
			  GError             **error)
{
  ActionWidgetsSubParserData *parser_data = (ActionWidgetsSubParserData*)user_data;
  guint i;

  if (strcmp (element_name, "action-widget") == 0)
    {
      for (i = 0; names[i]; i++)
        {
	  if (strcmp (names[i], "response") == 0)
	    parser_data->response = g_strdup (values[i]);
          else if (strcmp (names[i], "default") == 0)
            parser_data->is_default = TRUE;
        }
    }
  else if (strcmp (element_name, "action-widgets") == 0)
    return;
  else
    g_warning ("Unsupported tag for GtkDialog: %s\n", element_name);
}

static void
attributes_text_element (GMarkupParseContext *context,
			 const gchar         *text,
			 gsize                text_len,
			 gpointer             user_data,
			 GError             **error)
{
  ActionWidgetsSubParserData *parser_data = (ActionWidgetsSubParserData*)user_data;
  ActionWidgetInfo *item;

  
  if (!parser_data->response)
    return;

  item = g_new (ActionWidgetInfo, 1);
  item->widget_name = g_strndup (text, text_len);
  item->response_id = parse_response_id (parser_data->response);
  g_free (parser_data->response);
  item->is_default = parser_data->is_default;
  parser_data->items = g_slist_prepend (parser_data->items, item);
  parser_data->response = NULL;
  parser_data->is_default = FALSE;
}

static const GMarkupParser attributes_parser =
  {
    attributes_start_element,
    NULL,
    attributes_text_element,
  };

static gboolean
gtk_dialog_buildable_custom_tag_start (GtkBuildable  *buildable,
				       GtkBuilder    *builder,
				       GObject       *child,
				       const gchar   *tagname,
				       GMarkupParser *parser,
				       gpointer      *data)
{
  ActionWidgetsSubParserData *parser_data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "action-widgets") == 0)
    {
      parser_data = g_slice_new0 (ActionWidgetsSubParserData);
      parser_data->dialog = GTK_DIALOG (buildable);
      parser_data->items = NULL;

      *parser = attributes_parser;
      *data = parser_data;
      return TRUE;
    }

  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
						   tagname, parser, data);
}

static void
gtk_dialog_buildable_custom_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder,
				      GObject      *child,
				      const gchar  *tagname,
				      gpointer      user_data)
{
  GtkDialog *dialog = GTK_DIALOG (buildable);
  GSList *l;
  ActionWidgetsSubParserData *parser_data;
  GObject *object;
  ResponseData *ad;
  guint signal_id;

  if (strcmp (tagname, "action-widgets") != 0)
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
      return;
    }

  parser_data = (ActionWidgetsSubParserData*)user_data;
  parser_data->items = g_slist_reverse (parser_data->items);

  for (l = parser_data->items; l; l = l->next)
    {
      ActionWidgetInfo *item = l->data;
      gboolean is_action;

      object = gtk_builder_get_object (builder, item->widget_name);
      if (!object)
	{
	  g_warning ("Unknown object %s specified in action-widgets of %s",
		     item->widget_name,
		     gtk_buildable_get_name (GTK_BUILDABLE (buildable)));
	  continue;
	}

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

      if (signal_id)
	{
	  GClosure *closure;

	  closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
					   G_OBJECT (dialog));
	  g_signal_connect_closure_by_id (object, signal_id, 0, closure, FALSE);
	}

      if (is_action)
        {
          if (GTK_IS_HEADER_BAR (gtk_widget_get_parent (GTK_WIDGET (object))))
            apply_response_for_header_bar (dialog, GTK_WIDGET (object), ad->response_id);
          else
            apply_response_for_action_area (dialog, GTK_WIDGET (object), ad->response_id);
        }

      if (item->is_default)
        gtk_widget_grab_default (GTK_WIDGET (object));

      g_free (item->widget_name);
      g_free (item);
    }
  g_slist_free (parser_data->items);
  g_slice_free (ActionWidgetsSubParserData, parser_data);

  update_suggested_action (dialog);
}

static void
gtk_dialog_buildable_add_child (GtkBuildable  *buildable,
                                GtkBuilder    *builder,
                                GObject       *child,
                                const gchar   *type)
{
  GtkDialog *dialog = GTK_DIALOG (buildable);
  GtkDialogPrivate *priv = dialog->priv;

  if (type == NULL)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));

  /* Don't call gtk_window_set_titlebar() until we know whether we want
   * traditional titlebars or header bars.
   */
  else if (g_str_equal (type, "titlebar"))
    priv->headerbar = GTK_WIDGET (child);
  else if (g_strcmp0 (type, "action") == 0)
    gtk_dialog_add_action_widget (GTK_DIALOG (buildable), GTK_WIDGET (child), GTK_RESPONSE_NONE);
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

/**
 * gtk_dialog_get_action_area:
 * @dialog: a #GtkDialog
 *
 * Returns the action area of @dialog.
 *
 * Returns: (transfer none): the action area
 *
 * Since: 2.14
 *
 * Deprecated:3.12: Direct access to the action area
 *   is discouraged; use gtk_dialog_add_button(), etc.
 */
GtkWidget *
gtk_dialog_get_action_area (GtkDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return dialog->priv->action_area;
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
 *
 * Since: 3.12
 */
GtkWidget *
gtk_dialog_get_header_bar (GtkDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return dialog->priv->headerbar;
}

/**
 * gtk_dialog_get_content_area:
 * @dialog: a #GtkDialog
 *
 * Returns the content area of @dialog.
 *
 * Returns: (type Gtk.Box) (transfer none): the content area #GtkBox.
 *
 * Since: 2.14
 **/
GtkWidget *
gtk_dialog_get_content_area (GtkDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return dialog->priv->vbox;
}
