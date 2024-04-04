/* gtkaccesskitcontext.c: AccessKit GtkATContext implementation
 *
 * Copyright 2024  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkaccesskitcontextprivate.h"

#include "gtkaccessibleprivate.h"
#include "gtkaccesskitrootprivate.h"

#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtkeditable.h"
#include "gtkentryprivate.h"
#include "gtkroot.h"
#include "gtkstack.h"
#include "gtktextview.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"

#include <locale.h>

struct _GtkAccessKitContext
{
  GtkATContext parent_instance;

  /* Root object for the surface */
  GtkAccessKitRoot *root;

  /* The AccessKit node ID; meaningless if root is null */
  uint64_t node_id;
};

G_DEFINE_TYPE (GtkAccessKitContext, gtk_accesskit_context, GTK_TYPE_AT_CONTEXT)

static void
gtk_accesskit_context_unrealize (GtkATContext *context)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (context);
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);

  if (!self->root)
    return;

  GTK_DEBUG (A11Y, "Unrealizing AccessKit context for accessible '%s'",
                   G_OBJECT_TYPE_NAME (accessible));

  /* TODO: unregister from root? */

  g_clear_object (&self->root);
}

static void
gtk_accesskit_context_finalize (GObject *gobject)
{
  gtk_accesskit_context_unrealize (GTK_AT_CONTEXT (gobject));

  G_OBJECT_CLASS (gtk_accesskit_context_parent_class)->finalize (gobject);
}

static void
gtk_accesskit_context_realize (GtkATContext *context)
{
  /* TODO */
}

static void
gtk_accesskit_context_state_change (GtkATContext                *ctx,
                                    GtkAccessibleStateChange     changed_states,
                                    GtkAccessiblePropertyChange  changed_properties,
                                    GtkAccessibleRelationChange  changed_relations,
                                    GtkAccessibleAttributeSet   *states,
                                    GtkAccessibleAttributeSet   *properties,
                                    GtkAccessibleAttributeSet   *relations)
{
  /* TODO */
}

static void
gtk_accesskit_context_platform_change (GtkATContext                *ctx,
                                       GtkAccessiblePlatformChange  changed_platform)
{
  /* TODO */
}

static void
gtk_accesskit_context_bounds_change (GtkATContext *ctx)
{
  /* TODO */
}

static void
gtk_accesskit_context_child_change (GtkATContext             *ctx,
                                    GtkAccessibleChildChange  change,
                                    GtkAccessible            *child)
{
  /* TODO */
}

static void
gtk_accesskit_context_announce (GtkATContext                      *context,
                                const char                        *message,
                                GtkAccessibleAnnouncementPriority  priority)
{
  /* TODO */
}

static void
gtk_accesskit_context_update_caret_position (GtkATContext *context)
{
  /* TODO */
}

static void
gtk_accesskit_context_update_selection_bound (GtkATContext *context)
{
  /* TODO */
}

static void
gtk_accesskit_context_update_text_contents (GtkATContext *context,
                                            GtkAccessibleTextContentChange change,
                                            unsigned int start,
                                            unsigned int end)
{
  /* TODO */
}

static void
gtk_accesskit_context_class_init (GtkAccessKitContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  gobject_class->finalize = gtk_accesskit_context_finalize;

  context_class->realize = gtk_accesskit_context_realize;
  context_class->unrealize = gtk_accesskit_context_unrealize;
  context_class->state_change = gtk_accesskit_context_state_change;
  context_class->platform_change = gtk_accesskit_context_platform_change;
  context_class->bounds_change = gtk_accesskit_context_bounds_change;
  context_class->child_change = gtk_accesskit_context_child_change;
  context_class->announce = gtk_accesskit_context_announce;
  context_class->update_caret_position = gtk_accesskit_context_update_caret_position;
  context_class->update_selection_bound = gtk_accesskit_context_update_selection_bound;
  context_class->update_text_contents = gtk_accesskit_context_update_text_contents;
}

static void
gtk_accesskit_context_init (GtkAccessKitContext *self)
{
}

GtkATContext *
gtk_accesskit_create_context (GtkAccessibleRole  accessible_role,
                              GtkAccessible     *accessible,
                              GdkDisplay        *display)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), NULL);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return g_object_new (GTK_TYPE_ACCESSKIT_CONTEXT,
                       "accessible-role", accessible_role,
                       "accessible", accessible,
                       "display", display,
                       NULL);
}
