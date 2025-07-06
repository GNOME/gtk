/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <gio/gunixinputstream.h>
#endif
#include "gtk-path-tool.h"

#include <glib/gi18n-lib.h>

GskPath *
get_path (const char *arg)
{
  char *buffer = NULL;
  gsize len;
  GError *error = NULL;
  GskPath *path;

  if (arg[0] == '.' || arg[0] == '/')
    {
      if (!g_file_get_contents (arg, &buffer, &len, &error))
        {
          g_printerr ("%s\n", error->message);
          exit (1);
        }
    }
#ifdef G_OS_UNIX
  else if (strcmp (arg, "-") == 0)
    {
      GInputStream *in;
      GOutputStream *out;

      in = g_unix_input_stream_new (0, FALSE);
      out = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

      if (g_output_stream_splice (out, in, 0, NULL, &error) < 0)
        {
          g_printerr (_("Failed to read from standard input: %s\n"), error->message);
          exit (1);
        }

      if (!g_output_stream_close (out, NULL, &error))
        {
          g_printerr (_("Error reading from standard input: %s\n"), error->message);
          exit (1);
        }

      buffer = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (out));

      g_object_unref (out);
      g_object_unref (in);
    }
#endif
  else
    buffer = g_strdup (arg);

  g_strstrip (buffer);

  path = gsk_path_parse (buffer);

  if (path == NULL)
    {
      g_printerr (_("Failed to parse '%s' as path.\n"), arg);
      exit (1);
    }

  g_free (buffer);

  return path;
}

int
get_enum_value (GType       type,
                const char *type_nick,
                const char *str)
{
  GEnumClass *class = g_type_class_ref (type);
  GEnumValue *value;
  int val;

  value = g_enum_get_value_by_nick (class, str);
  if (value)
    val = value->value;
  else
    {
      GString *s;

      s = g_string_new ("");
      g_string_append_printf (s, _("Failed to parse '%s' as %s."), str, type_nick);
      g_string_append (s, "\n");
      g_string_append (s, _("Possible values: "));

      for (unsigned int i = 0; i < class->n_values; i++)
        {
          if (i > 0)
            g_string_append (s, ", ");
          g_string_append (s, class->values[i].value_nick);
        }
      g_printerr ("%s\n", s->str);
      g_string_free (s, TRUE);
      exit (1);
    }

  g_type_class_unref (class);

  return val;
}

void
get_color (GdkRGBA    *rgba,
           const char *str)
{
  if (!gdk_rgba_parse (rgba, str))
    {
      char *msg = g_strdup_printf (_("Could not parse '%s' as color"), str);
      g_printerr ("%s\n", msg);
      exit (1);
    }
}

void
_gsk_stroke_set_dashes (GskStroke  *stroke,
                        const char *dashes)
{
  GArray *d;
  char **strings;

  if (!dashes)
    return;

  d = g_array_new (FALSE, FALSE, sizeof (float));
  strings = g_strsplit (dashes, ",", 0);

  for (unsigned int i = 0; strings[i]; i++)
    {
      char *end = NULL;
      float f;

      f = (float) g_ascii_strtod (strings[i], &end);

      if (*end != '\0')
        {
          char *msg = g_strdup_printf (_("Failed to parse '%s' as number"), strings[i]);
          g_printerr ("%s\n", msg);
          exit (1);
        }

      g_array_append_val (d, f);
    }

  g_strfreev (strings);

  gsk_stroke_set_dash (stroke, (const float *)d->data, d->len);

  g_array_unref (d);
}

typedef struct
{
  GskPathBuilder *scaled_builder;
  GskPathBuilder *line_builder;
  GskPathBuilder *point_builder;
  gboolean points;
  gboolean controls;
  double zoom;
} ControlData;

static gboolean
collect_cb (GskPathOperation        op,
            const graphene_point_t *orig_pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                data)
{
  ControlData *cd = data;
  graphene_point_t pts[4];

  for (int i = 0; i < n_pts; i++)
    {
      pts[i].x = cd->zoom * orig_pts[i].x;
      pts[i].y = cd->zoom * orig_pts[i].y;
    }

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (cd->scaled_builder, pts[0].x, pts[0].y);
      if (cd->points)
        gsk_path_builder_add_circle (cd->point_builder, &pts[0], 4);
      if (cd->controls)
        gsk_path_builder_move_to (cd->line_builder, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_LINE:
    case GSK_PATH_CLOSE:
      gsk_path_builder_line_to (cd->scaled_builder, pts[1].x, pts[1].y);
      if (cd->points)
        gsk_path_builder_add_circle (cd->point_builder, &pts[1], 4);
      if (cd->controls)
        gsk_path_builder_line_to (cd->line_builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      if (op == GSK_PATH_QUAD)
        gsk_path_builder_quad_to (cd->scaled_builder, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
      else
        gsk_path_builder_conic_to (cd->scaled_builder, pts[1].x, pts[1].y, pts[2].x, pts[2].y, weight);
      if (cd->points)
        {
          gsk_path_builder_add_circle (cd->point_builder, &pts[2], 4);
        }
      if (cd->controls)
        {
          gsk_path_builder_add_circle (cd->point_builder, &pts[1], 3);
          gsk_path_builder_line_to (cd->line_builder, pts[1].x, pts[1].y);
          gsk_path_builder_line_to (cd->line_builder, pts[2].x, pts[2].y);
        }
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (cd->scaled_builder, pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
      if (cd->points)
        {
          gsk_path_builder_add_circle (cd->point_builder, &pts[3], 4);
        }
      if (cd->controls)
        {
          gsk_path_builder_add_circle (cd->point_builder, &pts[1], 3);
          gsk_path_builder_add_circle (cd->point_builder, &pts[2], 3);
          gsk_path_builder_line_to (cd->line_builder, pts[1].x, pts[1].y);
          gsk_path_builder_line_to (cd->line_builder, pts[2].x, pts[2].y);
          gsk_path_builder_line_to (cd->line_builder, pts[3].x, pts[3].y);
        }
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

void
collect_render_data (GskPath   *path,
                     gboolean   points,
                     gboolean   controls,
                     double     zoom,
                     GskPath  **scaled_path,
                     GskPath  **line_path,
                     GskPath  **point_path)
{
  ControlData cd;

  memset (&cd, 0, sizeof (ControlData));

  cd.points = points;
  cd.controls = controls;
  cd.zoom = zoom;

  cd.scaled_builder = gsk_path_builder_new ();
  if (controls)
    {
      cd.line_builder = gsk_path_builder_new ();
      cd.point_builder = gsk_path_builder_new ();
    }
  else if (points)
    {
      cd.point_builder = gsk_path_builder_new ();
    }

  gsk_path_foreach (path, -1, collect_cb, &cd);

  *scaled_path = gsk_path_builder_free_to_path (cd.scaled_builder);
  if (cd.line_builder)
    *line_path = gsk_path_builder_free_to_path (cd.line_builder);
  else
    *line_path = NULL;
  if (cd.point_builder)
    *point_path = gsk_path_builder_free_to_path (cd.point_builder);
  else
    *point_path = NULL;
}

typedef struct
{
  GskPathBuilder *builder;
  double zoom;
} ScaleData;

static gboolean
scale_op (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_pts,
          float                   weight,
          gpointer                user_data)
{
  ScaleData *d = user_data;
  graphene_point_t sp[4];

  for (int i = 0; i < n_pts; i++)
    {
      sp[i].x = pts[i].x * d->zoom;
      sp[i].y = pts[i].y * d->zoom;
    }

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (d->builder, sp[0].x, sp[0].y);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (d->builder);
      break;
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (d->builder, sp[1].x, sp[1].y);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (d->builder,
                                sp[1].x, sp[1].y,
                                sp[2].x, sp[2].y);
      break;
    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (d->builder,
                                 sp[1].x, sp[1].y,
                                 sp[2].x, sp[2].y,
                                 sp[3].x, sp[3].y);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (d->builder,
                                 sp[1].x, sp[1].y,
                                 sp[2].x, sp[2].y,
                                 weight);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static GskPath *
scale_path (GskPath *path, double zoom)
{
  ScaleData d;

  d.builder = gsk_path_builder_new ();
  d.zoom = zoom;

  gsk_path_foreach (path, GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CUBIC|GSK_PATH_FOREACH_ALLOW_CONIC, scale_op, &d);

  gsk_path_unref (path);

  return gsk_path_builder_free_to_path (d.builder);
}

typedef struct {
  GskPathBuilder *line_builder;
  GskPathBuilder *point_builder;
  GskPathPoint start;
  int segment;
  double zoom;
} IntersectionData;

static gboolean
intersection_cb (GskPath             *path1,
                 const GskPathPoint  *point1,
                 GskPath             *path2,
                 const GskPathPoint  *point2,
                 GskPathIntersection  kind,
                 gpointer             data)
{
  IntersectionData *id = data;
  graphene_point_t pos;

  switch (kind)
    {
    case GSK_PATH_INTERSECTION_NORMAL:
      gsk_path_point_get_position (point1, path1, &pos);
      pos.x = id->zoom * pos.x;
      pos.y = id->zoom * pos.y;
      gsk_path_builder_add_circle (id->point_builder, &pos, 3);
      break;

    case GSK_PATH_INTERSECTION_START:
      if (id->segment == 0)
        id->start = *point1;
      id->segment++;
      break;

    case GSK_PATH_INTERSECTION_END:
      id->segment--;
      if (id->segment == 0)
        gsk_path_builder_add_segment (id->line_builder, path1, &id->start, point1);
      break;

    case GSK_PATH_INTERSECTION_NONE:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

void
collect_intersections (GskPath  *path1,
                       GskPath  *path2,
                       double    zoom,
                       GskPath **lines,
                       GskPath **points)
{
  IntersectionData id;

  id.line_builder = gsk_path_builder_new ();
  id.point_builder = gsk_path_builder_new ();
  id.segment = 0;
  id.zoom = zoom;

  gsk_path_foreach_intersection (path1, path2, intersection_cb, &id);

  *lines = scale_path (gsk_path_builder_free_to_path (id.line_builder), zoom);
  *points = gsk_path_builder_free_to_path (id.point_builder);
}
