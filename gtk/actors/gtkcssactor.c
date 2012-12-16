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

#include "gtkcssactorprivate.h"

#include "gtkcontainer.h"
#include "gtkcontainerprivate.h"
#include "gtkcssboxprivate.h"
#include "gtkcssmatcherprivate.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkstylecontext.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetactorprivate.h"
#include "gtkwidgetprivate.h"

struct _GtkCssActorPrivate {
  GtkStyleContext *context;
};

enum
{
  PROP_0,

  PROP_STYLE_CONTEXT,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (GtkCssActor, _gtk_css_actor, GTK_TYPE_ACTOR)

static gboolean
gtk_css_actor_owns_context (GtkCssActor *actor)
{
  return GTK_IS_CSS_BOX (actor);
}

static void
gtk_css_actor_finalize (GObject *object)
{
  GtkCssActor *self = GTK_CSS_ACTOR (object);
  GtkCssActorPrivate *priv = self->priv;

  if (priv->context)
    {
      if (gtk_css_actor_owns_context (self))
        _gtk_style_context_set_source (priv->context, NULL, NULL);
      g_object_unref (priv->context);
      priv->context = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_actor_parent_class)->finalize (object);
}

static void
gtk_css_actor_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  //GtkCssActor *css_actor = GTK_CSS_ACTOR (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_actor_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkCssActor *css_actor = GTK_CSS_ACTOR (object);

  switch (prop_id)
    {
    case PROP_STYLE_CONTEXT:
      g_value_set_object (value, _gtk_css_actor_get_style_context (css_actor));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_actor_set_style_context (GtkCssActor     *self,
                                  GtkStyleContext *context)
{
  GtkCssActorPrivate *priv;
  GtkActor *iter;

  g_return_if_fail (GTK_IS_CSS_ACTOR (self));
  g_return_if_fail (context == NULL || GTK_IS_STYLE_CONTEXT (context));

  /* XXX: Use a default context for NULL? */

  priv = self->priv;

  if (priv->context == context)
    return;

  if (priv->context)
    g_object_unref (priv->context);

  if (context)
    g_object_ref (context);

  priv->context = context;

  for (iter = _gtk_actor_get_first_child (GTK_ACTOR (self));
       iter != NULL;
       iter = _gtk_actor_get_next_sibling (iter))
    {
      GtkCssActor *css_actor;

      if (!GTK_IS_CSS_ACTOR (iter))
        continue;

      css_actor = GTK_CSS_ACTOR (iter);
      if (gtk_css_actor_owns_context (css_actor))
        continue;

      gtk_css_actor_set_style_context (css_actor, context);
    }

  g_object_notify (G_OBJECT (self), "style-context");
}

static void 
gtk_css_actor_real_parent_set (GtkActor *actor,
                               GtkActor *old_parent)
{
  GtkCssActor *self = GTK_CSS_ACTOR (actor);
  GtkActor *parent;

  parent = _gtk_actor_get_parent (actor);

  if (parent != NULL)
    {
      g_warn_if_fail (GTK_IS_CSS_ACTOR (parent));
    }

  GTK_ACTOR_CLASS (_gtk_css_actor_parent_class)->parent_set (actor, old_parent);

  if (!gtk_css_actor_owns_context (self))
    {
      if (parent)
        gtk_css_actor_set_style_context (self, _gtk_css_actor_get_style_context (GTK_CSS_ACTOR (parent)));
      else
        gtk_css_actor_set_style_context (self, NULL);
    }
}

static void
gtk_css_actor_real_style_updated (GtkCssActor      *actor,
                                  const GtkBitmask *changed)
{
}

static void
_gtk_css_actor_class_init (GtkCssActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkActorClass *actor_class = GTK_ACTOR_CLASS (klass);

  object_class->finalize = gtk_css_actor_finalize;
  object_class->set_property = gtk_css_actor_set_property;
  object_class->get_property = gtk_css_actor_get_property;

  actor_class->parent_set = gtk_css_actor_real_parent_set;

  klass->style_updated = gtk_css_actor_real_style_updated;

  /**
   * GtkCssActor:style-context:
   *
   * The style context in use by the actor.
   *
   * FIXME: Do we want to replace this with a GtkCssComputedValues?
   */
  obj_props[PROP_STYLE_CONTEXT] =
    g_param_spec_object ("style-context",
                         P_("Style Context"),
                         P_("Style context used by this actor"),
                         GTK_TYPE_STYLE_CONTEXT,
                         GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);

  g_type_class_add_private (klass, sizeof (GtkCssActorPrivate));
}

static void
_gtk_css_actor_init (GtkCssActor *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GTK_TYPE_CSS_ACTOR,
                                            GtkCssActorPrivate);
}

GtkStyleContext *
_gtk_css_actor_get_style_context (GtkCssActor *actor)
{
  g_return_val_if_fail (GTK_IS_CSS_ACTOR (actor), NULL);

  return actor->priv->context;
}

static gboolean
actor_needs_to_use_widget_path (GtkActor *actor)
{
  GtkWidget *widget, *parent;

  if (!GTK_IS_WIDGET_ACTOR (actor))
    return FALSE;

  widget = _gtk_actor_get_widget (actor);
  if (widget == NULL)
    return FALSE;

  parent = gtk_widget_get_parent (widget);
  if (parent == NULL)
    return FALSE;

  if (GTK_CONTAINER_GET_CLASS (parent)->get_path_for_child == 
      GTK_CONTAINER_CLASS(g_type_class_peek (GTK_TYPE_CONTAINER))->get_path_for_child)
    return FALSE;

  return TRUE;
}

static gboolean
gtk_css_actor_source_init_css_matcher (GtkCssMatcher *matcher,
                                       gpointer       actor)
{
  if (actor_needs_to_use_widget_path (actor))
    return FALSE;

  _gtk_css_matcher_actor_init (matcher, actor);

  return TRUE;
}

static GtkWidgetPath *
gtk_css_actor_source_create_query_path (gpointer actor)
{
  GtkWidget *widget;
  
  widget = _gtk_actor_get_widget (actor);
  if (widget == NULL)
    return NULL;

  return _gtk_widget_create_path (widget);
}

static const GtkWidgetPath *
gtk_css_actor_source_get_path (gpointer actor)
{
  GtkWidget *widget;
  
  widget = _gtk_actor_get_widget (actor);
  if (widget == NULL)
    return NULL;

  return gtk_widget_get_path (widget);
}

static void 
gtk_css_actor_source_invalidate (gpointer actor)
{
  GtkStyleContext *context;
  GtkActor *iter;

  context = GTK_CSS_ACTOR (actor)->priv->context;

  GTK_CSS_ACTOR_GET_CLASS (actor)->style_updated (actor, _gtk_style_context_get_changes (context));

  for (iter = _gtk_actor_get_first_child (actor);
       iter != NULL;
       iter = _gtk_actor_get_next_sibling (iter))
    {
      if (!GTK_IS_CSS_ACTOR (iter))
        continue;

      if (gtk_css_actor_owns_context (GTK_CSS_ACTOR (iter)))
        continue;

      gtk_css_actor_source_invalidate (iter);
    }
}

static void 
gtk_css_actor_source_queue_invalidate (gpointer actor)
{
  GtkWidget *widget;

  widget = _gtk_actor_get_widget (actor);

  if (GTK_IS_RESIZE_CONTAINER (widget))
    _gtk_container_queue_restyle (GTK_CONTAINER (widget));
}

static gboolean
gtk_css_actor_source_should_animate (gpointer actor)
{
  if (!_gtk_actor_get_mapped (actor))
    return FALSE;

#if 0
  gboolean animate;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-enable-animations", &animate,
                NULL);

  return animate;
#endif

  return TRUE;
}

static GType
gtk_css_actor_source_get_widget_type (gpointer actor)
{
  GtkWidget *widget;
  
  widget = _gtk_actor_get_widget (actor);
  if (widget == NULL)
    return G_TYPE_INVALID;

  return G_OBJECT_TYPE (widget);
}

static void 
gtk_css_actor_source_destroy (gpointer actor)
{
}

static const GtkStyleContextSource gtk_css_actor_source = {
  TRUE,
  FALSE,
  gtk_css_actor_source_init_css_matcher,
  gtk_css_actor_source_create_query_path,
  gtk_css_actor_source_get_path,
  gtk_css_actor_source_invalidate,
  gtk_css_actor_source_queue_invalidate,
  gtk_css_actor_source_should_animate,
  gtk_css_actor_source_get_widget_type,
  gtk_css_actor_source_destroy
};

void
_gtk_css_actor_init_box (GtkCssActor *self)
{
  GtkActor *actor = GTK_ACTOR (self);
  GtkCssActorPrivate *priv = self->priv;

  priv->context = gtk_style_context_new ();
  gtk_style_context_set_screen (priv->context, _gtk_actor_get_screen (actor));
  _gtk_style_context_set_source (priv->context, &gtk_css_actor_source, actor);
}

