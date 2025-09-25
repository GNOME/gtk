/* gtkaccessiblehypertext.h: Interface for accessible objects containing links
 *
 * SPDX-FileCopyrightText: 2025  Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkaccessible.h>
#include <gtk/gtkaccessibletext.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GTK_ACCESSIBLE_HYPERLINK_TYPE (gtk_accessible_hyperlink_get_type ())

GDK_AVAILABLE_IN_4_22
G_DECLARE_FINAL_TYPE (GtkAccessibleHyperlink, gtk_accessible_hyperlink, GTK, ACCESSIBLE_HYPERLINK, GObject);

#define GTK_TYPE_ACCESSIBLE_HYPERTEXT (gtk_accessible_hypertext_get_type ())

/**
 * GtkAccessibleHypertext:
 *
 * An interface for accessible objects containing links.
 *
 * The `GtkAccessibleHypertext` interfaces is meant to be implemented by accessible
 * objects that contain links. Those links don't necessarily have to be part
 * of text, they can be associated with images and other things.
 *
 * Since: 4.22
 */
GDK_AVAILABLE_IN_4_22
G_DECLARE_INTERFACE (GtkAccessibleHypertext, gtk_accessible_hypertext, GTK, ACCESSIBLE_HYPERTEXT, GtkAccessible)

/**
 * GtkAccessibleHypertextInterface:
 *
 * The interface vtable for accessible objects containing links.
 *
 * Since: 4.22
 */
struct _GtkAccessibleHypertextInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/

  /**
   * GtkAccessibleHypertextInterface::get_n_links:
   * @self: the accessible object
   *
   * Retrieve the number of links in the accessible object.
   *
   * Returns: the number of links
   *
   * Since: 4.22
   */
  unsigned int (* get_n_links) (GtkAccessibleHypertext *self);

  /**
   * GtkAccessibleHypertextInterface::get_link:
   * @self: the accessible object
   * @index: the index of the link
   *
   * Retrieve the n-th link in the accessible object.
   *
   * @index must be smaller than the number of links.
   *
   * Returns: (transfer none): the link
   *
   * Since: 4.22
   */
  GtkAccessibleHyperlink *
               (* get_link) (GtkAccessibleHypertext *self,
                             unsigned int            index);

  /**
   * GtkAccessibleTextInterface::get_link_at:
   * @self: the accessible object
   * @offset: the character offset
   *
   * Retrieves the index of the link at the given character offset.
   *
   * Note that this method will return `G_MAXUINT` if the object
   * does not contain text.
   *
   * Returns: index of the link at the given character offset, or
   *   `G_MAXUINT` if there is no link
   *
   * Since: 4.22
   */
  unsigned int (* get_link_at) (GtkAccessibleHypertext *self,
                                unsigned int            offset);
};

GDK_AVAILABLE_IN_4_22
GtkAccessibleHyperlink * gtk_accessible_hyperlink_new (GtkAccessibleHypertext *parent,
                                                       unsigned int            index,
                                                       const char             *uri,
                                                       GtkAccessibleTextRange *bounds);

GDK_AVAILABLE_IN_4_22
void gtk_accessible_hyperlink_set_platform_state (GtkAccessibleHyperlink     *self,
                                                  GtkAccessiblePlatformState  state,
                                                  gboolean                    enabled);

G_END_DECLS
