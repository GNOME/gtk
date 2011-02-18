/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2011 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gaillinkbutton.h"

typedef struct _GailLinkButtonLink GailLinkButtonLink;
typedef struct _GailLinkButtonLinkClass GailLinkButtonLinkClass;

struct _GailLinkButtonLink
{
  AtkHyperlink parent;

  GailLinkButton *button;
  gchar *description;
};

struct _GailLinkButtonLinkClass
{
  AtkHyperlinkClass parent_class;
};

static void atk_action_interface_init (AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GailLinkButtonLink, gail_link_button_link, ATK_TYPE_HYPERLINK,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static gchar *
gail_link_button_link_get_uri (AtkHyperlink *link,
                               gint          i)
{
  GailLinkButtonLink *l = (GailLinkButtonLink *)link;
  GtkWidget *widget;
  const gchar *uri;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (l->button));
  uri = gtk_link_button_get_uri (GTK_LINK_BUTTON (widget));

  return g_strdup (uri);
}

static gint
gail_link_button_link_get_n_anchors (AtkHyperlink *link)
{
  return 1;
}

static gboolean
gail_link_button_link_is_valid (AtkHyperlink *link)
{
  return TRUE;
}

static AtkObject *
gail_link_button_link_get_object (AtkHyperlink *link,
                                  gint          i)
{
  GailLinkButtonLink *l = (GailLinkButtonLink *)link;

  return ATK_OBJECT (l->button);
}

static gint
gail_link_button_link_get_start_index (AtkHyperlink *link)
{
  return 0;
}

static gint
gail_link_button_link_get_end_index (AtkHyperlink *link)
{
  GailLinkButtonLink *l = (GailLinkButtonLink *)link;

  return atk_text_get_character_count (ATK_TEXT (l->button));
}

static void
gail_link_button_link_init (GailLinkButtonLink *link)
{
  link->description = NULL;
}

static void
gail_link_button_link_finalize (GObject *obj)
{
  GailLinkButtonLink *link = (GailLinkButtonLink *)obj;

  g_free (link->description);

  G_OBJECT_CLASS (gail_link_button_link_parent_class)->finalize (obj);
}

static void
gail_link_button_link_class_init (GailLinkButtonLinkClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  AtkHyperlinkClass *hyperlink_class = ATK_HYPERLINK_CLASS (class);

  object_class->finalize = gail_link_button_link_finalize;

  hyperlink_class->get_uri = gail_link_button_link_get_uri;
  hyperlink_class->get_n_anchors = gail_link_button_link_get_n_anchors;
  hyperlink_class->is_valid = gail_link_button_link_is_valid;
  hyperlink_class->get_object = gail_link_button_link_get_object;
  hyperlink_class->get_start_index = gail_link_button_link_get_start_index;
  hyperlink_class->get_end_index = gail_link_button_link_get_end_index;
}

static gboolean
gail_link_button_link_do_action (AtkAction *action,
                                 gint       i)
{
  GailLinkButtonLink *link = (GailLinkButtonLink *)action;
  GtkWidget *widget;

  widget = GTK_WIDGET (link->button);
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  gtk_button_clicked (GTK_BUTTON (widget));

  return TRUE;
}

static gint
gail_link_button_link_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gail_link_button_link_get_name (AtkAction *action,
                                gint i)
{
  g_return_val_if_fail (i == 0, NULL);

  return "activate";
}

static const gchar *
gail_link_button_link_get_description (AtkAction *action,
                                       gint       i)
{
  GailLinkButtonLink *link = (GailLinkButtonLink *)action;

  g_return_val_if_fail (i == 0, NULL);

  return link->description;
}

static gboolean
gail_link_button_link_set_description (AtkAction   *action,
                                       gint         i,
                                       const gchar *description)
{
  GailLinkButtonLink *link = (GailLinkButtonLink *)action;

  g_return_val_if_fail (i == 0, FALSE);

  g_free (link->description);
  link->description = g_strdup (description);

  return TRUE;
}


static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_link_button_link_do_action;
  iface->get_n_actions = gail_link_button_link_get_n_actions;
  iface->get_name = gail_link_button_link_get_name;
  iface->get_description = gail_link_button_link_get_description;
  iface->set_description = gail_link_button_link_set_description;
}

static gboolean
activate_link (GtkLinkButton *button, AtkHyperlink *link)
{
  g_signal_emit_by_name (link, "link-activated");

  return FALSE;
}

static AtkHyperlink *
gail_link_button_get_hyperlink (AtkHyperlinkImpl *impl)
{
  GailLinkButton *button = GAIL_LINK_BUTTON (impl);

  if (!button->link)
    {
      button->link = g_object_new (gail_link_button_link_get_type (), NULL);
      g_signal_connect (gtk_accessible_get_widget (GTK_ACCESSIBLE (button)),
                        "activate-link", G_CALLBACK (activate_link), button->link);
    }

  return button->link;
}

static void atk_hypertext_impl_interface_init (AtkHyperlinkImplIface *iface);

G_DEFINE_TYPE_WITH_CODE (GailLinkButton, gail_link_button, GAIL_TYPE_BUTTON,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_HYPERLINK_IMPL, atk_hypertext_impl_interface_init))

static void
gail_link_button_init (GailLinkButton *button)
{
}

static void
gail_link_button_finalize (GObject *object)
{
  GailLinkButton *button = GAIL_LINK_BUTTON (object);

  if (button->link)
    g_object_unref (button->link);

  G_OBJECT_CLASS (gail_link_button_parent_class)->finalize (object);
}

static void
gail_link_button_class_init (GailLinkButtonClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gail_link_button_finalize;
}

static void
atk_hypertext_impl_interface_init (AtkHyperlinkImplIface *iface)
{
  iface->get_hyperlink = gail_link_button_get_hyperlink;
}
