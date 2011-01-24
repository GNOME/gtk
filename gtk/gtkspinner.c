/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2007 John Stowers, Neil Jagdish Patel.
 * Copyright (C) 2009 Bastien Nocera, David Zeuthen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 *
 * Code adapted from egg-spinner
 * by Christian Hergert <christian.hergert@gmail.com>
 */

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkimage.h"
#include "gtkspinner.h"
#include "gtkstyle.h"


/**
 * SECTION:gtkspinner
 * @Short_description: Show a spinner animation
 * @Title: GtkSpinner
 * @See_also: #GtkCellRendererSpinner, #GtkProgressBar
 *
 * A GtkSpinner widget displays an icon-size spinning animation.
 * It is often used as an alternative to a #GtkProgressBar for
 * displaying indefinite activity, instead of actual progress.
 *
 * To start the animation, use gtk_spinner_start(), to stop it
 * use gtk_spinner_stop().
 */


#define SPINNER_SIZE 12

enum {
  PROP_0,
  PROP_ACTIVE
};

struct _GtkSpinnerPrivate
{
  gboolean active;
};

static gboolean gtk_spinner_draw       (GtkWidget       *widget,
                                        cairo_t         *cr);
static void gtk_spinner_get_property   (GObject         *object,
                                        guint            param_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);
static void gtk_spinner_set_property   (GObject         *object,
                                        guint            param_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_spinner_set_active     (GtkSpinner      *spinner,
                                        gboolean         active);
static void gtk_spinner_get_preferred_width (GtkWidget  *widget,
                                        gint            *minimum_size,
                                        gint            *natural_size);
static void gtk_spinner_get_preferred_height (GtkWidget *widget,
                                        gint            *minimum_size,
                                        gint            *natural_size);

static AtkObject *gtk_spinner_get_accessible      (GtkWidget *widget);
static GType      gtk_spinner_accessible_get_type (void);

G_DEFINE_TYPE (GtkSpinner, gtk_spinner, GTK_TYPE_WIDGET)

static void
gtk_spinner_class_init (GtkSpinnerClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS(klass);
  g_type_class_add_private (gobject_class, sizeof (GtkSpinnerPrivate));
  gobject_class->get_property = gtk_spinner_get_property;
  gobject_class->set_property = gtk_spinner_set_property;

  widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->draw = gtk_spinner_draw;
  widget_class->get_accessible = gtk_spinner_get_accessible;
  widget_class->get_preferred_width = gtk_spinner_get_preferred_width;
  widget_class->get_preferred_height = gtk_spinner_get_preferred_height;

  /* GtkSpinner:active:
   *
   * Whether the spinner is active
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the spinner is active"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
gtk_spinner_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER (object)->priv;

  switch (param_id)
    {
      case PROP_ACTIVE:
        g_value_set_boolean (value, priv->active);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (param_id)
    {
      case PROP_ACTIVE:
        gtk_spinner_set_active (GTK_SPINNER (object), g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_init (GtkSpinner *spinner)
{
  GtkSpinnerPrivate *priv;
  GtkStyleContext *context;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (spinner,
                                      GTK_TYPE_SPINNER,
                                      GtkSpinnerPrivate);
  spinner->priv = priv;

  gtk_widget_set_has_window (GTK_WIDGET (spinner), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (spinner));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SPINNER);
}

static void
gtk_spinner_get_preferred_width (GtkWidget *widget,
                                 gint      *minimum_size,
                                 gint      *natural_size)
{
  if (minimum_size)
    *minimum_size = SPINNER_SIZE;

  if (natural_size)
    *natural_size = SPINNER_SIZE;
}

static void
gtk_spinner_get_preferred_height (GtkWidget *widget,
                                  gint      *minimum_size,
                                  gint      *natural_size)
{
  if (minimum_size)
    *minimum_size = SPINNER_SIZE;

  if (natural_size)
    *natural_size = SPINNER_SIZE;
}

static gboolean
gtk_spinner_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_set_state (context, state);
  gtk_render_activity (context, cr, 0, 0,
                       gtk_widget_get_allocated_width (widget),
                       gtk_widget_get_allocated_height (widget));

  return FALSE;
}

static void
gtk_spinner_set_active (GtkSpinner *spinner,
                        gboolean    active)
{
  GtkSpinnerPrivate *priv = spinner->priv;

  active = !!active;

  if (priv->active != active)
    {
      priv->active = active;

      g_object_notify (G_OBJECT (spinner), "active");

      if (active)
        gtk_widget_set_state_flags (GTK_WIDGET (spinner),
                                    GTK_STATE_FLAG_ACTIVE, FALSE);
      else
        gtk_widget_unset_state_flags (GTK_WIDGET (spinner),
                                      GTK_STATE_FLAG_ACTIVE);
    }
}

/* accessible implementation */

static void
gtk_spinner_accessible_image_get_size (AtkImage *image,
                                       gint     *width,
                                       gint     *height)
{
  GtkAllocation allocation;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (image));
  if (widget == NULL)
    {
      *width = *height = 0;
    }
  else
    {
      gtk_widget_get_allocation (widget, &allocation);
      *width = allocation.width;
      *height = allocation.height;
    }
}

static void
gtk_spinner_accessible_image_iface_init (AtkImageIface *iface)
{
  iface->get_image_size = gtk_spinner_accessible_image_get_size;
}

/* dummy typedef */
typedef struct _GtkSpinnerAccessible            GtkSpinnerAccessible;
typedef struct _GtkSpinnerAccessibleClass       GtkSpinnerAccessibleClass;

ATK_DEFINE_TYPE_WITH_CODE (GtkSpinnerAccessible,
                           gtk_spinner_accessible,
                           GTK_TYPE_IMAGE,
                           G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE,
                                                  gtk_spinner_accessible_image_iface_init));

static void
gtk_spinner_accessible_initialize (AtkObject *accessible,
                                   gpointer   widget)
{
  ATK_OBJECT_CLASS (gtk_spinner_accessible_parent_class)->initialize (accessible, widget);

  atk_object_set_name (accessible, C_("throbbing progress animation widget", "Spinner"));
  atk_object_set_description (accessible, _("Provides visual indication of progress"));
}

static void
gtk_spinner_accessible_class_init (GtkSpinnerAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->initialize = gtk_spinner_accessible_initialize;
}

static void
gtk_spinner_accessible_init (GtkSpinnerAccessible *self)
{
}

/* factory */
typedef AtkObjectFactory        GtkSpinnerAccessibleFactory;
typedef AtkObjectFactoryClass   GtkSpinnerAccessibleFactoryClass;

G_DEFINE_TYPE (GtkSpinnerAccessibleFactory,
               _gtk_spinner_accessible_factory,
               ATK_TYPE_OBJECT_FACTORY);

static GType
gtk_spinner_accessible_factory_get_accessible_type (void)
{
  return gtk_spinner_accessible_get_type ();
}

static AtkObject *
gtk_spinner_accessible_factory_create_accessible (GObject *obj)
{
  AtkObject *accessible;

  accessible = g_object_new (gtk_spinner_accessible_get_type (), NULL);
  atk_object_initialize (accessible, obj);

  return accessible;
}

static void
_gtk_spinner_accessible_factory_class_init (AtkObjectFactoryClass *klass)
{
  klass->create_accessible = gtk_spinner_accessible_factory_create_accessible;
  klass->get_accessible_type = gtk_spinner_accessible_factory_get_accessible_type;
}

static void
_gtk_spinner_accessible_factory_init (AtkObjectFactory *factory)
{
}

static AtkObject *
gtk_spinner_get_accessible (GtkWidget *widget)
{
  static gboolean first_time = TRUE;

  if (first_time)
    {
      AtkObjectFactory *factory;
      AtkRegistry *registry;
      GType derived_type;
      GType derived_atk_type;

      /*
       * Figure out whether accessibility is enabled by looking at the
       * type of the accessible object which would be created for
       * the parent type of GtkSpinner.
       */
      derived_type = g_type_parent (GTK_TYPE_SPINNER);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry, derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE))
        atk_registry_set_factory_type (registry,
                                       GTK_TYPE_SPINNER,
                                       _gtk_spinner_accessible_factory_get_type ());
      first_time = FALSE;
    }

  return GTK_WIDGET_CLASS (gtk_spinner_parent_class)->get_accessible (widget);
}

/**
 * gtk_spinner_new:
 *
 * Returns a new spinner widget. Not yet started.
 *
 * Return value: a new #GtkSpinner
 *
 * Since: 2.20
 */
GtkWidget *
gtk_spinner_new (void)
{
  return g_object_new (GTK_TYPE_SPINNER, NULL);
}

/**
 * gtk_spinner_start:
 * @spinner: a #GtkSpinner
 *
 * Starts the animation of the spinner.
 *
 * Since: 2.20
 */
void
gtk_spinner_start (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_active (spinner, TRUE);
}

/**
 * gtk_spinner_stop:
 * @spinner: a #GtkSpinner
 *
 * Stops the animation of the spinner.
 *
 * Since: 2.20
 */
void
gtk_spinner_stop (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_active (spinner, FALSE);
}
