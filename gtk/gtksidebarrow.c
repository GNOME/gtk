/* gtksidebarrow.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gtksidebarrowprivate.h"
/* For section and place type enums */
#include "gtkplacessidebarprivate.h"
#include "gtkplacessidebar.h"
#include "gtkwidget.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkstylecontext.h"
#include "gtkrevealer.h"
#include "gtkselection.h"

struct _GtkSidebarRow
{
  GtkListBoxRow parent_instance;
  GIcon *icon;
  GtkWidget *icon_widget;
  gchar *label;
  gchar *tooltip;
  GtkWidget *label_widget;
  gboolean ejectable;
  GtkWidget *eject_button;
  gint order_index;
  GtkPlacesSidebarSectionType section_type;
  GtkPlacesSidebarPlaceType place_type;
  gchar *uri;
  GDrive *drive;
  GVolume *volume;
  GMount *mount;
  gboolean sensitive;
  gboolean placeholder;
  GtkPlacesSidebar *sidebar;
  GtkWidget *event_box;
  GtkWidget *revealer;
};

G_DEFINE_TYPE (GtkSidebarRow, gtk_sidebar_row, GTK_TYPE_LIST_BOX_ROW)

enum
{
  PROP_0,
  PROP_ICON,
  PROP_LABEL,
  PROP_TOOLTIP,
  PROP_EJECTABLE,
  PROP_SIDEBAR,
  PROP_ORDER_INDEX,
  PROP_SECTION_TYPE,
  PROP_PLACE_TYPE,
  PROP_URI,
  PROP_DRIVE,
  PROP_VOLUME,
  PROP_MOUNT,
  PROP_SENSITIVE,
  PROP_PLACEHOLDER,
  PROP_EJECT_BUTTON,
  PROP_EVENT_BOX,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gtk_sidebar_row_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkSidebarRow *self = GTK_SIDEBAR_ROW (object);

  switch (prop_id)
    {
    case PROP_SIDEBAR:
      {
        g_value_set_object (value, self->sidebar);
        break;
      }

    case PROP_ICON:
      {
        g_value_set_object (value, self->icon);
        break;
      }

    case PROP_LABEL:
      {
        g_value_set_string (value, self->label);
        break;
      }

    case PROP_TOOLTIP:
      {
        g_value_set_string (value, self->tooltip);
        break;
      }

    case PROP_EJECTABLE:
      {
        g_value_set_boolean (value, self->ejectable);
        break;
      }

    case PROP_ORDER_INDEX:
      {
        g_value_set_int (value, self->order_index);
          break;
      }

    case PROP_SECTION_TYPE:
      {
        g_value_set_int (value, self->section_type);
        break;
      }

    case PROP_PLACE_TYPE:
      {
        g_value_set_int (value, self->place_type);
        break;
      }

    case PROP_URI:
      {
        g_value_set_string (value, self->uri);
        break;
      }

    case PROP_DRIVE:
      {
        g_value_set_object (value, self->drive);
        break;
      }

    case PROP_VOLUME:
      {
        g_value_set_object (value, self->volume);
        break;
      }

    case PROP_MOUNT:
      {
        g_value_set_object (value, self->mount);
        break;
      }

    case PROP_SENSITIVE:
      {
        g_value_set_boolean (value, self->sensitive);
        break;
      }

    case PROP_PLACEHOLDER:
      {
        g_value_set_boolean (value, self->placeholder);
        break;
      }

    case PROP_EJECT_BUTTON:
      {
        g_value_set_object (value, self->eject_button);
        break;
      }

    case PROP_EVENT_BOX:
      {
        g_value_set_object (value, self->event_box);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_sidebar_row_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkSidebarRow *self = GTK_SIDEBAR_ROW (object);

  switch (prop_id)
    {
    case PROP_SIDEBAR:
      {
        self->sidebar = g_value_get_object (value);
        break;
      }

    case PROP_ICON:
      {
        g_clear_object (&self->icon);
        if (value != NULL)
          {
            self->icon = g_object_ref (g_value_get_object (value));
            gtk_image_set_from_gicon (GTK_IMAGE (self->icon_widget), self->icon, GTK_ICON_SIZE_MENU);
          }
        else
          {
            self->icon = NULL;
            gtk_image_clear (GTK_IMAGE (self->icon_widget));
          }
        break;
      }

    case PROP_LABEL:
      {
        g_free (self->label);
        self->label = g_strdup (g_value_get_string (value));
        gtk_label_set_text (GTK_LABEL (self->label_widget), self->label);
        break;
      }

    case PROP_TOOLTIP:
      {
        g_free (self->tooltip);
        self->tooltip = g_strdup (g_value_get_string (value));
        gtk_widget_set_tooltip_text (GTK_WIDGET (self), self->tooltip);
        break;
      }

    case PROP_EJECTABLE:
      {
        self->ejectable = g_value_get_boolean (value);
        if (self->ejectable)
          gtk_widget_show (self->eject_button);
        else
          gtk_widget_hide (self->eject_button);
        break;
      }

    case PROP_ORDER_INDEX:
      {
        self->order_index = g_value_get_int (value);
        break;
      }

    case PROP_SECTION_TYPE:
      {
        self->section_type = g_value_get_int (value);
        if (self->section_type != SECTION_COMPUTER)
          gtk_label_set_ellipsize (GTK_LABEL (self->label_widget), PANGO_ELLIPSIZE_END);
        else
          gtk_label_set_ellipsize (GTK_LABEL (self->label_widget), PANGO_ELLIPSIZE_NONE);
        break;
      }

    case PROP_PLACE_TYPE:
      {
        self->place_type = g_value_get_int (value);
        break;
      }

    case PROP_URI:
      {
        g_free (self->uri);
        self->uri = g_strdup (g_value_get_string (value));
        break;
      }

    case PROP_DRIVE:
      {
        gpointer *object;

        g_clear_object (&self->drive);
        object = g_value_get_object (value);
        if (object != NULL)
          self->drive = g_object_ref (object);
        break;
      }

    case PROP_VOLUME:
      {
        gpointer *object;

        g_clear_object (&self->volume);
        object = g_value_get_object (value);
        if (object != NULL)
          self->volume = g_object_ref (object);
        break;
      }

    case PROP_MOUNT:
      {
        gpointer *object;

        g_clear_object (&self->mount);
        object = g_value_get_object (value);
        if (object != NULL)
          self->mount = g_object_ref (object);
        break;
      }

    case PROP_SENSITIVE:
      {
        GtkStyleContext *style_context;

        self->sensitive = g_value_get_boolean (value);
       	style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
        /* Modifying the actual sensitivity of the widget makes the drag state
         * to change and calling drag-leave wich modifies the gtklistbox and a
         * style race ocurs. So since we only use it for show which rows are
         * drop targets, we can simple use a dim-label style */
        if (self->sensitive)
          gtk_style_context_remove_class (style_context, "dim-label");
        else
          gtk_style_context_add_class (style_context, "dim-label");

        break;
      }

    case PROP_PLACEHOLDER:
      {
	      GtkStyleContext *context;

        self->placeholder = g_value_get_boolean (value);
        if (self->placeholder)
          {
            g_clear_object (&self->icon);
            g_free (self->label);
            self->label = NULL;
            g_free (self->tooltip);
            self->tooltip = NULL;
            gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);
            self->ejectable = FALSE;
            self->section_type = SECTION_BOOKMARKS;
            self->place_type = PLACES_BOOKMARK_PLACEHOLDER;
            g_free (self->uri);
            self->uri = NULL;
            g_clear_object (&self->drive);
            g_clear_object (&self->volume);
            g_clear_object (&self->mount);

            gtk_container_foreach (GTK_CONTAINER (self),
                                   (GtkCallback) gtk_widget_destroy,
                                   NULL);

	          context = gtk_widget_get_style_context (GTK_WIDGET (self));
            gtk_style_context_add_class (context, "sidebar-placeholder-row");
          }

        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
on_child_revealed (GObject    *self,
                   GParamSpec *pspec,
                   gpointer    user_data)
{
 /* We need to hide the actual widget because if not the GtkListBoxRow will
  * still allocate the paddings, even if the revealer is not revealed, and
  * therefore the row will be still somewhat visible. */
  if (!gtk_revealer_get_reveal_child (GTK_REVEALER (self)))
    gtk_widget_hide (GTK_WIDGET (GTK_SIDEBAR_ROW (user_data)));
}

void
gtk_sidebar_row_reveal (GtkSidebarRow *self)
{
  gtk_widget_show_all (GTK_WIDGET (self));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
}

void
gtk_sidebar_row_hide (GtkSidebarRow *self,
                      gboolean       inmediate)
{
  guint transition_duration;

  transition_duration = gtk_revealer_get_transition_duration (GTK_REVEALER (self->revealer));
  if (inmediate)
      gtk_revealer_set_transition_duration (GTK_REVEALER (self->revealer), 0);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);

  gtk_revealer_set_transition_duration (GTK_REVEALER (self->revealer), transition_duration);
}

static void
gtk_sidebar_row_finalize (GObject *object)
{
  GtkSidebarRow *self = GTK_SIDEBAR_ROW (object);

  g_clear_object (&self->icon);
  g_free (self->label);
  self->label = NULL;
  g_free (self->tooltip);
  self->tooltip = NULL;
  g_free (self->uri);
  self->uri = NULL;
  g_clear_object (&self->drive);
  g_clear_object (&self->volume);
  g_clear_object (&self->mount);
}

static void
gtk_sidebar_row_init (GtkSidebarRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gtk_sidebar_row_class_init (GtkSidebarRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gtk_sidebar_row_get_property;
  object_class->set_property = gtk_sidebar_row_set_property;
  object_class->finalize = gtk_sidebar_row_finalize;

  gParamSpecs [PROP_SIDEBAR] =
    g_param_spec_object ("sidebar",
                         "Sidebar",
                         "Sidebar",
                         GTK_TYPE_PLACES_SIDEBAR,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SIDEBAR,
                                   gParamSpecs [PROP_SIDEBAR]);
  gParamSpecs [PROP_ICON] =
    g_param_spec_object ("icon",
                         "icon",
                         "The place icon.",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ICON,
                                   gParamSpecs [PROP_ICON]);

  gParamSpecs [PROP_LABEL] =
    g_param_spec_string ("label",
                         "label",
                         "The label text.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LABEL,
                                   gParamSpecs [PROP_LABEL]);

  gParamSpecs [PROP_TOOLTIP] =
    g_param_spec_string ("tooltip",
                         "Tooltip",
                         "Tooltip",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TOOLTIP,
                                   gParamSpecs [PROP_TOOLTIP]);

  gParamSpecs [PROP_EJECTABLE] =
    g_param_spec_boolean ("ejectable",
                          "Ejectable",
                          "Ejectable",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_EJECTABLE,
                                   gParamSpecs [PROP_EJECTABLE]);

  gParamSpecs [PROP_ORDER_INDEX] =
    g_param_spec_int ("order-index",
                      "OrderIndex",
                      "Order Index",
                      0, G_MAXINT, 0,
                      (G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ORDER_INDEX,
                                   gParamSpecs [PROP_ORDER_INDEX]);

  gParamSpecs [PROP_SECTION_TYPE] =
    g_param_spec_int ("section-type",
                      "section type",
                      "The section type.",
                      SECTION_INVALID, N_SECTIONS, SECTION_INVALID,
                      (G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SECTION_TYPE,
                                   gParamSpecs [PROP_SECTION_TYPE]);

  gParamSpecs [PROP_PLACE_TYPE] =
    g_param_spec_int ("place-type",
                      "place type",
                      "The place type.",
                      PLACES_INVALID, N_PLACES, PLACES_INVALID,
                      (G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PLACE_TYPE,
                                   gParamSpecs [PROP_PLACE_TYPE]);

  gParamSpecs [PROP_URI] =
    g_param_spec_string ("uri",
                         "Uri",
                         "Uri",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_URI,
                                   gParamSpecs [PROP_URI]);
  gParamSpecs [PROP_DRIVE] =
    g_param_spec_object ("drive",
                         "Drive",
                         "Drive",
                         G_TYPE_DRIVE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DRIVE,
                                   gParamSpecs [PROP_DRIVE]);

  gParamSpecs [PROP_VOLUME] =
    g_param_spec_object ("volume",
                         "Volume",
                         "Volume",
                         G_TYPE_VOLUME,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_VOLUME,
                                   gParamSpecs [PROP_VOLUME]);

  gParamSpecs [PROP_MOUNT] =
    g_param_spec_object ("mount",
                         "Mount",
                         "Mount",
                         G_TYPE_MOUNT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MOUNT,
                                   gParamSpecs [PROP_MOUNT]);
  gParamSpecs [PROP_SENSITIVE] =
    g_param_spec_boolean ("sensitive",
                          "Sensitive",
                          "Make the row sensitive or not",
                          TRUE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SENSITIVE,
                                   gParamSpecs [PROP_SENSITIVE]);

  gParamSpecs [PROP_PLACEHOLDER] =
    g_param_spec_boolean ("placeholder",
                          "Placeholder",
                          "Placeholder",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PLACEHOLDER,
                                   gParamSpecs [PROP_PLACEHOLDER]);

  gParamSpecs [PROP_EJECT_BUTTON] =
   g_param_spec_object ("eject-button",
                        "Eject Button",
                        "Eject button",
                        GTK_TYPE_WIDGET,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_EJECT_BUTTON,
                                   gParamSpecs [PROP_EJECT_BUTTON]);

  gParamSpecs [PROP_EVENT_BOX] =
    g_param_spec_object ("event-box",
                         "Event Box",
                         "Event Box",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_EVENT_BOX,
                                   gParamSpecs [PROP_EVENT_BOX]);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtksidebarrow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, icon_widget);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, label_widget);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, eject_button);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, event_box);
  gtk_widget_class_bind_template_child (widget_class, GtkSidebarRow, revealer);

  gtk_widget_class_bind_template_callback (widget_class, on_child_revealed);
}


GtkSidebarRow*
gtk_sidebar_row_clone (GtkSidebarRow *self)
{
 return  g_object_new (GTK_TYPE_SIDEBAR_ROW,
                       "sidebar", self->sidebar,
                       "icon", self->icon,
                       "label", self->label,
                       "tooltip", self->tooltip,
                       "ejectable", self->ejectable,
                       "order-index", self->order_index,
                       "section-type", self->section_type,
                       "place-type", self->place_type,
                       "uri", self->uri,
                       "drive", self->drive,
                       "volume", self->volume,
                       "mount", self->mount,
                       NULL);
}
