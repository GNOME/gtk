/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>

#include <gtk/gtkthemingengine.h>
#include <gtk/gtkstylecontext.h>
#include <gtk/gtkintl.h>

#include "gtkalias.h"

typedef struct GtkThemingEnginePrivate GtkThemingEnginePrivate;

struct GtkThemingEnginePrivate
{
  GtkStyleContext *context;
};

#define GTK_THEMING_ENGINE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_THEMING_ENGINE, GtkThemingEnginePrivate))

G_DEFINE_TYPE (GtkThemingEngine, gtk_theming_engine, G_TYPE_OBJECT)


typedef struct GtkThemingModule GtkThemingModule;
typedef struct GtkThemingModuleClass GtkThemingModuleClass;

struct GtkThemingModule
{
  GTypeModule parent_instance;
  gchar *name;

  GtkThemingEngine * (*create_engine) (void);
};

struct GtkThemingModuleClass
{
  GTypeModuleClass parent_class;
};

#define GTK_TYPE_THEMING_MODULE  (gtk_theming_module_get_type ())
#define GTK_THEMING_MODULE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_THEMING_MODULE, GtkThemingModule))
#define GTK_IS_THEMING_MODULE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_THEMING_MODULE))

G_DEFINE_TYPE (GtkThemingModule, gtk_theming_module, G_TYPE_TYPE_MODULE);

static void
gtk_theming_engine_class_init (GtkThemingEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (object_class, sizeof (GtkThemingEnginePrivate));
}

static void
gtk_theming_engine_init (GtkThemingEngine *engine)
{
  engine->priv = GTK_THEMING_ENGINE_GET_PRIVATE (engine);
}

void
_gtk_theming_engine_set_context (GtkThemingEngine *engine,
                                 GtkStyleContext  *context)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = engine->priv;
  priv->context = context;
}

void
gtk_theming_engine_get_property (GtkThemingEngine *engine,
                                 const gchar      *property,
                                 GtkStateType      state,
                                 GValue           *value)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (property != NULL);
  g_return_if_fail (state < GTK_STATE_LAST);
  g_return_if_fail (value != NULL);

  priv = engine->priv;
  gtk_style_context_get_property (priv->context, property, state, value);
}

void
gtk_theming_engine_get_valist (GtkThemingEngine *engine,
                               GtkStateType      state,
                               va_list           args)
{
  GtkThemingEnginePrivate *priv;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = engine->priv;
  gtk_style_context_get_valist (priv->context, state, args);
}

void
gtk_theming_engine_get (GtkThemingEngine *engine,
                        GtkStateType      state,
                        ...)
{
  GtkThemingEnginePrivate *priv;
  va_list args;

  g_return_if_fail (GTK_IS_THEMING_ENGINE (engine));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = engine->priv;

  va_start (args, state);
  gtk_style_context_get_valist (priv->context, state, args);
  va_end (args);
}

GtkStateFlags
gtk_theming_engine_get_state (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_get_state (priv->context);
}

gboolean
gtk_theming_engine_is_state_set (GtkThemingEngine *engine,
                                 GtkStateType      state)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), 0);

  priv = engine->priv;
  return gtk_style_context_is_state_set (priv->context, state);
}

G_CONST_RETURN GtkWidgetPath *
gtk_theming_engine_get_path (GtkThemingEngine *engine)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), NULL);

  priv = engine->priv;
  return gtk_style_context_get_path (priv->context);
}

gboolean
gtk_theming_engine_has_class (GtkThemingEngine *engine,
                              const gchar      *style_class)
{
  GtkThemingEnginePrivate *priv;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
  return gtk_style_context_has_class (priv->context, style_class);
}

gboolean
gtk_theming_engine_has_child_class (GtkThemingEngine   *engine,
                                    const gchar        *style_class,
                                    GtkChildClassFlags *flags)
{
  GtkThemingEnginePrivate *priv;

  if (flags)
    *flags = 0;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), FALSE);

  priv = engine->priv;
  return gtk_style_context_has_child_class (priv->context, style_class, flags);
}

/* GtkThemingModule */

static gboolean
gtk_theming_module_load (GTypeModule *type_module)
{
  GtkThemingModule *theming_module;
  GModule *module;
  gchar *name, *module_path;

  theming_module = GTK_THEMING_MODULE (type_module);
  name = theming_module->name;
  module_path = _gtk_find_module (name, "theming-engines");

  if (!module_path)
    {
      g_warning (_("Unable to locate theme engine in module path: \"%s\","), name);
      return FALSE;
    }

  module = g_module_open (module_path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  g_free (module_path);

  if (!module)
    {
      g_warning ("%s", g_module_error ());
      return FALSE;
    }

  if (!g_module_symbol (module, "create_engine",
                        (gpointer *) &theming_module->create_engine))
    {
      g_warning ("%s", g_module_error());
      g_module_close (module);

      return FALSE;
    }

  g_module_make_resident (module);

  return TRUE;
}

static void
gtk_theming_module_class_init (GtkThemingModuleClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (klass);

  module_class->load = gtk_theming_module_load;
}

static void
gtk_theming_module_init (GtkThemingModule *module)
{
}

G_CONST_RETURN GtkThemingEngine *
gtk_theming_engine_load (const gchar *name)
{
  static GHashTable *engines = NULL;
  static GtkThemingEngine *default_engine;
  GtkThemingEngine *engine = NULL;

  if (name)
    {
      if (!engines)
        engines = g_hash_table_new (g_str_hash, g_str_equal);

      engine = g_hash_table_lookup (engines, name);

      if (!engine)
        {
          GtkThemingModule *module;

          module = g_object_new (GTK_TYPE_THEMING_MODULE, NULL);
          g_type_module_set_name (G_TYPE_MODULE (module), name);
          module->name = g_strdup (name);

          if (module && g_type_module_use (G_TYPE_MODULE (module)))
            {
              engine = (module->create_engine) ();

              if (engine)
                g_hash_table_insert (engines, module->name, engine);
            }
        }
    }

  if (!engine)
    {
      if (!default_engine)
        default_engine = g_object_new (GTK_TYPE_THEMING_ENGINE, NULL);

      engine = default_engine;
    }

  return engine;
}

#define __GTK_THEMING_ENGINE_C__
#include "gtkaliasdef.c"
