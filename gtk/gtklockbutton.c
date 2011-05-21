/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
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

#include "config.h"

#include "gtklockbutton.h"

#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkeventbox.h"
#include "gtklabel.h"
#include "gtknotebook.h"
#include "gtkintl.h"

/**
 * SECTION:gtklockbutton
 * @title: GtkLockButton
 * @short_description: A widget to unlock or lock privileged operations
 *
 * GtkLockButton is a widget that can be used in control panels or
 * preference dialogs to allow users to obtain and revoke authorizations
 * needed to operate the controls. The required authorization is represented
 * by a #GPermission object. Concrete implementations of #GPermission may use
 * PolicyKit or some other authorization framework.
 *
 * If the user lacks the authorization but authorization can be obtained
 * through authentication, the widget looks like this
 * <informalexample><inlinegraphic fileref="lockbutton-locked.png"></inlinegraphic></informalexample>
 * and the user can click the button to obtain the authorization. Depending
 * on the platform, this may pop up an authentication dialog or ask the user
 * to authenticate in some other way. Once authorization is obtained, the
 * widget changes to this
 * <informalexample><inlinegraphic fileref="lockbutton-unlocked.png"></inlinegraphic></informalexample>
 * and the authorization can be dropped by clicking the button. If the user
 * is not able to obtain authorization at all, the widget looks like this
 * <informalexample><inlinegraphic fileref="lockbutton-sorry.png"></inlinegraphic></informalexample>
 * If the user is authorized and cannot drop the authorization, the button
 * is hidden.
 *
 * The text (and tooltips) that are shown in the various cases can be adjusted
 * with the #GtkLockButton:text-lock, #GtkLockButton:text-unlock,
 * #GtkLockButton:text-not-authorized, #GtkLockButton:tooltip-lock,
 * #GtkLockButton:tooltip-unlock and #GtkLockButton:tooltip-not-authorized
 * properties.
 */

struct _GtkLockButtonPrivate
{
  GPermission *permission;

  gchar *tooltip_lock;
  gchar *tooltip_unlock;
  gchar *tooltip_not_authorized;

  GtkWidget *box;
  GtkWidget *eventbox;
  GtkWidget *image;
  GtkWidget *button;
  GtkWidget *notebook;

  GtkWidget *label_lock;
  GtkWidget *label_unlock;
  GtkWidget *label_not_authorized;

  GCancellable *cancellable;
};

enum
{
  PROP_0,
  PROP_PERMISSION,
  PROP_TEXT_LOCK,
  PROP_TEXT_UNLOCK,
  PROP_TEXT_NOT_AUTHORIZED,
  PROP_TOOLTIP_LOCK,
  PROP_TOOLTIP_UNLOCK,
  PROP_TOOLTIP_NOT_AUTHORIZED
};

static void update_state   (GtkLockButton *button);
static void update_tooltip (GtkLockButton *button);

static void on_permission_changed (GPermission *permission,
                                   GParamSpec  *pspec,
                                   gpointer     user_data);

static void on_clicked (GtkButton *button,
                        gpointer   user_data);

static void on_button_press (GtkWidget      *widget,
                             GdkEventButton *event,
                             gpointer        user_data);

G_DEFINE_TYPE (GtkLockButton, gtk_lock_button, GTK_TYPE_BIN);

static void
gtk_lock_button_finalize (GObject *object)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (object);
  GtkLockButtonPrivate *priv = button->priv;

  g_free (priv->tooltip_lock);
  g_free (priv->tooltip_unlock);
  g_free (priv->tooltip_not_authorized);

  if (priv->cancellable != NULL)
    {
      g_cancellable_cancel (priv->cancellable);
      g_object_unref (priv->cancellable);
    }

  if (priv->permission)
    {
      g_signal_handlers_disconnect_by_func (priv->permission,
                                            on_permission_changed,
                                            button);
      g_object_unref (priv->permission);
    }

  G_OBJECT_CLASS (gtk_lock_button_parent_class)->finalize (object);
}

static void
gtk_lock_button_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (object);
  GtkLockButtonPrivate *priv = button->priv;

  switch (property_id)
    {
    case PROP_PERMISSION:
      g_value_set_object (value, priv->permission);
      break;

    case PROP_TEXT_LOCK:
      g_value_set_string (value,
                          gtk_label_get_text (GTK_LABEL (priv->label_lock)));
      break;

    case PROP_TEXT_UNLOCK:
      g_value_set_string (value,
                          gtk_label_get_text (GTK_LABEL (priv->label_unlock)));
      break;

    case PROP_TEXT_NOT_AUTHORIZED:
      g_value_set_string (value,
                          gtk_label_get_text (GTK_LABEL (priv->label_not_authorized)));
      break;

    case PROP_TOOLTIP_LOCK:
      g_value_set_string (value, priv->tooltip_lock);
      break;

    case PROP_TOOLTIP_UNLOCK:
      g_value_set_string (value, priv->tooltip_unlock);
      break;

    case PROP_TOOLTIP_NOT_AUTHORIZED:
      g_value_set_string (value, priv->tooltip_not_authorized);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_lock_button_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (object);
  GtkLockButtonPrivate *priv = button->priv;

  switch (property_id)
    {
    case PROP_PERMISSION:
      gtk_lock_button_set_permission (button, g_value_get_object (value));
      break;

    case PROP_TEXT_LOCK:
      gtk_label_set_text (GTK_LABEL (priv->label_lock), g_value_get_string (value));
      break;

    case PROP_TEXT_UNLOCK:
      gtk_label_set_text (GTK_LABEL (priv->label_unlock), g_value_get_string (value));
      break;

    case PROP_TEXT_NOT_AUTHORIZED:
      gtk_label_set_text (GTK_LABEL (priv->label_not_authorized), g_value_get_string (value));
      break;

    case PROP_TOOLTIP_LOCK:
      g_free (priv->tooltip_lock);
      priv->tooltip_lock = g_value_dup_string (value);
      update_tooltip (button);
      break;

    case PROP_TOOLTIP_UNLOCK:
      g_free (priv->tooltip_unlock);
      priv->tooltip_unlock = g_value_dup_string (value);
      update_tooltip (button);
      break;

    case PROP_TOOLTIP_NOT_AUTHORIZED:
      g_free (priv->tooltip_not_authorized);
      priv->tooltip_not_authorized = g_value_dup_string (value);
      update_tooltip (button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_lock_button_init (GtkLockButton *button)
{
  GtkLockButtonPrivate *priv;

  button->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (button,
                                                     GTK_TYPE_LOCK_BUTTON,
                                                     GtkLockButtonPrivate);

  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (button), priv->box);

  priv->eventbox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (priv->eventbox), FALSE);
  gtk_container_add (GTK_CONTAINER (priv->box), priv->eventbox);
  gtk_widget_show (priv->eventbox);
  priv->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (priv->eventbox), priv->image);
  gtk_widget_show (priv->image);

  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_widget_show (priv->notebook);

  priv->button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (priv->button), priv->notebook);
  gtk_widget_show (priv->button);

  priv->label_lock = gtk_label_new ("");
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->label_lock, NULL);
  gtk_widget_show (priv->label_lock);

  priv->label_unlock = gtk_label_new ("");
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->label_unlock, NULL);
  gtk_widget_show (priv->label_unlock);

  priv->label_not_authorized = gtk_label_new ("");
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->label_not_authorized, NULL);
  gtk_widget_show (priv->label_not_authorized);

  gtk_box_pack_start (GTK_BOX (priv->box), priv->button, FALSE, FALSE, 0);
  gtk_widget_show (priv->button);

  g_signal_connect (priv->eventbox, "button-press-event",
                    G_CALLBACK (on_button_press), button);
  g_signal_connect (priv->button, "clicked",
                    G_CALLBACK (on_clicked), button);

  gtk_widget_set_no_show_all (priv->box, TRUE);

  update_state (button);
}

static void
gtk_lock_button_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GtkLockButtonPrivate *priv = GTK_LOCK_BUTTON (widget)->priv;

  gtk_widget_get_preferred_width (priv->box, minimum, natural);
}

static void
gtk_lock_button_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  GtkLockButtonPrivate *priv = GTK_LOCK_BUTTON (widget)->priv;

  gtk_widget_get_preferred_height (priv->box, minimum, natural);
}

static void
gtk_lock_button_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkLockButtonPrivate *priv = GTK_LOCK_BUTTON (widget)->priv;
  GtkRequisition requisition;
  GtkAllocation child_allocation;

  gtk_widget_set_allocation (widget, allocation);
  gtk_widget_get_preferred_size (priv->box, &requisition, NULL);
  child_allocation.x = allocation->x;
  child_allocation.y = allocation->y;
  child_allocation.width = requisition.width;
  child_allocation.height = requisition.height;
  gtk_widget_size_allocate (priv->box, &child_allocation);
}

static void
gtk_lock_button_class_init (GtkLockButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize     = gtk_lock_button_finalize;
  gobject_class->get_property = gtk_lock_button_get_property;
  gobject_class->set_property = gtk_lock_button_set_property;

  widget_class->get_preferred_width  = gtk_lock_button_get_preferred_width;
  widget_class->get_preferred_height = gtk_lock_button_get_preferred_height;
  widget_class->size_allocate        = gtk_lock_button_size_allocate;

  g_type_class_add_private (klass, sizeof (GtkLockButtonPrivate));

  g_object_class_install_property (gobject_class, PROP_PERMISSION,
    g_param_spec_object ("permission",
                         P_("Permission"),
                         P_("The GPermission object controlling this button"),
                         G_TYPE_PERMISSION,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TEXT_LOCK,
    g_param_spec_string ("text-lock",
                         P_("Lock Text"),
                         P_("The text to display when prompting the user to lock"),
                         _("Lock"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TEXT_UNLOCK,
    g_param_spec_string ("text-unlock",
                         P_("Unlock Text"),
                         P_("The text to display when prompting the user to unlock"),
                         _("Unlock"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TEXT_NOT_AUTHORIZED,
    g_param_spec_string ("text-not-authorized",
                         P_("Not Authorized Text"),
                         P_("The text to display when prompting the user cannot obtain authorization"),
                         _("Locked"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TOOLTIP_LOCK,
    g_param_spec_string ("tooltip-lock",
                         P_("Lock Tooltip"),
                         P_("The tooltip to display when prompting the user to lock"),
                         _("Dialog is unlocked.\nClick to prevent further changes"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TOOLTIP_UNLOCK,
    g_param_spec_string ("tooltip-unlock",
                         P_("Unlock Tooltip"),
                         P_("The tooltip to display when prompting the user to unlock"),
                         _("Dialog is locked.\nClick to make changes"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TOOLTIP_NOT_AUTHORIZED,
    g_param_spec_string ("tooltip-not-authorized",
                         P_("Not Authorized Tooltip"),
                         P_("The tooltip to display when prompting the user cannot obtain authorization"),
                         _("System policy prevents changes.\nContact your system administrator"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));
}

static void
update_tooltip (GtkLockButton *button)
{
  GtkLockButtonPrivate *priv = button->priv;
  const gchar *tooltip;

  switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)))
    {
      case 0:
        tooltip = priv->tooltip_lock;
        break;
      case 1:
        tooltip = priv->tooltip_unlock;
        break;
      case 2:
        tooltip = priv->tooltip_not_authorized;
        break;
      default:
        tooltip = "";
        break;
    }

  gtk_widget_set_tooltip_markup (priv->box, tooltip);
}

static void
update_state (GtkLockButton *button)
{
  GtkLockButtonPrivate *priv = button->priv;
  gboolean allowed;
  gboolean can_acquire;
  gboolean can_release;
  gint page;
  gboolean sensitive;
  gboolean visible;
  GIcon *icon;

  if (priv->permission)
    {
      allowed = g_permission_get_allowed (priv->permission);
      can_acquire = g_permission_get_can_acquire (priv->permission);
      can_release = g_permission_get_can_release (priv->permission);
    }
  else
    {
      allowed = FALSE;
      can_acquire = FALSE;
      can_release = FALSE;
    }

  visible = TRUE;
  sensitive = TRUE;

  if (allowed)
    {
      if (can_release)
        {
          page = 0;
          sensitive = TRUE;
        }
      else
        {
          page = 0;
          visible = FALSE;
        }
    }
  else
    {
      if (can_acquire)
        {
          page = 1;
          sensitive = TRUE;
        }
      else
        {
          page = 2;
          sensitive = FALSE;
        }
    }

  if (allowed)
    {
      gchar *names[3];

      names[0] = "changes-allow-symbolic";
      names[1] = "changes-allow";
      names[2] = NULL;
      icon = g_themed_icon_new_from_names (names, -1);
    }
  else
    {
      gchar *names[3];

      names[0] = "changes-prevent-symbolic";
      names[1] = "changes-prevent";
      names[2] = NULL;
      icon = g_themed_icon_new_from_names (names, -1);
    }

  gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, GTK_ICON_SIZE_BUTTON);
  g_object_unref (icon);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), page);
  gtk_widget_set_sensitive (priv->box, sensitive);
  gtk_widget_set_visible (priv->box, visible);

  update_tooltip (button);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       gpointer     user_data)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (user_data);

  update_state (button);
}

static void
acquire_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (user_data);
  GtkLockButtonPrivate *priv = button->priv;
  GError *error;

  error = NULL;
  if (!g_permission_acquire_finish (priv->permission, result, &error))
    {
      g_warning ("Error acquiring permission: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (priv->cancellable);
  priv->cancellable = NULL;

  update_state (button);
}

static void
release_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  GtkLockButton *button = GTK_LOCK_BUTTON (user_data);
  GtkLockButtonPrivate *priv = button->priv;
  GError *error;

  error = NULL;
  if (!g_permission_release_finish (priv->permission, result, &error))
    {
      g_warning ("Error releasing permission: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (priv->cancellable);
  priv->cancellable = NULL;

  update_state (button);
}

static void
handle_click (GtkLockButton *button)
{
  GtkLockButtonPrivate *priv = button->priv;

  /* if we already have a pending interactive check, then do nothing */
  if (priv->cancellable != NULL)
    return;

  if (g_permission_get_allowed (priv->permission))
    {
      if (g_permission_get_can_release (priv->permission))
        {
          priv->cancellable = g_cancellable_new ();

          g_permission_release_async (priv->permission,
                                      priv->cancellable,
                                      release_cb,
                                      button);
        }
    }
  else
    {
      if (g_permission_get_can_acquire (priv->permission))
        {
          priv->cancellable = g_cancellable_new ();

          g_permission_acquire_async (priv->permission,
                                      priv->cancellable,
                                      acquire_cb,
                                      button);
        }
    }
}

static void
on_clicked (GtkButton *button,
            gpointer   user_data)

{
  handle_click (GTK_LOCK_BUTTON (user_data));
}

static void
on_button_press (GtkWidget      *widget,
                 GdkEventButton *event,
                 gpointer        user_data)
{
  handle_click (GTK_LOCK_BUTTON (user_data));
}


/**
 * gtk_lock_button_new:
 * @permission: (allow-none): a #GPermission
 *
 * Creates a new lock button which reflects the @permission.
 *
 * Returns: a new #GtkLockButton
 *
 * Since: 3.2
 */
GtkWidget *
gtk_lock_button_new (GPermission *permission)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_LOCK_BUTTON,
                                   "permission", permission,
                                   NULL));
}

/**
 * gtk_lock_button_get_permission:
 * @button: a #GtkLockButton
 *
 * Obtains the #GPermission object that controls @button.
 *
 * Returns: the #GPermission of @button
 *
 * Since: 3.2
 */
GPermission *
gtk_lock_button_get_permission (GtkLockButton *button)
{
  g_return_val_if_fail (GTK_IS_LOCK_BUTTON (button), NULL);

  return button->priv->permission;
}

/**
 * gtk_lock_button_set_permission:
 * @button: a #GtkLockButton
 * @permission: (allow-none): a #GPermission object, or %NULL
 *
 * Sets the #GPermission object that controls @button.
 *
 * Since: 3.2
 */
void
gtk_lock_button_set_permission (GtkLockButton *button,
                                GPermission   *permission)
{
  GtkLockButtonPrivate *priv;

  g_return_if_fail (GTK_IS_LOCK_BUTTON (button));
  g_return_if_fail (permission == NULL || G_IS_PERMISSION (permission));

  priv = button->priv;

  if (priv->permission != permission)
    {
      if (priv->permission)
        {
          g_signal_handlers_disconnect_by_func (priv->permission,
                                                on_permission_changed,
                                                button);
          g_object_unref (priv->permission);
        }

      priv->permission = permission;

      if (priv->permission)
        {
          g_object_ref (priv->permission);
          g_signal_connect (priv->permission, "notify",
                            G_CALLBACK (on_permission_changed), button);
        }

      update_state (button);

      g_object_notify (G_OBJECT (button), "permission");
    }
}
