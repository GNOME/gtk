/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernative.c: Native File selector dialog
 * Copyright (C) 2015, Red Hat, Inc.
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

#include "config.h"

#include "gtknativedialogprivate.h"

#include "gtkprivate.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserwidget.h"
#include "gtkfilechooserwidgetprivate.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooserembed.h"
#include "gtkfilesystem.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtktogglebutton.h"
#include "gtkstylecontext.h"
#include "gtkheaderbar.h"
#include "gtklabel.h"
#include "gtkfilechooserentry.h"

/**
 * SECTION:gtknativedialog
 * @Short_description: Integrate with native dialogs
 * @Title: GtkNativeDialog
 * @See_also: #GtkFileChooserNative, #GtkDialog
 *
 * Native dialogs are platform dialogs that don't use #GtkDialog or
 * #GtkWindow. They are used in order to integrate better with a
 * platform, by looking the same as other native applications and
 * supporting platform specific features.
 *
 * The #GtkDialog functions cannot be used on such objects, but we
 * need a similar API in order to drive them. The #GtkNativeDialog
 * object is an API that allows you to do this. It allows you to set
 * various common properties on the dialog, as well as show and hide
 * it and get a #GtkNativeDialog::response signal when the user finished
 * with the dialog.
 *
 * There is also a gtk_native_dialog_run() helper that makes it easy
 * to run any native dialog in a modal way with a recursive mainloop,
 * similar to gtk_dialog_run().
 */

typedef struct _GtkNativeDialogPrivate GtkNativeDialogPrivate;

struct _GtkNativeDialogPrivate
{
  GtkWindow *transient_for;
  char *title;

  guint visible : 1;
  guint modal : 1;

  /* Run state */
  gint run_response_id;
  GMainLoop *run_loop; /* Non-NULL when in run */
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_VISIBLE,
  PROP_MODAL,
  PROP_TRANSIENT_FOR,

  LAST_ARG,
};

enum {
  RESPONSE,

  LAST_SIGNAL
};

static GParamSpec *native_props[LAST_ARG] = { NULL, };
static guint native_signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkNativeDialog, gtk_native_dialog, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (GtkNativeDialog))

static void
gtk_native_dialog_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)

{
  GtkNativeDialog *self = GTK_NATIVE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_native_dialog_set_title (self, g_value_get_string (value));
      break;

    case PROP_MODAL:
      gtk_native_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_VISIBLE:
      if (g_value_get_boolean (value))
        gtk_native_dialog_show (self);
      else
        gtk_native_dialog_hide (self);
      break;

    case PROP_TRANSIENT_FOR:
      gtk_native_dialog_set_transient_for (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_native_dialog_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkNativeDialog *self = GTK_NATIVE_DIALOG (object);
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, priv->modal);
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visible);
      break;

    case PROP_TRANSIENT_FOR:
      g_value_set_object (value, priv->transient_for);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_native_dialog_dispose (GObject *object)
{
  GtkNativeDialog *self = GTK_NATIVE_DIALOG (object);
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  if (priv->visible)
    gtk_native_dialog_hide (self);

  G_OBJECT_CLASS (gtk_native_dialog_parent_class)->dispose (object);
}

static void
gtk_native_dialog_finalize (GObject *object)
{
  GtkNativeDialog *self = GTK_NATIVE_DIALOG (object);
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->transient_for);

  G_OBJECT_CLASS (gtk_native_dialog_parent_class)->finalize (object);
}

static void
gtk_native_dialog_class_init (GtkNativeDialogClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_native_dialog_set_property;
  gobject_class->get_property = gtk_native_dialog_get_property;
  gobject_class->finalize = gtk_native_dialog_finalize;
  gobject_class->dispose = gtk_native_dialog_dispose;

  /**
   * GtkNativeDialog:title:
   *
   * The title of the dialog window
   *
   * Since: 3.20
   */
  native_props[PROP_TITLE] =
    g_param_spec_string ("title",
                         P_("Dialog Title"),
                         P_("The title of the file chooser dialog"),
                         NULL,
                         GTK_PARAM_READWRITE);

  /**
   * GtkNativeDialog:modal:
   *
   * Whether the window should be modal with respect to its transient parent.
   *
   * Since: 3.20
   */
  native_props[PROP_MODAL] =
    g_param_spec_boolean ("modal",
                          P_("Modal"),
                          P_("If TRUE, the dialog is modal (other windows are not usable while this one is up)"),
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkNativeDialog:visible:
   *
   * Whether the window is currenlty visible.
   *
   * Since: 3.20
   */
  native_props[PROP_VISIBLE] =
    g_param_spec_boolean ("visible",
                          P_("Visible"),
                          P_("Whether the dialog is currently visible"),
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkNativeDialog:transient-for:
   *
   * The transient parent of the dialog, or %NULL for none.
   *
   * Since: 3.20
   */
  native_props[PROP_TRANSIENT_FOR] =
    g_param_spec_object ("transient-for",
                         P_("Transient for Window"),
                         P_("The transient parent of the dialog"),
                         GTK_TYPE_WINDOW,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_ARG, native_props);

  /**
   * GtkNativeDialog::response:
   * @self: the object on which the signal is emitted
   * @response_id: the response ID
   *
   * Emitted when the user responds to the dialog.
   *
   * When this is called the dialog has been hidden.
   *
   * If you call gtk_native_dialog_hide() before the user responds to
   * the dialog this signal will not be emitted.
   *
   * Since: 3.20
   */
  native_signals[RESPONSE] =
    g_signal_new (I_("response"),
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNativeDialogClass, response),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);
}

static void
gtk_native_dialog_init (GtkNativeDialog *self)
{
}

/**
 * gtk_native_dialog_show:
 * @self: a #GtkNativeDialog
 *
 * Shows the dialog on the display, allowing the user to interact with
 * it. When the user accepts the state of the dialog the dialog will
 * be automatically hidden and the #GtkNativeDialog::response signal
 * will be emitted.
 *
 * Multiple calls while the dialog is visible will be ignored.
 *
 * Since: 3.20
 **/
void
gtk_native_dialog_show (GtkNativeDialog *self)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);
  GtkNativeDialogClass *klass;

  g_return_if_fail (GTK_IS_NATIVE_DIALOG (self));

  if (priv->visible)
    return;

  klass = GTK_NATIVE_DIALOG_GET_CLASS (self);

  g_return_if_fail (klass->show != NULL);

  klass->show (self);

  priv->visible = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_VISIBLE]);
}

/**
 * gtk_native_dialog_hide:
 * @self: a #GtkNativeDialog
 *
 * Hides the dialog if it is visilbe, aborting any interaction. Once this
 * is called the  #GtkNativeDialog::response signal will not be emitted
 * until after the next call to gtk_native_dialog_show().
 *
 * If the dialog is not visible this does nothing.
 *
 * Since: 3.20
 **/
void
gtk_native_dialog_hide (GtkNativeDialog *self)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);
  GtkNativeDialogClass *klass;

  g_return_if_fail (GTK_IS_NATIVE_DIALOG (self));

  if (!priv->visible)
    return;

  priv->visible = FALSE;

  klass = GTK_NATIVE_DIALOG_GET_CLASS (self);

  g_return_if_fail (klass->hide != NULL);

  klass->hide (self);

  if (priv->run_loop && g_main_loop_is_running (priv->run_loop))
    g_main_loop_quit (priv->run_loop);

  g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_VISIBLE]);
}

/**
 * gtk_native_dialog_destroy:
 * @self: a #GtkNativeDialog
 *
 * Destroys a dialog.
 *
 * When a dialog is destroyed, it will break any references it holds
 * to other objects. If it is visible it will be hidden and any underlying
 * window system resources will be destroyed.
 *
 * Note that this does not release any reference to the object (as opposed to
 * destroying a GtkWindow) because there is no reference from the windowing
 * system to the #GtkNativeDialog.
 *
 * Since: 3.20
 **/
void
gtk_native_dialog_destroy (GtkNativeDialog *self)
{
  g_return_if_fail (GTK_IS_NATIVE_DIALOG (self));

  g_object_run_dispose (G_OBJECT (self));
}

void
_gtk_native_dialog_emit_response (GtkNativeDialog *self,
                                  int response_id)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);
  priv->visible = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_VISIBLE]);

  g_signal_emit (self, native_signals[RESPONSE], 0, response_id);
}

/**
 * gtk_native_dialog_get_visible:
 * @self: a #GtkNativeDialog
 *
 * Determines whether the dialog is visible.
 *
 * Returns: %TRUE if the dialog is visible
 *
 * Since: 3.20
 **/
gboolean
gtk_native_dialog_get_visible (GtkNativeDialog *self)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_NATIVE_DIALOG (self), FALSE);

  return priv->visible;
}

/**
 * gtk_native_dialog_set_modal:
 * @self: a #GtkNativeDialog
 * @modal: whether the window is modal
 *
 * Sets a dialog modal or non-modal. Modal dialogs prevent interaction
 * with other windows in the same application. To keep modal dialogs
 * on top of main application windows, use
 * gtk_native_dialog_set_transient_for() to make the dialog transient for the
 * parent; most [window managers][gtk-X11-arch]
 * will then disallow lowering the dialog below the parent.
 *
 * Since: 3.20
 **/
void
gtk_native_dialog_set_modal (GtkNativeDialog *self,
                             gboolean modal)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_return_if_fail (GTK_IS_NATIVE_DIALOG (self));

  modal = modal != FALSE;

  if (priv->modal == modal)
    return;

  priv->modal = modal;
  g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_MODAL]);
}

/**
 * gtk_native_dialog_get_modal:
 * @self: a #GtkNativeDialog
 *
 * Returns whether the dialog is modal. See gtk_native_dialog_set_modal().
 *
 * Returns: %TRUE if the dialog is set to be modal
 *
 * Since: 3.20
 **/
gboolean
gtk_native_dialog_get_modal (GtkNativeDialog *self)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_NATIVE_DIALOG (self), FALSE);

  return priv->modal;
}

/**
 * gtk_native_dialog_set_title:
 * @self: a #GtkNativeDialog
 * @title: title of the dialog
 *
 * Sets the title of the #GtkNativeDialog.
 *
 * Since: 3.20
 **/
void
gtk_native_dialog_set_title (GtkNativeDialog *self,
                                   const char *title)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_return_if_fail (GTK_IS_NATIVE_DIALOG (self));

  g_free (priv->title);
  priv->title = g_strdup (title);

  g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_TITLE]);
}

/**
 * gtk_native_dialog_get_title:
 * @self: a #GtkNativeDialog
 *
 * Gets the title of the #GtkNativeDialog.
 *
 * Returns: (nullable): the title of the dialog, or %NULL if none has
 *    been set explicitly. The returned string is owned by the widget
 *    and must not be modified or freed.
 *
 * Since: 3.20
 **/
const char *
gtk_native_dialog_get_title (GtkNativeDialog *self)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_NATIVE_DIALOG (self), NULL);

  return priv->title;
}

/**
 * gtk_native_dialog_set_transient_for:
 * @self: a #GtkNativeDialog
 * @parent: (allow-none): parent window, or %NULL
 *
 * Dialog windows should be set transient for the main application
 * window they were spawned from. This allows
 * [window managers][gtk-X11-arch] to e.g. keep the
 * dialog on top of the main window, or center the dialog over the
 * main window.
 *
 * Passing %NULL for @parent unsets the current transient window.
 *
 * Since: 3.20
 */
void
gtk_native_dialog_set_transient_for (GtkNativeDialog *self,
                                     GtkWindow *parent)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_return_if_fail (GTK_IS_NATIVE_DIALOG (self));

  if (g_set_object (&priv->transient_for, parent))
    g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_TRANSIENT_FOR]);
}

/**
 * gtk_native_dialog_get_transient_for:
 * @self: a #GtkNativeDialog
 *
 * Fetches the transient parent for this window. See
 * gtk_native_dialog_set_transient_for().
 *
 * Returns: (nullable) (transfer none): the transient parent for this window,
 * or %NULL if no transient parent has been set.
 *
 * Since: 3.20
 **/
GtkWindow *
gtk_native_dialog_get_transient_for (GtkNativeDialog *self)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_NATIVE_DIALOG (self), NULL);

  return priv->transient_for;
}

static void
run_response_cb (GtkNativeDialog *self,
                 gint response_id,
                 gpointer data)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);

  priv->run_response_id = response_id;
  if (priv->run_loop && g_main_loop_is_running (priv->run_loop))
    g_main_loop_quit (priv->run_loop);
}

/**
 * gtk_native_dialog_run:
 * @self: a #GtkNativeDialog
 *
 * Blocks in a recursive main loop until @self emits the
 * #GtkNativeDialog::response signal. It then returns the response ID
 * from the ::response signal emission.
 *
 * Before entering the recursive main loop, gtk_native_dialog_run()
 * calls gtk_native_dialog_show() on the dialog for you.
 *
 * After gtk_native_dialog_run() returns, then dialog will be hidden.
 *
 * Typical usage of this function might be:
 * |[<!-- language="C" -->
 *   gint result = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));
 *   switch (result)
 *     {
 *       case GTK_RESPONSE_ACCEPT:
 *          do_application_specific_something ();
 *          break;
 *       default:
 *          do_nothing_since_dialog_was_cancelled ();
 *          break;
 *     }
 *   g_object_unref (dialog);
 * ]|
 *
 * Note that even though the recursive main loop gives the effect of a
 * modal dialog (it prevents the user from interacting with other
 * windows in the same window group while the dialog is run), callbacks
 * such as timeouts, IO channel watches, DND drops, etc, will
 * be triggered during a gtk_native_dialog_run() call.
 *
 * Returns: response ID
 *
 * Since: 3.20
 **/
gint
gtk_native_dialog_run (GtkNativeDialog *self)
{
  GtkNativeDialogPrivate *priv = gtk_native_dialog_get_instance_private (self);
  gboolean was_modal;
  guint response_handler;

  g_return_val_if_fail (GTK_IS_NATIVE_DIALOG (self), -1);
  g_return_val_if_fail (!priv->visible, -1);
  g_return_val_if_fail (priv->run_loop == NULL, -1);

  if (priv->visible || priv->run_loop != NULL)
    return -1;

  g_object_ref (self);

  priv->run_response_id = GTK_RESPONSE_NONE;
  priv->run_loop = g_main_loop_new (NULL, FALSE);

  was_modal = priv->modal;
  gtk_native_dialog_set_modal (self, TRUE);

  response_handler =
    g_signal_connect (self,
                      "response",
                      G_CALLBACK (run_response_cb),
                      NULL);

  gtk_native_dialog_show (self);

  gdk_threads_leave ();
  g_main_loop_run (priv->run_loop);
  gdk_threads_enter ();

  g_signal_handler_disconnect (self, response_handler);

  g_main_loop_unref (priv->run_loop);
  priv->run_loop = NULL;

  if (!was_modal)
    gtk_native_dialog_set_modal (self, FALSE);

  g_object_unref (self);

  return priv->run_response_id;
}
