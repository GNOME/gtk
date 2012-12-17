/*
 * Copyright © 2006, 2007, 2008 OpenedHand Ltd
 * Copyright © 2009, 2010 Intel Corp
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

#include "gtkactorprivate.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtksizerequestcacheprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"
#include "actors/gtklayoutmanagerprivate.h"

struct _GtkActorPrivate {
  SizeRequestCache requests;

  GtkLayoutManager *layout_manager;

  GtkWidget *widget;

  cairo_matrix_t transform;
  gfloat width;
  gfloat height;

  /* scene graph */
  GtkActor *parent;
  GtkActor *prev_sibling;
  GtkActor *next_sibling;
  GtkActor *first_child;
  GtkActor *last_child;

  gint n_children;

  /* tracks whenever the children of an actor are changed; the
   * age is incremented by 1 whenever an actor is added or
   * removed. the age is not incremented when the first or the
   * last child pointers are changed, or when grandchildren of
   * an actor are changed.
   */
  gint age;

  /* the text direction configured for this child */
  GtkTextDirection text_direction;

  /* visibility flags */
  guint visible                     : 1;
  guint mapped                      : 1;
  guint realized                    : 1;
  /* cached request is invalid (implies allocation is too) */
  guint needs_width_request         : 1;
  /* cached request is invalid (implies allocation is too) */
  guint needs_height_request        : 1;
  /* cached allocation is invalid (request has changed, probably) */
  guint needs_allocation            : 1;
  guint needs_compute_expand        : 1;
  guint needs_x_expand              : 1;
  guint needs_y_expand              : 1;
};

enum
{
  PROP_0,

  PROP_VISIBLE,
  PROP_MAPPED,
  PROP_REALIZED,

  PROP_WIDTH,
  PROP_HEIGHT,

  PROP_TEXT_DIRECTION,

  PROP_LAYOUT_MANAGER,
  PROP_WIDGET,

  PROP_FIRST_CHILD,
  PROP_LAST_CHILD,

  PROP_LAST
};

enum {
  ACTOR_ADDED,
  ACTOR_REMOVED,

  LAST_SIGNAL
};

static GParamSpec *obj_props[PROP_LAST];
static guint       actor_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkActor, _gtk_actor, G_TYPE_INITIALLY_UNOWNED)

static void
gtk_actor_dispose (GObject *object)
{
  GtkActor *self = GTK_ACTOR (object);
  GtkActorPrivate *priv = self->priv;

  _gtk_actor_set_layout_manager (self, NULL);

  if (priv->widget)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->widget), (gpointer *) &priv->widget);
      priv->widget = NULL;
    }

  G_OBJECT_CLASS (_gtk_actor_parent_class)->dispose (object);
}

static void
gtk_actor_finalize (GObject *object)
{
  GtkActor *self = GTK_ACTOR (object);
  GtkActorPrivate *priv = self->priv;

  _gtk_size_request_cache_free (&priv->requests);

  G_OBJECT_CLASS (_gtk_actor_parent_class)->finalize (object);
}

static void
gtk_actor_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkActor *actor = GTK_ACTOR (object);
  GtkActorPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_VISIBLE:
      _gtk_actor_set_visible (actor, g_value_get_boolean (value));
      break;

    case PROP_TEXT_DIRECTION:
      _gtk_actor_set_text_direction (actor, g_value_get_enum (value));
      break;

    case PROP_LAYOUT_MANAGER:
      _gtk_actor_set_layout_manager (actor, g_value_get_object (value));
      break;

    case PROP_WIDGET:
      priv->widget = g_value_get_object (value);
      if (priv->widget)
        g_object_add_weak_pointer (G_OBJECT (priv->widget), (gpointer *) &priv->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_actor_get_property (GObject    *object,
			guint       prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
  GtkActor *actor = GTK_ACTOR (object);
  GtkActorPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_VISIBLE:
      g_value_set_boolean (value, _gtk_actor_get_visible (actor));
      break;

    case PROP_MAPPED:
      g_value_set_boolean (value, _gtk_actor_get_mapped (actor));
      break;

    case PROP_REALIZED:
      g_value_set_boolean (value, _gtk_actor_get_realized (actor));
      break;

    case PROP_WIDTH:
      g_value_set_float (value, _gtk_actor_get_width (actor));
      break;

    case PROP_HEIGHT:
      g_value_set_float (value, _gtk_actor_get_height (actor));
      break;

    case PROP_TEXT_DIRECTION:
      g_value_set_enum (value, _gtk_actor_get_text_direction (actor));
      break;

    case PROP_LAYOUT_MANAGER:
      g_value_set_object (value, _gtk_actor_get_layout_manager (actor));
      break;

    case PROP_WIDGET:
      g_value_set_object (value, _gtk_actor_get_widget (actor));
      break;

    case PROP_FIRST_CHILD:
      g_value_set_object (value, priv->first_child);
      break;

    case PROP_LAST_CHILD:
      g_value_set_object (value, priv->last_child);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_actor_queue_only_relayout (GtkActor *self)
{
  GtkActorPrivate *priv = self->priv;

  if (priv->needs_allocation &&
      _gtk_size_request_cache_is_empty (&priv->requests))
    return; /* save some cpu cycles */

  GTK_ACTOR_GET_CLASS (self)->queue_relayout (self);
}

static void
gtk_actor_real_queue_relayout (GtkActor *self)
{
  GtkActorPrivate *priv = self->priv;

  priv->needs_allocation = TRUE;
  /* reset the cached size requests and request mode */
  _gtk_size_request_cache_clear (&priv->requests);

  /* We need to go all the way up the hierarchy */
  if (priv->parent != NULL)
    {
      gtk_actor_queue_only_relayout (priv->parent);
    }
  else
    {
      GtkWidget *widget = _gtk_actor_get_widget (self);

      if (widget)
        gtk_widget_queue_resize (widget);
    }
}

static gboolean
matrix_is_translation (const cairo_matrix_t *matrix)
{
    return (matrix->xx == 1.0 && matrix->yx == 0.0 &&
	    matrix->xy == 0.0 && matrix->yy == 1.0);
}

static void
transform_rectangle (cairo_rectangle_t       *dest,
                     const cairo_rectangle_t *src,
                     const cairo_matrix_t    *matrix)
{
  if (matrix_is_translation (matrix))
    {
      dest->x = src->x + matrix->x0;
      dest->y = src->y + matrix->y0;
      dest->width = src->width;
      dest->height = src->height;
    }
  else
    {
      double x0, x1, x2, x3;
      double y0, y1, y2, y3;

      x0 = src->x;
      y0 = src->y;
      x1 = src->x + src->width;
      y1 = src->y;
      x2 = src->x + src->width;
      y2 = src->y + src->height;
      x3 = src->x;
      y3 = src->y + src->height;
      cairo_matrix_transform_point (matrix, &x0, &y0);
      cairo_matrix_transform_point (matrix, &x1, &y1);
      cairo_matrix_transform_point (matrix, &x2, &y2);
      cairo_matrix_transform_point (matrix, &x3, &y3);

      dest->x = MIN (MIN (x0, x1), MIN (x2, x3));
      dest->y = MIN (MIN (y0, y1), MIN (y2, y3));
      dest->width = MAX (MAX (x0, x1), MAX (x2, x3)) - dest->x;
      dest->height = MAX (MAX (y0, y1), MAX (y2, y3)) - dest->y;
    }
}

static void
gtk_actor_real_queue_redraw (GtkActor                *self,
                             const cairo_rectangle_t *box)
{
  GtkActorPrivate *priv = self->priv;

  /* We need to go all the way up the hierarchy */
  if (priv->parent != NULL)
    {
      cairo_rectangle_t parent_box;

      transform_rectangle (&parent_box, box, &priv->transform);
      _gtk_actor_queue_redraw_area (priv->parent, &parent_box);
    }
  else
    {
      g_warning ("Toplevel actor %s %p failed to queue a redraw", G_OBJECT_TYPE_NAME (self), self);
    }
}

static void
gtk_actor_real_show (GtkActor *self)
{
  self->priv->visible = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_VISIBLE]);
}

static void
gtk_actor_real_hide (GtkActor *self)
{
  self->priv->visible = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_VISIBLE]);
}

static void
gtk_actor_real_map (GtkActor *self)
{
  GtkActorPrivate *priv = self->priv;
  GtkActor *iter;

  priv->mapped = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_MAPPED]);

  for (iter = self->priv->first_child;
       iter != NULL;
       iter = iter->priv->next_sibling)
    {
      _gtk_actor_map (iter);
    }
}

static void
gtk_actor_real_unmap (GtkActor *self)
{
  GtkActorPrivate *priv = self->priv;
  GtkActor *iter;

  for (iter = priv->first_child;
       iter != NULL;
       iter = iter->priv->next_sibling)
    {
      _gtk_actor_unmap (iter);
    }

  priv->mapped = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_MAPPED]);
}

static void
gtk_actor_real_realize (GtkActor *self)
{
  GtkActorPrivate *priv = self->priv;

  priv->realized = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_REALIZED]);
}

static void
gtk_actor_real_unrealize (GtkActor *self)
{
  GtkActorPrivate *priv = self->priv;

  priv->realized = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_REALIZED]);
}

void
gtk_actor_real_draw (GtkActor *self,
                     cairo_t  *cr)
{
  GtkActorPrivate *priv = self->priv;
  GtkActor *iter;

  for (iter = priv->first_child;
       iter != NULL;
       iter = iter->priv->next_sibling)
    {
      cairo_save (cr);
      
      cairo_transform (cr, &iter->priv->transform);

      _gtk_actor_draw (iter, cr);

      cairo_restore (cr);
    }
}

static void 
gtk_actor_real_parent_set (GtkActor *self,
                           GtkActor *old_parent)
{
}

static GtkSizeRequestMode
gtk_actor_real_get_request_mode (GtkActor *self)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_actor_real_get_preferred_size (GtkActor       *self,
                                   GtkOrientation  orientation,
                                   gfloat          for_size,
                                   gfloat         *min_size_p,
                                   gfloat         *nat_size_p)
{
  GtkActorPrivate *priv = self->priv;

  if (priv->layout_manager)
    {
      _gtk_layout_manager_get_preferred_size (priv->layout_manager,
                                              orientation,
                                              for_size,
                                              min_size_p,
                                              nat_size_p);
    }
  else
    {
      *min_size_p = 0;
      *nat_size_p = 0;
    }
}

static void
gtk_actor_real_allocate (GtkActor             *self,
                         const cairo_matrix_t *position,
                         gfloat                width,
                         gfloat                height)
{
  GtkActorPrivate *priv = self->priv;

  if (priv->layout_manager &&
      GTK_ACTOR_GET_CLASS (self)->allocate == gtk_actor_real_allocate)
    {
      cairo_matrix_t identity = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };

      _gtk_layout_manager_allocate (priv->layout_manager,
                                    &identity,
                                    width, height);
    }

  /* _gtk_actor_allocate() freezes the notify queue, so we can notify
   * before actually setting the property here */
  if (priv->width != width)
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_WIDTH]);
  if (priv->height != height)
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_HEIGHT]);

  priv->transform = *position;
  priv->width = width;
  priv->height = height;
  priv->needs_allocation = FALSE;
}

static void
gtk_actor_real_screen_changed (GtkActor  *self,
                               GdkScreen *new_screen,
                               GdkScreen *old_screen)
{
}

static void
_gtk_actor_class_init (GtkActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_actor_dispose;
  object_class->finalize = gtk_actor_finalize;
  object_class->set_property = gtk_actor_set_property;
  object_class->get_property = gtk_actor_get_property;

  klass->show = gtk_actor_real_show;
  klass->hide = gtk_actor_real_hide;
  klass->realize = gtk_actor_real_realize;
  klass->unrealize = gtk_actor_real_unrealize;
  klass->map = gtk_actor_real_map;
  klass->unmap = gtk_actor_real_unmap;
  klass->draw = gtk_actor_real_draw;
  klass->parent_set = gtk_actor_real_parent_set;
  klass->queue_relayout = gtk_actor_real_queue_relayout;
  klass->queue_redraw = gtk_actor_real_queue_redraw;
  klass->get_request_mode = gtk_actor_real_get_request_mode;
  klass->get_preferred_size = gtk_actor_real_get_preferred_size;
  klass->allocate = gtk_actor_real_allocate;
  klass->screen_changed = gtk_actor_real_screen_changed;

  /**
   * GtkActor:visible:
   *
   * Whether the actor is set to be visible or not
   *
   * See also #GtkActor:mapped
   */
  obj_props[PROP_VISIBLE] =
    g_param_spec_boolean ("visible",
                          P_("Visible"),
                          P_("Whether the actor is visible or not"),
                          FALSE,
                          GTK_PARAM_READWRITE);

  /**
   * GtkActor:mapped:
   *
   * Whether the actor is mapped (will be painted when the stage
   * to which it belongs is mapped)
   *
   * Since: 1.0
   */
  obj_props[PROP_MAPPED] =
    g_param_spec_boolean ("mapped",
                          P_("Mapped"),
                          P_("Whether the actor will be painted"),
                          FALSE,
                          GTK_PARAM_READABLE);

  /**
   * GtkActor:realized:
   *
   * Whether the actor has been realized
   *
   * Since: 1.0
   */
  obj_props[PROP_REALIZED] =
    g_param_spec_boolean ("realized",
                          P_("Realized"),
                          P_("Whether the actor has been realized"),
                          FALSE,
                          GTK_PARAM_READABLE);

  /**
   * GtkActor:width:
   *
   * The width allocated to the #GtkActor. See _gtk_actor_allocate() for
   * details.
   *
   * Since: 1.0
   */
  obj_props[PROP_WIDTH] =
    g_param_spec_float ("width",
                        P_("Width"),
                        P_("Width of the actor"),
                        0.0f,
                        G_MAXFLOAT,
                        0.0f,
                        GTK_PARAM_READABLE);

  /**
   * GtkActor:height:
   *
   * The height allocated to the #GtkActor. See _gtk_actor_allocate() for
   * details.
   *
   * Since: 1.0
   */
  obj_props[PROP_HEIGHT] =
    g_param_spec_float ("height",
                        P_("Height"),
                        P_("Height of the actor"),
                        0.0f,
                        G_MAXFLOAT,
                        0.0f,
                        GTK_PARAM_READABLE);

  /**
   * GtkActor:text-direction:
   *
   * The direction of the text inside a #GtkActor.
   *
   * Since: 1.0
   */
  obj_props[PROP_TEXT_DIRECTION] =
    g_param_spec_enum ("text-direction",
                       P_("Text Direction"),
                       P_("Direction of the text"),
                       GTK_TYPE_TEXT_DIRECTION,
                       GTK_TEXT_DIR_LTR,
                       GTK_PARAM_READWRITE);

  /**
   * GtkActor:layout-manager:
   *
   * A delegate object for controlling the layout of the children of
   * an actor.
   *
   * Since: 1.10
   */
  obj_props[PROP_LAYOUT_MANAGER] =
    g_param_spec_object ("layout-manager",
                         P_("Layout Manager"),
                         P_("The object controlling the layout of an actor's children"),
                         GTK_TYPE_LAYOUT_MANAGER,
                         GTK_PARAM_READWRITE);

  /**
   * GtkActor:widget:
   *
   * The widget this actor belongs to or %NULL if none.
   *
   * Since: 1.10
   */
  obj_props[PROP_WIDGET] =
    g_param_spec_object ("widget",
                         P_("Widget"),
                         P_("The widget this actor belongs to"),
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkActor:first-child:
   *
   * The actor's first child.
   *
   * Since: 1.10
   */
  obj_props[PROP_FIRST_CHILD] =
    g_param_spec_object ("first-child",
                         P_("First Child"),
                         P_("The actor's first child"),
                         GTK_TYPE_ACTOR,
                         GTK_PARAM_READABLE);

  /**
   * GtkActor:last-child:
   *
   * The actor's last child.
   *
   * Since: 1.10
   */
  obj_props[PROP_LAST_CHILD] =
    g_param_spec_object ("last-child",
                         P_("Last Child"),
                         P_("The actor's last child"),
                         GTK_TYPE_ACTOR,
                         GTK_PARAM_READABLE);

  /**
   * GtkActor::actor-added:
   * @actor: the actor which received the signal
   * @child: the new child that has been added to @actor
   *
   * The ::actor-added signal is emitted each time a child
   * has been added to @actor.
   *
   * Since: 0.4
   */
  actor_signals[ACTOR_ADDED] =
    g_signal_new (I_("actor-added"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkActorClass, actor_added),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_ACTOR);
  /**
   * GtkActor::actor-removed:
   * @actor: the actor which received the signal
   * @child: the child that has been removed from @actor
   *
   * The ::actor-removed signal is emitted each time a child
   * is removed from @actor.
   *
   * Since: 0.4
   */
  actor_signals[ACTOR_REMOVED] =
    g_signal_new (I_("actor-removed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkActorClass, actor_removed),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_ACTOR);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);

  g_type_class_add_private (klass, sizeof (GtkActorPrivate));
}

static void
_gtk_actor_init (GtkActor *actor)
{
  GtkActorPrivate *priv;

  actor->priv = G_TYPE_INSTANCE_GET_PRIVATE (actor,
                                             GTK_TYPE_ACTOR,
                                             GtkActorPrivate);
  priv = actor->priv;

  priv->visible = TRUE;
  priv->needs_allocation = TRUE;
  cairo_matrix_init_identity (&priv->transform);

  _gtk_size_request_cache_init (&priv->requests);
}

/*< private >
 * insert_child_at_depth:
 * @self: a #GtkActor
 * @child: a #GtkActor
 *
 * Inserts @child inside the list of children held by @self, using
 * the depth as the insertion criteria.
 *
 * This sadly makes the insertion not O(1), but we can keep the
 * list sorted so that the painters algorithm we use for painting
 * the children will work correctly.
 */
static void
insert_child_at_depth (GtkActor *self,
                       GtkActor *child,
                       gpointer  dummy G_GNUC_UNUSED)
{
  GtkActor *iter;
  float child_depth;

  child->priv->parent = self;

  child_depth = 0;
    //_clutter_actor_get_transform_info_or_defaults (child)->z_position;

  /* special-case the first child */
  if (self->priv->n_children == 0)
    {
      self->priv->first_child = child;
      self->priv->last_child = child;

      child->priv->next_sibling = NULL;
      child->priv->prev_sibling = NULL;

      return;
    }

  /* Find the right place to insert the child so that it will still be
     sorted and the child will be after all of the actors at the same
     dept */
  for (iter = self->priv->first_child;
       iter != NULL;
       iter = iter->priv->next_sibling)
    {
      float iter_depth;

      iter_depth = 0;
        //_clutter_actor_get_transform_info_or_defaults (iter)->z_position;

      if (iter_depth > child_depth)
        break;
    }

  if (iter != NULL)
    {
      GtkActor *tmp = iter->priv->prev_sibling;

      if (tmp != NULL)
        tmp->priv->next_sibling = child;

      /* Insert the node before the found one */
      child->priv->prev_sibling = iter->priv->prev_sibling;
      child->priv->next_sibling = iter;
      iter->priv->prev_sibling = child;
    }
  else
    {
      GtkActor *tmp = self->priv->last_child;

      if (tmp != NULL)
        tmp->priv->next_sibling = child;

      /* insert the node at the end of the list */
      child->priv->prev_sibling = self->priv->last_child;
      child->priv->next_sibling = NULL;
    }

  if (child->priv->prev_sibling == NULL)
    self->priv->first_child = child;

  if (child->priv->next_sibling == NULL)
    self->priv->last_child = child;
}

static void
insert_child_at_index (GtkActor *self,
                       GtkActor *child,
                       gpointer      data_)
{
  gint index_ = GPOINTER_TO_INT (data_);

  child->priv->parent = self;

  if (index_ == 0)
    {
      GtkActor *tmp = self->priv->first_child;

      if (tmp != NULL)
        tmp->priv->prev_sibling = child;

      child->priv->prev_sibling = NULL;
      child->priv->next_sibling = tmp;
    }
  else if (index_ < 0 || index_ >= self->priv->n_children)
    {
      GtkActor *tmp = self->priv->last_child;

      if (tmp != NULL)
        tmp->priv->next_sibling = child;

      child->priv->prev_sibling = tmp;
      child->priv->next_sibling = NULL;
    }
  else
    {
      GtkActor *iter;
      int i;

      for (iter = self->priv->first_child, i = 0;
           iter != NULL;
           iter = iter->priv->next_sibling, i += 1)
        {
          if (index_ == i)
            {
              GtkActor *tmp = iter->priv->prev_sibling;

              child->priv->prev_sibling = tmp;
              child->priv->next_sibling = iter;

              iter->priv->prev_sibling = child;

              if (tmp != NULL)
                tmp->priv->next_sibling = child;

              break;
            }
        }
    }

  if (child->priv->prev_sibling == NULL)
    self->priv->first_child = child;

  if (child->priv->next_sibling == NULL)
    self->priv->last_child = child;
}

static void
insert_child_above (GtkActor *self,
                    GtkActor *child,
                    gpointer      data)
{
  GtkActor *sibling = data;

  child->priv->parent = self;

  if (sibling == NULL)
    sibling = self->priv->last_child;

  child->priv->prev_sibling = sibling;

  if (sibling != NULL)
    {
      GtkActor *tmp = sibling->priv->next_sibling;

      child->priv->next_sibling = tmp;

      if (tmp != NULL)
        tmp->priv->prev_sibling = child;

      sibling->priv->next_sibling = child;
    }
  else
    child->priv->next_sibling = NULL;

  if (child->priv->prev_sibling == NULL)
    self->priv->first_child = child;

  if (child->priv->next_sibling == NULL)
    self->priv->last_child = child;
}

static void
insert_child_below (GtkActor *self,
                    GtkActor *child,
                    gpointer      data)
{
  GtkActor *sibling = data;

  child->priv->parent = self;

  if (sibling == NULL)
    sibling = self->priv->first_child;

  child->priv->next_sibling = sibling;

  if (sibling != NULL)
    {
      GtkActor *tmp = sibling->priv->prev_sibling;

      child->priv->prev_sibling = tmp;

      if (tmp != NULL)
        tmp->priv->next_sibling = child;

      sibling->priv->prev_sibling = child;
    }
  else
    child->priv->prev_sibling = NULL;

  if (child->priv->prev_sibling == NULL)
    self->priv->first_child = child;

  if (child->priv->next_sibling == NULL)
    self->priv->last_child = child;
}

/*< private >
 * _gtk_actor_get_debug_name:
 * @actor: a #GtkActor
 *
 * Retrieves a printable name of @actor for debugging messages
 *
 * Return value: a string with a printable name
 */
const gchar *
_gtk_actor_get_debug_name (GtkActor *actor)
{
  return G_OBJECT_TYPE_NAME (actor);
}

static gboolean
update_direction_recursive (GtkActor *actor)
{
  GtkActor *child;

  /* we need to emit the notify::text-direction first, so that
   * the sub-classes can catch that and do specific handling of
   * the text direction; see clutter_text_direction_changed_cb()
   * inside clutter-text.c
   */
  g_object_notify_by_pspec (G_OBJECT (actor), obj_props[PROP_TEXT_DIRECTION]);

  for (child = actor->priv->first_child; child != NULL; child = child->priv->next_sibling)
    {
      if (child->priv->text_direction == GTK_TEXT_DIR_NONE)
        update_direction_recursive (actor);
    }
  
  _gtk_actor_queue_relayout (actor);

  return TRUE;
}

/**
 * _gtk_actor_set_text_direction:
 * @self: a #GtkActor
 * @text_dir: the text direction for @self
 *
 * Sets the #GtkTextDirection for an actor
 *
 * Actors requiring special handling when the text direction changes
 * should connect to the #GObject::notify signal for the
 * #GtkActor:text-direction property
 *
 * Since: 1.2
 */
void
_gtk_actor_set_text_direction (GtkActor         *self,
                               GtkTextDirection  text_dir)
{
  GtkActorPrivate *priv;
  GtkTextDirection previous_text_dir, new_text_dir;

  g_return_if_fail (GTK_IS_ACTOR (self));

  priv = self->priv;
  if (priv->text_direction == text_dir)
    return;

  previous_text_dir = _gtk_actor_get_text_direction (self);

  priv->text_direction = text_dir;

  new_text_dir = _gtk_actor_get_text_direction (self);

  if (previous_text_dir == new_text_dir)
    return;

  update_direction_recursive (self);
}

/**
 * clutter_actor_get_text_direction:
 * @self: a #ClutterActor
 *
 * Retrieves the value set using clutter_actor_set_text_direction()
 *
 * If no text direction has been previously set, the default text
 * direction, as returned by clutter_get_default_text_direction(), will
 * be returned instead
 *
 * Return value: the #ClutterTextDirection for the actor
 *
 * Since: 1.2
 */
GtkTextDirection
_gtk_actor_get_text_direction (GtkActor *self)
{
  GtkActorPrivate *priv;

  g_return_val_if_fail (GTK_IS_ACTOR (self), GTK_TEXT_DIR_LTR);

  priv = self->priv;

  /* if no direction has been set yet use the default */
  if (priv->text_direction == GTK_TEXT_DIR_NONE)
    {
      GtkActor *parent = _gtk_actor_get_parent (self);

      if (parent)
        return _gtk_actor_get_text_direction (parent);
      else
        return gtk_widget_get_default_direction ();
    }

  return priv->text_direction;
}

/*< private >
 * gtk_actor_queue_compute_expand:
 * @self: a #GtkActor
 *
 * Invalidates the needs_x_expand and needs_y_expand flags on @self
 * and its parents up to the top-level actor.
 *
 * This function also queues a relayout if anything changed.
 */
static inline void
gtk_actor_queue_compute_expand (GtkActor *self)
{
  GtkActor *parent;

  if (self->priv->needs_compute_expand)
    return;

  for (parent = self; parent; parent = parent->priv->parent)
    {
      if (!parent->priv->needs_compute_expand)
        break;

      parent->priv->needs_compute_expand = TRUE;
    }

  _gtk_actor_queue_relayout (self);
}

typedef void (* GtkActorAddChildFunc) (GtkActor *parent,
                                       GtkActor *child,
                                       gpointer  data);

/*< private >
 * gtk_actor_add_child_internal:
 * @self: a #GtkActor
 * @child: a #GtkActor
 * @add_func: delegate function
 * @data: (closure): data to pass to @add_func
 *
 * Adds @child to the list of children of @self.
 *
 * The actual insertion inside the list is delegated to @add_func: this
 * function will just set up the state, perform basic checks, and emit
 * signals.
 *
 * The @flags argument is used to perform additional operations.
 */
static inline void
gtk_actor_add_child_internal (GtkActor              *self,
                              GtkActor              *child,
                              GtkActorAddChildFunc   add_func,
                              gpointer               data)
{
  GtkActor *old_first_child, *old_last_child;

  if (child->priv->parent != NULL)
    {
      g_warning ("The actor '%s' already has a parent, '%s'. You must "
                 "use gtk_actor_remove_child() first.",
                 _gtk_actor_get_debug_name (child),
                 _gtk_actor_get_debug_name (child->priv->parent));
      return;
    }

  old_first_child = self->priv->first_child;
  old_last_child = self->priv->last_child;

  g_object_freeze_notify (G_OBJECT (self));

  g_object_ref_sink (child);
  child->priv->parent = NULL;
  child->priv->next_sibling = NULL;
  child->priv->prev_sibling = NULL;

  /* delegate the actual insertion */
  add_func (self, child, data);

  g_assert (child->priv->parent == self);

  self->priv->n_children += 1;

  self->priv->age += 1;

  /* children may cause their parent to expand, if they are set
   * to expand; if a child is not expanded then it cannot change
   * its parent's state. any further change later on will queue
   * an expand state check.
   *
   * this check, with the initial state of the needs_compute_expand
   * flag set to FALSE, should avoid recomputing the expand flags
   * state while building the actor tree.
   */
  if (_gtk_actor_get_visible (child) &&
      (child->priv->needs_compute_expand ||
       child->priv->needs_x_expand ||
       child->priv->needs_y_expand))
    {
      gtk_actor_queue_compute_expand (self);
    }

  GTK_ACTOR_GET_CLASS (child)->parent_set (child, NULL);

  /* If parent is mapped or realized, we need to also be mapped or
   * realized once we're inside the parent.
   */
  if (_gtk_actor_get_mapped (self))
    _gtk_actor_map (child);

  /* propagate the parent's text direction to the child */
  _gtk_actor_set_text_direction (child, _gtk_actor_get_text_direction (self));

  if (_gtk_actor_get_mapped (child))
    _gtk_actor_queue_redraw (child);

  /* maintain the invariant that if an actor needs layout,
   * its parents do as well
   */
  if (child->priv->needs_allocation)
    {
      /* we work around the short-circuiting we do
       * in _gtk_actor_queue_relayout() since we
       * want to force a relayout
       */
      _gtk_actor_queue_relayout (child->priv->parent);
    }

  g_signal_emit (self, actor_signals[ACTOR_ADDED], 0, child);

  if (old_first_child != self->priv->first_child)
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_FIRST_CHILD]);

  if (old_last_child != self->priv->last_child)
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_LAST_CHILD]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * _gtk_actor_add_child:
 * @self: a #GtkActor
 * @child: a #GtkActor
 *
 * Adds @child to the children of @self.
 *
 * This function will acquire a reference on @child that will only
 * be released when calling gtk_actor_remove_child().
 *
 * This function will take into consideration the #GtkActor:depth
 * of @child, and will keep the list of children sorted.
 *
 * This function will emit the #ClutterContainer::actor-added signal
 * on @self.
 *
 * Since: 1.10
 */
void
_gtk_actor_add_child (GtkActor *self,
                      GtkActor *child)
{
  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (GTK_IS_ACTOR (child));
  g_return_if_fail (self != child);
  g_return_if_fail (child->priv->parent == NULL);

  gtk_actor_add_child_internal (self, child,
                                insert_child_at_depth,
                                NULL);
}

/**
 * _gtk_actor_insert_child_at_index:
 * @self: a #GtkActor
 * @child: a #GtkActor
 * @index_: the index
 *
 * Inserts @child into the list of children of @self, using the
 * given @index_. If @index_ is greater than the number of children
 * in @self, or is less than 0, then the new child is added at the end.
 *
 * This function will acquire a reference on @child that will only
 * be released when calling gtk_actor_remove_child().
 *
 * This function will not take into consideration the #GtkActor:depth
 * of @child.
 *
 * This function will emit the #ClutterContainer::actor-added signal
 * on @self.
 *
 * Since: 1.10
 */
void
_gtk_actor_insert_child_at_index (GtkActor *self,
                                  GtkActor *child,
                                  gint      index_)
{
  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (GTK_IS_ACTOR (child));
  g_return_if_fail (self != child);
  g_return_if_fail (child->priv->parent == NULL);

  gtk_actor_add_child_internal (self, child,
                                insert_child_at_index,
                                GINT_TO_POINTER (index_));
}

/**
 * gtk_actor_insert_child_above:
 * @self: a #GtkActor
 * @child: a #GtkActor
 * @sibling: (allow-none): a child of @self, or %NULL
 *
 * Inserts @child into the list of children of @self, above another
 * child of @self or, if @sibling is %NULL, above all the children
 * of @self.
 *
 * This function will acquire a reference on @child that will only
 * be released when calling gtk_actor_remove_child().
 *
 * This function will not take into consideration the #GtkActor:depth
 * of @child.
 *
 * This function will emit the #ClutterContainer::actor-added signal
 * on @self.
 *
 * Since: 1.10
 */
void
gtk_actor_insert_child_above (GtkActor *self,
                              GtkActor *child,
                              GtkActor *sibling)
{
  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (GTK_IS_ACTOR (child));
  g_return_if_fail (self != child);
  g_return_if_fail (child != sibling);
  g_return_if_fail (child->priv->parent == NULL);
  g_return_if_fail (sibling == NULL ||
                    (GTK_IS_ACTOR (sibling) &&
                     sibling->priv->parent == self));

  gtk_actor_add_child_internal (self, child,
                                insert_child_above,
                                sibling);
}

/**
 * gtk_actor_insert_child_below:
 * @self: a #GtkActor
 * @child: a #GtkActor
 * @sibling: (allow-none): a child of @self, or %NULL
 *
 * Inserts @child into the list of children of @self, below another
 * child of @self or, if @sibling is %NULL, below all the children
 * of @self.
 *
 * This function will acquire a reference on @child that will only
 * be released when calling gtk_actor_remove_child().
 *
 * This function will not take into consideration the #GtkActor:depth
 * of @child.
 *
 * This function will emit the #ClutterContainer::actor-added signal
 * on @self.
 *
 * Since: 1.10
 */
void
gtk_actor_insert_child_below (GtkActor *self,
                              GtkActor *child,
                              GtkActor *sibling)
{
  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (GTK_IS_ACTOR (child));
  g_return_if_fail (self != child);
  g_return_if_fail (child != sibling);
  g_return_if_fail (child->priv->parent == NULL);
  g_return_if_fail (sibling == NULL ||
                    (GTK_IS_ACTOR (sibling) &&
                     sibling->priv->parent == self));

  gtk_actor_add_child_internal (self, child,
                                insert_child_below,
                                sibling);
}

/**
 * _gtk_actor_queue_relayout:
 * @self: A #ClutterActor
 *
 * Indicates that the actor's size request or other layout-affecting
 * properties may have changed. This function is used inside #GtkActor
 * subclass implementations, not by applications directly.
 *
 * Queueing a new layout automatically queues a redraw as well.
 *
 * Since: 0.8
 */
void
_gtk_actor_queue_relayout (GtkActor *self)
{
  g_return_if_fail (GTK_IS_ACTOR (self));

  gtk_actor_queue_only_relayout (self);
  _gtk_actor_queue_redraw (self);
}

/**
 * _gtk_actor_queue_redraw:
 * @self: A #GtkActor
 *
 * Queues up a redraw of an actor and any children. The redraw occurs
 * once the main loop becomes idle (after the current batch of events
 * has been processed, roughly).
 *
 * Applications rarely need to call this, as redraws are handled
 * automatically by modification functions.
 *
 * This function will not do anything if @self is not visible, or
 * if the actor is inside an invisible part of the scenegraph.
 *
 * Also be aware that painting is a NOP for actors with an opacity of
 * 0
 *
 * When you are implementing a custom actor you must queue a redraw
 * whenever some private state changes that will affect painting or
 * picking of your actor.
 */
void
_gtk_actor_queue_redraw (GtkActor *self)
{
  GtkActorPrivate *priv;
  cairo_rectangle_t rect;

  g_return_if_fail (GTK_IS_ACTOR (self));

  priv = self->priv;

  if (!_gtk_actor_get_mapped (self) ||
      priv->needs_allocation)
    return;

  rect.x = 0;
  rect.y = 0;
  rect.width = priv->width;
  rect.height = priv->height;

  _gtk_actor_queue_redraw_area (self, &rect);
}

void
_gtk_actor_queue_redraw_area (GtkActor                *self,
                              const cairo_rectangle_t *box)
{
  g_return_if_fail (GTK_IS_ACTOR (self));

  if (!_gtk_actor_get_mapped (self) ||
      self->priv->needs_allocation)
    return;

  GTK_ACTOR_GET_CLASS (self)->queue_redraw (self, box);
}

/**
 * _gtk_actor_get_request_mode:
 * @self: a #GtkActor
 *
 * Retrieves the geometry request mode of @self
 *
 * Return value: the request mode for the actor
 *
 * Since: 1.2
 */
GtkSizeRequestMode
_gtk_actor_get_request_mode (GtkActor *self)
{
  GtkActorPrivate *priv;

  g_return_val_if_fail (GTK_IS_ACTOR (self), GTK_SIZE_REQUEST_CONSTANT_SIZE);

  priv = self->priv;

  if (!priv->requests.request_mode_valid)
    {
      priv->requests.request_mode = GTK_ACTOR_GET_CLASS (self)->get_request_mode (self);
      priv->requests.request_mode_valid = TRUE;
    }

  return priv->requests.request_mode;
}

/**
 * _gtk_actor_get_preferred_size:
 * @self: A #GtkActor
 * @orientation: orientation to query the size for
 * @for_size: available size in opposite orientation,
 *   or a negative value to indicate that no size is defined
 * @min_size_p: (out) (allow-none): return location for minimum size,
 *   or %NULL
 * @natural_size_p: (out) (allow-none): return location for the natural
 *   size, or %NULL
 *
 * Computes the requested minimum and natural sizes for an actor,
 * optionally depending on the specified height, or if they are
 * already computed, returns the cached values.
 *
 * An actor may not get its request - depending on the layout
 * manager that's in effect.
 *
 * A request should not incorporate the actor's scale or anchor point;
 * those transformations do not affect layout, only rendering.
 *
 * Since: 0.8
 */
void
_gtk_actor_get_preferred_size (GtkActor       *self,
                               GtkOrientation  orientation,
                               gfloat          for_size,
                               gfloat         *min_size_p,
                               gfloat         *nat_size_p)
{
  GtkActorPrivate *priv;
  float min_size = 0;
  float nat_size = 0;
  gboolean found_in_cache;

  priv = self->priv;

  if (!priv->visible)
    {
      if (min_size_p)
        *min_size_p = 0;
      if (nat_size_p)
        *nat_size_p = 0;
      return;
    }

  if (_gtk_actor_get_request_mode (self) == GTK_SIZE_REQUEST_CONSTANT_SIZE)
    for_size = -1;

  found_in_cache = _gtk_size_request_cache_lookup (&priv->requests,
                                                   orientation,
                                                   for_size,
                                                   &min_size,
                                                   &nat_size);
  
  if (!found_in_cache)
    {
      GTK_ACTOR_GET_CLASS (self)->get_preferred_size (self, orientation, for_size, &min_size, &nat_size);

      if (min_size > nat_size)
        {
          g_warning ("%s %p reported min size %f and natural size %f for size %f; natural size must be >= min size",
                     G_OBJECT_TYPE_NAME (self), self, min_size, nat_size, for_size);
        }

      _gtk_size_request_cache_commit (&priv->requests,
                                      orientation,
                                      for_size,
                                      min_size,
                                      nat_size);
    }

  if (min_size_p)
    *min_size_p = min_size;

  if (nat_size_p)
    *nat_size_p = nat_size;

  g_assert (min_size <= nat_size);

  GTK_NOTE (SIZE_REQUEST,
            g_print ("[%p] %s\t%s: %f is minimum %f and natural: %f (hit cache: %s)\n",
                     self, G_OBJECT_TYPE_NAME (self),
                     orientation == GTK_ORIENTATION_HORIZONTAL ?
                     "width for height" : "height for width" ,
                     for_size, min_size, nat_size,
                     found_in_cache ? "yes" : "no"));
}

static void
remove_child (GtkActor *self,
              GtkActor *child)
{
  GtkActor *prev_sibling, *next_sibling;

  prev_sibling = child->priv->prev_sibling;
  next_sibling = child->priv->next_sibling;

  if (prev_sibling != NULL)
    prev_sibling->priv->next_sibling = next_sibling;

  if (next_sibling != NULL)
    next_sibling->priv->prev_sibling = prev_sibling;

  if (self->priv->first_child == child)
    self->priv->first_child = next_sibling;

  if (self->priv->last_child == child)
    self->priv->last_child = prev_sibling;

  child->priv->parent = NULL;
  child->priv->prev_sibling = NULL;
  child->priv->next_sibling = NULL;
}

/*< private >
 * gtk_actor_remove_child_internal:
 * @self: a #GtkActor
 * @child: the child of @self that has to be removed
 * @flags: control the removal operations
 *
 * Removes @child from the list of children of @self.
 */
static void
gtk_actor_remove_child_internal (GtkActor *self,
                                 GtkActor *child)
{
  GtkActor *old_first, *old_last;

  g_object_freeze_notify (G_OBJECT (self));

  if (_gtk_actor_get_mapped (child))
    _gtk_actor_queue_relayout (self);

  /* we need to unrealize *before* we set parent_actor to NULL,
   * because in an unrealize method actors are dissociating from the
   * stage, which means they need to be able to
   * clutter_actor_get_stage().
   */
  _gtk_actor_unmap (child);
  _gtk_actor_unrealize (child);

  old_first = self->priv->first_child;
  old_last = self->priv->last_child;

  remove_child (self, child);

  self->priv->n_children -= 1;

  self->priv->age += 1;

  /* if the child that got removed was visible and set to
   * expand then we want to reset the parent's state in
   * case the child was the only thing that was making it
   * expand.
   */
  if (_gtk_actor_get_visible (child) &&
      (child->priv->needs_compute_expand ||
       child->priv->needs_x_expand ||
       child->priv->needs_y_expand))
    {
      gtk_actor_queue_compute_expand (self);
    }

  GTK_ACTOR_GET_CLASS (child)->parent_set (child, self);

  /* we need to emit the signal before dropping the reference */
  g_signal_emit (self, actor_signals[ACTOR_REMOVED], 0, child);

  if (old_first != self->priv->first_child)
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_FIRST_CHILD]);

  if (old_last != self->priv->last_child)
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_LAST_CHILD]);

  g_object_thaw_notify (G_OBJECT (self));

  /* remove the reference we acquired in _gtk_actor_add_child() */
  g_object_unref (child);
}

/**
 * _gtk_actor_remove_child:
 * @self: a #GtkActor
 * @child: a #GtkActor
 *
 * Removes @child from the children of @self.
 *
 * This function will release the reference added by
 * _gtk_actor_add_child(), so if you want to keep using @child
 * you will have to acquire a referenced on it before calling this
 * function.
 *
 * This function will emit the #ClutterContainer::actor-removed
 * signal on @self.
 *
 * Since: 1.10
 */
void
_gtk_actor_remove_child (GtkActor *self,
                         GtkActor *child)
{
  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (GTK_IS_ACTOR (child));
  g_return_if_fail (self != child);
  g_return_if_fail (child->priv->parent != NULL);
  g_return_if_fail (child->priv->parent == self);

  gtk_actor_remove_child_internal (self, child);
}

/**
 * _gtk_actor_remove_all_children:
 * @self: a #GtkActor
 *
 * Removes all children of @self.
 *
 * This function releases the reference added by inserting a child actor
 * in the list of children of @self.
 *
 * If the reference count of a child drops to zero, the child will be
 * destroyed. If you want to ensure the destruction of all the children
 * of @self, use clutter_actor_destroy_all_children().
 *
 * Since: 1.10
 */
void
_gtk_actor_remove_all_children (GtkActor *self)
{
  GtkActor *child;

  g_return_if_fail (GTK_IS_ACTOR (self));

  if (self->priv->n_children == 0)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  for (child = _gtk_actor_get_first_child (self);
       child;
       child = _gtk_actor_get_first_child (self))
    {
      _gtk_actor_remove_child (self, child);
    }

  g_object_thaw_notify (G_OBJECT (self));

  /* sanity check */
  g_assert (self->priv->first_child == NULL);
  g_assert (self->priv->last_child == NULL);
  g_assert (self->priv->n_children == 0);
}

/**
 * _gtk_actor_get_n_children:
 * @self: a #GtkActor
 *
 * Retrieves the number of children of @self.
 *
 * Return value: the number of children of an actor
 *
 * Since: 1.10
 */
gint
_gtk_actor_get_n_children (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), 0);

  return self->priv->n_children;
}

/**
 * _gtk_actor_get_child_at_index:
 * @self: a #GtkActor
 * @index_: the position in the list of children
 *
 * Retrieves the actor at the given @index_ inside the list of
 * children of @self.
 *
 * Return value: (transfer none): a pointer to a #GtkActor, or %NULL
 *
 * Since: 1.10
 */
GtkActor *
_gtk_actor_get_child_at_index (GtkActor *self,
                               gint      index_)
{
  GtkActor *iter;
  int i;

  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);
  g_return_val_if_fail (index_ <= self->priv->n_children, NULL);

  for (iter = self->priv->first_child, i = 0;
       iter != NULL && i < index_;
       iter = iter->priv->next_sibling, i += 1)
    ;

  return iter;
}

/**
 * _gtk_actor_get_previous_sibling:
 * @self: a #GtkActor
 *
 * Retrieves the sibling of @self that comes before it in the list
 * of children of @self's parent.
 *
 * The returned pointer is only valid until the scene graph changes; it
 * is not safe to modify the list of children of @self while iterating
 * it.
 *
 * Return value: (transfer none): a pointer to a #GtkActor, or %NULL
 *
 * Since: 1.10
 */
GtkActor *
_gtk_actor_get_previous_sibling (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);

  return self->priv->prev_sibling;
}

/**
 * _gtk_actor_get_next_sibling:
 * @self: a #GtkActor
 *
 * Retrieves the sibling of @self that comes after it in the list
 * of children of @self's parent.
 *
 * The returned pointer is only valid until the scene graph changes; it
 * is not safe to modify the list of children of @self while iterating
 * it.
 *
 * Return value: (transfer none): a pointer to a #GtkActor, or %NULL
 *
 * Since: 1.10
 */
GtkActor *
_gtk_actor_get_next_sibling (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);

  return self->priv->next_sibling;
}

/**
 * _gtk_actor_get_first_child:
 * @self: a #GtkActor
 *
 * Retrieves the first child of @self.
 *
 * The returned pointer is only valid until the scene graph changes; it
 * is not safe to modify the list of children of @self while iterating
 * it.
 *
 * Return value: (transfer none): a pointer to a #GtkActor, or %NULL
 *
 * Since: 1.10
 */
GtkActor *
_gtk_actor_get_first_child (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);

  return self->priv->first_child;
}

/**
 * _gtk_actor_get_last_child:
 * @self: a #GtkActor
 *
 * Retrieves the last child of @self.
 *
 * The returned pointer is only valid until the scene graph changes; it
 * is not safe to modify the list of children of @self while iterating
 * it.
 *
 * Return value: (transfer none): a pointer to a #GtkActor, or %NULL
 *
 * Since: 1.10
 */
GtkActor *
_gtk_actor_get_last_child (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);

  return self->priv->last_child;
}

/**
 * _gtk_actor_get_parent:
 * @self: A #GtkActor
 *
 * Retrieves the parent of @self.
 *
 * Return Value: (transfer none): The #GtkActor parent, or %NULL
 *  if no parent is set
 */
GtkActor *
_gtk_actor_get_parent (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);

  return self->priv->parent;
}

/**
 * _gtk_actor_contains:
 * @self: A #GtkActor
 * @descendant: A #GtkActor, possibly contained in @self
 *
 * Determines if @descendant is contained inside @self (either as an
 * immediate child, or as a deeper descendant). If @self and
 * @descendant point to the same actor then it will also return %TRUE.
 *
 * Return value: whether @descendent is contained within @self
 *
 * Since: 1.4
 */
gboolean
_gtk_actor_contains (GtkActor *self,
		     GtkActor *descendant)
{
  GtkActor *actor;

  g_return_val_if_fail (GTK_IS_ACTOR (self), FALSE);
  g_return_val_if_fail (GTK_IS_ACTOR (descendant), FALSE);

  for (actor = descendant; actor; actor = actor->priv->parent)
    if (actor == self)
      return TRUE;

  return FALSE;
}

/**
 * _gtk_actor_set_visible:
 * @actor: a #GtkActor
 * @visible: whether the actor should be shown or not
 *
 * Sets the visibility state of @actor. Note that setting this to
 * %TRUE doesn't mean the actor is actually viewable, see
 * _gtk_actor_get_visible().
 *
 * This function simply calls _gtk_actor_show() or _gtk_actor_hide()
 * but is nicer to use when the visibility of the actor depends on
 * some condition.
 *
 * Actors are visible by default.
 *
 * Since: 2.18
 **/
void
_gtk_actor_set_visible (GtkActor *self,
                        gboolean  visible)
{
  g_return_if_fail (GTK_IS_ACTOR (self));

  if (visible != _gtk_actor_get_visible (self))
    {
      if (visible)
        _gtk_actor_show (self);
      else
        _gtk_actor_hide (self);
    }
}

/**
 * _gtk_actor_get_visible:
 * @actor: a #GtkActor
 *
 * Determines whether the actor is visible. This function does not take
 * into account wether the parent actor is also marked as visible.
 *
 * This function does not check if the actor is obscured in any way.
 *
 * See _gtk_actor_set_visible().
 *
 * Return value: %TRUE if the actor is visible
 *
 * Since: 2.18
 **/
gboolean
_gtk_actor_get_visible (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), FALSE);

  return self->priv->visible;
}

/**
 * _gtk_actor_get_realized:
 * @self: A #GtkActor
 *
 * Evaluates to %TRUE if the actor is realized.
 *
 * The realized state has an actor-dependant interpretation. If an
 * actor wants to delay allocating resources until it is attached to a
 * screen, it may use the realize state to do so. If an actor is mapped
 * it must also be realized, but an actor can be realized and unmapped
 * (this is so hiding an actor temporarily doesn't do an expensive
 * unrealize/realize).
 *
 * To be realized an actor must be inside a toplevel.
 *
 * Since: 0.2
 */
gboolean
_gtk_actor_get_realized (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), FALSE);

  return self->priv->realized;
}

/**
 * _gtk_actor_realize:
 * @self: A #ClutterActor
 *
 * Realization informs the actor that it is attached to a toplevel. It
 * can use this to allocate resources if it wanted to delay allocation
 * until it would be rendered.
 *
 * This function does nothing if the actor is already realized.
 *
 * Because a realized actor must have realized parent actors, calling
 * clutter_actor_realize() will also realize all parents of the actor.
 *
 * This function does not realize child actors.
 **/
void
_gtk_actor_realize (GtkActor *self)
{
  GtkActorPrivate *priv;

  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (_gtk_actor_is_toplevel (self) || _gtk_actor_get_mapped (_gtk_actor_get_parent (self)));

  if (_gtk_actor_get_realized (self))
    return;

  priv = self->priv;

  /* To be realized, our parent actors must be realized first.
   * This will only succeed if we're inside a toplevel.
   */
  if (priv->parent != NULL)
    _gtk_actor_realize (priv->parent);

  GTK_ACTOR_GET_CLASS (self)->realize (self);
}

/**
 * _gtk_actor_unrealize:
 * @self: A #GtkActor
 *
 * Unrealization informs the actor that it may be being destroyed or
 * moved to another stage. The actor may want to destroy any
 * underlying graphics resources at this point.
 *
 * Because mapped actors must be realized, actors may not be
 * unrealized if they are mapped.
 *
 * This function should not be called by application code.
 */
void
_gtk_actor_unrealize (GtkActor *self)
{
  GtkActor *iter;

  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (!_gtk_actor_get_mapped (self));

  if (!_gtk_actor_get_realized (self))
    return;

  for (iter = self->priv->first_child;
       iter != NULL;
       iter = iter->priv->next_sibling)
    {
      _gtk_actor_unrealize (iter);
    }

  GTK_ACTOR_GET_CLASS (self)->unrealize (self);
}

/**
 * _gtk_actor_get_mapped:
 * @self: a #GtkActor
 *
 * Checks if the given actor is mapped.
 *
 * The mapped state is set when the actor is visible and all its parents up
 * to a top-level are visible, realized, and mapped.
 *
 * This check can be used to see if an actor is going to be painted, as actors
 * that are not mapped will never be painted.
 *
 * Since: 0.2
 */
gboolean
_gtk_actor_get_mapped (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), FALSE);

  return self->priv->mapped;
}

/**
 * _gtk_actor_map:
 * @self: A #GtkActor
 *
 * Marks an actor as mapped and possibly maps and realizes its children
 * if they are visible. Does nothing if the actor is not visible.
 *
 * Calling this function is strongly disencouraged: the default
 * implementation of #ClutterActorClass.map() will map all the children
 * of an actor when mapping its parent.
 *
 * When overriding map, it is mandatory to chain up to the parent
 * implementation.
 *
 * Since: 1.0
 */
void
_gtk_actor_map (GtkActor *self)
{
  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (_gtk_actor_is_toplevel (self) || _gtk_actor_get_mapped (_gtk_actor_get_parent (self)));

  if (_gtk_actor_get_mapped (self))
    return;

  if (!_gtk_actor_get_visible (self))
    return;

  _gtk_actor_realize (self);

  GTK_ACTOR_GET_CLASS (self)->map (self);
}

/**
 * _gtk_actor_unmap:
 * @self: A #GtkActor
 *
 * Unsets the mapped state on the actor and unmaps its children if they
 * were mapped.
 *
 * Calling this function is not encouraged: the default #GtkActor
 * implementation of #GtkActorClass.unmap() will also unmap any
 * eventual children by default when their parent is unmapped.
 *
 * When overriding #GtkActorClass.unmap(), it is mandatory to
 * chain up to the parent implementation.
 *
 * <note>It is important to note that the implementation of the
 * #GtkActorClass.unmap() virtual function may be called after
 * the #GObjectClass.dispose() implementation, but it is
 * guaranteed to be called before the #GObjectClass.finalize()
 * implementation.</note>
 *
 * Since: 1.0
 */
void
_gtk_actor_unmap (GtkActor *self)
{
  g_return_if_fail (GTK_IS_ACTOR (self));

  if (!_gtk_actor_get_mapped (self))
    return;

  GTK_ACTOR_GET_CLASS (self)->unmap (self);
}

/**
 * _gtk_actor_show:
 * @self: A #GtkActor
 *
 * Flags an actor to be displayed. An actor that isn't shown will not
 * be rendered on the stage.
 *
 * Actors are visible by default.
 */
void
_gtk_actor_show (GtkActor *self)
{
  GtkActorPrivate *priv;

  g_return_if_fail (GTK_IS_ACTOR (self));

  if (_gtk_actor_get_visible (self))
    return;

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  /* if we're showing a child that needs to expand, or may
   * expand, then we need to recompute the expand flags for
   * its parent as well
   */
  if (priv->parent &&
      (priv->needs_compute_expand ||
       priv->needs_x_expand ||
       priv->needs_y_expand))
    {
      gtk_actor_queue_compute_expand (priv->parent);
    }

  GTK_ACTOR_GET_CLASS (self)->show (self);

  if (priv->parent != NULL)
    {
      if (_gtk_actor_get_mapped (priv->parent))
        _gtk_actor_map (self);

      _gtk_actor_queue_relayout (priv->parent);
    }
  
  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * _gtk_actor_hide:
 * @self: A #GtkActor
 *
 * Flags an actor to be hidden. A hidden actor will not be
 * rendered on the stage.
 *
 * Actors are visible by default.
 */
void
_gtk_actor_hide (GtkActor *self)
{
  GtkActorPrivate *priv;

  g_return_if_fail (GTK_IS_ACTOR (self));

  /* simple optimization */
  if (!_gtk_actor_get_visible (self))
    return;

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  /* if we're hiding a child that needs to expand, or may
   * expand, then we need to recompute the expand flags for
   * its parent as well
   */
  if (priv->parent &&
      (priv->needs_x_expand ||
       priv->needs_y_expand))
    {
      gtk_actor_queue_compute_expand (priv->parent);
    }

  GTK_ACTOR_GET_CLASS (self)->hide (self);

  if (priv->parent != NULL)
    _gtk_actor_queue_relayout (priv->parent);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * _gtk_actor_draw:
 * @self: The #GtkActor to draw.
 * @cr: a cairo context to draw to
 *
 * Draws @self to @cr. The top left corner of the actor will be
 * drawn to the currently set origin point of @cr.
 *
 * The actor must be visible and a size must be allocated or this
 * function will not draw anything.
 *
 * You should pass a cairo context as @cr argument that is in an
 * original state. Otherwise the resulting drawing is undefined. For
 * example changing the operator using cairo_set_operator() or the
 * line width using cairo_set_line_width() might have unwanted side
 * effects.
 * You may however change the context's transform matrix - like with
 * cairo_scale(), cairo_translate() or cairo_set_matrix() and clip
 * region with cairo_clip() prior to calling this function. Also, it
 * is fine to modify the context with cairo_save() and
 * cairo_push_group() prior to calling this function.
 *
 * Since: 3.0
 **/
void
_gtk_actor_draw (GtkActor *self,
                 cairo_t  *cr)
{
  GtkActorPrivate *priv;

  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (cr != NULL);

  priv = self->priv;

  if (priv->needs_allocation ||
      !_gtk_actor_get_visible (self))
    return;

  cairo_save (cr);

  GTK_ACTOR_GET_CLASS (self)->draw (self, cr);

  cairo_restore (cr);
}

/**
 * _gtk_actor_is_toplevel:
 * @self: A #GtkActor
 *
 * Checks if the actor is a toplevel. Toplevel actors don't need
 * to have a parent.
 *
 * Returns: %TRUE if actor can act as a toplevel
 **/
gboolean
_gtk_actor_is_toplevel (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), FALSE);

  g_warning ("FIXME: implement is_toplevel");

  return self->priv->parent == NULL;
}

/**
 * _gtk_actor_get_width:
 * @self: A #GtkActor
 *
 * Retrieves the width of a #GtkActor.
 *
 * If the actor has a valid allocation and is visible, this function
 * will return the width of the allocated area given to the actor.
 *
 * If the actor does not have a valid allocation or is not visible,
 * this function will return 0.
 *
 * Return value: the width of the actor, in pixels
 */
gfloat
_gtk_actor_get_width (GtkActor *self)
{
  GtkActorPrivate *priv;

  g_return_val_if_fail (GTK_IS_ACTOR (self), 0);

  priv = self->priv;

  if (priv->needs_allocation ||
      !_gtk_actor_get_visible (self))
    return 0;

  return priv->width;
}

/**
 * _gtk_actor_get_height:
 * @self: A #GtkActor
 *
 * Retrieves the height of a #GtkActor.
 *
 * If the actor has a valid allocation and is visible, this function
 * will return the height of the allocated area given to the actor.
 *
 * If the actor does not have a valid allocation or is not visible,
 * this function will return 0.
 *
 * Return value: the height of the actor, in pixels
 */
gfloat
_gtk_actor_get_height (GtkActor *self)
{
  GtkActorPrivate *priv;

  g_return_val_if_fail (GTK_IS_ACTOR (self), 0);

  priv = self->priv;

  if (priv->needs_allocation ||
      !_gtk_actor_get_visible (self))
    return 0;

  return priv->height;
}

/**
 * _gtk_actor_allocate:
 * @self: A #GtkActor
 * @position: the position to allocate. See _gtk_actor_position()
 * @width: new width of the actor
 * @height: new width of the actor
 *
 * Called by the parent of an actor to assign the actor its size.
 * Should never be called by applications (except when implementing
 * a container or layout manager).
 *
 * Since: 0.8
 */
void
_gtk_actor_allocate (GtkActor             *self,
                     const cairo_matrix_t *position,
                     gfloat                width,
                     gfloat                height)
{
  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (position != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  if (!_gtk_actor_get_visible (self))
    return;

  g_object_freeze_notify (G_OBJECT (self));

  GTK_ACTOR_GET_CLASS (self)->allocate (self, position, width, height);

  g_object_thaw_notify (G_OBJECT (self));
}

void
_gtk_actor_position (GtkActor             *actor,
                     const cairo_matrix_t *position)
{
  g_return_if_fail (GTK_IS_ACTOR (actor));
  g_return_if_fail (position != NULL);

  _gtk_actor_queue_redraw (actor);

  actor->priv->transform = *position;

  _gtk_actor_queue_redraw (actor);

  /* g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_POSITION]); */
}

/**
 * _gtk_actor_get_screen:
 * @self: a #GtkActor
 *
 * Get the #GdkScreen from the toplevel actor associated with
 * this actor.
 *
 * In general, you should only create screen specific
 * resources when an actor has been realized, and you should
 * free those resources when the actor is unrealized.
 *
 * Return value: (transfer none): the #GdkScreen for the toplevel for this actor.
 *
 * Since: 2.2
 **/
GdkScreen *
_gtk_actor_get_screen (GtkActor *self)
{
  GtkWidget *widget;

  g_return_val_if_fail (GTK_IS_ACTOR (self), gdk_screen_get_default ());

  widget = _gtk_actor_get_widget (self);
  if (widget == NULL)
    return gdk_screen_get_default ();

  return gtk_widget_get_screen (widget);
}

/**
 * _gtk_actor_get_widget:
 * @self: a #GtkActor
 *
 * Get the widget that owns the actor. If the actor isn't owned by a widget,
 * %NULL is returned.
 *
 * Returns: the widget owning the actor or %NULL
 *
 * Since: 1.2
 **/
GtkWidget *
_gtk_actor_get_widget (GtkActor *self)
{
  GtkActor *iter;

  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);

  for (iter = self;
       iter != NULL;
       iter = _gtk_actor_get_parent (iter))
    {
      if (iter->priv->widget)
        return iter->priv->widget;
    }

  return NULL;
}

/**
 * _gtk_actor_set_layout_manager:
 * @self: a #GtkActor
 * @manager: (allow-none): a #GtkLayoutManager, or %NULL to unset it
 *
 * Sets the #GtkLayoutManager delegate object that will be used to
 * lay out the children of @self. The @manager must not yet be used with
 * a different #GtkActor.
 *
 * The #GtkActor will take a reference on the passed @manager which
 * will be released either when the layout manager is removed, or when
 * the actor is destroyed.
 *
 * Since: 1.10
 */
void
_gtk_actor_set_layout_manager (GtkActor         *self,
                               GtkLayoutManager *manager)
{
  GtkActorPrivate *priv;

  g_return_if_fail (GTK_IS_ACTOR (self));
  g_return_if_fail (manager == NULL || GTK_IS_LAYOUT_MANAGER (manager));
  if (manager)
    g_return_if_fail (_gtk_layout_manager_get_actor (manager) == NULL);

  priv = self->priv;

  if (priv->layout_manager == manager)
    return;

  if (priv->layout_manager != NULL)
    {
      _gtk_layout_manager_set_actor (priv->layout_manager, NULL);
      g_clear_object (&priv->layout_manager);
    }

  priv->layout_manager = manager;

  if (priv->layout_manager != NULL)
    {
      g_object_ref_sink (priv->layout_manager);
      _gtk_layout_manager_set_actor (priv->layout_manager, self);
    }

  _gtk_actor_layout_manager_changed (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_LAYOUT_MANAGER]);
}

void
_gtk_actor_layout_manager_changed (GtkActor *self)
{
  _gtk_actor_queue_relayout (self);
}

/**
 * _gtk_actor_get_layout_manager:
 * @self: a #GtkActor
 *
 * Retrieves the #GtkLayoutManager used by @self.
 *
 * Return value: (transfer none): a pointer to the #GtkLayoutManager,
 *   or %NULL
 *
 * Since: 1.10
 */
GtkLayoutManager *
_gtk_actor_get_layout_manager (GtkActor *self)
{
  g_return_val_if_fail (GTK_IS_ACTOR (self), NULL);

  return self->priv->layout_manager;
}

