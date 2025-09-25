/* gtkaccessiblehypertext.c: Interface for accessible objects with links
 *
 * SPDX-FileCopyrightText: 2025 Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkaccessiblehypertextprivate.h"

#include "gtkatcontextprivate.h"

G_DEFINE_INTERFACE (GtkAccessibleHypertext, gtk_accessible_hypertext, GTK_TYPE_ACCESSIBLE)

static unsigned int
gtk_accessible_hypertext_default_get_n_links (GtkAccessibleHypertext *self)
{
  g_warning ("GtkAccessibleHypertext::get_n_links not implemented for %s",
             G_OBJECT_TYPE_NAME (self));

  return 0;
}

static GtkAccessibleHyperlink *
gtk_accessible_hypertext_default_get_link (GtkAccessibleHypertext *self,
                                           unsigned int            index)
{
  g_warning ("GtkAccessibleHypertext::get_link not implemented for %s",
             G_OBJECT_TYPE_NAME (self));

  return NULL;
}

static unsigned int
gtk_accessible_hypertext_default_get_link_at (GtkAccessibleHypertext *self,
                                              unsigned int            offset)
{
  return G_MAXUINT;
}

static void
gtk_accessible_hypertext_default_init (GtkAccessibleHypertextInterface *iface)
{
  iface->get_n_links = gtk_accessible_hypertext_default_get_n_links;
  iface->get_link = gtk_accessible_hypertext_default_get_link;
  iface->get_link_at = gtk_accessible_hypertext_default_get_link_at;
}

/*< private >
 * gtk_accessible_hypertext_get_n_links:
 * @self: the accessible object
 *
 * Retrieve the number of links in the object.
 *
 * Returns: the number of links
 *
 * Since: 4.22
 */
unsigned int
gtk_accessible_hypertext_get_n_links (GtkAccessibleHypertext *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_HYPERTEXT (self), 0);

  return GTK_ACCESSIBLE_HYPERTEXT_GET_IFACE (self)->get_n_links (self);
}

/*< private >
 * gtk_accessible_hypertext_get_link:
 * @self: the accessible object
 * @index: the index of the link to retrieve
 *
 * Retrieve the n-th link of the object.
 *
 * Returns: the link
 *
 * Since: 4.22
 */
GtkAccessibleHyperlink *
gtk_accessible_hypertext_get_link (GtkAccessibleHypertext *self,
                                   unsigned int            index)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_HYPERTEXT (self), NULL);

  return GTK_ACCESSIBLE_HYPERTEXT_GET_IFACE (self)->get_link (self, index);
}

/*< private >
 * gtk_accessible_hypertext_get_link_at:
 * @self: the accessible object
 * @offset: the character offset
 *
 * Retrieve the index of the ink at the given character offset.
 *
 * Returns: the index, or `G_MAXUINT`
 *
 * Since: 4.22
 */
unsigned int
gtk_accessible_hypertext_get_link_at (GtkAccessibleHypertext *self,
                                      unsigned int            offset)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_HYPERTEXT (self), G_MAXUINT);

  return GTK_ACCESSIBLE_HYPERTEXT_GET_IFACE (self)->get_link_at (self, offset);
}

/* {{{ GtkAccessibleHyperlink */

enum {
  PROP_ACCESSIBLE_ROLE = 1,
};

/**
 * GtkAccessibleHyperlink:
 *
 * Represents a link (i.e. a uri).
 *
 * A widget that contains one or more links should implement
 * the [iface@Gtk.AccessibleHypertext] interface and return
 * `GtkAccessibleHyperlink] objects for each of the links.
 *
 * Since: 4.22
 */
struct _GtkAccessibleHyperlink
{
  GObject parent_instance;

  GtkATContext *at_context;

  GtkAccessibleHypertext *parent;
  unsigned int index;

  char *uri;
  gsize start;
  gsize length;

  unsigned int platform_state;
};

static GtkATContext *
gtk_accessible_hyperlink_get_at_context (GtkAccessible *accessible)
{
  GtkAccessibleHyperlink *self = GTK_ACCESSIBLE_HYPERLINK (accessible);

  if (self->at_context == NULL)
    {
      GtkAccessibleRole role = GTK_ACCESSIBLE_ROLE_LINK;
      GdkDisplay *display;

      display = gdk_display_get_default ();

      self->at_context = gtk_at_context_create (role, accessible, display);
      if (self->at_context == NULL)
        return NULL;
    }

  return g_object_ref (self->at_context);
}

static gboolean
gtk_accessible_hyperlink_get_platform_state (GtkAccessible              *accessible,
                                             GtkAccessiblePlatformState  state)
{
  GtkAccessibleHyperlink *self = GTK_ACCESSIBLE_HYPERLINK (accessible);

  return (self->platform_state & (1 << state)) != 0;
}

static GtkAccessible *
gtk_accessible_hyperlink_get_accessible_parent (GtkAccessible *accessible)
{
  GtkAccessibleHyperlink *self = GTK_ACCESSIBLE_HYPERLINK (accessible);

  if (self->parent == NULL)
    return NULL;

  return g_object_ref (GTK_ACCESSIBLE (self->parent));
}

static GtkAccessible *
gtk_accessible_hyperlink_get_first_accessible_child (GtkAccessible *accessible)
{
  return NULL;
}

static GtkAccessible *
gtk_accessible_hyperlink_get_next_accessible_sibling (GtkAccessible *accessible)
{
  GtkAccessibleHyperlink *self = GTK_ACCESSIBLE_HYPERLINK (accessible);

  if (self->index + 1 < gtk_accessible_hypertext_get_n_links (self->parent))
    return g_object_ref (GTK_ACCESSIBLE (gtk_accessible_hypertext_get_link (self->parent, self->index + 1)));

  return NULL;
}

static gboolean
gtk_accessible_hyperlink_get_bounds (GtkAccessible *accessible,
                                     int           *x,
                                     int           *y,
                                     int           *width,
                                     int           *height)
{
  return FALSE;
}

static void
gtk_accessible_hyperlink_accessible_init (GtkAccessibleInterface *iface)
{
  iface->get_at_context = gtk_accessible_hyperlink_get_at_context;
  iface->get_platform_state = gtk_accessible_hyperlink_get_platform_state;
  iface->get_accessible_parent = gtk_accessible_hyperlink_get_accessible_parent;
  iface->get_first_accessible_child = gtk_accessible_hyperlink_get_first_accessible_child;
  iface->get_next_accessible_sibling = gtk_accessible_hyperlink_get_next_accessible_sibling;
  iface->get_bounds = gtk_accessible_hyperlink_get_bounds;
}

G_DEFINE_TYPE_WITH_CODE (GtkAccessibleHyperlink, gtk_accessible_hyperlink, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                gtk_accessible_hyperlink_accessible_init))

static void
gtk_accessible_hyperlink_init (GtkAccessibleHyperlink *self)
{
}

static void
gtk_accessible_hyperlink_dispose (GObject *object)
{
  GtkAccessibleHyperlink *self = GTK_ACCESSIBLE_HYPERLINK (object);

  g_clear_object (&self->at_context);

  G_OBJECT_CLASS (gtk_accessible_hyperlink_parent_class)->dispose (object);
}

static void
gtk_accessible_hyperlink_finalize (GObject *object)
{
  GtkAccessibleHyperlink *self = GTK_ACCESSIBLE_HYPERLINK (object);

  g_free (self->uri);

  G_OBJECT_CLASS (gtk_accessible_hyperlink_parent_class)->finalize (object);
}

static void
gtk_accessible_hyperlink_get_property (GObject      *object,
                                       unsigned int  prop_id,
                                       GValue       *value,
                                       GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_ACCESSIBLE_ROLE:
      g_value_set_enum (value, GTK_ACCESSIBLE_ROLE_LINK);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accessible_hyperlink_set_property (GObject      *object,
                                       unsigned int  prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
}

static void
gtk_accessible_hyperlink_class_init (GtkAccessibleHyperlinkClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_accessible_hyperlink_dispose;
  object_class->finalize = gtk_accessible_hyperlink_finalize;
  object_class->get_property = gtk_accessible_hyperlink_get_property;
  object_class->set_property = gtk_accessible_hyperlink_set_property;

  g_object_class_override_property (object_class, PROP_ACCESSIBLE_ROLE, "accessible-role");
}

/**
 * gtk_accessible_hyperlink_new:
 * @parent: the parent
 * @index: the index of this link in the parent
 * @uri: the uri
 * @bounds: the text range that the link occupies (or 0, 0)
 *
 * Creates an accessible object that represents a hyperlink.
 *
 * This is meant to be used with an implementation of the
 * [iface@Gtk.AccessibleHypertext] interface.
 *
 * Since: 4.22
 */
GtkAccessibleHyperlink *
gtk_accessible_hyperlink_new (GtkAccessibleHypertext *parent,
                              unsigned int            index,
                              const char             *uri,
                              GtkAccessibleTextRange *bounds)
{
  GtkAccessibleHyperlink *self;

  self = g_object_new (GTK_ACCESSIBLE_HYPERLINK_TYPE, NULL);

  self->parent = parent;
  self->index = index;

  self->uri = g_strdup (uri);
  self->start = bounds->start;
  self->length = bounds->length;

  return self;
}

/**
 * gtk_accessible_hyperlink_set_platform_state:
 * @self: the accessible
 * @state: the platform state to change
 * @enabled: the new value for the platform state
 *
 * Sets a platform state on the accessible.
 *
 * Since: 4.22
 */
void
gtk_accessible_hyperlink_set_platform_state (GtkAccessibleHyperlink     *self,
                                             GtkAccessiblePlatformState  state,
                                             gboolean                    enabled)
{
  if (enabled)
    self->platform_state |= 1 << state;
  else
    self->platform_state ^= 1 << state;

  gtk_accessible_update_platform_state (GTK_ACCESSIBLE (self), state);
}

unsigned int
gtk_accessible_hyperlink_get_index (GtkAccessibleHyperlink *self)
{
  return self->index;
}

const char *
gtk_accessible_hyperlink_get_uri (GtkAccessibleHyperlink *self)
{
  return self->uri;
}

void
gtk_accessible_hyperlink_get_extents (GtkAccessibleHyperlink *self,
                                      GtkAccessibleTextRange *bounds)
{
  bounds->start = self->start;
  bounds->length = self->length;
}

/* }}} */

/* vim:set foldmethod=marker: */
