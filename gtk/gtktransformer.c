/*
 * Copyright © 2019 Timm Bäder
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtktransformer.h"


/**
 * ──────────▄▄▄▄▄▄▄▄▄──────────
 * ───────▄█████████████▄───────
 *▐███▌─█████████████████─▐███▌
 *─████▄─▀███▄─────▄███▀─▄████─
 *─▐█████▄─▀███▄─▄███▀─▄█████▌─
 *──██▄▀███▄─▀█████▀─▄███▀▄██──
 *──▐█▀█▄▀███─▄─▀─▄─███▀▄█▀█▌──
 *───██▄▀█▄██─██▄██─██▄█▀▄██───
 *────▀██▄▀██─█████─██▀▄██▀────
 *───▄──▀████─█████─████▀──▄───
 *───██────────███────────██───
 *───██▄────▄█─███─█▄────▄██───
 *───████─▄███─███─███▄─████───
 *───████─████─███─████─████───
 *───████─████─███─████─████───
 *───████─████▄▄▄▄▄████─████───
 *───▀███─█████████████─███▀───
 *─────▀█─███─▄▄▄▄▄─███─█▀─────
 *────────▀█▌▐█████▌▐█▀────────
 *───────────███████───────────
 */
struct _GtkTransformer
{
  GtkWidget parent_instance;

  graphene_matrix_t child_transform;
};

G_DEFINE_TYPE (GtkTransformer, gtk_transformer, GTK_TYPE_WIDGET);

static void
gtk_transformer_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    gtk_widget_measure (child,
                        orientation,
                        for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gtk_transformer_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  GtkTransformer *self = GTK_TRANSFORMER (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    gtk_widget_size_allocate_transformed (child,
                                          width,
                                          height,
                                          baseline,
                                          &self->child_transform);
}

static void
gtk_transformer_dispose (GObject *object)
{
  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (object));

  g_clear_pointer (&child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_transformer_parent_class)->dispose (object);
}

static void
gtk_transformer_class_init (GtkTransformerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_transformer_dispose;

  widget_class->measure = gtk_transformer_measure;
  widget_class->size_allocate = gtk_transformer_size_allocate;
}

static void
gtk_transformer_init (GtkTransformer *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

  graphene_matrix_init_identity (&self->child_transform);
}

GtkWidget *
gtk_transformer_new (GtkWidget *child)
{
  GtkTransformer *self = GTK_TRANSFORMER (g_object_new (GTK_TYPE_TRANSFORMER, NULL));

  gtk_widget_set_parent (child, GTK_WIDGET (self));

  return GTK_WIDGET (self);
}

void
gtk_transformer_set_transform (GtkTransformer          *self,
                               const graphene_matrix_t *transform)
{
  g_return_if_fail (GTK_IS_TRANSFORMER (self));

  graphene_matrix_init_from_matrix (&self->child_transform, transform);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}
