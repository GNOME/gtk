/*
 * Copyright © 2025 Red Hat, Inc
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvggpaprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgprivate.h"
#include "gtksvgelementinternal.h"
#include "gtksvganimationprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgdasharrayprivate.h"
#include "gtksvgfilterrefprivate.h"
#include "gtksvgfilterfunctionsprivate.h"
#include "gtksvgstringprivate.h"
#include "gtksvgtimespecprivate.h"


gboolean
state_match (uint64_t     states,
             unsigned int state)
{
  return (states & BIT (state)) != 0;
}

static inline gboolean
is_state_name_start (char c)
{
  return g_ascii_isalpha (c);
}

static inline gboolean
is_state_name_char (char c)
{
  return c == '-' || g_ascii_isalnum (c);
}

gboolean
valid_state_name (const char *name)
{
  if (strcmp (name, "all") == 0 ||
      strcmp (name, "none") == 0)
    return FALSE;

  if (!is_state_name_start (name[0]))
    return FALSE;

  for (unsigned int i = 0; name[i]; i++)
    {
      if (!is_state_name_char (name[i]))
        return FALSE;
    }

  return TRUE;
}

gboolean
find_named_state (GtkSvg       *svg,
                  const char   *name,
                  unsigned int *state)
{
  for (unsigned int i = 0; i < svg->n_state_names; i++)
    {
      if (strcmp (name, svg->state_names[i]) == 0)
        {
          *state = i;
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
parse_states_css (GtkCssParser *parser,
                  GtkSvg       *svg,
                  uint64_t     *states)
{
  gtk_css_parser_skip_whitespace (parser);

  if (gtk_css_parser_try_ident (parser, "all"))
    {
      *states = ALL_STATES;
      return TRUE;
    }
  else if (gtk_css_parser_try_ident (parser, "none"))
    {
      *states = NO_STATES;
      return TRUE;
    }

  *states = NO_STATES;
  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          char *id = gtk_css_parser_consume_ident (parser);
          unsigned int u;

          if (!find_named_state (svg, id, &u))
            {
              *states = NO_STATES;
              g_free (id);
              return FALSE;
            }

          g_free (id);

          *states |= BIT (u);
        }
      else if (gtk_css_parser_has_integer (parser))
        {
          int i;

          gtk_css_parser_consume_integer (parser, &i);
          if (i < 0 || i > 63)
            {
              *states = NO_STATES;
              return FALSE;
            }

          *states |= BIT ((unsigned int) i);
        }
      else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        return TRUE;
      else
        return FALSE;
    }

  return TRUE;
}

gboolean
parse_states (GtkSvg     *svg,
              const char *text,
              uint64_t   *states)
{
  GtkCssParser *parser = parser_new_for_string (text);
  gboolean ret;

  gtk_css_parser_skip_whitespace (parser);
  ret = parse_states_css (parser, svg, states);
  gtk_css_parser_skip_whitespace (parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    ret = FALSE;
  gtk_css_parser_unref (parser);

  return ret;
}

void
print_states (GString  *s,
              GtkSvg   *svg,
              uint64_t  states)
{
  if (states == ALL_STATES)
    {
      g_string_append (s, "all");
    }
  else if (states == NO_STATES)
    {
      g_string_append (s, "none");
    }
  else
    {
      gboolean first = TRUE;
      unsigned int n_state_names = 0;
      const char **state_names = NULL;
      if (svg)
        {
          n_state_names = svg->n_state_names;
          state_names = (const char **) svg->state_names;
        }
      for (unsigned int u = 0; u < 64; u++)
        {
          if ((states & BIT (u)) != 0)
            {
              if (!first)
                g_string_append_c (s, ' ');
              first = FALSE;
              if (u < n_state_names)
                g_string_append (s, state_names[u]);
              else
                g_string_append_printf (s, "%u", u);
            }
        }
    }
}

static struct {
  double params[4];
} easing_funcs[] = {
  { { 0, 0, 1, 1 } },
  { { 0.42, 0, 0.58, 1 } },
  { { 0.42, 0, 1, 1 } },
  { { 0, 0, 0.58, 1 } },
  { { 0.25, 0.1, 0.25, 1 } },
};

void
shape_apply_state (GtkSvg       *self,
                   SvgElement   *shape,
                   unsigned int  state)
{
  if (svg_element_type_is_path (svg_element_get_type (shape)))
    {
      Visibility visibility;

      if (svg_element_get_states (shape) & BIT (state))
        visibility = VISIBILITY_VISIBLE;
      else
        visibility = VISIBILITY_HIDDEN;

      if ((self->features & GTK_SVG_ANIMATIONS) == 0)
        {
          SvgValue *value = svg_visibility_new (visibility);
          svg_element_set_base_value (shape, SVG_PROPERTY_VISIBILITY, value);
          svg_value_unref (value);
        }

      if (!self->playing && shape->animations)
        {
          for (unsigned int i = shape->animations->len; i > 0; i--)
            {
              SvgAnimation *a = g_ptr_array_index (shape->animations, i - 1);

              if ((visibility == VISIBILITY_VISIBLE &&
                   g_str_has_prefix (a->id, "gpa:transition:fade-in")) ||
                  (visibility == VISIBILITY_HIDDEN &&
                   g_str_has_prefix (a->id, "gpa:transition:fade-out")))
                {
                  a->status = ANIMATION_STATUS_DONE;
                  a->previous.begin = self->current_time;
                  a->current.begin = INDEFINITE;
                  a->current.end = INDEFINITE;
                  a->state_changed = TRUE;
                  g_ptr_array_steal_index (shape->animations, i - 1);
                  g_ptr_array_add (shape->animations, a);
                }
              if (g_str_has_prefix (a->id, "gpa:out-of-state"))
                {
                  if (visibility == VISIBILITY_HIDDEN)
                    {
                      a->status = ANIMATION_STATUS_RUNNING;
                      a->previous.begin = self->current_time;
                      a->current.begin = self->current_time;
                      a->current.end = INDEFINITE;
                    }
                  else
                    {
                      a->status = ANIMATION_STATUS_DONE;
                      a->previous.begin = self->current_time;
                      a->current.begin = INDEFINITE;
                      a->current.end = INDEFINITE;
                    }
                  a->state_changed = TRUE;
                  g_ptr_array_steal_index (shape->animations, i - 1);
                  g_ptr_array_add (shape->animations, a);
                }
              if (g_str_has_prefix (a->id, "gpa:in-state"))
                {
                  if (visibility == VISIBILITY_VISIBLE)
                    {
                      a->status = ANIMATION_STATUS_RUNNING;
                      a->previous.begin = self->current_time;
                      a->current.begin = self->current_time;
                      a->current.end = INDEFINITE;
                    }
                  else
                    {
                      a->status = ANIMATION_STATUS_DONE;
                      a->previous.begin = self->current_time;
                      a->current.begin = INDEFINITE;
                      a->current.end = INDEFINITE;
                    }
                  a->state_changed = TRUE;
                  g_ptr_array_steal_index (shape->animations, i - 1);
                  g_ptr_array_add (shape->animations, a);
                }
            }
        }
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          shape_apply_state (self, sh, state);
        }
    }
}

void
apply_state (GtkSvg   *self,
             uint64_t  state)
{
  shape_apply_state (self, self->content, state);
}

/* {{{ States */

static void
create_visibility_setter (SvgElement   *shape,
                          Timeline     *timeline,
                          uint64_t      states,
                          int64_t       delay,
                          unsigned int  initial_state)
{
  SvgAnimation *a = svg_animation_new (ANIMATION_TYPE_SET);
  TimeSpec *begin, *end;
  Visibility initial_visibility;
  Visibility opposite_visibility;
  SvgValue *value;

  if (svg_element_is_specified (shape, SVG_PROPERTY_VISIBILITY))
    {
      value = svg_element_get_specified_value (shape, SVG_PROPERTY_VISIBILITY);
      g_assert (value != NULL);
      initial_visibility = svg_enum_get (value);
    }
  else
    initial_visibility = VISIBILITY_VISIBLE;

  a->attr = SVG_PROPERTY_VISIBILITY;

  if (initial_visibility == VISIBILITY_VISIBLE)
    {
      a->id = g_strdup_printf ("gpa:out-of-state:%s", svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = svg_animation_add_end (a, timeline_get_states (timeline, ALL_STATES & ~states, states, - (MAX (0, - delay))));
      time_spec_add_animation (end, a);

      opposite_visibility = VISIBILITY_HIDDEN;
    }
  else
    {
      a->id = g_strdup_printf ("gpa:in-state:%s", svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = svg_animation_add_end (a, timeline_get_states (timeline, states, ALL_STATES & ~states, - (MAX (0, - delay))));
      time_spec_add_animation (end, a);

      opposite_visibility = VISIBILITY_VISIBLE;
    }

  if (state_match (states, initial_state) != (initial_visibility == VISIBILITY_VISIBLE))
    {
      begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
      time_spec_add_animation (begin, a);
    }


  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_visibility_new (opposite_visibility);
  a->frames[1].value = svg_visibility_new (opposite_visibility);

  a->fill = ANIMATION_FILL_REMOVE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
}

void
create_states (SvgElement   *shape,
               Timeline     *timeline,
               uint64_t      states,
               int64_t       delay,
               unsigned int  initial)
{
  if (states != ALL_STATES)
    create_visibility_setter (shape, timeline, states, delay, initial);
}

/* }}} */
/* {{{ Transitions */

void
create_path_length (SvgElement *shape,
                    Timeline   *timeline)
{
  SvgAnimation *a = svg_animation_new (ANIMATION_TYPE_SET);
  TimeSpec *begin, *end;

  a->attr = SVG_PROPERTY_PATH_LENGTH;

  a->id = g_strdup_printf ("gpa:path-length:%s", svg_element_get_id (shape));
  begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
  end = svg_animation_add_end (a, timeline_get_end_of_time (timeline));
  time_spec_add_animation (begin, a);
  time_spec_add_animation (end, a);

  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_number_new (1);
  a->frames[1].value = svg_number_new (1);

  a->fill = ANIMATION_FILL_REMOVE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
}

static void
create_transition (SvgElement    *shape,
                   unsigned int   idx,
                   Timeline      *timeline,
                   uint64_t       states,
                   int64_t        duration,
                   int64_t        delay,
                   GpaEasing      easing,
                   double         origin,
                   GpaTransition  type,
                   SvgProperty    attr,
                   SvgValue      *from,
                   SvgValue      *to)
{
  SvgAnimation *a;
  TimeSpec *begin;

  a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
  a->idx = idx;
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-in:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, delay));

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  memcpy (a->frames[0].params, easing_funcs[easing].params, sizeof (double) * 4);
  a->calc_mode = CALC_MODE_SPLINE;

  a->attr = attr;
  a->frames[0].value = from;
  a->frames[1].value = to;

  a->fill = ANIMATION_FILL_FREEZE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a->gpa.transition = type;
  a->gpa.easing = easing;
  a->gpa.origin = origin;

  a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
  a->idx = idx;
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, - (duration + delay)));

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  memcpy (a->frames[0].params, easing_funcs[easing].params, sizeof (double) * 4);
  a->calc_mode = CALC_MODE_SPLINE;

  a->attr = attr;
  a->frames[0].value = svg_value_ref (to);
  a->frames[1].value = svg_value_ref (from);

  a->fill = ANIMATION_FILL_FREEZE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a->gpa.transition = type;
  a->gpa.easing = easing;
  a->gpa.origin = origin;

  if (delay > 0)
    {
      a = svg_animation_new (ANIMATION_TYPE_SET);
      a->idx = idx;
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-in:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));
      time_spec_add_animation (begin, a);

      a->has_begin = 1;
      a->has_simple_duration = 1;
      a->has_repeat_duration = 1;

      a->n_frames = 2;
      a->frames = g_new0 (Frame, a->n_frames);
      a->frames[0].time = 0;
      a->frames[1].time = 1;
      a->frames[0].value = svg_value_ref (from);
      a->frames[1].value = svg_value_ref (from);

      a->fill = ANIMATION_FILL_FREEZE;
      a->shape = shape;

      svg_element_add_animation (shape, a);

      a = svg_animation_new (ANIMATION_TYPE_SET);
      a->idx = idx;
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-out:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, 0));
      time_spec_add_animation (begin, a);

      a->has_begin = 1;
      a->has_simple_duration = 1;
      a->has_repeat_duration = 1;

      a->n_frames = 2;
      a->frames = g_new0 (Frame, a->n_frames);
      a->frames[0].time = 0;
      a->frames[1].time = 1;
      a->frames[0].value = svg_value_ref (to);
      a->frames[1].value = svg_value_ref (to);

      a->fill = ANIMATION_FILL_FREEZE;
      a->shape = shape;

      svg_element_add_animation (shape, a);
    }
}

static void
create_transition_delay (SvgElement  *shape,
                         Timeline    *timeline,
                         uint64_t     states,
                         int64_t      delay,
                         SvgProperty  attr,
                         SvgValue    *value)
{
  SvgAnimation *a;
  TimeSpec *begin;

  a = svg_animation_new (ANIMATION_TYPE_SET);
  a->simple_duration = delay;
  a->repeat_duration = delay;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-in-delay:%s:%s", svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a = svg_animation_new (ANIMATION_TYPE_SET);
  a->simple_duration = delay;
  a->repeat_duration = delay;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out-delay:%s:%s", svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, -delay));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  svg_value_unref (value);
}

/* The filter we create here looks roughly like this:
 *
 * <feGaussianBlur -> blurred
 * <feComponentTransfer in=SourceGraphic ... make source alpha solid
 * <feGaussianBlur
 * <feComponentTransfer threshold alpha, white-out color -> blobbed
 * <feComposite ...multiply blurred and blobbed
 *
 * The blurs and the second component transfer are animated
 * from their full effect to identity.
 */
static void
create_morph_filter (SvgElement *shape,
                     Timeline   *timeline,
                     GHashTable *shapes,
                     uint64_t    states,
                     int64_t     duration,
                     int64_t     delay,
                     GpaEasing   easing)
{
  SvgElement *parent = NULL;
  SvgElement *filter;
  SvgFilter *f;
  SvgValue *value;
  SvgAnimation *a;
  char *str;
  TimeSpec *begin;
  TimeSpec *end;

  for (unsigned int i = 0; i < svg_element_get_parent (shape)->shapes->len; i++)
    {
      SvgElement *sh = g_ptr_array_index (svg_element_get_parent (shape)->shapes, i);

      if (sh == shape)
        break;

      if (sh->type == SVG_ELEMENT_DEFS)
        {
          parent = sh;
          break;
        }
    }

  if (parent == NULL)
    {
      parent = svg_element_new (svg_element_get_parent (shape), SVG_ELEMENT_DEFS);
      g_ptr_array_insert (svg_element_get_parent (shape)->shapes, 0, parent);
    }

  filter = svg_element_new (parent, SVG_ELEMENT_FILTER);
  svg_element_add_child (parent, filter);
  filter->id = g_strdup_printf ("gpa:morph-filter:%s", svg_element_get_id (shape));

  g_hash_table_insert (shapes, filter->id, filter);

  value = svg_percentage_new (-50);
  svg_element_set_specified_value (filter, SVG_PROPERTY_X, value);
  svg_element_set_specified_value (filter, SVG_PROPERTY_Y, value);
  svg_value_unref (value);
  value = svg_percentage_new (200);
  svg_element_set_specified_value (filter, SVG_PROPERTY_WIDTH, value);
  svg_element_set_specified_value (filter, SVG_PROPERTY_HEIGHT, value);
  svg_value_unref (value);

  f = svg_filter_new (filter, SVG_FILTER_BLUR);
  svg_element_add_filter (filter, f);
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", svg_element_get_id (shape));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_RESULT, svg_string_new_take (str));

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_STD_DEV,
                     svg_numbers_new1 (4),
                     svg_numbers_new1 (0));

  f = svg_filter_new (filter, SVG_FILTER_COMPONENT_TRANSFER);
  svg_element_add_filter (filter, f);
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_IN, svg_filter_ref_new (FILTER_REF_SOURCE_GRAPHIC));

  f = svg_filter_new (filter, SVG_FILTER_FUNC_A);
  svg_element_add_filter (filter, f);
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_TYPE, svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_SLOPE, svg_number_new (100));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_INTERCEPT, svg_number_new (0));

  f = svg_filter_new (filter, SVG_FILTER_BLUR);
  svg_element_add_filter (filter, f);

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_STD_DEV,
                     svg_numbers_new1 (4),
                     svg_numbers_new1 (0));

  f = svg_filter_new (filter, SVG_FILTER_COMPONENT_TRANSFER);
  svg_element_add_filter (filter, f);

  for (unsigned int func = SVG_FILTER_FUNC_R; func <= SVG_FILTER_FUNC_B; func++)
    {
      f = svg_filter_new (filter, func);
      svg_element_add_filter (filter, f);
      svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_TYPE, svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR));
      svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_SLOPE, svg_number_new (0));
      svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_INTERCEPT, svg_number_new (1));
    }

  f = svg_filter_new (filter, SVG_FILTER_FUNC_A);
  svg_element_add_filter (filter, f);
  svg_filter_set_specified_value (f, SVG_PROPERTY_FE_FUNC_TYPE, svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR));

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_FUNC_SLOPE,
                     svg_number_new (100),
                     svg_number_new (1));

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_FUNC_INTERCEPT,
                     svg_number_new (-20),
                     svg_number_new (0));

  f = svg_filter_new (filter, SVG_FILTER_COMPOSITE);
  svg_element_add_filter (filter, f);
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_COMPOSITE_OPERATOR, svg_composite_operator_new (COMPOSITE_OPERATOR_ARITHMETIC));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_COMPOSITE_K1, svg_number_new (1));
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", svg_element_get_id (shape));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_IN2, svg_filter_ref_new_ref (str));
  g_free (str);

  a = svg_animation_new (ANIMATION_TYPE_SET);
  a->id = g_strdup_printf ("gpa:set:morph:%s", svg_element_get_id (shape));
  a->attr = SVG_PROPERTY_FILTER;

  begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
  time_spec_add_animation (begin, a);
  end = svg_animation_add_end (a, timeline_get_end_of_time (timeline));
  time_spec_add_animation (end, a);

  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  str = g_strdup_printf ("url(#%s)", filter->id);
  a->frames[0].value = svg_filter_functions_parse (str);
  a->frames[1].value = svg_value_ref (a->frames[0].value);
  g_free (str);
  a->shape = shape;

  svg_element_add_animation (shape, a);
}

void
create_transitions (SvgElement    *shape,
                    Timeline      *timeline,
                    GHashTable    *shapes,
                    uint64_t       states,
                    GpaTransition  type,
                    int64_t        duration,
                    int64_t        delay,
                    GpaEasing      easing,
                    double         origin)
{
  switch (type)
    {
    case GPA_TRANSITION_NONE:
      break;
    case GPA_TRANSITION_ANIMATE:
      create_transition (shape, 0, timeline, states,
                         duration, delay, easing,
                         origin, type,
                         SVG_PROPERTY_STROKE_DASHARRAY,
                         svg_dash_array_new ((double[]) { 0, 2 }, 2),
                         svg_dash_array_new ((double[]) { 1, 0 }, 2));
      if (delay != 0)
        create_transition_delay (shape, timeline, states, delay,
                                 SVG_PROPERTY_STROKE_DASHOFFSET,
                                 svg_number_new (0.5));
      if (!G_APPROX_VALUE (origin, 0, 0.001))
        create_transition (shape, 0, timeline, states,
                           duration, delay, easing,
                           origin, type,
                           SVG_PROPERTY_STROKE_DASHOFFSET,
                           svg_number_new (-origin),
                           svg_number_new (0));
      break;
    case GPA_TRANSITION_MORPH:
      create_morph_filter (shape, timeline, shapes, states,
                           duration, delay, easing);
      break;
    case GPA_TRANSITION_FADE:
      create_transition (shape, 0, timeline, states,
                         duration, delay, easing,
                         origin, type,
                         SVG_PROPERTY_OPACITY,
                         svg_number_new (0), svg_number_new (1));
      break;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ Animations */

static SvgAnimation *
create_animation (SvgElement   *shape,
                  Timeline     *timeline,
                  uint64_t      states,
                  unsigned int  initial,
                  double        repeat,
                  int64_t       duration,
                  CalcMode      calc_mode,
                  SvgProperty   attr,
                  GArray       *frames)
{
  SvgAnimation *a;
  TimeSpec *begin, *end;

  a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
  a->repeat_count = repeat;
  a->simple_duration = duration;
  if (repeat == REPEAT_FOREVER)
    a->repeat_duration = INDEFINITE;
  else
    a->repeat_duration =duration * repeat;

  a->has_begin = 1;
  a->has_end = 1;
  a->has_simple_duration = 1;
  a->has_repeat_count = 1;

  a->id = g_strdup_printf ("gpa:animation:%s-%s", svg_element_get_id (shape), svg_property_get_name (attr));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));
  time_spec_add_animation (begin, a);

  if (state_match (states, initial))
    {
      begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
      time_spec_add_animation (begin, a);
    }

  end = svg_animation_add_end (a, timeline_get_states (timeline, states, ALL_STATES & ~states, 0));
  time_spec_add_animation (end, a);

  a->attr = attr;
  a->n_frames = frames->len;
  a->frames = g_array_steal (frames, NULL);

  a->calc_mode = calc_mode;
  a->shape = shape;

  svg_element_add_animation (shape, a);

  return a;
}

static void
add_frame (GArray    *a,
           double     time,
           SvgValue  *value,
           GpaEasing  easing)
{
  Frame frame;
  frame.time = time;
  frame.value = value;
  frame.point = 0;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (a, frame);
}

static void
add_point_frame (GArray    *a,
                 double     time,
                 double     point,
                 GpaEasing  easing)
{
  Frame frame;
  frame.time = time;
  frame.value = NULL;
  frame.point = point;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (a, frame);
}

static void
construct_animation_frames (GpaAnimation  direction,
                            GpaEasing     easing,
                            double        segment,
                            double        origin,
                            GArray       *array,
                            GArray       *offset)
{
  switch (direction)
    {
    case GPA_ANIMATION_NORMAL:
      add_frame (array, 0, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0, svg_number_new (-origin), easing);
          add_frame (offset, 1, svg_number_new (0), easing);
        }
      break;

    case GPA_ANIMATION_REVERSE:
      add_frame (array, 0, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0, svg_number_new (0), easing);
          add_frame (offset, 1, svg_number_new (-origin), easing);
        }
      break;

    case GPA_ANIMATION_ALTERNATE:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0,   svg_number_new (-origin), easing);
          add_frame (offset, 0.5, svg_number_new (0), easing);
          add_frame (offset, 1,   svg_number_new (-origin), easing);
        }
      break;

    case GPA_ANIMATION_REVERSE_ALTERNATE:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0,   svg_number_new (0), easing);
          add_frame (offset, 0.5, svg_number_new (-origin), easing);
          add_frame (offset, 1,   svg_number_new (0), easing);
        }
      break;

    case GPA_ANIMATION_IN_OUT:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { 0, 1, 0, 2 }, 4), easing);
      add_frame (offset, 0,   svg_number_new (-origin), easing);
      add_frame (offset, 0.5, svg_number_new (0), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case GPA_ANIMATION_IN_OUT_REVERSE:
      add_frame (array, 0  , svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (offset, 0,   svg_number_new (0), easing);
      add_frame (offset, 0.5, svg_number_new (-origin), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case GPA_ANIMATION_IN_OUT_ALTERNATE:
      add_frame (array, 0,    svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 0.25, svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 0.5,  svg_dash_array_new ((double[]) { 0, 1, 0, 2 }, 4), easing);
      add_frame (array, 0.75, svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 1,    svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      if (origin != 0)
        {
          add_frame (offset, 0,    svg_number_new (-origin), easing);
          add_frame (offset, 0.25, svg_number_new (0), easing);
          add_frame (offset, 0.5,  svg_number_new (0), easing);
          add_frame (offset, 0.75, svg_number_new (0), easing);
          add_frame (offset, 1,    svg_number_new (-origin), easing);
        }
      break;

    case GPA_ANIMATION_SEGMENT:
      add_frame (array, 0, svg_dash_array_new ((double[]) { segment, 1 - segment }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { segment, 1 - segment }, 2), easing);
      add_frame (offset, 0, svg_number_new (0), easing);
      add_frame (offset, 1, svg_number_new (-1), easing);
      break;

    case GPA_ANIMATION_SEGMENT_ALTERNATE:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (offset, 0,   svg_number_new (0), easing);
      add_frame (offset, 0.5, svg_number_new (segment - 1), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case GPA_ANIMATION_NONE:
    default:
      g_assert_not_reached ();
    }
}

static void
construct_moving_frames (GpaAnimation  direction,
                         GpaEasing     easing,
                         double        segment,
                         double        origin,
                         double        attach_pos,
                         GArray       *array)
{
  switch ((int)direction)
    {
    case GPA_ANIMATION_NORMAL:
      add_point_frame (array, 0, origin, easing);
      add_point_frame (array, 1, attach_pos, easing);
      break;

    case GPA_ANIMATION_ALTERNATE:
      add_point_frame (array, 0, origin, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1, origin, easing);
      break;

    case GPA_ANIMATION_REVERSE:
      add_point_frame (array, 0, attach_pos, easing);
      add_point_frame (array, 1, origin, easing);
      break;

    case GPA_ANIMATION_REVERSE_ALTERNATE:
      add_point_frame (array, 0,   attach_pos, easing);
      add_point_frame (array, 0.5, origin, easing);
      add_point_frame (array, 1,   attach_pos, easing);
      break;

    case GPA_ANIMATION_IN_OUT:
      add_point_frame (array, 0,   origin, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1,   1, easing);
      break;

    case GPA_ANIMATION_IN_OUT_REVERSE:
      add_point_frame (array, 0,   1, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1,   origin, easing);
      break;

    case GPA_ANIMATION_IN_OUT_ALTERNATE:
      add_point_frame (array, 0,    origin, easing);
      add_point_frame (array, 0.25, attach_pos, easing);
      add_point_frame (array, 0.5,  1, easing);
      add_point_frame (array, 0.75, attach_pos, easing);
      add_point_frame (array, 1,    origin, easing);
      break;

    case GPA_ANIMATION_SEGMENT:
      add_point_frame (array, 0, attach_pos * segment, easing);
      add_point_frame (array, 1, 1 + attach_pos * segment, easing);
      break;

    case GPA_ANIMATION_SEGMENT_ALTERNATE:
      add_point_frame (array, 0, attach_pos * segment, easing);
      add_point_frame (array, 0.5, (1 - segment) + attach_pos * segment, easing);
      add_point_frame (array, 1, attach_pos * segment, easing);
      break;

    default:
      g_assert_not_reached ();
    }
}

static double
repeat_duration_for_direction (GpaAnimation direction,
                               double       duration)
{
  switch (direction)
    {
    case GPA_ANIMATION_NONE:              return 0;
    case GPA_ANIMATION_NORMAL:            return duration;
    case GPA_ANIMATION_ALTERNATE:         return 2 * duration;
    case GPA_ANIMATION_REVERSE:           return duration;
    case GPA_ANIMATION_REVERSE_ALTERNATE: return 2 * duration;
    case GPA_ANIMATION_IN_OUT:            return 2 * duration;
    case GPA_ANIMATION_IN_OUT_ALTERNATE:  return 4 * duration;
    case GPA_ANIMATION_IN_OUT_REVERSE:    return 2 * duration;
    case GPA_ANIMATION_SEGMENT:           return duration;
    case GPA_ANIMATION_SEGMENT_ALTERNATE: return 2 * duration;
    default: g_assert_not_reached ();
    }
}

void
create_animations (SvgElement   *shape,
                   Timeline     *timeline,
                   uint64_t      states,
                   unsigned int  initial,
                   double        repeat,
                   int64_t       duration,
                   GpaAnimation  direction,
                   GpaEasing     easing,
                   double        segment,
                   double        origin)
{
  GArray *array, *offset;
  CalcMode calc_mode;
  SvgAnimation *a;
  double repeat_duration;

  if (direction == GPA_ANIMATION_NONE)
    return;

  if (duration == 0)
    {
      g_warning ("SVG: not creating zero-duration animations");
      return;
    }

  array = g_array_new (FALSE, FALSE, sizeof (Frame));
  offset = g_array_new (FALSE, FALSE, sizeof (Frame));

  construct_animation_frames (direction, easing, segment, origin, array, offset);
  repeat_duration = repeat_duration_for_direction (direction, duration);

  if (easing == GPA_EASING_LINEAR)
    calc_mode = CALC_MODE_LINEAR;
  else
    calc_mode = CALC_MODE_SPLINE;

  a = create_animation (shape, timeline, states, initial,
                        repeat, repeat_duration, calc_mode,
                        SVG_PROPERTY_STROKE_DASHARRAY,
                        array);

  a->gpa.animation = direction;
  a->gpa.easing = easing;
  a->gpa.origin = origin;
  a->gpa.segment = segment;

  if (offset->len > 0)
    a = create_animation (shape, timeline, states, initial,
                          repeat, repeat_duration, calc_mode,
                          SVG_PROPERTY_STROKE_DASHOFFSET,
                          offset);

  g_array_unref (array);
  g_array_unref (offset);
}

/* }}} */
/* {{{ Attachment */

void
create_attachment (SvgElement *shape,
                   Timeline   *timeline,
                   uint64_t    states,
                   const char *attach_to,
                   double      attach_pos,
                   double      origin)
{
  SvgAnimation *a;
  GArray *frames;
  TimeSpec *begin, *end;

  a = svg_animation_new (ANIMATION_TYPE_MOTION);

  a->has_begin = 1;
  a->has_end = 1;
  a->has_simple_duration = 1;

  a->simple_duration = 1;

  a->id = g_strdup_printf ("gpa:attachment:%s", svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
  end = svg_animation_add_end (a, timeline_get_fixed (timeline, 1));

  frames = g_array_new (FALSE, FALSE, sizeof (Frame));
  add_point_frame (frames, 0, attach_pos, GPA_EASING_LINEAR);
  add_point_frame (frames, 1, attach_pos, GPA_EASING_LINEAR);

  a->n_frames = frames->len;
  a->frames = g_array_steal (frames, NULL);

  g_array_unref (frames);

  a->motion.path_ref = g_strdup (attach_to);

  a->calc_mode = CALC_MODE_LINEAR;
  a->fill = ANIMATION_FILL_FREEZE;
  a->motion.rotate = ROTATE_AUTO;

  a->gpa.origin = origin;
  a->gpa.attach_pos = attach_pos;
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);
  time_spec_add_animation (end, a);
}

static void
create_attachment_connection_to (SvgAnimation *a,
                                 SvgAnimation *da,
                                 Timeline     *timeline)
{
  SvgAnimation *a2;
  GArray *frames;
  GpaAnimation direction;
  TimeSpec *begin, *end;

  a2 = svg_animation_new (ANIMATION_TYPE_MOTION);
  a2->simple_duration = da->simple_duration;
  a2->repeat_count = da->repeat_count;
  if (g_str_has_prefix (da->id, "gpa:animation:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-animation:%s", svg_element_get_id (a->shape));
      direction = da->gpa.animation;
    }
  else if (g_str_has_prefix (da->id, "gpa:transition:fade-in:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-transition:fade-in:%s", svg_element_get_id (a->shape));
      direction = GPA_ANIMATION_NORMAL;
    }
  else if (g_str_has_prefix (da->id, "gpa:transition:fade-out:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-transition:fade-out:%s", svg_element_get_id (a->shape));
      direction = GPA_ANIMATION_REVERSE;
    }
  else
    g_assert_not_reached ();

  a2->has_begin = 1;
  a2->has_end = 1;
  a2->has_simple_duration = 1;

  begin = svg_animation_add_begin (a2, timeline_get_sync (timeline, da->id, da, TIME_SPEC_SIDE_BEGIN, 0));
  end = svg_animation_add_end (a2, timeline_get_sync (timeline, da->id, da, TIME_SPEC_SIDE_END, 0));

  frames = g_array_new (FALSE, FALSE, sizeof (Frame));

  construct_moving_frames (direction,
                           da->gpa.easing,
                           da->gpa.segment,
                           da->gpa.origin,
                           a->gpa.attach_pos,
                           frames);

  a2->n_frames = frames->len;
  a2->frames = g_array_steal (frames, NULL);

  g_array_unref (frames);

  a2->motion.path_shape = da->shape;

  a2->calc_mode = da->calc_mode;
  a2->fill = ANIMATION_FILL_FREEZE;
  a2->motion.rotate = ROTATE_AUTO;

  a2->gpa.origin = a->gpa.origin;
  a2->shape = a->shape;

  svg_element_add_animation (a->shape, a2);
  time_spec_add_animation (begin, a2);
  time_spec_add_animation (end, a2);

  svg_animation_add_dep (da, a2);
}

void
create_attachment_connection (SvgAnimation *a,
                              SvgElement   *sh,
                              Timeline     *timeline)
{
  if (sh->animations == NULL)
    return;

  for (unsigned int i = 0; i < sh->animations->len; i++)
    {
      SvgAnimation *sha = g_ptr_array_index (sh->animations, i);
      if (sha->id &&
          (g_str_has_prefix (sha->id, "gpa:animation:") ||
           g_str_has_prefix (sha->id, "gpa:transition:")))
        {
          if (sha->attr == SVG_PROPERTY_STROKE_DASHARRAY)
            create_attachment_connection_to (a, sha, timeline);
        }
    }
}

/* }}} */

/* vim:set foldmethod=marker: */
