/*
 * Copyright © 2012 Red Hat Inc.
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

#include "gtkprivate.h"
#include "gtkcssstyleprivate.h"

#include "gtkcssanimationprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsssectionprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssStyle, gtk_css_style, G_TYPE_OBJECT)

static void
gtk_css_style_dispose (GObject *object)
{
  GtkCssStyle *style = GTK_CSS_STYLE (object);

  if (style->values)
    {
      g_ptr_array_unref (style->values);
      style->values = NULL;
    }
  if (style->sections)
    {
      g_ptr_array_unref (style->sections);
      style->sections = NULL;
    }
  if (style->animated_values)
    {
      g_ptr_array_unref (style->animated_values);
      style->animated_values = NULL;
    }

  g_slist_free_full (style->animations, g_object_unref);
  style->animations = NULL;

  G_OBJECT_CLASS (gtk_css_style_parent_class)->dispose (object);
}

static void
gtk_css_style_finalize (GObject *object)
{
  GtkCssStyle *style = GTK_CSS_STYLE (object);

  _gtk_bitmask_free (style->depends_on_parent);
  _gtk_bitmask_free (style->equals_parent);
  _gtk_bitmask_free (style->depends_on_color);
  _gtk_bitmask_free (style->depends_on_font_size);

  G_OBJECT_CLASS (gtk_css_style_parent_class)->finalize (object);
}

static void
gtk_css_style_class_init (GtkCssStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_css_style_dispose;
  object_class->finalize = gtk_css_style_finalize;
}

static void
gtk_css_style_init (GtkCssStyle *style)
{
  style->depends_on_parent = _gtk_bitmask_new ();
  style->equals_parent = _gtk_bitmask_new ();
  style->depends_on_color = _gtk_bitmask_new ();
  style->depends_on_font_size = _gtk_bitmask_new ();
}

GtkCssStyle *
gtk_css_style_new (void)
{
  return g_object_new (GTK_TYPE_CSS_STYLE, NULL);
}

static void
maybe_unref_section (gpointer section)
{
  if (section)
    gtk_css_section_unref (section);
}

void
gtk_css_style_compute_value (GtkCssStyle             *style,
                             GtkStyleProviderPrivate *provider,
			     int                      scale,
                             GtkCssStyle             *parent_style,
                             guint                    id,
                             GtkCssValue             *specified,
                             GtkCssSection           *section)
{
  GtkCssDependencies dependencies;
  GtkCssValue *value;

  gtk_internal_return_if_fail (GTK_IS_CSS_STYLE (style));
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));
  gtk_internal_return_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style));

  /* http://www.w3.org/TR/css3-cascade/#cascade
   * Then, for every element, the value for each property can be found
   * by following this pseudo-algorithm:
   * 1) Identify all declarations that apply to the element
   */
  if (specified == NULL)
    {
      GtkCssStyleProperty *prop = _gtk_css_style_property_lookup_by_id (id);

      if (_gtk_css_style_property_is_inherit (prop))
        specified = _gtk_css_inherit_value_new ();
      else
        specified = _gtk_css_initial_value_new ();
    }
  else
    _gtk_css_value_ref (specified);

  value = _gtk_css_value_compute (specified, id, provider, scale, style, parent_style, &dependencies);

  if (style->values == NULL)
    style->values = g_ptr_array_new_full (_gtk_css_style_property_get_n_properties (),
					   (GDestroyNotify)_gtk_css_value_unref);
  if (id >= style->values->len)
   g_ptr_array_set_size (style->values, id + 1);

  if (g_ptr_array_index (style->values, id))
    _gtk_css_value_unref (g_ptr_array_index (style->values, id));
  g_ptr_array_index (style->values, id) = _gtk_css_value_ref (value);

  if (dependencies & (GTK_CSS_DEPENDS_ON_PARENT | GTK_CSS_EQUALS_PARENT))
    style->depends_on_parent = _gtk_bitmask_set (style->depends_on_parent, id, TRUE);
  if (dependencies & (GTK_CSS_EQUALS_PARENT))
    style->equals_parent = _gtk_bitmask_set (style->equals_parent, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_COLOR))
    style->depends_on_color = _gtk_bitmask_set (style->depends_on_color, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_FONT_SIZE))
    style->depends_on_font_size = _gtk_bitmask_set (style->depends_on_font_size, id, TRUE);

  if (style->sections && style->sections->len > id && g_ptr_array_index (style->sections, id))
    {
      gtk_css_section_unref (g_ptr_array_index (style->sections, id));
      g_ptr_array_index (style->sections, id) = NULL;
    }

  if (section)
    {
      if (style->sections == NULL)
        style->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (style->sections->len <= id)
        g_ptr_array_set_size (style->sections, id + 1);

      g_ptr_array_index (style->sections, id) = gtk_css_section_ref (section);
    }

  _gtk_css_value_unref (value);
  _gtk_css_value_unref (specified);
}

void
gtk_css_style_set_animated_value (GtkCssStyle *style,
                                  guint        id,
                                  GtkCssValue *value)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_STYLE (style));
  gtk_internal_return_if_fail (value != NULL);

  if (style->animated_values == NULL)
    style->animated_values = g_ptr_array_new_with_free_func ((GDestroyNotify)_gtk_css_value_unref);
  if (id >= style->animated_values->len)
   g_ptr_array_set_size (style->animated_values, id + 1);

  if (g_ptr_array_index (style->animated_values, id))
    _gtk_css_value_unref (g_ptr_array_index (style->animated_values, id));
  g_ptr_array_index (style->animated_values, id) = _gtk_css_value_ref (value);

}

GtkCssValue *
gtk_css_style_get_value (GtkCssStyle *style,
                          guint        id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  if (style->animated_values &&
      id < style->animated_values->len &&
      g_ptr_array_index (style->animated_values, id))
    return g_ptr_array_index (style->animated_values, id);

  return gtk_css_style_get_intrinsic_value (style, id);
}

GtkCssValue *
gtk_css_style_get_intrinsic_value (GtkCssStyle *style,
                                   guint        id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  if (style->values == NULL ||
      id >= style->values->len)
    return NULL;

  return g_ptr_array_index (style->values, id);
}

GtkCssSection *
gtk_css_style_get_section (GtkCssStyle *style,
                           guint        id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  if (style->sections == NULL ||
      id >= style->sections->len)
    return NULL;

  return g_ptr_array_index (style->sections, id);
}

GtkBitmask *
gtk_css_style_get_difference (GtkCssStyle *style,
                              GtkCssStyle *other)
{
  GtkBitmask *result;
  guint i, len;

  len = MIN (style->values->len, other->values->len);
  result = _gtk_bitmask_new ();
  if (style->values->len != other->values->len)
    result = _gtk_bitmask_invert_range (result, len, MAX (style->values->len, other->values->len));
  
  for (i = 0; i < len; i++)
    {
      if (!_gtk_css_value_equal (g_ptr_array_index (style->values, i),
                                 g_ptr_array_index (other->values, i)))
        result = _gtk_bitmask_set (result, i, TRUE);
    }

  return result;
}

/* TRANSITIONS */

typedef struct _TransitionInfo TransitionInfo;
struct _TransitionInfo {
  guint index;                  /* index into value arrays */
  gboolean pending;             /* TRUE if we still need to handle it */
};

static void
transition_info_add (TransitionInfo    infos[GTK_CSS_PROPERTY_N_PROPERTIES],
                     GtkStyleProperty *property,
                     guint             index)
{
  if (property == NULL)
    {
      guint i;

      for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
        {
          GtkCssStyleProperty *prop = _gtk_css_style_property_lookup_by_id (i);

          transition_info_add (infos, GTK_STYLE_PROPERTY (prop), index);
        }
    }
  else if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
    {
      GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
      guint i;

      for (i = 0; i < _gtk_css_shorthand_property_get_n_subproperties (shorthand); i++)
        {
          GtkCssStyleProperty *prop = _gtk_css_shorthand_property_get_subproperty (shorthand, i);

          transition_info_add (infos, GTK_STYLE_PROPERTY (prop), index);
        }
    }
  else if (GTK_IS_CSS_STYLE_PROPERTY (property))
    {
      guint id;
      
      if (!_gtk_css_style_property_is_animated (GTK_CSS_STYLE_PROPERTY (property)))
        return;

      id = _gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (property));
      g_assert (id < GTK_CSS_PROPERTY_N_PROPERTIES);
      infos[id].index = index;
      infos[id].pending = TRUE;
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
transition_infos_set (TransitionInfo  infos[GTK_CSS_PROPERTY_N_PROPERTIES],
                      GtkCssValue    *transitions)
{
  guint i;

  for (i = 0; i < _gtk_css_array_value_get_n_values (transitions); i++)
    {
      GtkStyleProperty *property;
      GtkCssValue *prop_value;

      prop_value = _gtk_css_array_value_get_nth (transitions, i);
      if (g_ascii_strcasecmp (_gtk_css_ident_value_get (prop_value), "all") == 0)
        property = NULL;
      else
        {
          property = _gtk_style_property_lookup (_gtk_css_ident_value_get (prop_value));
          if (property == NULL)
            continue;
        }
      
      transition_info_add (infos, property, i);
    }
}

static GtkStyleAnimation *
gtk_css_style_find_transition (GtkCssStyle *style,
                               guint        property_id)
{
  GSList *list;

  for (list = style->animations; list; list = list->next)
    {
      if (!GTK_IS_CSS_TRANSITION (list->data))
        continue;

      if (_gtk_css_transition_get_property (list->data) == property_id)
        return list->data;
    }

  return NULL;
}

static void
gtk_css_style_create_css_transitions (GtkCssStyle *style,
                                      gint64       timestamp,
                                      GtkCssStyle *source)
{
  TransitionInfo transitions[GTK_CSS_PROPERTY_N_PROPERTIES] = { { 0, } };
  GtkCssValue *durations, *delays, *timing_functions;
  guint i;

  transition_infos_set (transitions, gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TRANSITION_PROPERTY));

  durations = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TRANSITION_DURATION);
  delays = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TRANSITION_DELAY);
  timing_functions = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION);

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      GtkStyleAnimation *animation;
      GtkCssValue *start, *end;
      double duration, delay;

      if (!transitions[i].pending)
        continue;

      duration = _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, transitions[i].index), 100);
      delay = _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, transitions[i].index), 100);
      if (duration + delay == 0.0)
        continue;

      start = gtk_css_style_get_intrinsic_value (source, i);
      end = gtk_css_style_get_intrinsic_value (style, i);
      if (_gtk_css_value_equal (start, end))
        {
          animation = gtk_css_style_find_transition (GTK_CSS_STYLE (source), i);
          if (animation)
            style->animations = g_slist_prepend (style->animations, g_object_ref (animation));
        }
      else
        {
          animation = _gtk_css_transition_new (i,
                                               gtk_css_style_get_value (source, i),
                                               _gtk_css_array_value_get_nth (timing_functions, i),
                                               timestamp + delay * G_USEC_PER_SEC,
                                               timestamp + (delay + duration) * G_USEC_PER_SEC);
          style->animations = g_slist_prepend (style->animations, animation);
        }
    }
}

static GtkStyleAnimation *
gtk_css_style_find_animation (GtkCssStyle *style,
                              const char  *name)
{
  GSList *list;

  for (list = style->animations; list; list = list->next)
    {
      if (!GTK_IS_CSS_ANIMATION (list->data))
        continue;

      if (g_str_equal (_gtk_css_animation_get_name (list->data), name))
        return list->data;
    }

  return NULL;
}

static void
gtk_css_style_create_css_animations (GtkCssStyle             *style,
                                     GtkCssStyle             *parent_style,
                                     gint64                   timestamp,
                                     GtkStyleProviderPrivate *provider,
				     int                      scale,
                                     GtkCssStyle             *source)
{
  GtkCssValue *durations, *delays, *timing_functions, *animations;
  GtkCssValue *iteration_counts, *directions, *play_states, *fill_modes;
  guint i;

  animations = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_NAME);
  durations = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_DURATION);
  delays = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_DELAY);
  timing_functions = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION);
  iteration_counts = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT);
  directions = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_DIRECTION);
  play_states = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE);
  fill_modes = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ANIMATION_FILL_MODE);

  for (i = 0; i < _gtk_css_array_value_get_n_values (animations); i++)
    {
      GtkStyleAnimation *animation;
      GtkCssKeyframes *keyframes;
      const char *name;
      
      name = _gtk_css_ident_value_get (_gtk_css_array_value_get_nth (animations, i));
      if (g_ascii_strcasecmp (name, "none") == 0)
        continue;

      animation = gtk_css_style_find_animation (style, name);
      if (animation)
        continue;

      if (source)
        animation = gtk_css_style_find_animation (source, name);

      if (animation)
        {
          animation = _gtk_css_animation_copy (GTK_CSS_ANIMATION (animation),
                                               timestamp,
                                               _gtk_css_play_state_value_get (_gtk_css_array_value_get_nth (play_states, i)));
        }
      else
        {
          keyframes = _gtk_style_provider_private_get_keyframes (provider, name);
          if (keyframes == NULL)
            continue;

          keyframes = _gtk_css_keyframes_compute (keyframes, provider, scale, style, parent_style);

          animation = _gtk_css_animation_new (name,
                                              keyframes,
                                              timestamp,
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, i), 100) * G_USEC_PER_SEC,
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, i), 100) * G_USEC_PER_SEC,
                                              _gtk_css_array_value_get_nth (timing_functions, i),
                                              _gtk_css_direction_value_get (_gtk_css_array_value_get_nth (directions, i)),
                                              _gtk_css_play_state_value_get (_gtk_css_array_value_get_nth (play_states, i)),
                                              _gtk_css_fill_mode_value_get (_gtk_css_array_value_get_nth (fill_modes, i)),
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (iteration_counts, i), 100));
          _gtk_css_keyframes_unref (keyframes);
        }
      style->animations = g_slist_prepend (style->animations, animation);
    }
}

/* PUBLIC API */

void
gtk_css_style_create_animations (GtkCssStyle             *style,
                                 GtkCssStyle             *parent_style,
                                 gint64                   timestamp,
                                 GtkStyleProviderPrivate *provider,
                                 int                      scale,
                                 GtkCssStyle             *source)
{
  if (source != NULL)
    gtk_css_style_create_css_transitions (style, timestamp, source);
  gtk_css_style_create_css_animations (style, parent_style, timestamp, provider, scale, source);
}

GtkBitmask *
gtk_css_style_advance (GtkCssStyle *style,
                       gint64       timestamp)
{
  GtkBitmask *changed;
  GPtrArray *old_computed_values;
  GSList *list;
  guint i;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);
  gtk_internal_return_val_if_fail (timestamp >= style->current_time, NULL);

  style->current_time = timestamp;
  old_computed_values = style->animated_values;
  style->animated_values = NULL;

  list = style->animations;
  while (list)
    {
      GtkStyleAnimation *animation = list->data;
      
      list = list->next;

      _gtk_style_animation_set_values (animation,
                                       timestamp,
                                       GTK_CSS_STYLE (style));
      
      if (_gtk_style_animation_is_finished (animation, timestamp))
        {
          style->animations = g_slist_remove (style->animations, animation);
          g_object_unref (animation);
        }
    }

  /* figure out changes */
  changed = _gtk_bitmask_new ();

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      GtkCssValue *old_animated, *new_animated;

      old_animated = old_computed_values && i < old_computed_values->len ? g_ptr_array_index (old_computed_values, i) : NULL;
      new_animated = style->animated_values && i < style->animated_values->len ? g_ptr_array_index (style->animated_values, i) : NULL;

      if (!_gtk_css_value_equal0 (old_animated, new_animated))
        changed = _gtk_bitmask_set (changed, i, TRUE);
    }

  if (old_computed_values)
    g_ptr_array_unref (old_computed_values);

  return changed;
}

gboolean
gtk_css_style_is_static (GtkCssStyle *style)
{
  GSList *list;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), TRUE);

  for (list = style->animations; list; list = list->next)
    {
      if (!_gtk_style_animation_is_static (list->data, style->current_time))
        return FALSE;
    }

  return TRUE;
}

void
gtk_css_style_cancel_animations (GtkCssStyle *style)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_STYLE (style));

  if (style->animated_values)
    {
      g_ptr_array_unref (style->animated_values);
      style->animated_values = NULL;
    }

  g_slist_free_full (style->animations, g_object_unref);
  style->animations = NULL;
}

GtkBitmask *
gtk_css_style_compute_dependencies (GtkCssStyle      *style,
                                    const GtkBitmask *parent_changes)
{
  GtkBitmask *changes;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), _gtk_bitmask_new ());

  changes = _gtk_bitmask_copy (parent_changes);
  changes = _gtk_bitmask_intersect (changes, style->depends_on_parent);
  if (_gtk_bitmask_get (changes, GTK_CSS_PROPERTY_COLOR))
    changes = _gtk_bitmask_union (changes, style->depends_on_color);
  if (_gtk_bitmask_get (changes, GTK_CSS_PROPERTY_FONT_SIZE))
    changes = _gtk_bitmask_union (changes, style->depends_on_font_size);

  return changes;
}

void
gtk_css_style_print (GtkCssStyle *style,
                     GString     *string)
{
  guint i;

  g_return_if_fail (GTK_IS_CSS_STYLE (style));
  g_return_if_fail (string != NULL);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssSection *section = gtk_css_style_get_section (style, i);
      g_string_append (string, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (_gtk_css_style_property_lookup_by_id (i))));
      g_string_append (string, ": ");
      _gtk_css_value_print (gtk_css_style_get_value (style, i), string);
      g_string_append (string, ";");
      if (section)
        {
          g_string_append (string, " /* ");
          _gtk_css_section_print (section, string);
          g_string_append (string, " */");
        }
      g_string_append (string, "\n");
    }
}

char *
gtk_css_style_to_string (GtkCssStyle *style)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  string = g_string_new ("");

  gtk_css_style_print (style, string);

  return g_string_free (string, FALSE);
}

