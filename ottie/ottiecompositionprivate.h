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

#ifndef __OTTIE_COMPOSITION_PRIVATE_H__
#define __OTTIE_COMPOSITION_PRIVATE_H__

#include "ottielayerprivate.h"

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_COMPOSITION         (ottie_composition_get_type ())
#define OTTIE_COMPOSITION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_COMPOSITION, OttieComposition))
#define OTTIE_COMPOSITION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_COMPOSITION, OttieCompositionClass))
#define OTTIE_IS_COMPOSITION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_COMPOSITION))
#define OTTIE_IS_COMPOSITION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_COMPOSITION))
#define OTTIE_COMPOSITION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_COMPOSITION, OttieCompositionClass))

typedef struct _OttieComposition OttieComposition;
typedef struct _OttieCompositionClass OttieCompositionClass;

GType                   ottie_composition_get_type          (void) G_GNUC_CONST;

gboolean                ottie_composition_parse_layers      (JsonReader                 *reader,
                                                             gsize                       offset,
                                                             gpointer                    data);

G_END_DECLS

#endif /* __OTTIE_COMPOSITION_PRIVATE_H__ */
