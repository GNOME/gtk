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
#include "gtkcsscomputedvaluesprivate.h"

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
#include "gtkstylepropertiesprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssComputedValues, _gtk_css_computed_values, G_TYPE_OBJECT)

static void
gtk_css_computed_values_dispose (GObject *object)
{
  GtkCssComputedValues *values = GTK_CSS_COMPUTED_VALUES (object);

  if (values->values)
    {
      g_ptr_array_unref (values->values);
      values->values = NULL;
    }
  if (values->sections)
    {
      g_ptr_array_unref (values->sections);
      values->sections = NULL;
    }
  if (values->animated_values)
    {
      g_ptr_array_unref (values->animated_values);
      values->animated_values = NULL;
    }

  g_slist_free_full (values->animations, g_object_unref);
  values->animations = NULL;

  G_OBJECT_CLASS (_gtk_css_computed_values_parent_class)->dispose (object);
}

static void
gtk_css_computed_values_finalize (GObject *object)
{
  GtkCssComputedValues *values = GTK_CSS_COMPUTED_VALUES (object);

  _gtk_bitmask_free (values->depends_on_parent);
  _gtk_bitmask_free (values->equals_parent);
  _gtk_bitmask_free (values->depends_on_color);
  _gtk_bitmask_free (values->depends_on_font_size);

  G_OBJECT_CLASS (_gtk_css_computed_values_parent_class)->finalize (object);
}

static void
_gtk_css_computed_values_class_init (GtkCssComputedValuesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_css_computed_values_dispose;
  object_class->finalize = gtk_css_computed_values_finalize;
}

static void
_gtk_css_computed_values_init (GtkCssComputedValues *values)
{
  values->depends_on_parent = _gtk_bitmask_new ();
  values->equals_parent = _gtk_bitmask_new ();
  values->depends_on_color = _gtk_bitmask_new ();
  values->depends_on_font_size = _gtk_bitmask_new ();
}

GtkCssComputedValues *
_gtk_css_computed_values_new (void)
{
  return g_object_new (GTK_TYPE_CSS_COMPUTED_VALUES, NULL);
}

static void
maybe_unref_section (gpointer section)
{
  if (section)
    gtk_css_section_unref (section);
}

void
_gtk_css_computed_values_compute_value (GtkCssComputedValues    *values,
                                        GtkStyleProviderPrivate *provider,
					int                      scale,
                                        GtkCssComputedValues    *parent_values,
                                        guint                    id,
                                        GtkCssValue             *specified,
                                        GtkCssSection           *section)
{
  GtkCssDependencies dependencies;
  GtkCssValue *value;

  gtk_internal_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));
  gtk_internal_return_if_fail (parent_values == NULL || GTK_IS_CSS_COMPUTED_VALUES (parent_values));

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

  value = _gtk_css_value_compute (specified, id, provider, scale, values, parent_values, &dependencies);

  if (values->values == NULL)
    values->values = g_ptr_array_new_full (_gtk_css_style_property_get_n_properties (),
					   (GDestroyNotify)_gtk_css_value_unref);
  if (id >= values->values->len)
   g_ptr_array_set_size (values->values, id + 1);

  if (g_ptr_array_index (values->values, id))
    _gtk_css_value_unref (g_ptr_array_index (values->values, id));
  g_ptr_array_index (values->values, id) = _gtk_css_value_ref (value);

  if (dependencies & (GTK_CSS_DEPENDS_ON_PARENT | GTK_CSS_EQUALS_PARENT))
    values->depends_on_parent = _gtk_bitmask_set (values->depends_on_parent, id, TRUE);
  if (dependencies & (GTK_CSS_EQUALS_PARENT))
    values->equals_parent = _gtk_bitmask_set (values->equals_parent, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_COLOR))
    values->depends_on_color = _gtk_bitmask_set (values->depends_on_color, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_FONT_SIZE))
    values->depends_on_font_size = _gtk_bitmask_set (values->depends_on_font_size, id, TRUE);

  if (values->sections && values->sections->len > id && g_ptr_array_index (values->sections, id))
    {
      gtk_css_section_unref (g_ptr_array_index (values->sections, id));
      g_ptr_array_index (values->sections, id) = NULL;
    }

  if (section)
    {
      if (values->sections == NULL)
        values->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (values->sections->len <= id)
        g_ptr_array_set_size (values->sections, id + 1);

      g_ptr_array_index (values->sections, id) = gtk_css_section_ref (section);
    }

  _gtk_css_value_unref (value);
  _gtk_css_value_unref (specified);
}

void
_gtk_css_computed_values_set_animated_value (GtkCssComputedValues *values,
                                             guint                 id,
                                             GtkCssValue          *value)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  gtk_internal_return_if_fail (value != NULL);

  if (values->animated_values == NULL)
    values->animated_values = g_ptr_array_new_with_free_func ((GDestroyNotify)_gtk_css_value_unref);
  if (id >= values->animated_values->len)
   g_ptr_array_set_size (values->animated_values, id + 1);

  if (g_ptr_array_index (values->animated_values, id))
    _gtk_css_value_unref (g_ptr_array_index (values->animated_values, id));
  g_ptr_array_index (values->animated_values, id) = _gtk_css_value_ref (value);

}

GtkCssValue *
_gtk_css_computed_values_get_value (GtkCssComputedValues *values,
                                    guint                 id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->animated_values &&
      id < values->animated_values->len &&
      g_ptr_array_index (values->animated_values, id))
    return g_ptr_array_index (values->animated_values, id);

  return _gtk_css_computed_values_get_intrinsic_value (values, id);
}

GtkCssValue *
_gtk_css_computed_values_get_intrinsic_value (GtkCssComputedValues *values,
                                              guint                 id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->values == NULL ||
      id >= values->values->len)
    return NULL;

  return g_ptr_array_index (values->values, id);
}

GtkCssSection *
_gtk_css_computed_values_get_section (GtkCssComputedValues *values,
                                      guint                 id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->sections == NULL ||
      id >= values->sections->len)
    return NULL;

  return g_ptr_array_index (values->sections, id);
}

GtkBitmask *
_gtk_css_computed_values_get_difference (GtkCssComputedValues *values,
                                         GtkCssComputedValues *other)
{
  GtkBitmask *result;
  guint i, len;

  len = MIN (values->values->len, other->values->len);
  result = _gtk_bitmask_new ();
  if (values->values->len != other->values->len)
    result = _gtk_bitmask_invert_range (result, len, MAX (values->values->len, other->values->len));
  
  for (i = 0; i < len; i++)
    {
      if (!_gtk_css_value_equal (g_ptr_array_index (values->values, i),
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
gtk_css_computed_values_find_transition (GtkCssComputedValues *values,
                                         guint                 property_id)
{
  GSList *list;

  for (list = values->animations; list; list = list->next)
    {
      if (!GTK_IS_CSS_TRANSITION (list->data))
        continue;

      if (_gtk_css_transition_get_property (list->data) == property_id)
        return list->data;
    }

  return NULL;
}

static void
gtk_css_computed_values_create_css_transitions (GtkCssComputedValues *values,
                                                gint64                timestamp,
                                                GtkCssComputedValues *source)
{
  TransitionInfo transitions[GTK_CSS_PROPERTY_N_PROPERTIES] = { { 0, } };
  GtkCssValue *durations, *delays, *timing_functions;
  guint i;

  transition_infos_set (transitions, _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_TRANSITION_PROPERTY));

  durations = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_TRANSITION_DURATION);
  delays = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_TRANSITION_DELAY);
  timing_functions = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION);

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

      start = _gtk_css_computed_values_get_intrinsic_value (source, i);
      end = _gtk_css_computed_values_get_intrinsic_value (values, i);
      if (_gtk_css_value_equal (start, end))
        {
          animation = gtk_css_computed_values_find_transition (GTK_CSS_COMPUTED_VALUES (source), i);
          if (animation)
            values->animations = g_slist_prepend (values->animations, g_object_ref (animation));
        }
      else
        {
          animation = _gtk_css_transition_new (i,
                                               _gtk_css_computed_values_get_value (source, i),
                                               _gtk_css_array_value_get_nth (timing_functions, i),
                                               timestamp + delay * G_USEC_PER_SEC,
                                               timestamp + (delay + duration) * G_USEC_PER_SEC);
          values->animations = g_slist_prepend (values->animations, animation);
        }
    }
}

static GtkStyleAnimation *
gtk_css_computed_values_find_animation (GtkCssComputedValues *values,
                                        const char           *name)
{
  GSList *list;

  for (list = values->animations; list; list = list->next)
    {
      if (!GTK_IS_CSS_ANIMATION (list->data))
        continue;

      if (g_str_equal (_gtk_css_animation_get_name (list->data), name))
        return list->data;
    }

  return NULL;
}

static void
gtk_css_computed_values_create_css_animations (GtkCssComputedValues    *values,
                                               GtkCssComputedValues    *parent_values,
                                               gint64                   timestamp,
                                               GtkStyleProviderPrivate *provider,
					       int                      scale,
                                               GtkCssComputedValues    *source)
{
  GtkCssValue *durations, *delays, *timing_functions, *animations;
  GtkCssValue *iteration_counts, *directions, *play_states, *fill_modes;
  guint i;

  animations = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_NAME);
  durations = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_DURATION);
  delays = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_DELAY);
  timing_functions = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION);
  iteration_counts = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT);
  directions = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_DIRECTION);
  play_states = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE);
  fill_modes = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_ANIMATION_FILL_MODE);

  for (i = 0; i < _gtk_css_array_value_get_n_values (animations); i++)
    {
      GtkStyleAnimation *animation;
      GtkCssKeyframes *keyframes;
      const char *name;
      
      name = _gtk_css_ident_value_get (_gtk_css_array_value_get_nth (animations, i));
      if (g_ascii_strcasecmp (name, "none") == 0)
        continue;

      animation = gtk_css_computed_values_find_animation (values, name);
      if (animation)
        continue;

      if (source)
        animation = gtk_css_computed_values_find_animation (source, name);

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

          keyframes = _gtk_css_keyframes_compute (keyframes, provider, scale, values, parent_values);

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
      values->animations = g_slist_prepend (values->animations, animation);
    }
}

/* PUBLIC API */

void
_gtk_css_computed_values_create_animations (GtkCssComputedValues    *values,
                                            GtkCssComputedValues    *parent_values,
                                            gint64                   timestamp,
                                            GtkStyleProviderPrivate *provider,
					    int                      scale,
                                            GtkCssComputedValues    *source)
{
  if (source != NULL)
    gtk_css_computed_values_create_css_transitions (values, timestamp, source);
  gtk_css_computed_values_create_css_animations (values, parent_values, timestamp, provider, scale, source);
}

GtkBitmask *
_gtk_css_computed_values_advance (GtkCssComputedValues *values,
                                  gint64                timestamp)
{
  GtkBitmask *changed;
  GPtrArray *old_computed_values;
  GSList *list;
  guint i;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);
  gtk_internal_return_val_if_fail (timestamp >= values->current_time, NULL);

  values->current_time = timestamp;
  old_computed_values = values->animated_values;
  values->animated_values = NULL;

  list = values->animations;
  while (list)
    {
      GtkStyleAnimation *animation = list->data;
      
      list = list->next;

      _gtk_style_animation_set_values (animation,
                                       timestamp,
                                       GTK_CSS_COMPUTED_VALUES (values));
      
      if (_gtk_style_animation_is_finished (animation, timestamp))
        {
          values->animations = g_slist_remove (values->animations, animation);
          g_object_unref (animation);
        }
    }

  /* figure out changes */
  changed = _gtk_bitmask_new ();

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      GtkCssValue *old_animated, *new_animated;

      old_animated = old_computed_values && i < old_computed_values->len ? g_ptr_array_index (old_computed_values, i) : NULL;
      new_animated = values->animated_values && i < values->animated_values->len ? g_ptr_array_index (values->animated_values, i) : NULL;

      if (!_gtk_css_value_equal0 (old_animated, new_animated))
        changed = _gtk_bitmask_set (changed, i, TRUE);
    }

  if (old_computed_values)
    g_ptr_array_unref (old_computed_values);

  return changed;
}

gboolean
_gtk_css_computed_values_is_static (GtkCssComputedValues *values)
{
  GSList *list;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), TRUE);

  for (list = values->animations; list; list = list->next)
    {
      if (!_gtk_style_animation_is_static (list->data, values->current_time))
        return FALSE;
    }

  return TRUE;
}

void
_gtk_css_computed_values_cancel_animations (GtkCssComputedValues *values)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));

  if (values->animated_values)
    {
      g_ptr_array_unref (values->animated_values);
      values->animated_values = NULL;
    }

  g_slist_free_full (values->animations, g_object_unref);
  values->animations = NULL;
}

GtkBitmask *
_gtk_css_computed_values_compute_dependencies (GtkCssComputedValues *values,
                                               const GtkBitmask     *parent_changes)
{
  GtkBitmask *changes;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), _gtk_bitmask_new ());

  changes = _gtk_bitmask_copy (parent_changes);
  changes = _gtk_bitmask_intersect (changes, values->depends_on_parent);
  if (_gtk_bitmask_get (changes, GTK_CSS_PROPERTY_COLOR))
    changes = _gtk_bitmask_union (changes, values->depends_on_color);
  if (_gtk_bitmask_get (changes, GTK_CSS_PROPERTY_FONT_SIZE))
    changes = _gtk_bitmask_union (changes, values->depends_on_font_size);

  return changes;
}

void
gtk_css_computed_values_print (GtkCssComputedValues *values,
                               GString              *string)
{
  guint i;

  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (string != NULL);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssSection *section = _gtk_css_computed_values_get_section (values, i);
      g_string_append (string, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (_gtk_css_style_property_lookup_by_id (i))));
      g_string_append (string, ": ");
      _gtk_css_value_print (_gtk_css_computed_values_get_value (values, i), string);
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
gtk_css_computed_values_to_string (GtkCssComputedValues *values)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  string = g_string_new ("");

  gtk_css_computed_values_print (values, string);

  return g_string_free (string, FALSE);
}

