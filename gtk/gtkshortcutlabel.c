/* gtkshortcutlabel.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutlabelprivate.h"
#include "gtklabel.h"
#include "gtkframe.h"
#include "gtkstylecontext.h"
#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkShortcutLabel
{
  GtkBox  parent_instance;
  gchar  *accelerator;
};

struct _GtkShortcutLabelClass
{
  GtkBoxClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutLabel, gtk_shortcut_label, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
gtk_shortcut_label_rebuild (GtkShortcutLabel *self)
{
  gchar **keys = NULL;
  gchar *label = NULL;
  GdkModifierType modifier = 0;
  guint key = 0;
  guint i;

  gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback)gtk_widget_destroy, NULL);

  if (self->accelerator == NULL)
    return;

  gtk_accelerator_parse (self->accelerator, &key, &modifier);
  if ((key == 0) && (modifier == 0))
    return;

  label = gtk_accelerator_get_label (key, modifier);
  if (label == NULL)
    return;

  keys = g_strsplit (label, "+", 0);
  for (i = 0; keys[i]; i++)
    {
      GtkFrame *frame;
      GtkLabel *disp;

      if (i > 0)
        {
          GtkLabel *plus;
          GtkStyleContext *context;

          plus = g_object_new (GTK_TYPE_LABEL,
                               "label", "+",
                               "visible", TRUE,
                               NULL);
          context = gtk_widget_get_style_context (GTK_WIDGET (plus));
          gtk_style_context_add_class (context, "dim-label");
          gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (plus));
        }

      frame = g_object_new (GTK_TYPE_FRAME,
                            "visible", TRUE,
                            NULL);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (frame));

      /*
       * FIXME: Check if the item is a modifier.
       *
       * If we have a size group, size everything the same except for the
       * last item. This has the side effect of basically matching all
       * modifiers together. Not always the case, but simple and easy
       * hack.
       */
      if (keys[i + 1] != NULL)
        gtk_widget_set_size_request (GTK_WIDGET (frame), 50, -1);

      disp = g_object_new (GTK_TYPE_LABEL,
                           "label", keys [i],
                           "visible", TRUE,
                           NULL);
      gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (disp));
    }

  g_strfreev (keys);
  g_free (label);
}

static void
gtk_shortcut_label_finalize (GObject *object)
{
  GtkShortcutLabel *self = (GtkShortcutLabel *)object;

  g_free (self->accelerator);

  G_OBJECT_CLASS (gtk_shortcut_label_parent_class)->finalize (object);
}

static void
gtk_shortcut_label_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkShortcutLabel *self = GTK_SHORTCUT_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_set_string (value, gtk_shortcut_label_get_accelerator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcut_label_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkShortcutLabel *self = GTK_SHORTCUT_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      gtk_shortcut_label_set_accelerator (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcut_label_class_init (GtkShortcutLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_shortcut_label_finalize;
  object_class->get_property = gtk_shortcut_label_get_property;
  object_class->set_property = gtk_shortcut_label_set_property;

  properties[PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", P_("Accelerator"), P_("Accelerator"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_shortcut_label_init (GtkShortcutLabel *self)
{
  gtk_box_set_spacing (GTK_BOX (self), 6);
}

GtkWidget *
gtk_shortcut_label_new (const gchar *accelerator)
{
  return g_object_new (GTK_TYPE_SHORTCUT_LABEL,
                       "accelerator", accelerator,
                       NULL);
}

const gchar *
gtk_shortcut_label_get_accelerator (GtkShortcutLabel *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_LABEL (self), NULL);

  return self->accelerator;
}

void
gtk_shortcut_label_set_accelerator (GtkShortcutLabel *self,
                                    const gchar      *accelerator)
{
  g_return_if_fail (GTK_IS_SHORTCUT_LABEL (self));

  if (g_strcmp0 (accelerator, self->accelerator) != 0)
    {
      g_free (self->accelerator);
      self->accelerator = g_strdup (accelerator);
      gtk_shortcut_label_rebuild (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCELERATOR]);
    }
}
