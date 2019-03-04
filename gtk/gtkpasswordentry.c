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
#include "gtkbox.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkstylecontext.h"
#include "gtkeventcontrollerkey.h"

#include "a11y/gtkentryaccessible.h"

/**
 * SECTION:gtkpasswordentry
 * @Short_description: An entry for secrets
 * @Title: GtkPasswordEntry
 *
 * #GtkPasswordEntry is entry that has been tailored for
 * entering secrets. It does not show its contents in clear text,
 * does not allow to copy it to the clipboard, and it shows a
 * warning when Caps-Lock is engaged.
 *
 * GtkPasswordEntry provides no API of its own and should be used
 * with the #GtkEditable API.
 */

typedef struct {
  GtkWidget *box;
  GtkWidget *entry;
  GtkWidget *icon;
  GdkKeymap *keymap;
} GtkPasswordEntryPrivate;

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
      gdk_keymap_get_caps_lock_state (priv->keymap))
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
gtk_password_entry_init (GtkPasswordEntry *entry)
{
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  gtk_widget_set_has_surface (GTK_WIDGET (entry), FALSE);

  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (priv->box, FALSE);
  gtk_widget_set_vexpand (priv->box, FALSE);
  gtk_widget_set_parent (priv->box, GTK_WIDGET (entry));

  priv->entry = gtk_text_new ();
  gtk_text_set_visibility (GTK_TEXT (priv->entry), FALSE);
  gtk_widget_set_hexpand (priv->entry, TRUE);
  gtk_widget_set_vexpand (priv->entry, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->box), priv->entry);
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  g_signal_connect_swapped (priv->entry, "notify::has-focus", G_CALLBACK (focus_changed), entry);

  priv->icon = gtk_image_new_from_icon_name ("dialog-warning-symbolic");
  gtk_widget_set_tooltip_text (priv->icon, _("Caps Lock is on"));
  gtk_container_add (GTK_CONTAINER (priv->box), priv->icon);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), I_("password"));
}

static void
gtk_password_entry_realize (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  GTK_WIDGET_CLASS (gtk_password_entry_parent_class)->realize (widget);

  priv->keymap = gdk_display_get_keymap (gtk_widget_get_display (widget));
  g_signal_connect (priv->keymap, "state-changed", G_CALLBACK (keymap_state_changed), entry);
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
  g_clear_pointer (&priv->box, gtk_widget_unparent);

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
  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gtk_password_entry_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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

  gtk_widget_measure (priv->box, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_password_entry_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkPasswordEntryPrivate *priv = gtk_password_entry_get_instance_private (entry);

  gtk_widget_size_allocate (priv->box,
                            &(GtkAllocation) { 0, 0, width, height },
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
gtk_password_entry_class_init (GtkPasswordEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_password_entry_dispose;
  object_class->finalize = gtk_password_entry_finalize;
  object_class->get_property = gtk_password_entry_get_property;
  object_class->set_property = gtk_password_entry_set_property;

  widget_class->realize = gtk_password_entry_realize;
  widget_class->measure = gtk_password_entry_measure;
  widget_class->size_allocate = gtk_password_entry_size_allocate;
  widget_class->get_accessible = gtk_password_entry_get_accessible;
  widget_class->grab_focus = gtk_password_entry_grab_focus;
  widget_class->mnemonic_activate = gtk_password_entry_mnemonic_activate;
 
  gtk_editable_install_properties (object_class, 1);

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
