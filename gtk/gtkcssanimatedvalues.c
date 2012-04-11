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

#include "gtkcssanimatedvaluesprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"

G_DEFINE_TYPE (GtkCssAnimatedValues, _gtk_css_animated_values, GTK_TYPE_CSS_COMPUTED_VALUES)

static void
gtk_css_animated_values_dispose (GObject *object)
{
  GtkCssAnimatedValues *values = GTK_CSS_ANIMATED_VALUES (object);

  g_clear_object (&values->computed);
  g_slist_free_full (values->animations, g_object_unref);
  values->animations = NULL;

  G_OBJECT_CLASS (_gtk_css_animated_values_parent_class)->dispose (object);
}

static void
_gtk_css_animated_values_class_init (GtkCssAnimatedValuesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_css_animated_values_dispose;
}

static void
_gtk_css_animated_values_init (GtkCssAnimatedValues *animated_values)
{
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
gtk_css_animated_values_find_transition (GtkCssAnimatedValues *values,
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
gtk_css_animated_values_start_transitions (GtkCssAnimatedValues *values,
                                           gint64                timestamp,
                                           GtkCssComputedValues *source)
{
  TransitionInfo transitions[GTK_CSS_PROPERTY_N_PROPERTIES] = { { 0, } };
  GtkCssComputedValues *source_computed, *computed;
  GtkCssValue *durations, *delays, *timing_functions;
  guint i;

  computed = GTK_CSS_COMPUTED_VALUES (values);
  if (GTK_IS_CSS_ANIMATED_VALUES (source))
    source_computed = GTK_CSS_ANIMATED_VALUES (source)->computed;
  else
    source_computed = source;

  transition_infos_set (transitions, _gtk_css_computed_values_get_value (computed, GTK_CSS_PROPERTY_TRANSITION_PROPERTY));

  durations = _gtk_css_computed_values_get_value (computed, GTK_CSS_PROPERTY_TRANSITION_DURATION);
  delays = _gtk_css_computed_values_get_value (computed, GTK_CSS_PROPERTY_TRANSITION_DELAY);
  timing_functions = _gtk_css_computed_values_get_value (computed, GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION);


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

      start = _gtk_css_computed_values_get_value (source_computed, i);
      end = _gtk_css_computed_values_get_value (values->computed, i);
      if (_gtk_css_value_equal (start, end))
        {
          if (GTK_IS_CSS_ANIMATED_VALUES (source))
            {
              animation = gtk_css_animated_values_find_transition (GTK_CSS_ANIMATED_VALUES (source), i);
              if (animation)
                values->animations = g_slist_prepend (values->animations, g_object_ref (animation));
            }
        }
      else
        {
          animation = _gtk_css_transition_new (i,
                                               start,
                                               end,
                                               _gtk_css_array_value_get_nth (timing_functions, i),
                                               timestamp + delay * G_USEC_PER_SEC,
                                               timestamp + (delay + duration) * G_USEC_PER_SEC);
          values->animations = g_slist_prepend (values->animations, animation);
        }
    }
}

/* PUBLIC API */

static void
gtk_css_animated_values_start_animations (GtkCssAnimatedValues *values,
                                          gint64                timestamp,
                                          GtkCssComputedValues *source)
{
  gtk_css_animated_values_start_transitions (values, timestamp, source);
}

GtkCssComputedValues *
_gtk_css_animated_values_new (GtkCssComputedValues *computed,
                              GtkCssComputedValues *source,
                              gint64                timestamp)
{
  GtkCssAnimatedValues *values;
  GtkCssValue *value;
  GtkBitmask *ignore;
  guint i;

  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (computed), NULL);
  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (source), NULL);

  values = g_object_new (GTK_TYPE_CSS_ANIMATED_VALUES, NULL);

  values->computed = g_object_ref (computed);
  for (i = 0; ; i++)
    {
      value = _gtk_css_computed_values_get_value (computed, i);

      if (value == NULL)
        break;

      _gtk_css_computed_values_set_value (GTK_CSS_COMPUTED_VALUES (values), 
                                          i,
                                          value,
                                          _gtk_css_computed_values_get_section (computed, i));
    }

  gtk_css_animated_values_start_animations (values, timestamp, source);

  ignore = _gtk_css_animated_values_advance (values, timestamp);
  _gtk_bitmask_free (ignore);

  return GTK_CSS_COMPUTED_VALUES (values);
}

GtkBitmask *
_gtk_css_animated_values_advance (GtkCssAnimatedValues *values,
                                  gint64                timestamp)
{
  GtkBitmask *changed;
  GSList *list;

  g_return_val_if_fail (GTK_IS_CSS_ANIMATED_VALUES (values), NULL);
  g_return_val_if_fail (timestamp >= values->current_time, NULL);

  changed = _gtk_bitmask_new ();

  values->current_time = timestamp;

  list = values->animations;
  while (list)
    {
      GtkStyleAnimation *animation = list->data;
      
      list = list->next;

      changed = _gtk_style_animation_set_values (animation,
                                                 changed,
                                                 timestamp,
                                                 GTK_CSS_COMPUTED_VALUES (values));
      
      if (_gtk_style_animation_is_finished (animation, timestamp))
        {
          values->animations = g_slist_remove (values->animations, animation);
          g_object_unref (animation);
        }
    }
  
  return changed;
}

gboolean
_gtk_css_animated_values_is_finished (GtkCssAnimatedValues *values)
{
  g_return_val_if_fail (GTK_IS_CSS_ANIMATED_VALUES (values), TRUE);

  return values->animations == NULL;
}
