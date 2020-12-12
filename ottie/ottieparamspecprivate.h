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

#ifndef __OTTIE_PARAM_SPEC_PRIVATE_H__
#define __OTTIE_PARAM_SPEC_PRIVATE_H__

#include <gsk/gsk.h>

#include "ottietypesprivate.h"

G_BEGIN_DECLS

#define OTTIE_TYPE_PARAM_SPEC                   (ottie_param_spec_get_type())
#define OTTIE_PARAM_SPEC(self)                  (G_TYPE_CHECK_INSTANCE_CAST ((self), OTTIE_TYPE_PARAM_SPEC, OttieParamSpec))
#define OTTIE_PARAM_SPEC_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), OTTIE_TYPE_PARAM_SPEC, OttieParamSpecClass))
#define OTTIE_IS_PARAM_SPEC(self)               (G_TYPE_CHECK_INSTANCE_TYPE ((self), OTTIE_TYPE_PARAM_SPEC))
#define OTTIE_IS_PARAM_SPEC_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), OTTIE_TYPE_PARAM_SPEC))
#define OTTIE_PARAM_SPEC_GET_CLASS(self)        (G_TYPE_INSTANCE_GET_CLASS ((self), OTTIE_TYPE_PARAM_SPEC, OttieParamSpecClass))

typedef struct _OttieParamSpec OttieParamSpec;
typedef struct _OttieParamSpecClass OttieParamSpecClass;

struct _OttieParamSpec
{
  GParamSpec parent;
};

struct _OttieParamSpecClass
{
  GParamSpecClass parent_class;

  void                  (* get_property)                        (OttieParamSpec         *pspec,
                                                                 GObject                *object,
                                                                 GValue                 *value);
  void                  (* set_property)                        (OttieParamSpec         *pspec,
                                                                 GObject                *object,
                                                                 const GValue           *value);
};

GType                   ottie_param_spec_get_type               (void) G_GNUC_CONST;

void                    ottie_param_spec_set_property           (GObject                *object,
                                                                 guint                   prop_id,
                                                                 const GValue           *value,
                                                                 GParamSpec             *pspec);
void                    ottie_param_spec_get_property           (GObject                *object,
                                                                 guint                   prop_id,
                                                                 GValue                 *value,
                                                                 GParamSpec             *pspec);

#define OTTIE_TYPE_PARAM_SPEC_STRING                   (ottie_param_spec_string_get_type())
#define OTTIE_PARAM_SPEC_STRING(self)                  (G_TYPE_CHECK_INSTANCE_CAST ((self), OTTIE_TYPE_PARAM_SPEC_STRING, OttieParamSpecString))
#define OTTIE_PARAM_SPEC_STRING_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), OTTIE_TYPE_PARAM_SPEC_STRING, OttieParamSpecStringClass))
#define OTTIE_IS_PARAM_SPEC_STRING(self)               (G_TYPE_CHECK_INSTANCE_TYPE ((self), OTTIE_TYPE_PARAM_SPEC_STRING))
#define OTTIE_IS_PARAM_SPEC_STRING_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), OTTIE_TYPE_PARAM_SPEC_STRING))
#define OTTIE_PARAM_SPEC_STRING_GET_CLASS(self)        (G_TYPE_INSTANCE_GET_CLASS ((self), OTTIE_TYPE_PARAM_SPEC_STRING, OttieParamSpecStringClass))
GType                   ottie_param_spec_string_get_type        (void) G_GNUC_CONST;

GParamSpec *            ottie_param_spec_string                 (GObjectClass           *klass,
                                                                 const char             *name,
                                                                 const char             *nick,
                                                                 const char             *blurb,
                                                                 const char             *default_value,
                                                                 const char             *(* getter) (gpointer),
                                                                 void                    (* setter) (gpointer, const char *));

G_END_DECLS

#endif /* __OTTIE_PARAM_SPEC_PRIVATE_H__ */
