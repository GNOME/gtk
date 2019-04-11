/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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

#include "gtkpasswordentry.h"

#include "gtkaccessible.h"
#include "gtkbindings.h"
#include "gtktextprivate.h"
#include "gtkeditable.h"
#include "gtkgestureclick.h"
#include "gtkbox.h"
#include "gtkimage.h"
#include "gtkcheckmenuitem.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkstylecontext.h"
#include "gtkeventcontrollerkey.h"

#include "a11y/gtkentryaccessible.h"

/**
 * SECTION:gtkpasswordentry
 * @Short_description: An entry for secrets
 * @Title: GtkPasswordEntry
 *
 * #GtkPasswordEntry is entry that has been tailored for entering secrets.
 * It does not show its contents in clear text, does not allow to copy it
 * to the clipboard, and it shows a warning when Caps Lock is engaged.
 *
 * Optionally, it can offer a way to reveal the contents in clear text.
 *
 * GtkPasswordEntry provides only minimal API and should be used with the
 * #GtkEditable API.
 */

typedef struct {
  GtkWidget *entry;
  GtkWidget *icon;
  GtkWidget *peek_icon;
  GdkKeymap *keymap;
} GtkPasswordEntryPrivate;

struct _GtkPasswordEntryClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_PLACEHOLDER_TEXT = 1,
  PROP_ACTIVATES_DEFAULT,
  PROP_SHOW_PEEK_ICON,
  NUM_PROPERTIES 
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static GMenuModel *gtk_password_entry_get_default_menu (GtkWidget *widget);

static void gtk_password_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPasswordEntry, gtk_password_entry, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkPasswordEntry)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, gtk_password_entry_editable_init))

static void
keymap_state_changed (GdkKeymap *keymap,
                      GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  if (gtk_editable_get_editable (GTK_EDITABLE (entry)) &&
      gtk_widget_has_focus (priv->entry) &&
      gdk_keymap_get_caps_lock_state (priv->keymap) &&
      !priv->peek_icon)
    gtk_widget_show (priv->icon);
  else
    gtk_widget_hide (priv->icon);
}

static void
focus_changed (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  if (priv->keymap)
    keymap_state_changed (priv->keymap, widget);
}
 
static void
gtk_password_entry_toggle_peek (GtkPasswordEntry *entry)
{
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  if (gtk_text_get_visibility (GTK_TEXT (priv->entry)))
    {
      gtk_text_set_visibility (GTK_TEXT (priv->entry), FALSE);
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->peek_icon), "eye-not-looking-symbolic");
      gtk_widget_set_tooltip_text (priv->peek_icon, _("Show text"));
    }
  else
    {
      gtk_text_set_visibility (GTK_TEXT (priv->entry), TRUE);
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->peek_icon), "eye-open-negative-filled-symbolic");
      gtk_widget_set_tooltip_text (priv->peek_icon, _("Hide text"));
    }
}

static void
gtk_password_entry_init (GtkPasswordEntry *entry)
{
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  priv->entry = gtk_text_new ();
  gtk_text_set_visibility (GTK_TEXT (priv->entry), FALSE);
  gtk_widget_set_parent (priv->entry, GTK_WIDGET (entry));
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  g_signal_connect_swapped (priv->entry, "notify::has-focus", G_CALLBACK (focus_changed), entry);

  priv->icon = gtk_image_new_from_icon_name ("caps-lock-symbolic");
  gtk_widget_set_tooltip_text (priv->icon, _("Caps Lock is on"));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->icon), "caps-lock-indicator");
  gtk_widget_set_cursor (priv->icon, gtk_widget_get_cursor (priv->entry));
  gtk_widget_set_parent (priv->icon, GTK_WIDGET (entry));

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), I_("password"));

  gtk_widget_set_context_menu (GTK_WIDGET (entry),
                               gtk_password_entry_get_default_menu (GTK_WIDGET (entry)));
}

static void
gtk_password_entry_realize (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  GTK_WIDGET_CLASS (gtk_password_entry_parent_class)->realize (widget);

  priv->keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));
  g_signal_connect (priv->keymap, "state-changed", G_CALLBACK (keymap_state_changed), entry);
  keymap_state_changed (priv->keymap, widget);
}

static void
gtk_password_entry_dispose (GObject *object)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (object);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  if (priv->keymap)
    g_signal_handlers_disconnect_by_func (priv->keymap, keymap_state_changed, entry);

  if (priv->entry)
    gtk_editable_finish_delegate (GTK_EDITABLE (entry));

  g_clear_pointer (&priv->entry, gtk_widget_unparent);
  g_clear_pointer (&priv->icon, gtk_widget_unparent);
  g_clear_pointer (&priv->peek_icon, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_password_entry_parent_class)->dispose (object);
}

static void
gtk_password_entry_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_password_entry_parent_class)->finalize (object);
}

static void
gtk_password_entry_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (object);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      gtk_text_set_placeholder_text (GTK_TEXT (priv->entry), g_value_get_string (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      if (gtk_text_get_activates_default (GTK_TEXT (priv->entry)) != g_value_get_boolean (value))
        {
          gtk_text_set_activates_default (GTK_TEXT (priv->entry), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SHOW_PEEK_ICON:
      gtk_password_entry_set_show_peek_icon (entry, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_password_entry_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (object);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (GTK_TEXT (priv->entry)));
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, gtk_text_get_activates_default (GTK_TEXT (priv->entry)));
      break;

    case PROP_SHOW_PEEK_ICON:
      g_value_set_boolean (value, gtk_password_entry_get_show_peek_icon (entry));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_password_entry_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);
  int icon_min = 0, icon_nat = 0;

  gtk_widget_measure (priv->entry, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

  if (priv->icon && gtk_widget_get_visible (priv->icon))
    gtk_widget_measure (priv->icon, orientation, for_size,
                        &icon_min, &icon_nat,
                        NULL, NULL);
   
  if (priv->peek_icon && gtk_widget_get_visible (priv->peek_icon))
    gtk_widget_measure (priv->peek_icon, orientation, for_size,
                        &icon_min, &icon_nat,
                        NULL, NULL);
}

static void
gtk_password_entry_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);
  int icon_min = 0, icon_nat = 0;
  int peek_min = 0, peek_nat = 0;
  int text_width;

  if (priv->icon && gtk_widget_get_visible (priv->icon))
    gtk_widget_measure (priv->icon, GTK_ORIENTATION_HORIZONTAL, -1,
                        &icon_min, &icon_nat,
                        NULL, NULL);
   
  if (priv->peek_icon && gtk_widget_get_visible (priv->peek_icon))
    gtk_widget_measure (priv->peek_icon, GTK_ORIENTATION_HORIZONTAL, -1,
                        &peek_min, &peek_nat,
                        NULL, NULL);
   
  text_width = width - icon_nat - peek_nat;

  gtk_widget_size_allocate (priv->entry,
                            &(GtkAllocation) { 0, 0, text_width, height },
                            baseline);

  if (priv->icon && gtk_widget_get_visible (priv->icon))
    gtk_widget_size_allocate (priv->icon,
                              &(GtkAllocation) { text_width, 0, icon_nat, height },
                              baseline);

  if (priv->peek_icon && gtk_widget_get_visible (priv->peek_icon))
    gtk_widget_size_allocate (priv->peek_icon,
                              &(GtkAllocation) { text_width + icon_nat, 0, peek_nat, height },
                              baseline);
}

static AtkObject *
gtk_password_entry_get_accessible (GtkWidget *widget)
{
  AtkObject *atk_obj;

  atk_obj = GTK_WIDGET_CLASS (gtk_password_entry_parent_class)->get_accessible (widget);
  atk_object_set_name (atk_obj, _("Password"));

  return atk_obj;
}

static void
gtk_password_entry_grab_focus (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->entry);
}

static gboolean
gtk_password_entry_mnemonic_activate (GtkWidget *widget,
                                      gboolean   group_cycling)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->entry);

  return TRUE;
}

static void
gtk_password_entry_notify (GObject    *object,
                           GParamSpec *pspec)
{
  if (strcmp (pspec->name, "context-menu") == 0)
    {
      GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (object);
      GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);
      GMenuModel *menu = gtk_widget_get_context_menu (GTK_WIDGET (entry));
      gtk_widget_set_context_menu (priv->entry, menu);
    }

  if (G_OBJECT_CLASS (gtk_password_entry_parent_class)->notify)
    G_OBJECT_CLASS (gtk_password_entry_parent_class)->notify (object, pspec);
}

static void
gtk_password_entry_class_init (GtkPasswordEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_password_entry_dispose;
  object_class->finalize = gtk_password_entry_finalize;
  object_class->get_property = gtk_password_entry_get_property;
  object_class->set_property = gtk_password_entry_set_property;
  object_class->notify = gtk_password_entry_notify;

  widget_class->realize = gtk_password_entry_realize;
  widget_class->measure = gtk_password_entry_measure;
  widget_class->size_allocate = gtk_password_entry_size_allocate;
  widget_class->get_accessible = gtk_password_entry_get_accessible;
  widget_class->grab_focus = gtk_password_entry_grab_focus;
  widget_class->mnemonic_activate = gtk_password_entry_mnemonic_activate;
  props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text",
                           P_("Placeholder text"),
                           P_("Show text in the entry when it’s empty and unfocused"),
                           NULL,
                           GTK_PARAM_READWRITE);

  props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default",
                            P_("Activates default"),
                            P_("Whether to activate the default widget (such as the default button in a dialog) when Enter is pressed"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SHOW_PEEK_ICON] =
      g_param_spec_boolean ("show-peek-icon",
                            P_("Show Peek Icon"),
                            P_("Whether to show an icon for revealing the content"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
  gtk_editable_install_properties (object_class, NUM_PROPERTIES);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_ENTRY_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("entry"));
}

static GtkEditable *
gtk_password_entry_get_delegate (GtkEditable *editable)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (editable);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  return GTK_EDITABLE (priv->entry);
}

static void
gtk_password_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_password_entry_get_delegate;
}

/**
 * gtk_password_entry_new:
 *
 * Creates a #GtkPasswordEntry.
 *
 * Returns: a new #GtkPasswordEntry
 */
GtkWidget *
gtk_password_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_PASSWORD_ENTRY, NULL));
}

/**
 * gtk_password_entry_set_show_peek_icon:
 * @entry: a #GtkPasswordEntry
 * @show_peek_icon: whether to show the peek icon
 *
 * Sets whether the entry should have a clickable icon
 * to show the contents of the entry in clear text.
 *
 * Setting this to %FALSE also hides the text again.
 */
void
gtk_password_entry_set_show_peek_icon (GtkPasswordEntry *entry,
                                       gboolean          show_peek_icon)
{
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_PASSWORD_ENTRY (entry));

  if (show_peek_icon == (priv->peek_icon != NULL))
    return;

  if (show_peek_icon)
    {
      GtkGesture *press;

      priv->peek_icon = gtk_image_new_from_icon_name ("eye-not-looking-symbolic");
      gtk_widget_set_tooltip_text (priv->peek_icon, _("Show text"));
      gtk_widget_set_parent (priv->peek_icon, GTK_WIDGET (entry));

      press = gtk_gesture_click_new ();
      g_signal_connect_swapped (press, "released",
                                G_CALLBACK (gtk_password_entry_toggle_peek), entry);
      gtk_widget_add_controller (priv->peek_icon, GTK_EVENT_CONTROLLER (press));
    }
  else
    {
      g_clear_pointer (&priv->peek_icon, gtk_widget_unparent);
      gtk_text_set_visibility (GTK_TEXT (priv->entry), FALSE);
    }

  keymap_state_changed (priv->keymap, GTK_WIDGET (entry));

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_SHOW_PEEK_ICON]);
}

/**
 * gtk_password_entry_get_show_peek_icon:
 * @entry: a #GtkPasswordEntry
 *
 * Returns whether the entry is showing a clickable icon
 * to reveal the contents of the entry in clear text.
 *
 * Returns: %TRUE if an icon is shown
 */
gboolean
gtk_password_entry_get_show_peek_icon (GtkPasswordEntry *entry)
{
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_PASSWORD_ENTRY (entry), FALSE);

  return priv->peek_icon != NULL;
}

static GMenuModel *
gtk_password_entry_get_default_menu (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);
  GMenuModel *menu;
  GMenuItem *item;

  menu = gtk_widget_get_context_menu (priv->entry);
  item = g_menu_item_new (_("_Show Text"), "context.toggle-visibility");
  g_menu_item_set_attribute (item, "touch-icon", "s", "eye-not-looking-symbolic");
  g_menu_append_item (G_MENU (menu), item);

  return menu;
}

