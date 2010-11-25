/* gtkcellareacontext.c
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkcellareacontext.h"
#include "gtkprivate.h"

/* GObjectClass */
static void      gtk_cell_area_context_dispose                        (GObject            *object);
static void      gtk_cell_area_context_get_property                   (GObject            *object,
								       guint               prop_id,
								       GValue             *value,
								       GParamSpec         *pspec);
static void      gtk_cell_area_context_set_property                   (GObject            *object,
								       guint               prop_id,
								       const GValue       *value,
								       GParamSpec         *pspec);

/* GtkCellAreaContextClass */
static void      gtk_cell_area_context_real_flush_preferred_width            (GtkCellAreaContext *context);
static void      gtk_cell_area_context_real_flush_preferred_height           (GtkCellAreaContext *context);
static void      gtk_cell_area_context_real_flush_allocation                 (GtkCellAreaContext *context);
static void      gtk_cell_area_context_real_allocate_width                   (GtkCellAreaContext *context,
									      gint                width);
static void      gtk_cell_area_context_real_allocate_height                  (GtkCellAreaContext *context,
									      gint                height);

struct _GtkCellAreaContextPrivate
{
  GtkCellArea *cell_area;

  gint         min_width;
  gint         nat_width;
  gint         min_height;
  gint         nat_height;
  gint         alloc_width;
  gint         alloc_height;
};

enum {
  PROP_0,
  PROP_CELL_AREA,
  PROP_MIN_WIDTH,
  PROP_NAT_WIDTH,
  PROP_MIN_HEIGHT,
  PROP_NAT_HEIGHT
};

G_DEFINE_TYPE (GtkCellAreaContext, gtk_cell_area_context, G_TYPE_OBJECT);

static void
gtk_cell_area_context_init (GtkCellAreaContext *context)
{
  GtkCellAreaContextPrivate *priv;

  context->priv = G_TYPE_INSTANCE_GET_PRIVATE (context,
					       GTK_TYPE_CELL_AREA_CONTEXT,
					       GtkCellAreaContextPrivate);
  priv = context->priv;

  priv->min_width  = -1;
  priv->nat_width  = -1;
  priv->min_height = -1;
  priv->nat_height = -1;
}

static void 
gtk_cell_area_context_class_init (GtkCellAreaContextClass *class)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (class);

  /* GObjectClass */
  object_class->dispose      = gtk_cell_area_context_dispose;
  object_class->get_property = gtk_cell_area_context_get_property;
  object_class->set_property = gtk_cell_area_context_set_property;

  /* GtkCellAreaContextClass */
  class->flush_preferred_width            = gtk_cell_area_context_real_flush_preferred_width;
  class->flush_preferred_height           = gtk_cell_area_context_real_flush_preferred_height;
  class->flush_allocation                 = gtk_cell_area_context_real_flush_allocation;

  class->sum_preferred_width   = NULL;
  class->sum_preferred_height  = NULL;

  class->allocate_width  = gtk_cell_area_context_real_allocate_width;
  class->allocate_height = gtk_cell_area_context_real_allocate_height;

  g_object_class_install_property (object_class,
                                   PROP_CELL_AREA,
                                   g_param_spec_object ("area",
							P_("Area"),
							P_("The Cell Area this context was created for"),
							GTK_TYPE_CELL_AREA,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_MIN_WIDTH,
                                   g_param_spec_int ("minimum-width",
						     P_("Minimum Width"),
						     P_("Minimum cached width"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_NAT_WIDTH,
                                   g_param_spec_int ("natural-width",
						     P_("Minimum Width"),
						     P_("Minimum cached width"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_MIN_HEIGHT,
                                   g_param_spec_int ("minimum-height",
						     P_("Minimum Height"),
						     P_("Minimum cached height"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_NAT_HEIGHT,
                                   g_param_spec_int ("natural-height",
						     P_("Minimum Height"),
						     P_("Minimum cached height"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_type_class_add_private (object_class, sizeof (GtkCellAreaContextPrivate));
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_context_dispose (GObject *object)
{
  GtkCellAreaContext        *context = GTK_CELL_AREA_CONTEXT (object);
  GtkCellAreaContextPrivate *priv = context->priv;

  if (priv->cell_area)
    {
      g_object_unref (priv->cell_area);

      priv->cell_area = NULL;
    }

  G_OBJECT_CLASS (gtk_cell_area_context_parent_class)->dispose (object);
}

static void
gtk_cell_area_context_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  GtkCellAreaContext        *context = GTK_CELL_AREA_CONTEXT (object);
  GtkCellAreaContextPrivate *priv = context->priv;

  switch (prop_id)
    {
    case PROP_CELL_AREA:
      priv->cell_area = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_area_context_get_property (GObject     *object,
				    guint        prop_id,
				    GValue      *value,
				    GParamSpec  *pspec)
{
  GtkCellAreaContext        *context = GTK_CELL_AREA_CONTEXT (object);
  GtkCellAreaContextPrivate *priv = context->priv;

  switch (prop_id)
    {
    case PROP_CELL_AREA:
      g_value_set_object (value, priv->cell_area);
      break;
    case PROP_MIN_WIDTH:
      g_value_set_int (value, priv->min_width);
      break;
    case PROP_NAT_WIDTH:
      g_value_set_int (value, priv->nat_width);
      break;
    case PROP_MIN_HEIGHT:
      g_value_set_int (value, priv->min_height);
      break;
    case PROP_NAT_HEIGHT:
      g_value_set_int (value, priv->nat_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*************************************************************
 *                    GtkCellAreaContextClass                   *
 *************************************************************/
static void
gtk_cell_area_context_real_flush_preferred_width (GtkCellAreaContext *context)
{
  GtkCellAreaContextPrivate *priv = context->priv;
  
  priv->min_width = -1;
  priv->nat_width = -1;

  g_object_freeze_notify (G_OBJECT (context));
  g_object_notify (G_OBJECT (context), "minimum-width");
  g_object_notify (G_OBJECT (context), "natural-width");
  g_object_thaw_notify (G_OBJECT (context));
}

static void
gtk_cell_area_context_real_flush_preferred_height (GtkCellAreaContext *context)
{
  GtkCellAreaContextPrivate *priv = context->priv;
  
  priv->min_height = -1;
  priv->nat_height = -1;

  g_object_freeze_notify (G_OBJECT (context));
  g_object_notify (G_OBJECT (context), "minimum-height");
  g_object_notify (G_OBJECT (context), "natural-height");
  g_object_thaw_notify (G_OBJECT (context));
}

static void
gtk_cell_area_context_real_flush_allocation (GtkCellAreaContext *context)
{
  GtkCellAreaContextPrivate *priv = context->priv;

  priv->alloc_width  = 0;
  priv->alloc_height = 0;
}

static void
gtk_cell_area_context_real_allocate_width (GtkCellAreaContext *context,
					   gint                width)
{
  GtkCellAreaContextPrivate *priv = context->priv;

  priv->alloc_width = width;
}

static void
gtk_cell_area_context_real_allocate_height (GtkCellAreaContext *context,
					    gint                height)
{
  GtkCellAreaContextPrivate *priv = context->priv;

  priv->alloc_height = height;
}


/*************************************************************
 *                            API                            *
 *************************************************************/
GtkCellArea *
gtk_cell_area_context_get_area (GtkCellAreaContext *context)
{
  GtkCellAreaContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA_CONTEXT (context), NULL);

  priv = context->priv;

  return priv->cell_area;
}

void
gtk_cell_area_context_flush (GtkCellAreaContext *context)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  gtk_cell_area_context_flush_preferred_width (context);
  gtk_cell_area_context_flush_preferred_height (context);
  gtk_cell_area_context_flush_allocation (context);
}

void
gtk_cell_area_context_flush_preferred_width (GtkCellAreaContext *context)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->flush_preferred_width (context);
}

void
gtk_cell_area_context_flush_preferred_height (GtkCellAreaContext *context)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->flush_preferred_height (context);
}

void
gtk_cell_area_context_flush_allocation (GtkCellAreaContext *context)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->flush_allocation (context);
}

void
gtk_cell_area_context_sum_preferred_width (GtkCellAreaContext *context)
{
  GtkCellAreaContextClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  class = GTK_CELL_AREA_CONTEXT_GET_CLASS (context);

  if (class->sum_preferred_width)
    class->sum_preferred_width (context);
}

void
gtk_cell_area_context_sum_preferred_height (GtkCellAreaContext *context)
{
  GtkCellAreaContextClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  class = GTK_CELL_AREA_CONTEXT_GET_CLASS (context);

  if (class->sum_preferred_height)
    class->sum_preferred_height (context);
}

void
gtk_cell_area_context_allocate_width (GtkCellAreaContext *context,
				      gint                width)
{
  GtkCellAreaContextClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  class = GTK_CELL_AREA_CONTEXT_GET_CLASS (context);

  class->allocate_width (context, width);
}

void
gtk_cell_area_context_allocate_height (GtkCellAreaContext *context,
				       gint                height)
{
  GtkCellAreaContextClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  class = GTK_CELL_AREA_CONTEXT_GET_CLASS (context);

  class->allocate_height (context, height);
}

void
gtk_cell_area_context_get_preferred_width (GtkCellAreaContext *context,
					   gint               *minimum_width,
					   gint               *natural_width)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  if (minimum_width)
    *minimum_width = priv->min_width;

  if (natural_width)
    *natural_width = priv->nat_width;
}

void
gtk_cell_area_context_get_preferred_height (GtkCellAreaContext *context,
					    gint               *minimum_height,
					    gint               *natural_height)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  if (minimum_height)
    *minimum_height = priv->min_height;

  if (natural_height)
    *natural_height = priv->nat_height;
}

void
gtk_cell_area_context_get_allocation (GtkCellAreaContext *context,
				      gint               *width,
				      gint               *height)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  if (width)
    *width = priv->alloc_width;

  if (height)
    *height = priv->alloc_height;
}

void
gtk_cell_area_context_push_preferred_width (GtkCellAreaContext *context,
					    gint                minimum_width,
					    gint                natural_width)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  g_object_freeze_notify (G_OBJECT (context));

  if (minimum_width > priv->min_width)
    {
      priv->min_width = minimum_width;

      g_object_notify (G_OBJECT (context), "minimum-width");
    }

  if (natural_width > priv->nat_width)
    {
      priv->nat_width = natural_width;

      g_object_notify (G_OBJECT (context), "natural-width");
    }

  g_object_thaw_notify (G_OBJECT (context));
}

void
gtk_cell_area_context_push_preferred_height (GtkCellAreaContext *context,
					     gint                minimum_height,
					     gint                natural_height)
{
  GtkCellAreaContextPrivate *priv;
  
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  g_object_freeze_notify (G_OBJECT (context));

  if (minimum_height > priv->min_height)
    {
      priv->min_height = minimum_height;

      g_object_notify (G_OBJECT (context), "minimum-height");
    }

  if (natural_height > priv->nat_height)
    {
      priv->nat_height = natural_height;

      g_object_notify (G_OBJECT (context), "natural-height");
    }

  g_object_thaw_notify (G_OBJECT (context));
}
