/* gtkaccessiblehypertextprivate.h: Private definitions for GtkAccessibleHypertext
 *
 * SPDX-FileCopyrightText: 2025 Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gtk/gtkaccessible.h>
#include <gtk/gtkaccessibletext.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GTK_ACCESSIBLE_HYPERLINK_TYPE (gtk_accessible_hyperlink_get_type ())

G_DECLARE_FINAL_TYPE (GtkAccessibleHyperlink, gtk_accessible_hyperlink, GTK, ACCESSIBLE_HYPERLINK, GObject);

#define GTK_TYPE_ACCESSIBLE_HYPERTEXT (gtk_accessible_hypertext_get_type ())

G_DECLARE_INTERFACE (GtkAccessibleHypertext, gtk_accessible_hypertext, GTK, ACCESSIBLE_HYPERTEXT, GtkAccessible)

struct _GtkAccessibleHypertextInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/

  unsigned int (* get_n_links) (GtkAccessibleHypertext *self);

  GtkAccessibleHyperlink *
               (* get_link) (GtkAccessibleHypertext *self,
                             unsigned int            index);

  unsigned int (* get_link_at) (GtkAccessibleHypertext *self,
                                unsigned int            offset);
};

GtkAccessibleHyperlink * gtk_accessible_hyperlink_new (GtkAccessibleHypertext *parent,
                                                       unsigned int            index,
                                                       const char             *uri,
                                                       GtkAccessibleTextRange *bounds);

void gtk_accessible_hyperlink_set_platform_state (GtkAccessibleHyperlink     *self,
                                                  GtkAccessiblePlatformState  state,
                                                  gboolean                    enabled);

unsigned int gtk_accessible_hypertext_get_n_links (GtkAccessibleHypertext *self);
GtkAccessibleHyperlink *
             gtk_accessible_hypertext_get_link    (GtkAccessibleHypertext *self,
                                                   unsigned int            index);
unsigned int gtk_accessible_hypertext_get_link_at (GtkAccessibleHypertext *self,
                                                   unsigned int            offset);

unsigned int gtk_accessible_hyperlink_get_index   (GtkAccessibleHyperlink *self);
const char * gtk_accessible_hyperlink_get_uri     (GtkAccessibleHyperlink *self);
void         gtk_accessible_hyperlink_get_extents (GtkAccessibleHyperlink *self,
                                                   GtkAccessibleTextRange *bounds);

G_END_DECLS
