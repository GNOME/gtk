/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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

#include "gtktaggedentry.h"

#include "gtkaccessible.h"
#include "gtktextprivate.h"
#include "gtkeditable.h"
#include "gtklabel.h"
#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkimage.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkstylecontext.h"
#include "gtkgesturemultipress.h"

#include "a11y/gtkentryaccessible.h"

/**
 * SECTION:gtktaggedhentry
 * @Short_description: An entry that can show tags
 * @Title: GtkTaggedEntry
 *
 * #GtkTaggedEntry is an entry that can show tags in
 * addition to text.
 */

typedef struct {
  GtkWidget *box;
  GtkWidget *entry;
} GtkTaggedEntryPrivate;

static void gtk_tagged_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTaggedEntry, gtk_tagged_entry, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkTaggedEntry)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, gtk_tagged_entry_editable_init))

static void
gtk_tagged_entry_init (GtkTaggedEntry *entry)
{
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  gtk_widget_set_has_surface (GTK_WIDGET (entry), FALSE);

  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent (priv->box, GTK_WIDGET (entry));

  priv->entry = gtk_text_new ();
  gtk_widget_set_hexpand (priv->entry, TRUE);
  gtk_widget_set_vexpand (priv->entry, TRUE);
  gtk_widget_set_hexpand (priv->box, FALSE);
  gtk_widget_set_vexpand (priv->box, FALSE);
  gtk_container_add (GTK_CONTAINER (priv->box), priv->entry);
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
}

static void
gtk_tagged_entry_dispose (GObject *object)
{
  GtkTaggedEntry *entry = GTK_TAGGED_ENTRY (object);
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  if (priv->entry)
    gtk_editable_finish_delegate (GTK_EDITABLE (entry));

  g_clear_pointer (&priv->entry, gtk_widget_unparent);
  g_clear_pointer (&priv->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_tagged_entry_parent_class)->dispose (object);
}

static void
gtk_tagged_entry_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_tagged_entry_parent_class)->finalize (object);
}

static void
gtk_tagged_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gtk_tagged_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gtk_tagged_entry_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkTaggedEntry *entry = GTK_TAGGED_ENTRY (widget);
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  gtk_widget_measure (priv->box, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_tagged_entry_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GtkTaggedEntry *entry = GTK_TAGGED_ENTRY (widget);
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  gtk_widget_size_allocate (priv->box,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}

static void
gtk_tagged_entry_grab_focus (GtkWidget *widget)
{
  GtkTaggedEntry *entry = GTK_TAGGED_ENTRY (widget);
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->entry);
}

static void
gtk_tagged_entry_class_init (GtkTaggedEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_tagged_entry_dispose;
  object_class->finalize = gtk_tagged_entry_finalize;
  object_class->get_property = gtk_tagged_entry_get_property;
  object_class->set_property = gtk_tagged_entry_set_property;

  widget_class->measure = gtk_tagged_entry_measure;
  widget_class->size_allocate = gtk_tagged_entry_size_allocate;
  widget_class->grab_focus = gtk_tagged_entry_grab_focus;
 
  gtk_editable_install_properties (object_class, 1);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_ENTRY_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("entry"));
}

static GtkEditable *
gtk_tagged_entry_get_delegate (GtkEditable *editable)
{
  GtkTaggedEntry *entry = GTK_TAGGED_ENTRY (editable);
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  return GTK_EDITABLE (priv->entry);
}

static void
gtk_tagged_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_tagged_entry_get_delegate;
}

/**
 * gtk_tagged_entry_new:
 *
 * Creates a #GtkTaggedEntry.
 *
 * Returns: a new #GtkTaggedEntry
 */
GtkWidget *
gtk_tagged_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_TAGGED_ENTRY, NULL));
}

void
gtk_tagged_entry_add_tag (GtkTaggedEntry *entry,
                          GtkWidget      *tag)
{
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TAGGED_ENTRY (entry));

  gtk_container_add (GTK_CONTAINER (priv->box), tag);
}

void
gtk_tagged_entry_insert_tag (GtkTaggedEntry *entry,
                             GtkWidget      *tag,
                             int             position)
{
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TAGGED_ENTRY (entry));

  if (position == -1)
    gtk_container_add (GTK_CONTAINER (priv->box), tag);
  else
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (priv->box));
      GtkWidget *sibling = g_list_nth_data (children, position);
      gtk_box_insert_child_after (GTK_BOX (priv->box), tag, sibling); 
      g_list_free (children);
    }
}

void
gtk_tagged_entry_remove_tag (GtkTaggedEntry *entry,
                             GtkWidget      *tag)
{
  GtkTaggedEntryPrivate *priv = gtk_tagged_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_TAGGED_ENTRY (entry));

  gtk_container_remove (GTK_CONTAINER (priv->box), tag);
}

#define GTK_TYPE_ENTRY_TAG                (gtk_entry_tag_get_type ())
#define GTK_ENTRY_TAG(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_TAG, GtkEntryTag))
#define GTK_ENTRY_TAG_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ENTRY_TAG, GtkEntryTag))
#define GTK_IS_ENTRY_TAG(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_TAG))
#define GTK_IS_ENTRY_TAG_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ENTRY_TAG))
#define GTK_ENTRY_TAG_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ENTRY_TAG, GtkEntryTagClass))

typedef struct _GtkEntryTag       GtkEntryTag;
typedef struct _GtkEntryTagClass  GtkEntryTagClass;

struct _GtkEntryTag
{
  GtkWidget parent;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *button;

  gboolean has_close_button;
  char *style;
};

struct _GtkEntryTagClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_HAS_CLOSE_BUTTON,
};

enum {
  SIGNAL_CLICKED,
  SIGNAL_BUTTON_CLICKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GtkEntryTag, gtk_entry_tag, GTK_TYPE_WIDGET)

static void
on_released (GtkGestureMultiPress *gesture,
             int                   n_press,
             double                x,
             double                y,
             GtkEntryTag          *tag)
{
  g_signal_emit (tag, signals[SIGNAL_CLICKED], 0);
}

static void
gtk_entry_tag_init (GtkEntryTag *tag)
{
  GtkGesture *gesture;

  gtk_widget_set_has_surface (GTK_WIDGET (tag), FALSE);

  tag->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent (tag->box, GTK_WIDGET (tag));
  tag->label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (tag->box), tag->label);

  gesture = gtk_gesture_multi_press_new ();
  g_signal_connect (gesture, "released", G_CALLBACK (on_released), tag);
  gtk_widget_add_controller (GTK_WIDGET (tag), GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_entry_tag_dispose (GObject *object)
{
  GtkEntryTag *tag = GTK_ENTRY_TAG (object);

  g_clear_pointer (&tag->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_entry_tag_parent_class)->dispose (object);
}

static void
gtk_entry_tag_finalize (GObject *object)
{
  GtkEntryTag *tag = GTK_ENTRY_TAG (object);

  g_clear_pointer (&tag->box, gtk_widget_unparent);
  g_clear_pointer (&tag->style, g_free);

  G_OBJECT_CLASS (gtk_entry_tag_parent_class)->finalize (object);
}

static void
gtk_entry_tag_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkEntryTag *tag = GTK_ENTRY_TAG (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_entry_tag_set_label (tag, g_value_get_string (value));
      break;

    case PROP_HAS_CLOSE_BUTTON:
      gtk_entry_tag_set_has_close_button (tag, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_entry_tag_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkEntryTag *tag = GTK_ENTRY_TAG (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_entry_tag_get_label (tag));
      break;

    case PROP_HAS_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_entry_tag_get_has_close_button (tag));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_entry_tag_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkEntryTag *tag = GTK_ENTRY_TAG (widget);

  gtk_widget_measure (tag->box, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_entry_tag_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkEntryTag *tag = GTK_ENTRY_TAG (widget);

  gtk_widget_size_allocate (tag->box,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}

static void
gtk_entry_tag_class_init (GtkEntryTagClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_entry_tag_dispose;
  object_class->finalize = gtk_entry_tag_finalize;
  object_class->set_property = gtk_entry_tag_set_property;
  object_class->get_property = gtk_entry_tag_get_property;

  widget_class->measure = gtk_entry_tag_measure;
  widget_class->size_allocate = gtk_entry_tag_size_allocate;

  signals[SIGNAL_CLICKED] =
    g_signal_new ("clicked",
                  GTK_TYPE_ENTRY_TAG,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  signals[SIGNAL_BUTTON_CLICKED] =
    g_signal_new ("button-clicked",
                  GTK_TYPE_ENTRY_TAG,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class, PROP_LABEL,
      g_param_spec_string ("label", "Label", "Label",
                           NULL, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_HAS_CLOSE_BUTTON,
      g_param_spec_boolean ("has-close-button", "Has close button", "Whether this tag has a close button",
                            FALSE, GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, I_("tag"));
}

GtkEntryTag *
gtk_entry_tag_new (const char *label)
{
  return GTK_ENTRY_TAG (g_object_new (GTK_TYPE_ENTRY_TAG, "label", label, NULL));
}

const char *
gtk_entry_tag_get_label (GtkEntryTag *tag)
{
  g_return_val_if_fail (GTK_IS_ENTRY_TAG (tag), NULL);

  return gtk_label_get_label (GTK_LABEL (tag->label));
}

void
gtk_entry_tag_set_label (GtkEntryTag *tag,
                         const char  *label)
{
  g_return_if_fail (GTK_IS_ENTRY_TAG (tag));

  gtk_label_set_label (GTK_LABEL (tag->label), label);
}

static void
on_button_clicked (GtkButton *button, GtkEntryTag *tag)
{
  g_signal_emit (tag, signals[SIGNAL_BUTTON_CLICKED], 0);
}

void
gtk_entry_tag_set_has_close_button (GtkEntryTag *tag,
                                    gboolean     has_close_button)
{
  g_return_if_fail (GTK_IS_ENTRY_TAG (tag));

  if ((tag->button != NULL) == has_close_button)
    return;

  if (!has_close_button && tag->button)
    {
      gtk_container_remove (GTK_CONTAINER (tag->box), tag->button);
      tag->button = NULL;
    }
  else if (has_close_button && tag->button == NULL)
    {
      GtkWidget *image;

      image = gtk_image_new_from_icon_name ("window-close-symbolic");
      gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
      tag->button = gtk_button_new ();
      gtk_container_add (GTK_CONTAINER (tag->button), image);
      gtk_button_set_relief (GTK_BUTTON (tag->button), GTK_RELIEF_NONE);
      gtk_container_add (GTK_CONTAINER (tag->box), tag->button);
      g_signal_connect (tag->button, "clicked", G_CALLBACK (on_button_clicked), tag);
    }

  g_object_notify (G_OBJECT (tag), "has-close-button");
}

gboolean
gtk_entry_tag_get_has_close_button (GtkEntryTag *tag)
{
  g_return_val_if_fail (GTK_IS_ENTRY_TAG (tag), FALSE);

  return tag->button != NULL;
}
