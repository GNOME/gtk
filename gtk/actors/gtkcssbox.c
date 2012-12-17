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

#include "gtkcssboxprivate.h"

#include "gtkcssenumvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtklayoutmanagerprivate.h"
#include "gtkprivate.h"
#include "gtkstylecontext.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"

/* state flags that when set will automatically be set on all parents */
#define GTK_STATE_FLAGS_PROPAGATE_TO_PARENT (GTK_STATE_FLAG_PRELIGHT)

/* state flags that when set will automatically propagate to all children */
#define GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN (GTK_STATE_FLAG_INSENSITIVE | GTK_STATE_FLAG_BACKDROP)

G_STATIC_ASSERT((GTK_STATE_FLAGS_PROPAGATE_TO_PARENT & GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN) == 0);

#define GTK_STATE_FLAGS_NO_PROPAGATE (~(GTK_STATE_FLAGS_PROPAGATE_TO_PARENT | GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN))

struct _GtkCssBoxPrivate {
  GtkStateFlags    state;

  char *           id;
};

enum
{
  PROP_0,

  PROP_EFFECTIVE_STATE,
  PROP_STATE,
  PROP_ID,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (GtkCssBox, _gtk_css_box, GTK_TYPE_CSS_ACTOR)

static void
gtk_css_box_set_effective_state (GtkCssBox     *box,
                                 GtkStateFlags  state)
{
  gtk_style_context_set_state (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (box)), state);
  g_object_notify_by_pspec (G_OBJECT (box), obj_props[PROP_EFFECTIVE_STATE]);
}

GtkStateFlags
_gtk_css_box_get_effective_state (GtkCssBox *self)
{
  g_return_val_if_fail (GTK_IS_CSS_BOX (self), 0);

  return gtk_style_context_get_state (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)));
}

static void
gtk_css_box_update_state_on_parent (GtkCssBox     *box,
                                    GtkStateFlags  state_to_set,
                                    GtkStateFlags  state_to_unset)
{
  GtkStateFlags effective;
  GtkActor *iter, *parent;

  effective = _gtk_css_box_get_effective_state (box);
  state_to_set = ~effective & state_to_set;
  state_to_unset = effective & state_to_unset;

  for (iter = _gtk_actor_get_first_child (GTK_ACTOR (box));
       iter && state_to_unset != 0;
       iter = _gtk_actor_get_next_sibling (iter))
    {
      if (!GTK_IS_CSS_BOX (iter))
        continue;

      state_to_unset &= ~_gtk_css_box_get_effective_state (GTK_CSS_BOX (iter));
    }

  if (state_to_set == 0 && state_to_unset == 0)
    return;

  gtk_css_box_set_effective_state (box, (effective | state_to_set) & ~state_to_unset);

  parent = _gtk_actor_get_parent (GTK_ACTOR (box));
  if (GTK_IS_CSS_BOX (parent))
    gtk_css_box_update_state_on_parent (GTK_CSS_BOX (parent), state_to_set, state_to_unset);
}

static void
gtk_css_box_update_state_on_children (GtkCssBox     *box,
                                      GtkStateFlags  state_to_set,
                                      GtkStateFlags  state_to_unset)
{
  GtkStateFlags effective;
  GtkActor *iter;

  effective = _gtk_css_box_get_effective_state (box);
  state_to_set = ~effective & state_to_set;
  state_to_unset = effective & state_to_unset;
  
  gtk_css_box_set_effective_state (box, (effective | state_to_set) & ~state_to_unset);

  if (state_to_set == 0 && state_to_unset == 0)
    return;
  
  for (iter = _gtk_actor_get_first_child (GTK_ACTOR (box));
       iter;
       iter = _gtk_actor_get_next_sibling (iter))
    {
      if (!GTK_IS_CSS_BOX (iter))
        continue;

      gtk_css_box_update_state_on_children (GTK_CSS_BOX (iter), state_to_set, state_to_unset);
    }
}

static void
gtk_css_box_finalize (GObject *object)
{
  GtkCssBox *self = GTK_CSS_BOX (object);
  GtkCssBoxPrivate *priv = self->priv;

  g_free (priv->id);

  G_OBJECT_CLASS (_gtk_css_box_parent_class)->finalize (object);
}

static void
gtk_css_box_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkCssBox *css_box = GTK_CSS_BOX (object);

  switch (prop_id)
    {
    case PROP_STATE:
      _gtk_css_box_set_state (css_box, g_value_get_enum (value));
      break;

    case PROP_ID:
      _gtk_css_box_set_id (css_box, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_box_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkCssBox *css_box = GTK_CSS_BOX (object);

  switch (prop_id)
    {
    case PROP_EFFECTIVE_STATE:
      g_value_set_enum (value, _gtk_css_box_get_effective_state (css_box));
      break;

    case PROP_STATE:
      g_value_set_enum (value, _gtk_css_box_get_state (css_box));
      break;

    case PROP_ID:
      g_value_set_string (value, _gtk_css_box_get_id (css_box));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_box_queue_restyle (GtkCssBox    *self,
                           GtkCssChange  change)
{
  _gtk_style_context_queue_invalidate (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)), change);
}

static void
gtk_css_box_queue_restyle_siblings (GtkCssBox    *self,
                                    GtkCssChange  change)
{
  GtkActor *iter;

  for (iter = _gtk_actor_get_previous_sibling (GTK_ACTOR (self));
       iter;
       iter = _gtk_actor_get_previous_sibling (iter))
    {
      if (GTK_IS_CSS_BOX (iter))
        gtk_css_box_queue_restyle (GTK_CSS_BOX (iter), change);
    }

  for (iter = _gtk_actor_get_next_sibling (GTK_ACTOR (self));
       iter;
       iter = _gtk_actor_get_next_sibling (iter))
    {
      if (GTK_IS_CSS_BOX (iter))
        gtk_css_box_queue_restyle (GTK_CSS_BOX (iter), change);
    }
}

static void
gtk_css_box_real_show (GtkActor *self)
{
  GtkCssBox *box = GTK_CSS_BOX (self);

  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->show (self);

  gtk_css_box_queue_restyle_siblings (box, GTK_CSS_CHANGE_ANY_SIBLING);
}

static void
gtk_css_box_real_hide (GtkActor *self)
{
  GtkCssBox *box = GTK_CSS_BOX (self);

  gtk_css_box_queue_restyle_siblings (box, GTK_CSS_CHANGE_ANY_SIBLING);

  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->hide (self);
}

static void
gtk_css_box_real_map (GtkActor *self)
{
  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->map (self);
  
  _gtk_style_context_update_animating (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)));
}

static void
gtk_css_box_real_unmap (GtkActor *self)
{
  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->unmap (self);
  
  _gtk_style_context_update_animating (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)));
}

static void 
gtk_css_box_real_parent_set (GtkActor *self,
                             GtkActor *old_parent)
{
  GtkCssBox *box = GTK_CSS_BOX (self);
  GtkActor *parent;

  parent = _gtk_actor_get_parent (self);

  if (old_parent == NULL)
    {
      g_warn_if_fail (GTK_IS_CSS_BOX (parent));
    }

  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->parent_set (self, old_parent);

  gtk_style_context_set_parent (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)),
                                parent ? _gtk_css_actor_get_style_context (GTK_CSS_ACTOR (parent)) : NULL);

  if (parent)
    {
      gtk_css_box_update_state_on_children (box, 
                                            _gtk_css_box_get_effective_state (GTK_CSS_BOX (parent)) & GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN,
                                            0);
      gtk_css_box_update_state_on_parent (GTK_CSS_BOX (parent), 
                                          _gtk_css_box_get_effective_state (box) & GTK_STATE_FLAGS_PROPAGATE_TO_PARENT,
                                          0);
    }
  else
    {
      gtk_css_box_update_state_on_children (box, 
                                            0,
                                            _gtk_css_box_get_effective_state (GTK_CSS_BOX (old_parent)) & GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN);
      gtk_css_box_update_state_on_parent (GTK_CSS_BOX (old_parent), 
                                          0,
                                          _gtk_css_box_get_effective_state (box) & GTK_STATE_FLAGS_PROPAGATE_TO_PARENT);
    }
}

static double
gtk_css_box_get_edge (GtkCssBox  *self,
                      GtkCssSide  side)
{
  const struct {
    guint margin;
    guint border;
    guint border_style;
    guint padding;
  } properties[] = {
    /* [GTK_CSS_TOP]    = */ { GTK_CSS_PROPERTY_MARGIN_TOP,
                               GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
                               GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
                               GTK_CSS_PROPERTY_PADDING_TOP },
    /* [GTK_CSS_RIGHT]  = */ { GTK_CSS_PROPERTY_MARGIN_RIGHT,
                               GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                               GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
                               GTK_CSS_PROPERTY_PADDING_RIGHT },
    /* [GTK_CSS_BOTTOM] = */ { GTK_CSS_PROPERTY_MARGIN_BOTTOM,
                               GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                               GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                               GTK_CSS_PROPERTY_PADDING_BOTTOM },
    /* [GTK_CSS_LEFT]   = */ { GTK_CSS_PROPERTY_MARGIN_LEFT,
                               GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
                               GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
                               GTK_CSS_PROPERTY_PADDING_LEFT }
  };
  GtkStyleContext *context;
  GtkBorderStyle border_style;
  double result;

  context = _gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self));

  result = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, properties[side].margin), 100)
           + _gtk_css_number_value_get (_gtk_style_context_peek_property (context, properties[side].padding), 100);

  border_style = _gtk_css_border_style_value_get (_gtk_style_context_peek_property (context, properties[side].border_style));
  if (border_style != GTK_BORDER_STYLE_NONE && border_style != GTK_BORDER_STYLE_HIDDEN)
    result += _gtk_css_number_value_get (_gtk_style_context_peek_property (context, properties[side].border), 100);

  return result;
}

static void
adjust_size_for_css (GtkCssBox      *self,
                     GtkOrientation  orientation,
                     gfloat         *min_size_p,
                     gfloat         *nat_size_p)
{
  GtkStyleContext *context;
  gfloat min_css, nat_css;

  context = _gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self));

  min_css = _gtk_css_number_value_get (
                _gtk_style_context_peek_property (context,
                      orientation == GTK_ORIENTATION_HORIZONTAL ? GTK_CSS_PROPERTY_MIN_WIDTH
                                                                : GTK_CSS_PROPERTY_MIN_HEIGHT), 0);
  nat_css = _gtk_css_number_value_get (
                _gtk_style_context_peek_property (context,
                      orientation == GTK_ORIENTATION_HORIZONTAL ? GTK_CSS_PROPERTY_MAX_WIDTH 
                                                                : GTK_CSS_PROPERTY_MAX_HEIGHT), 0);

  *min_size_p = MAX (min_css, *min_size_p);
  *nat_size_p = MIN (nat_css, *nat_size_p);
  *nat_size_p = MAX (*min_size_p, *nat_size_p);
}

static void
gtk_css_box_real_get_preferred_size (GtkActor       *self,
                                     GtkOrientation  orientation,
                                     gfloat          for_size,
                                     gfloat         *min_size_p,
                                     gfloat         *nat_size_p)
{
  GtkCssBox *box = GTK_CSS_BOX (self);
  float extra_size;

  if (for_size >= 0)
    {
      if (orientation == GTK_ORIENTATION_VERTICAL)
        extra_size = gtk_css_box_get_edge (box, GTK_CSS_LEFT) + gtk_css_box_get_edge (box, GTK_CSS_RIGHT);
      else
        extra_size = gtk_css_box_get_edge (box, GTK_CSS_TOP) + gtk_css_box_get_edge (box, GTK_CSS_BOTTOM);

      for_size = MAX (0, for_size - extra_size);
    }

  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->get_preferred_size (self, orientation, for_size, min_size_p, nat_size_p);

  adjust_size_for_css (box, orientation, min_size_p, nat_size_p);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    extra_size = gtk_css_box_get_edge (box, GTK_CSS_LEFT) + gtk_css_box_get_edge (box, GTK_CSS_RIGHT);
  else
    extra_size = gtk_css_box_get_edge (box, GTK_CSS_TOP) + gtk_css_box_get_edge (box, GTK_CSS_BOTTOM);

  *min_size_p = MAX (0, *min_size_p + extra_size);
  *nat_size_p = MAX (0, *nat_size_p + extra_size);
}

static void
gtk_css_box_real_allocate (GtkActor             *actor,
                           const cairo_matrix_t *position,
                           gfloat                width,
                           gfloat                height)
{
  GtkCssBox *self = GTK_CSS_BOX (actor);
  GtkLayoutManager *layout_manager;

  layout_manager = _gtk_actor_get_layout_manager (actor);

  if (layout_manager)
    {
      cairo_matrix_t transform;
      double top, left, bottom, right;

      top = gtk_css_box_get_edge (self, GTK_CSS_TOP);
      left = gtk_css_box_get_edge (self, GTK_CSS_LEFT);
      bottom = gtk_css_box_get_edge (self, GTK_CSS_BOTTOM);
      right = gtk_css_box_get_edge (self, GTK_CSS_RIGHT);

      cairo_matrix_init_translate (&transform, left, top);

      _gtk_layout_manager_allocate (layout_manager,
                                    &transform,
                                    width - left - right,
                                    height - top - bottom);
    }
  
  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->allocate (actor, position, width, height);
}

static void
gtk_css_box_real_draw (GtkActor *actor,
                       cairo_t  *cr)
{
  GtkCssBox *self = GTK_CSS_BOX (actor);
  GtkStyleContext *context;
  double width, height;
  double top, left, bottom, right;

  context = _gtk_css_actor_get_style_context (GTK_CSS_ACTOR (actor));
  top = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_MARGIN_TOP), 100);
  right = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_MARGIN_RIGHT), 100);
  bottom = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_MARGIN_BOTTOM), 100);
  left = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_MARGIN_LEFT), 100);
  width = _gtk_actor_get_width (actor) - left - right;
  height = _gtk_actor_get_height (actor) - bottom - top;

  gtk_render_background (context,
                         cr,
                         left, top,
                         width, height);
  gtk_render_frame (context,
                    cr,
                    left, top,
                    width, height);

  cairo_translate (cr,
                   gtk_css_box_get_edge (self, GTK_CSS_LEFT),
                   gtk_css_box_get_edge (self, GTK_CSS_TOP));

  GTK_ACTOR_CLASS (_gtk_css_box_parent_class)->draw (actor, cr);
}

static void
gtk_css_box_real_screen_changed (GtkActor  *actor,
                                 GdkScreen *new_screen,
                                 GdkScreen *old_screen)
{
  GtkStyleContext *context;

  context = _gtk_css_actor_get_style_context (GTK_CSS_ACTOR (actor));

  gtk_style_context_set_screen (context, new_screen);
}

static void
gtk_css_box_real_style_updated (GtkCssActor      *actor,
                                const GtkBitmask *changes)
{
  if (_gtk_css_style_property_changes_affect_size (changes))
    _gtk_actor_queue_relayout (GTK_ACTOR (actor));
  else
    _gtk_actor_queue_redraw (GTK_ACTOR (actor));
}

static void
_gtk_css_box_class_init (GtkCssBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkActorClass *actor_class = GTK_ACTOR_CLASS (klass);
  GtkCssActorClass *css_actor_class = GTK_CSS_ACTOR_CLASS (klass);

  object_class->finalize = gtk_css_box_finalize;
  object_class->set_property = gtk_css_box_set_property;
  object_class->get_property = gtk_css_box_get_property;

  actor_class->show = gtk_css_box_real_show;
  actor_class->hide = gtk_css_box_real_hide;
  actor_class->map = gtk_css_box_real_map;
  actor_class->unmap = gtk_css_box_real_unmap;
  actor_class->draw = gtk_css_box_real_draw;
  actor_class->parent_set = gtk_css_box_real_parent_set;
  actor_class->get_preferred_size = gtk_css_box_real_get_preferred_size;
  actor_class->allocate = gtk_css_box_real_allocate;
  actor_class->screen_changed = gtk_css_box_real_screen_changed;

  css_actor_class->style_updated = gtk_css_box_real_style_updated;

  /**
   * GtkCssBox:state:
   *
   * The state flags explicitly set on the box.
   *
   * See also GtkCssBox:effective-state.
   */
  obj_props[PROP_STATE] =
    g_param_spec_flags ("state",
                        P_("State"),
                        P_("State flags set on the box"),
                        GTK_TYPE_STATE_FLAGS,
                        0,
                        GTK_PARAM_READWRITE);

  /**
   * GtkCssBox:effective-state:
   *
   * The state flags explicitly set on the box. These are the flags cpomputed
   * from the box's own state and propagated state from parent or children boxes.
   *
   * See also GtkCssBox:state.
   */
  obj_props[PROP_EFFECTIVE_STATE] =
    g_param_spec_flags ("effective-state",
                        P_("Effective State"),
                        P_("Effective state computed for the box"),
                        GTK_TYPE_STATE_FLAGS,
                        0,
                        GTK_PARAM_READWRITE);

  /**
   * GtkCssBox:id:
   *
   * The ID given to this box or %NULL if the box does not have an ID. This is the
   * default.
   *
   * The ID is used for matching in CSS and is supposed to be unique for an actor tree.
   * Currently this is not enforced, but it might be in the future.
   */
  obj_props[PROP_ID] =
    g_param_spec_string ("id",
                        P_("ID"),
                        P_("ID given to this actor"),
                        NULL,
                        GTK_PARAM_READWRITE);

  g_type_class_add_private (klass, sizeof (GtkCssBoxPrivate));
}

static void
_gtk_css_box_init (GtkCssBox *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GTK_TYPE_CSS_BOX,
                                            GtkCssBoxPrivate);

  _gtk_css_actor_init_box (GTK_CSS_ACTOR (self));
}

GtkActor *
_gtk_css_box_new (void)
{
  return g_object_new (GTK_TYPE_CSS_BOX, NULL);
}

void
_gtk_css_box_set_state (GtkCssBox     *self,
                        GtkStateFlags  state)
{
  GtkCssBoxPrivate *priv;
  GtkStateFlags changed;

  g_return_if_fail (GTK_IS_CSS_BOX (self));

  priv = self->priv;

  if (priv->state == state)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  changed = priv->state ^ state;

  priv->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_STATE]);

  if (changed & GTK_STATE_FLAGS_PROPAGATE_TO_PARENT)
    gtk_css_box_update_state_on_parent (self,
                                        state & changed & GTK_STATE_FLAGS_PROPAGATE_TO_PARENT,
                                        ~state & changed & GTK_STATE_FLAGS_PROPAGATE_TO_PARENT);
  if (changed & GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN)
    gtk_css_box_update_state_on_children (self,
                                          state & changed & GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN,
                                          ~state & changed & GTK_STATE_FLAGS_PROPAGATE_TO_CHILDREN);

  gtk_css_box_set_effective_state (self,
                                   (_gtk_css_box_get_effective_state (self) & GTK_STATE_FLAGS_NO_PROPAGATE)
                                   | (state & ~GTK_STATE_FLAGS_NO_PROPAGATE));

  g_object_thaw_notify (G_OBJECT (self));
}

GtkStateFlags
_gtk_css_box_get_state (GtkCssBox *self)
{
  g_return_val_if_fail (GTK_IS_CSS_BOX (self), 0);

  return self->priv->state;
}

void
_gtk_css_box_add_class (GtkCssBox   *self,
                        const gchar *class_name)
{
  g_return_if_fail (GTK_IS_CSS_BOX (self));
  g_return_if_fail (class_name != NULL);

  gtk_style_context_add_class (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)), class_name);
}

void
_gtk_css_box_remove_class (GtkCssBox   *self,
                           const gchar *class_name)
{
  g_return_if_fail (GTK_IS_CSS_BOX (self));
  g_return_if_fail (class_name != NULL);

  gtk_style_context_remove_class (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)), class_name);
}

gboolean
_gtk_css_box_has_class (GtkCssBox   *self,
                        const gchar *class_name)
{
  g_return_val_if_fail (GTK_IS_CSS_BOX (self), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  return gtk_style_context_has_class (_gtk_css_actor_get_style_context (GTK_CSS_ACTOR (self)), class_name);
}

void
_gtk_css_box_set_id (GtkCssBox  *self,
                     const char *id)
{
  GtkCssBoxPrivate *priv;

  g_return_if_fail (GTK_IS_CSS_BOX (self));

  priv = self->priv;

  if (g_strcmp0 (priv->id, id) == 0)
    return;

  g_free (priv->id);
  priv->id = g_strdup (id);

  gtk_css_box_queue_restyle (self, GTK_CSS_CHANGE_NAME);
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_ID]);
}

const char *
_gtk_css_box_get_id (GtkCssBox *self)
{
  g_return_val_if_fail (GTK_IS_CSS_BOX (self), NULL);

  return self->priv->id;
}
