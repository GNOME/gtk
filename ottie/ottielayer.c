/*
 * Copyright © 2020 Benjamin Otte
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

#include "ottielayerprivate.h"

#include <glib/gi18n-lib.h>
#include <json-glib/json-glib.h>

/*
 * OttieLayer is the parent class for all kinds of layers
 * in Lottie animations.
 *
 * OttieComposition - a layer that contains other layers
 * OttieShapeLayer - a layer containing shapes
 * OttieNullLayer - a layer that does nothing
 *
 * Layers are organized in a tree (via composition layers),
 * and rendering an animation is just rendering all its layers.
 */

G_DEFINE_TYPE (OttieLayer, ottie_layer, OTTIE_TYPE_OBJECT)

static void
ottie_layer_default_update (OttieLayer *self,
                            GHashTable *compositions)
{
}

static void
ottie_layer_default_render (OttieLayer  *self,
                            OttieRender *render,
                            double       timestamp)
{
}

static void
ottie_layer_print (OttieObject  *obj,
                   OttiePrinter *printer)
{
  OttieLayer *self = OTTIE_LAYER (obj);

  OTTIE_OBJECT_CLASS (ottie_layer_parent_class)->print (obj, printer);

  if (self->auto_orient != 0)
    ottie_printer_add_int (printer, "ao", self->auto_orient);
  if (self->blend_mode != GSK_BLEND_MODE_DEFAULT)
    ottie_printer_add_int (printer, "bm", self->blend_mode);
  if (self->layer_name != NULL)
    ottie_printer_add_string (printer, "ln", self->layer_name);
  ottie_object_print (OTTIE_OBJECT (self->transform), "ks", printer);
  ottie_printer_add_double (printer, "ip", self->start_frame);
  ottie_printer_add_int (printer, "op", self->end_frame);
  if (self->index != OTTIE_INT_UNSET)
    ottie_printer_add_int (printer, "ind", self->index);
  if (self->parent_index != OTTIE_INT_UNSET)
    ottie_printer_add_int (printer, "parent", self->parent_index);
  ottie_printer_add_double (printer, "st", self->start_time);
  if (self->stretch != 1)
    ottie_printer_add_double (printer, "sr", self->stretch);
  ottie_printer_add_int (printer, "ddd", 0);
}

static void
ottie_layer_dispose (GObject *object)
{
  OttieLayer *self = OTTIE_LAYER (object);

  g_clear_object (&self->transform);
  g_clear_pointer (&self->layer_name, g_free);

  G_OBJECT_CLASS (ottie_layer_parent_class)->dispose (object);
}

static void
ottie_layer_class_init (OttieLayerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  OttieObjectClass *oobject_class = OTTIE_OBJECT_CLASS (klass);

  klass->update = ottie_layer_default_update;
  klass->render = ottie_layer_default_render;

  gobject_class->dispose = ottie_layer_dispose;

  oobject_class->print = ottie_layer_print;
}

static void
ottie_layer_init (OttieLayer *self)
{
  self->start_frame = -G_MAXDOUBLE;
  self->end_frame = G_MAXDOUBLE;
  self->stretch = 1;
  self->blend_mode = GSK_BLEND_MODE_DEFAULT;
  self->parent_index = OTTIE_INT_UNSET;
  self->index = OTTIE_INT_UNSET;
}

void
ottie_layer_update (OttieLayer *self,
                    GHashTable *compositions)
{
  OTTIE_LAYER_GET_CLASS (self)->update (self, compositions);
}

void
ottie_layer_render (OttieLayer  *self,
                    OttieRender *render,
                    double       timestamp)
{
  if (timestamp < self->start_frame ||
      timestamp > self->end_frame)
    return;

  timestamp -= self->start_time;
  timestamp /= self->stretch;

  ottie_render_start_object (render, OTTIE_OBJECT (self), timestamp);

  OTTIE_LAYER_GET_CLASS (self)->render (self, render, timestamp);

  ottie_render_end_object (render, OTTIE_OBJECT (self));
}
