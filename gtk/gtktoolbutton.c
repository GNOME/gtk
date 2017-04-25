/* gtktoolbutton.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gtktoolbutton.h"
#include "gtkbutton.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkintl.h"
#include "gtktoolbarprivate.h"
#include "gtkicontheme.h"
#include "gtkactionable.h"
#include "gtkprivate.h"

#include <string.h>


/**
 * SECTION:gtktoolbutton
 * @Short_description: A GtkToolItem subclass that displays buttons
 * @Title: GtkToolButton
 * @See_also: #GtkToolbar, #GtkMenuToolButton, #GtkToggleToolButton,
 *   #GtkRadioToolButton, #GtkSeparatorToolItem
 *
 * #GtkToolButtons are #GtkToolItems containing buttons.
 *
 * Use gtk_tool_button_new() to create a new #GtkToolButton.
 *
 * The label of a #GtkToolButton is determined by the properties
 * #GtkToolButton:label-widget and #GtkToolButton:label.
 * If #GtkToolButton:label-widget is
 * non-%NULL, then that widget is used as the label. Otherwise, if
 * #GtkToolButton:label is non-%NULL, that string is used as the label.
 *
 * The icon of a #GtkToolButton is determined by the
 * #GtkToolButton:icon-widget property. If
 * #GtkToolButton:icon-widget is non-%NULL, then
 * that widget is used as the icon. Otherwise it does not have an icon.
 *
 * # CSS nodes
 *
 * GtkToolButton has a single CSS node with name toolbutton.
 */


#define MENU_ID "gtk-tool-button-menu-id"

enum {
  CLICKED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_LABEL_WIDGET,
  PROP_ICON_NAME,
  PROP_ICON_WIDGET,
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET
};

static void gtk_tool_button_init          (GtkToolButton      *button,
					   GtkToolButtonClass *klass);
static void gtk_tool_button_class_init    (GtkToolButtonClass *klass);
static void gtk_tool_button_set_property  (GObject            *object,
					   guint               prop_id,
					   const GValue       *value,
					   GParamSpec         *pspec);
static void gtk_tool_button_get_property  (GObject            *object,
					   guint               prop_id,
					   GValue             *value,
					   GParamSpec         *pspec);
static void gtk_tool_button_property_notify (GObject          *object,
					     GParamSpec       *pspec);
static void gtk_tool_button_finalize      (GObject            *object);

static void gtk_tool_button_toolbar_reconfigured (GtkToolItem *tool_item);
static gboolean   gtk_tool_button_create_menu_proxy (GtkToolItem     *item);
static void       button_clicked                    (GtkWidget       *widget,
						     GtkToolButton   *button);

static void gtk_tool_button_construct_contents (GtkToolItem *tool_item);

static void gtk_tool_button_actionable_iface_init      (GtkActionableInterface *iface);


struct _GtkToolButtonPrivate
{
  GtkWidget *button;

  gchar *icon_name;
  gchar *label_text;
  GtkWidget *label_widget;
  GtkWidget *icon_widget;

  GtkSizeGroup *text_size_group;

  guint use_underline : 1;
  guint contents_invalid : 1;
};

static GObjectClass        *parent_class = NULL;
static guint                toolbutton_signals[LAST_SIGNAL] = { 0 };

GType
gtk_tool_button_get_type (void)
{
  static GType g_define_type_id = 0;

  if (!g_define_type_id)
    {
      const GInterfaceInfo actionable_info =
      {
        (GInterfaceInitFunc) gtk_tool_button_actionable_iface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      g_define_type_id = g_type_register_static_simple (GTK_TYPE_TOOL_ITEM,
                                                        I_("GtkToolButton"),
                                                        sizeof (GtkToolButtonClass),
                                                        (GClassInitFunc) gtk_tool_button_class_init,
                                                        sizeof (GtkToolButton),
                                                        (GInstanceInitFunc) gtk_tool_button_init,
                                                        0);

      g_type_add_interface_static (g_define_type_id,
                                   GTK_TYPE_ACTIONABLE, &actionable_info);
    }
  return g_define_type_id;
}

static void
gtk_tool_button_class_init (GtkToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkToolItemClass *tool_item_class;
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  tool_item_class = (GtkToolItemClass *)klass;
  
  object_class->set_property = gtk_tool_button_set_property;
  object_class->get_property = gtk_tool_button_get_property;
  object_class->notify = gtk_tool_button_property_notify;
  object_class->finalize = gtk_tool_button_finalize;

  tool_item_class->create_menu_proxy = gtk_tool_button_create_menu_proxy;
  tool_item_class->toolbar_reconfigured = gtk_tool_button_toolbar_reconfigured;
  
  klass->button_type = GTK_TYPE_BUTTON;

  /* Properties are interpreted like this:
   *
   *          - if the tool button has an icon_widget, then that widget
   *            will be used as the icon. Otherwise, if the tool button
   *            has a stock id, the corresponding stock icon will be
   *            used. Otherwise, if the tool button has an icon name,
   *            the corresponding icon from the theme will be used.
   *            Otherwise, the tool button will not have an icon.
   *
   *          - if the tool button has a label_widget then that widget
   *            will be used as the label. Otherwise, if the tool button
   *            has a label text, that text will be used as label. Otherwise,
   *            if the toolbutton has a stock id, the corresponding text
   *            will be used as label. Otherwise, if the tool button has
   *            an icon name, the corresponding icon name from the theme will
   *            be used. Otherwise, the toolbutton will have an empty label.
   *
   *	      - The use_underline property only has an effect when the label
   *            on the toolbutton comes from the label property (ie. not from
   *            label_widget or from stock_id).
   *
   *            In that case, if use_underline is set,
   *
   *			- underscores are removed from the label text before
   *                      the label is shown on the toolbutton unless the
   *                      underscore is followed by another underscore
   *
   *			- an underscore indicates that the next character when
   *                      used in the overflow menu should be used as a
   *                      mnemonic.
   *
   *		In short: use_underline = TRUE means that the label text has
   *            the form "_Open" and the toolbar should take appropriate
   *            action.
   */

  g_object_class_install_property (object_class,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							P_("Label"),
							P_("Text to show in the item."),
							NULL,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_USE_UNDERLINE,
				   g_param_spec_boolean ("use-underline",
							 P_("Use underline"),
							 P_("If set, an underline in the label property indicates that the next character should be used for the mnemonic accelerator key in the overflow menu"),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
				   PROP_LABEL_WIDGET,
				   g_param_spec_object ("label-widget",
							P_("Label widget"),
							P_("Widget to use as the item label"),
							GTK_TYPE_WIDGET,
							GTK_PARAM_READWRITE));
  /**
   * GtkToolButton:icon-name:
   *
   * The name of the themed icon displayed on the item.
   * This property only has an effect if not overridden by
   * #GtkToolButton:label-widget or #GtkToolButton:icon-widget
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
				   PROP_ICON_NAME,
				   g_param_spec_string ("icon-name",
							P_("Icon name"),
							P_("The name of the themed icon displayed on the item"),
							NULL,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_ICON_WIDGET,
				   g_param_spec_object ("icon-widget",
							P_("Icon widget"),
							P_("Icon widget to display in the item"),
							GTK_TYPE_WIDGET,
							GTK_PARAM_READWRITE));

  g_object_class_override_property (object_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (object_class, PROP_ACTION_TARGET, "action-target");

/**
 * GtkToolButton::clicked:
 * @toolbutton: the object that emitted the signal
 *
 * This signal is emitted when the tool button is clicked with the mouse
 * or activated with the keyboard.
 **/
  toolbutton_signals[CLICKED] =
    g_signal_new (I_("clicked"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkToolButtonClass, clicked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  g_type_class_add_private (object_class, sizeof (GtkToolButtonPrivate));

  gtk_widget_class_set_css_name (widget_class, "toolbutton");
}

static void
gtk_tool_button_init (GtkToolButton      *button,
		      GtkToolButtonClass *klass)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (button);

  button->priv = G_TYPE_INSTANCE_GET_PRIVATE (button,
                                              GTK_TYPE_TOOL_BUTTON,
                                              GtkToolButtonPrivate);

  button->priv->contents_invalid = TRUE;

  gtk_tool_item_set_homogeneous (toolitem, TRUE);

  /* create button */
  button->priv->button = g_object_new (klass->button_type, NULL);
  gtk_widget_set_focus_on_click (GTK_WIDGET (button->priv->button), FALSE);
  g_signal_connect_object (button->priv->button, "clicked",
			   G_CALLBACK (button_clicked), button, 0);

  gtk_container_add (GTK_CONTAINER (button), button->priv->button);
}

static void
gtk_tool_button_construct_contents (GtkToolItem *tool_item)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (tool_item);
  GtkWidget *child;
  GtkWidget *label = NULL;
  GtkWidget *icon = NULL;
  GtkToolbarStyle style;
  gboolean need_label = FALSE;
  gboolean need_icon = FALSE;
  GtkIconSize icon_size;
  GtkWidget *box = NULL;
  GtkOrientation text_orientation = GTK_ORIENTATION_HORIZONTAL;
  GtkSizeGroup *size_group = NULL;
  GtkWidget *parent;

  button->priv->contents_invalid = FALSE;

  if (button->priv->icon_widget)
    {
      parent = gtk_widget_get_parent (button->priv->icon_widget);
      if (parent)
        {
          gtk_container_remove (GTK_CONTAINER (parent),
                                button->priv->icon_widget);
        }
    }

  if (button->priv->label_widget)
    {
      parent = gtk_widget_get_parent (button->priv->label_widget);
      if (parent)
        {
          gtk_container_remove (GTK_CONTAINER (parent),
                                button->priv->label_widget);
        }
    }

  child = gtk_bin_get_child (GTK_BIN (button->priv->button));
  if (child)
    {
      /* Note: we are not destroying the label_widget or icon_widget
       * here because they were removed from their containers above
       */
      gtk_widget_destroy (child);
    }

  style = gtk_tool_item_get_toolbar_style (GTK_TOOL_ITEM (button));
  
  if (style != GTK_TOOLBAR_TEXT)
    need_icon = TRUE;

  if (style != GTK_TOOLBAR_ICONS && style != GTK_TOOLBAR_BOTH_HORIZ)
    need_label = TRUE;

  if (style == GTK_TOOLBAR_BOTH_HORIZ &&
      (gtk_tool_item_get_is_important (GTK_TOOL_ITEM (button)) ||
       gtk_tool_item_get_orientation (GTK_TOOL_ITEM (button)) == GTK_ORIENTATION_VERTICAL ||
       gtk_tool_item_get_text_orientation (GTK_TOOL_ITEM (button)) == GTK_ORIENTATION_VERTICAL))
    {
      need_label = TRUE;
    }

  if (style != GTK_TOOLBAR_TEXT && button->priv->icon_widget == NULL &&
      button->priv->icon_name == NULL)
    {
      need_label = TRUE;
      need_icon = FALSE;
      style = GTK_TOOLBAR_TEXT;
    }

  if (style == GTK_TOOLBAR_TEXT && button->priv->label_widget == NULL &&
      button->priv->label_text == NULL)
    {
      need_label = FALSE;
      need_icon = TRUE;
      style = GTK_TOOLBAR_ICONS;
    }

  if (need_label)
    {
      if (button->priv->label_widget)
	{
	  label = button->priv->label_widget;
	}
      else
	{
	  gboolean elide;
          const char *orig_label;
	  gchar *label_text;

	  if (button->priv->label_text)
	    {
	      orig_label = button->priv->label_text;
	      elide = button->priv->use_underline;
	    }
	  else
	    {
	      orig_label = "";
	      elide = FALSE;
	    }

	  if (elide)
	    label_text = _gtk_toolbar_elide_underscores (orig_label);
	  else
	    label_text = g_strdup (orig_label);

	  label = gtk_label_new (label_text);

	  g_free (label_text);
	}

      if (GTK_IS_LABEL (label))
        {
          gtk_label_set_ellipsize (GTK_LABEL (label),
			           gtk_tool_item_get_ellipsize_mode (GTK_TOOL_ITEM (button)));
          text_orientation = gtk_tool_item_get_text_orientation (GTK_TOOL_ITEM (button));
          if (text_orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
              gfloat align;

              align = gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button));
              if (align < 0.4)
                gtk_widget_set_halign (label, GTK_ALIGN_START);
              else if (align > 0.6)
                gtk_widget_set_halign (label, GTK_ALIGN_END);
              else
                gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
            }
          else
            {
              gfloat align;

              gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_NONE);
              align = gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button));
              if (align < 0.4)
                gtk_widget_set_valign (label, GTK_ALIGN_END);
              else if (align > 0.6)
                gtk_widget_set_valign (label, GTK_ALIGN_START);
              else
                gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
            }
        }
    }

  icon_size = gtk_tool_item_get_icon_size (GTK_TOOL_ITEM (button));
  if (need_icon)
    {
      if (button->priv->icon_widget)
	{
	  icon = button->priv->icon_widget;

	  if (GTK_IS_IMAGE (icon))
	    {
	      g_object_set (button->priv->icon_widget,
			    "icon-size", icon_size,
			    NULL);
	    }
	}
      else if (button->priv->icon_name)
	{
	  icon = gtk_image_new_from_icon_name (button->priv->icon_name, icon_size);
	}

      if (icon)
	{
          if (text_orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              gfloat align;

              align = gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button));
              if (align > 0.6) 
                gtk_widget_set_halign (icon, GTK_ALIGN_START);
              else if (align < 0.4)
                gtk_widget_set_halign (icon, GTK_ALIGN_END);
              else
                gtk_widget_set_halign (icon, GTK_ALIGN_CENTER);
            }
          else
            {
              gfloat align;

              align = gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button));
              if (align > 0.6) 
                gtk_widget_set_valign (icon, GTK_ALIGN_END);
              else if (align < 0.4)
                gtk_widget_set_valign (icon, GTK_ALIGN_START);
              else
               gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
            }

	  size_group = gtk_tool_item_get_text_size_group (GTK_TOOL_ITEM (button));
	  if (size_group != NULL)
	    gtk_size_group_add_widget (size_group, icon);
	}
    }

  switch (style)
    {
    case GTK_TOOLBAR_ICONS:
      if (icon)
        gtk_container_add (GTK_CONTAINER (button->priv->button), icon);
      gtk_style_context_add_class (gtk_widget_get_style_context (button->priv->button), "image-button");
      gtk_style_context_remove_class (gtk_widget_get_style_context (button->priv->button), "text-button");
      break;

    case GTK_TOOLBAR_BOTH:
      if (text_orientation == GTK_ORIENTATION_HORIZONTAL)
        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      else
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL,0);
      if (icon)
          gtk_box_pack_start (GTK_BOX (box), icon);
      gtk_box_pack_end (GTK_BOX (box), label);
      gtk_container_add (GTK_CONTAINER (button->priv->button), box);
      gtk_style_context_add_class (gtk_widget_get_style_context (button->priv->button), "image-button");
      gtk_style_context_add_class (gtk_widget_get_style_context (button->priv->button), "text-button");
      break;

    case GTK_TOOLBAR_BOTH_HORIZ:
      if (text_orientation == GTK_ORIENTATION_HORIZONTAL)
	{
          box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	  if (icon)
            gtk_box_pack_start (GTK_BOX (box), icon);
	  if (label)
            gtk_box_pack_end (GTK_BOX (box), label);
	}
      else
	{
          box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	  if (icon)
            gtk_box_pack_end (GTK_BOX (box), icon);
	  if (label)
            gtk_box_pack_start (GTK_BOX (box), label);
	}
      gtk_container_add (GTK_CONTAINER (button->priv->button), box);
      gtk_style_context_add_class (gtk_widget_get_style_context (button->priv->button), "image-button");
      gtk_style_context_add_class (gtk_widget_get_style_context (button->priv->button), "text-button");
      break;

    case GTK_TOOLBAR_TEXT:
      gtk_container_add (GTK_CONTAINER (button->priv->button), label);
      gtk_style_context_add_class (gtk_widget_get_style_context (button->priv->button), "text-button");
      gtk_style_context_remove_class (gtk_widget_get_style_context (button->priv->button), "image-button");
      break;
    }

  if (box)
    gtk_widget_show (box);

  gtk_tool_item_rebuild_menu (tool_item);
  
  gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
gtk_tool_button_set_property (GObject         *object,
			      guint            prop_id,
			      const GValue    *value,
			      GParamSpec      *pspec)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);
  
  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_tool_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_tool_button_set_use_underline (button, g_value_get_boolean (value));
      break;
    case PROP_LABEL_WIDGET:
      gtk_tool_button_set_label_widget (button, g_value_get_object (value));
      break;
    case PROP_ICON_NAME:
      gtk_tool_button_set_icon_name (button, g_value_get_string (value));
      break;
    case PROP_ICON_WIDGET:
      gtk_tool_button_set_icon_widget (button, g_value_get_object (value));
      break;
    case PROP_ACTION_NAME:
      g_object_set_property (G_OBJECT (button->priv->button), "action-name", value);
      break;
    case PROP_ACTION_TARGET:
      g_object_set_property (G_OBJECT (button->priv->button), "action-target", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tool_button_property_notify (GObject          *object,
				 GParamSpec       *pspec)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);

  if (button->priv->contents_invalid ||
      strcmp ("is-important", pspec->name) == 0)
    gtk_tool_button_construct_contents (GTK_TOOL_ITEM (object));

  if (parent_class->notify)
    parent_class->notify (object, pspec);
}

static void
gtk_tool_button_get_property (GObject         *object,
			      guint            prop_id,
			      GValue          *value,
			      GParamSpec      *pspec)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_tool_button_get_label (button));
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value, gtk_tool_button_get_label_widget (button));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, gtk_tool_button_get_use_underline (button));
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, button->priv->icon_name);
      break;
    case PROP_ICON_WIDGET:
      g_value_set_object (value, button->priv->icon_widget);
      break;
    case PROP_ACTION_NAME:
      g_object_get_property (G_OBJECT (button->priv->button), "action-name", value);
      break;
    case PROP_ACTION_TARGET:
      g_object_get_property (G_OBJECT (button->priv->button), "action-target", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static const gchar *
gtk_tool_button_get_action_name (GtkActionable *actionable)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (actionable);

  return gtk_actionable_get_action_name (GTK_ACTIONABLE (button->priv->button));
}

static void
gtk_tool_button_set_action_name (GtkActionable *actionable,
                                 const gchar   *action_name)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (actionable);

  gtk_actionable_set_action_name (GTK_ACTIONABLE (button->priv->button), action_name);
}

static GVariant *
gtk_tool_button_get_action_target_value (GtkActionable *actionable)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (actionable);

  return gtk_actionable_get_action_target_value (GTK_ACTIONABLE (button->priv->button));
}

static void
gtk_tool_button_set_action_target_value (GtkActionable *actionable,
                                         GVariant      *action_target)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (actionable);

  gtk_actionable_set_action_target_value (GTK_ACTIONABLE (button->priv->button), action_target);
}

static void
gtk_tool_button_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_tool_button_get_action_name;
  iface->set_action_name = gtk_tool_button_set_action_name;
  iface->get_action_target_value = gtk_tool_button_get_action_target_value;
  iface->set_action_target_value = gtk_tool_button_set_action_target_value;
}

static void
gtk_tool_button_finalize (GObject *object)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);

  g_free (button->priv->icon_name);
  g_free (button->priv->label_text);

  if (button->priv->label_widget)
    g_object_unref (button->priv->label_widget);

  if (button->priv->icon_widget)
    g_object_unref (button->priv->icon_widget);
  
  parent_class->finalize (object);
}

static GtkWidget *
clone_image_menu_size (GtkImage *image)
{
  GtkImageType storage_type = gtk_image_get_storage_type (image);

  if (storage_type == GTK_IMAGE_ICON_NAME)
    {
      const gchar *icon_name;
      gtk_image_get_icon_name (image, &icon_name, NULL);
      return gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
    }
  else if (storage_type == GTK_IMAGE_GICON)
    {
      GIcon *icon;
      gtk_image_get_gicon (image, &icon, NULL);
      return gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    }
  else if (storage_type == GTK_IMAGE_PIXBUF)
    {
      gint width, height;
      
      if (gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height))
	{
	  GdkPixbuf *src_pixbuf, *dest_pixbuf;
	  GtkWidget *cloned_image;

	  src_pixbuf = gtk_image_get_pixbuf (image);
	  dest_pixbuf = gdk_pixbuf_scale_simple (src_pixbuf, width, height,
						 GDK_INTERP_BILINEAR);

	  cloned_image = gtk_image_new_from_pixbuf (dest_pixbuf);
	  g_object_unref (dest_pixbuf);

	  return cloned_image;
	}
    }

  return NULL;
}

static gboolean
gtk_tool_button_create_menu_proxy (GtkToolItem *item)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (item);
  GtkWidget *menu_item;
  GtkWidget *menu_image = NULL;
  gboolean use_mnemonic = TRUE;
  const char *label_text;
  GtkWidget *box;
  GtkWidget *label;

  if (_gtk_tool_item_create_menu_proxy (item))
    return TRUE;

  if (GTK_IS_LABEL (button->priv->label_widget))
    {
      label_text = gtk_label_get_label (GTK_LABEL (button->priv->label_widget));
      use_mnemonic = gtk_label_get_use_underline (GTK_LABEL (button->priv->label_widget));
    }
  else if (button->priv->label_text)
    {
      label_text = button->priv->label_text;
      use_mnemonic = button->priv->use_underline;
    }
  else
    {
      label_text = "";
    }

  if (GTK_IS_IMAGE (button->priv->icon_widget))
    {
      menu_image = clone_image_menu_size (GTK_IMAGE (button->priv->icon_widget));
    }

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  if (use_mnemonic)
     label = gtk_label_new_with_mnemonic (label_text);
  else
     label = gtk_label_new (label_text);

  if (menu_image)
      gtk_container_add (GTK_CONTAINER (box), menu_image);

  gtk_container_add (GTK_CONTAINER (box), label);

  menu_item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu_item), box);

  g_signal_connect_closure_by_id (menu_item,
				  g_signal_lookup ("activate", G_OBJECT_TYPE (menu_item)), 0,
				  g_cclosure_new_object_swap (G_CALLBACK (gtk_button_clicked),
							      G_OBJECT (GTK_TOOL_BUTTON (button)->priv->button)),
				  FALSE);

  gtk_tool_item_set_proxy_menu_item (GTK_TOOL_ITEM (button), MENU_ID, menu_item);
  
  return TRUE;
}

static void
button_clicked (GtkWidget     *widget,
		GtkToolButton *button)
{
  g_signal_emit_by_name (button, "clicked");
}

static void
gtk_tool_button_toolbar_reconfigured (GtkToolItem *tool_item)
{
  gtk_tool_button_construct_contents (tool_item);
}

/**
 * gtk_tool_button_new:
 * @label: (allow-none): a string that will be used as label, or %NULL
 * @icon_widget: (allow-none): a widget that will be used as the button contents, or %NULL
 *
 * Creates a new #GtkToolButton using @icon_widget as contents and @label as
 * label.
 *
 * Returns: A new #GtkToolButton
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_tool_button_new (GtkWidget	 *icon_widget,
		     const gchar *label)
{
  GtkToolButton *button;

  g_return_val_if_fail (icon_widget == NULL || GTK_IS_WIDGET (icon_widget), NULL);

  button = g_object_new (GTK_TYPE_TOOL_BUTTON,
                         "label", label,
                         "icon-widget", icon_widget,
			 NULL);

  return GTK_TOOL_ITEM (button);  
}

/**
 * gtk_tool_button_set_label:
 * @button: a #GtkToolButton
 * @label: (allow-none): a string that will be used as label, or %NULL.
 *
 * Sets @label as the label used for the tool button. The #GtkToolButton:label
 * property only has an effect if not overridden by a non-%NULL 
 * #GtkToolButton:label-widget property. If both the #GtkToolButton:label-widget
 * and #GtkToolButton:label properties are %NULL, @button will not have a label.
 * 
 * Since: 2.4
 **/
void
gtk_tool_button_set_label (GtkToolButton *button,
			   const gchar   *label)
{
  gchar *old_label;
  gchar *elided_label;
  AtkObject *accessible;
  
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));

  old_label = button->priv->label_text;

  button->priv->label_text = g_strdup (label);
  button->priv->contents_invalid = TRUE;     

  if (label)
    {
      elided_label = _gtk_toolbar_elide_underscores (label);
      accessible = gtk_widget_get_accessible (GTK_WIDGET (button->priv->button));
      atk_object_set_name (accessible, elided_label);
      g_free (elided_label);
    }

  g_free (old_label);
 
  g_object_notify (G_OBJECT (button), "label");
}

/**
 * gtk_tool_button_get_label:
 * @button: a #GtkToolButton
 *
 * Returns the label used by the tool button, or %NULL if the tool button
 * doesn’t have a label. The returned
 * string is owned by GTK+, and must not be modified or freed.
 *
 * Returns: (nullable) (transfer none): The label, or %NULL
 *
 * Since: 2.4
 **/
const gchar *
gtk_tool_button_get_label (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->label_text;
}

/**
 * gtk_tool_button_set_use_underline:
 * @button: a #GtkToolButton
 * @use_underline: whether the button label has the form “_Open”
 *
 * If set, an underline in the label property indicates that the next character
 * should be used for the mnemonic accelerator key in the overflow menu. For
 * example, if the label property is “_Open” and @use_underline is %TRUE,
 * the label on the tool button will be “Open” and the item on the overflow
 * menu will have an underlined “O”.
 * 
 * Labels shown on tool buttons never have mnemonics on them; this property
 * only affects the menu item on the overflow menu.
 * 
 * Since: 2.4
 **/
void
gtk_tool_button_set_use_underline (GtkToolButton *button,
				   gboolean       use_underline)
{
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));

  use_underline = use_underline != FALSE;

  if (use_underline != button->priv->use_underline)
    {
      button->priv->use_underline = use_underline;
      button->priv->contents_invalid = TRUE;

      g_object_notify (G_OBJECT (button), "use-underline");
    }
}

/**
 * gtk_tool_button_get_use_underline:
 * @button: a #GtkToolButton
 * 
 * Returns whether underscores in the label property are used as mnemonics
 * on menu items on the overflow menu. See gtk_tool_button_set_use_underline().
 * 
 * Returns: %TRUE if underscores in the label property are used as
 * mnemonics on menu items on the overflow menu.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_button_get_use_underline (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), FALSE);

  return button->priv->use_underline;
}

/**
 * gtk_tool_button_set_icon_name:
 * @button: a #GtkToolButton
 * @icon_name: (allow-none): the name of the themed icon
 *
 * Sets the icon for the tool button from a named themed icon.
 * See the docs for #GtkIconTheme for more details.
 * The #GtkToolButton:icon-name property only has an effect if not
 * overridden by non-%NULL #GtkToolButton:label-widget or
 * #GtkToolButton:icon-widget properties.
 * 
 * Since: 2.8
 **/
void
gtk_tool_button_set_icon_name (GtkToolButton *button,
			       const gchar   *icon_name)
{
  gchar *old_icon_name;

  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));

  old_icon_name = button->priv->icon_name;

  button->priv->icon_name = g_strdup (icon_name);
  button->priv->contents_invalid = TRUE; 

  g_free (old_icon_name);

  g_object_notify (G_OBJECT (button), "icon-name");
}

/**
 * gtk_tool_button_get_icon_name:
 * @button: a #GtkToolButton
 *
 * Returns the name of the themed icon for the tool button,
 * see gtk_tool_button_set_icon_name().
 *
 * Returns: (nullable): the icon name or %NULL if the tool button has
 * no themed icon
 *
 * Since: 2.8
 **/
const gchar*
gtk_tool_button_get_icon_name (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->icon_name;
}

/**
 * gtk_tool_button_set_icon_widget:
 * @button: a #GtkToolButton
 * @icon_widget: (allow-none): the widget used as icon, or %NULL
 *
 * Sets @icon as the widget used as icon on @button.
 *
 * Since: 2.4
 **/
void
gtk_tool_button_set_icon_widget (GtkToolButton *button,
				 GtkWidget     *icon_widget)
{
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));
  g_return_if_fail (icon_widget == NULL || GTK_IS_WIDGET (icon_widget));

  if (icon_widget != button->priv->icon_widget)
    {
      if (button->priv->icon_widget)
	{
          GtkWidget *parent;

          parent = gtk_widget_get_parent (button->priv->icon_widget);
	  if (parent)
            gtk_container_remove (GTK_CONTAINER (parent),
                                  button->priv->icon_widget);

	  g_object_unref (button->priv->icon_widget);
	}
      
      if (icon_widget)
	g_object_ref_sink (icon_widget);

      button->priv->icon_widget = icon_widget;
      button->priv->contents_invalid = TRUE;
      
      g_object_notify (G_OBJECT (button), "icon-widget");
    }
}

/**
 * gtk_tool_button_set_label_widget:
 * @button: a #GtkToolButton
 * @label_widget: (allow-none): the widget used as label, or %NULL
 *
 * Sets @label_widget as the widget that will be used as the label
 * for @button. If @label_widget is %NULL the #GtkToolButton:label property is used
 * as label. If #GtkToolButton:label is also %NULL, @button does not have a label.
 *
 * Since: 2.4
 **/
void
gtk_tool_button_set_label_widget (GtkToolButton *button,
				  GtkWidget     *label_widget)
{
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));

  if (label_widget != button->priv->label_widget)
    {
      if (button->priv->label_widget)
	{
          GtkWidget *parent;

          parent = gtk_widget_get_parent (button->priv->label_widget);
          if (parent)
            gtk_container_remove (GTK_CONTAINER (parent),
                                  button->priv->label_widget);

	  g_object_unref (button->priv->label_widget);
	}
      
      if (label_widget)
	g_object_ref_sink (label_widget);

      button->priv->label_widget = label_widget;
      button->priv->contents_invalid = TRUE;
      
      g_object_notify (G_OBJECT (button), "label-widget");
    }
}

/**
 * gtk_tool_button_get_label_widget:
 * @button: a #GtkToolButton
 *
 * Returns the widget used as label on @button.
 * See gtk_tool_button_set_label_widget().
 *
 * Returns: (nullable) (transfer none): The widget used as label
 *     on @button, or %NULL.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_button_get_label_widget (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->label_widget;
}

/**
 * gtk_tool_button_get_icon_widget:
 * @button: a #GtkToolButton
 *
 * Return the widget used as icon widget on @button.
 * See gtk_tool_button_set_icon_widget().
 *
 * Returns: (nullable) (transfer none): The widget used as icon
 *     on @button, or %NULL.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_button_get_icon_widget (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->icon_widget;
}

GtkWidget *
_gtk_tool_button_get_button (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->button;
}
