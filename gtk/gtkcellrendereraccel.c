/* gtkcellrendereraccel.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcellrendereraccel.h"

#include "gtkintl.h"
#include "gtkaccelgroup.h"
#include "gtkmarshalers.h"
#include "gtklabel.h"
#include "gtkeventbox.h"
#include "gtkmain.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkcellrendereraccel
 * @Short_description: Renders a keyboard accelerator in a cell
 * @Title: GtkCellRendererAccel
 *
 * #GtkCellRendererAccel displays a keyboard accelerator (i.e. a
 * key combination like <keycombo><keycap>Control</keycap><keycap>a</keycap></keycombo>.
 * If the cell renderer is editable, the accelerator can be changed by
 * simply typing the new combination.
 *
 * The #GtkCellRendererAccel cell renderer was added in GTK+ 2.10.
 */



static void gtk_cell_renderer_accel_get_property (GObject         *object,
                                                  guint            param_id,
                                                  GValue          *value,
                                                  GParamSpec      *pspec);
static void gtk_cell_renderer_accel_set_property (GObject         *object,
                                                  guint            param_id,
                                                  const GValue    *value,
                                                  GParamSpec      *pspec);
static void gtk_cell_renderer_accel_get_preferred_width 
                                                 (GtkCellRenderer *cell,
                                                  GtkWidget       *widget,
                                                  gint            *minimum_size,
                                                  gint            *natural_size);
static GtkCellEditable *
           gtk_cell_renderer_accel_start_editing (GtkCellRenderer      *cell,
                                                  GdkEvent             *event,
                                                  GtkWidget            *widget,
                                                  const gchar          *path,
                                                  const GdkRectangle   *background_area,
                                                  const GdkRectangle   *cell_area,
                                                  GtkCellRendererState  flags);
static gchar *convert_keysym_state_to_string     (GtkCellRendererAccel *accel,
                                                  guint                 keysym,
                                                  GdkModifierType       mask,
                                                  guint                 keycode);

enum {
  ACCEL_EDITED,
  ACCEL_CLEARED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACCEL_KEY,
  PROP_ACCEL_MODS,
  PROP_KEYCODE,
  PROP_ACCEL_MODE
};

struct _GtkCellRendererAccelPrivate
{
  GtkWidget *edit_widget;
  GtkWidget *grab_widget;
  GtkWidget *sizing_label;

  GdkDevice *grab_keyboard;
  GdkDevice *grab_pointer;

  GtkCellRendererAccelMode accel_mode;

  GdkModifierType accel_mods;

  guint accel_key;
  guint keycode;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererAccel, gtk_cell_renderer_accel, GTK_TYPE_CELL_RENDERER_TEXT)

static void
gtk_cell_renderer_accel_init (GtkCellRendererAccel *cell_accel)
{
  gchar *text;

  cell_accel->priv = gtk_cell_renderer_accel_get_instance_private (cell_accel);

  text = convert_keysym_state_to_string (cell_accel, 0, 0, 0);
  g_object_set (cell_accel, "text", text, NULL);
  g_free (text);
}

static void
gtk_cell_renderer_accel_class_init (GtkCellRendererAccelClass *cell_accel_class)
{
  GObjectClass *object_class;
  GtkCellRendererClass *cell_renderer_class;

  object_class = G_OBJECT_CLASS (cell_accel_class);
  cell_renderer_class = GTK_CELL_RENDERER_CLASS (cell_accel_class);

  object_class->set_property = gtk_cell_renderer_accel_set_property;
  object_class->get_property = gtk_cell_renderer_accel_get_property;

  cell_renderer_class->get_preferred_width = gtk_cell_renderer_accel_get_preferred_width;
  cell_renderer_class->start_editing = gtk_cell_renderer_accel_start_editing;

  /**
   * GtkCellRendererAccel:accel-key:
   *
   * The keyval of the accelerator.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_KEY,
                                   g_param_spec_uint ("accel-key",
                                                     P_("Accelerator key"),
                                                     P_("The keyval of the accelerator"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE));
  
  /**
   * GtkCellRendererAccel:accel-mods:
   *
   * The modifier mask of the accelerator.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MODS,
                                   g_param_spec_flags ("accel-mods",
                                                       P_("Accelerator modifiers"),
                                                       P_("The modifier mask of the accelerator"),
                                                       GDK_TYPE_MODIFIER_TYPE,
                                                       0,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererAccel:keycode:
   *
   * The hardware keycode of the accelerator. Note that the hardware keycode is
   * only relevant if the key does not have a keyval. Normally, the keyboard
   * configuration should assign keyvals to all keys.
   *
   * Since: 2.10
   */ 
  g_object_class_install_property (object_class,
                                   PROP_KEYCODE,
                                   g_param_spec_uint ("keycode",
                                                      P_("Accelerator keycode"),
                                                      P_("The hardware keycode of the accelerator"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererAccel:accel-mode:
   *
   * Determines if the edited accelerators are GTK+ accelerators. If
   * they are, consumed modifiers are suppressed, only accelerators
   * accepted by GTK+ are allowed, and the accelerators are rendered
   * in the same way as they are in menus.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MODE,
                                   g_param_spec_enum ("accel-mode",
                                                      P_("Accelerator Mode"),
                                                      P_("The type of accelerators"),
                                                      GTK_TYPE_CELL_RENDERER_ACCEL_MODE,
                                                      GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                                                      GTK_PARAM_READWRITE));
  
  /**
   * GtkCellRendererAccel::accel-edited:
   * @accel: the object reveiving the signal
   * @path_string: the path identifying the row of the edited cell
   * @accel_key: the new accelerator keyval
   * @accel_mods: the new acclerator modifier mask
   * @hardware_keycode: the keycode of the new accelerator
   *
   * Gets emitted when the user has selected a new accelerator.
   *
   * Since: 2.10
   */
  signals[ACCEL_EDITED] = g_signal_new (I_("accel-edited"),
                                        GTK_TYPE_CELL_RENDERER_ACCEL,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GtkCellRendererAccelClass, accel_edited),
                                        NULL, NULL,
                                        _gtk_marshal_VOID__STRING_UINT_FLAGS_UINT,
                                        G_TYPE_NONE, 4,
                                        G_TYPE_STRING,
                                        G_TYPE_UINT,
                                        GDK_TYPE_MODIFIER_TYPE,
                                        G_TYPE_UINT);

  /**
   * GtkCellRendererAccel::accel-cleared:
   * @accel: the object reveiving the signal
   * @path_string: the path identifying the row of the edited cell
   *
   * Gets emitted when the user has removed the accelerator.
   *
   * Since: 2.10
   */
  signals[ACCEL_CLEARED] = g_signal_new (I_("accel-cleared"),
                                         GTK_TYPE_CELL_RENDERER_ACCEL,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GtkCellRendererAccelClass, accel_cleared),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__STRING,
                                         G_TYPE_NONE, 1,
                                         G_TYPE_STRING);
}


/**
 * gtk_cell_renderer_accel_new:
 *
 * Creates a new #GtkCellRendererAccel.
 * 
 * Returns: the new cell renderer
 *
 * Since: 2.10
 */
GtkCellRenderer *
gtk_cell_renderer_accel_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_ACCEL, NULL);
}

static gchar *
convert_keysym_state_to_string (GtkCellRendererAccel *accel,
                                guint                 keysym,
                                GdkModifierType       mask,
                                guint                 keycode)
{
  GtkCellRendererAccelPrivate *priv = accel->priv;

  if (keysym == 0 && keycode == 0)
    /* This label is displayed in a treeview cell displaying
     * a disabled accelerator key combination.
     */
    return g_strdup (C_("Accelerator", "Disabled"));
  else 
    {
      if (priv->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        {
          if (!gtk_accelerator_valid (keysym, mask))
            /* This label is displayed in a treeview cell displaying
             * an accelerator key combination that is not valid according
             * to gtk_accelerator_valid().
             */
            return g_strdup (C_("Accelerator", "Invalid"));

          return gtk_accelerator_get_label (keysym, mask);
        }
      else 
        {
          gchar *name;

          name = gtk_accelerator_get_label_with_keycode (NULL, keysym, keycode, mask);
          if (name == NULL)
            name = gtk_accelerator_name_with_keycode (NULL, keysym, keycode, mask);

          return name;
        }
    }
}

static void
gtk_cell_renderer_accel_get_property  (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkCellRendererAccelPrivate *priv = GTK_CELL_RENDERER_ACCEL (object)->priv;

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      g_value_set_uint (value, priv->accel_key);
      break;

    case PROP_ACCEL_MODS:
      g_value_set_flags (value, priv->accel_mods);
      break;

    case PROP_KEYCODE:
      g_value_set_uint (value, priv->keycode);
      break;

    case PROP_ACCEL_MODE:
      g_value_set_enum (value, priv->accel_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_accel_set_property  (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkCellRendererAccel *accel = GTK_CELL_RENDERER_ACCEL (object);
  GtkCellRendererAccelPrivate *priv = accel->priv;
  gboolean changed = FALSE;

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      {
        guint accel_key = g_value_get_uint (value);

        if (priv->accel_key != accel_key)
          {
            priv->accel_key = accel_key;
            changed = TRUE;
          }
      }
      break;

    case PROP_ACCEL_MODS:
      {
        guint accel_mods = g_value_get_flags (value);

        if (priv->accel_mods != accel_mods)
          {
            priv->accel_mods = accel_mods;
            changed = TRUE;
          }
      }
      break;
    case PROP_KEYCODE:
      {
        guint keycode = g_value_get_uint (value);

        if (priv->keycode != keycode)
          {
            priv->keycode = keycode;
            changed = TRUE;
          }
      }
      break;

    case PROP_ACCEL_MODE:
      priv->accel_mode = g_value_get_enum (value);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }

  if (changed)
    {
      gchar *text;

      text = convert_keysym_state_to_string (accel, priv->accel_key, priv->accel_mods, priv->keycode);
      g_object_set (accel, "text", text, NULL);
      g_free (text);
    }
}

static void
gtk_cell_renderer_accel_get_preferred_width (GtkCellRenderer    *cell,
                                             GtkWidget          *widget,
                                             gint               *minimum_size,
                                             gint               *natural_size)

{
  GtkCellRendererAccelPrivate *priv = GTK_CELL_RENDERER_ACCEL (cell)->priv;
  GtkRequisition min_req, nat_req;

  if (priv->sizing_label == NULL)
    priv->sizing_label = gtk_label_new (_("New accelerator…"));

  gtk_widget_get_preferred_size (priv->sizing_label, &min_req, &nat_req);

  GTK_CELL_RENDERER_CLASS (gtk_cell_renderer_accel_parent_class)->get_preferred_width (cell, widget,
                                                                                       minimum_size, natural_size);

  /* FIXME: need to take the cell_area et al. into account */
  if (minimum_size)
    *minimum_size = MAX (*minimum_size, min_req.width);
  if (natural_size)
    *natural_size = MAX (*natural_size, nat_req.width);
}

static gboolean
grab_key_callback (GtkWidget            *widget,
                   GdkEventKey          *event,
                   GtkCellRendererAccel *accel)
{
  GtkCellRendererAccelPrivate *priv = accel->priv;
  GdkModifierType accel_mods = 0;
  guint accel_key;
  guint keyval;
  gchar *path;
  gboolean edited;
  gboolean cleared;
  GdkModifierType consumed_modifiers;
  GdkDisplay *display;

  display = gtk_widget_get_display (widget);

  if (event->is_modifier)
    return TRUE;

  edited = FALSE;
  cleared = FALSE;

  accel_mods = event->state;

  if (event->keyval == GDK_KEY_Sys_Req && 
      (accel_mods & GDK_MOD1_MASK) != 0)
    {
      /* HACK: we don't want to use SysRq as a keybinding (but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq
       */
      keyval = GDK_KEY_Print;
      consumed_modifiers = 0;
    }
  else
    {
      _gtk_translate_keyboard_accel_state (gdk_keymap_get_for_display (display),
                                           event->hardware_keycode,
                                           event->state,
                                           gtk_accelerator_get_default_mod_mask (),
                                           event->group,
                                           &keyval, NULL, NULL, &consumed_modifiers);
    }

  accel_key = gdk_keyval_to_lower (keyval);
  if (accel_key == GDK_KEY_ISO_Left_Tab) 
    accel_key = GDK_KEY_Tab;

  accel_mods &= gtk_accelerator_get_default_mod_mask ();

  /* Filter consumed modifiers 
   */
  if (priv->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
    accel_mods &= ~consumed_modifiers;
  
  /* Put shift back if it changed the case of the key, not otherwise.
   */
  if (accel_key != keyval)
    accel_mods |= GDK_SHIFT_MASK;
    
  if (accel_mods == 0)
    {
      switch (keyval)
	{
	case GDK_KEY_Escape:
	  goto out; /* cancel */
	case GDK_KEY_BackSpace:
	  /* clear the accelerator on Backspace */
	  cleared = TRUE;
	  goto out;
	default:
	  break;
	}
    }

  if (priv->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
    {
      if (!gtk_accelerator_valid (accel_key, accel_mods))
        {
          gtk_widget_error_bell (widget);

          return TRUE;
        }
    }

  edited = TRUE;

 out:
  gtk_device_grab_remove (priv->grab_widget, priv->grab_pointer);
  gdk_device_ungrab (priv->grab_keyboard, event->time);
  gdk_device_ungrab (priv->grab_pointer, event->time);

  path = g_strdup (g_object_get_data (G_OBJECT (priv->edit_widget), "gtk-cell-renderer-text"));

  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (priv->edit_widget));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (priv->edit_widget));
  priv->edit_widget = NULL;
  priv->grab_widget = NULL;
  priv->grab_keyboard = NULL;
  priv->grab_pointer = NULL;

  if (edited)
    g_signal_emit (accel, signals[ACCEL_EDITED], 0, path, 
                   accel_key, accel_mods, event->hardware_keycode);
  else if (cleared)
    g_signal_emit (accel, signals[ACCEL_CLEARED], 0, path);

  g_free (path);

  return TRUE;
}

static void
ungrab_stuff (GtkWidget            *widget,
              GtkCellRendererAccel *accel)
{
  GtkCellRendererAccelPrivate *priv = accel->priv;

  gtk_device_grab_remove (priv->grab_widget, priv->grab_pointer);
  gdk_device_ungrab (priv->grab_keyboard, GDK_CURRENT_TIME);
  gdk_device_ungrab (priv->grab_pointer, GDK_CURRENT_TIME);

  priv->grab_keyboard = NULL;
  priv->grab_pointer = NULL;

  g_signal_handlers_disconnect_by_func (priv->grab_widget,
                                        G_CALLBACK (grab_key_callback),
                                        accel);
}

static void
_gtk_cell_editable_event_box_start_editing (GtkCellEditable *cell_editable,
                                            GdkEvent        *event)
{
  /* do nothing, because we are pointless */
}

static void
_gtk_cell_editable_event_box_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = _gtk_cell_editable_event_box_start_editing;
}

typedef struct _GtkCellEditableEventBox GtkCellEditableEventBox;
typedef         GtkEventBoxClass        GtkCellEditableEventBoxClass;

struct _GtkCellEditableEventBox
{
  GtkEventBox box;
  gboolean editing_canceled;
};

GType _gtk_cell_editable_event_box_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GtkCellEditableEventBox, _gtk_cell_editable_event_box, GTK_TYPE_EVENT_BOX, { \
    G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE, _gtk_cell_editable_event_box_cell_editable_init)   \
      })

enum {
  PROP_ZERO,
  PROP_EDITING_CANCELED
};

static void
gtk_cell_editable_event_box_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GtkCellEditableEventBox *box = (GtkCellEditableEventBox*)object;

  switch (prop_id)
    {
    case PROP_EDITING_CANCELED:
      box->editing_canceled = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_editable_event_box_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkCellEditableEventBox *box = (GtkCellEditableEventBox*)object;

  switch (prop_id)
    {
    case PROP_EDITING_CANCELED:
      g_value_set_boolean (value, box->editing_canceled);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_cell_editable_event_box_class_init (GtkCellEditableEventBoxClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_cell_editable_event_box_set_property;
  gobject_class->get_property = gtk_cell_editable_event_box_get_property;

  g_object_class_override_property (gobject_class,
                                    PROP_EDITING_CANCELED,
                                    "editing-canceled");
}

static void
_gtk_cell_editable_event_box_init (GtkCellEditableEventBox *box)
{
}

static GtkCellEditable *
gtk_cell_renderer_accel_start_editing (GtkCellRenderer      *cell,
                                       GdkEvent             *event,
                                       GtkWidget            *widget,
                                       const gchar          *path,
                                       const GdkRectangle   *background_area,
                                       const GdkRectangle   *cell_area,
                                       GtkCellRendererState  flags)
{
  GtkCellRendererAccelPrivate *priv;
  GtkCellRendererText *celltext;
  GtkCellRendererAccel *accel;
  GtkStyleContext *context;
  GdkRGBA color;
  GtkWidget *label;
  GtkWidget *eventbox;
  GdkDevice *device, *keyb, *pointer;
  GdkWindow *window;
  gboolean editable;
  guint32 time;

  celltext = GTK_CELL_RENDERER_TEXT (cell);
  accel = GTK_CELL_RENDERER_ACCEL (cell);
  priv = accel->priv;

  /* If the cell isn't editable we return NULL. */
  g_object_get (celltext, "editable", &editable, NULL);
  if (editable == FALSE)
    return NULL;

  window = gtk_widget_get_window (widget);
  context = gtk_widget_get_style_context (widget);

  g_return_val_if_fail (window != NULL, NULL);

  if (event)
    device = gdk_event_get_device (event);
  else
    device = gtk_get_current_event_device ();

  if (!device)
    return NULL;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      keyb = device;
      pointer = gdk_device_get_associated_device (device);
    }
  else
    {
      pointer = device;
      keyb = gdk_device_get_associated_device (device);
    }

  time = gdk_event_get_time (event);

  if (gdk_device_grab (keyb, window,
                       GDK_OWNERSHIP_WINDOW, FALSE,
                       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                       NULL, time) != GDK_GRAB_SUCCESS)
    return NULL;

  if (gdk_device_grab (pointer, window,
                       GDK_OWNERSHIP_WINDOW, FALSE,
                       GDK_BUTTON_PRESS_MASK,
                       NULL, time) != GDK_GRAB_SUCCESS)
    {
      gdk_device_ungrab (keyb, time);
      return NULL;
    }

  priv->grab_keyboard = keyb;
  priv->grab_pointer = pointer;
  priv->grab_widget = widget;

  g_signal_connect (G_OBJECT (widget), "key-press-event",
                    G_CALLBACK (grab_key_callback),
                    accel);

  eventbox = g_object_new (_gtk_cell_editable_event_box_get_type (), NULL);
  priv->edit_widget = eventbox;
  g_object_add_weak_pointer (G_OBJECT (priv->edit_widget),
                             (gpointer) &priv->edit_widget);
  
  label = gtk_label_new (NULL);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  gtk_style_context_get_background_color (context, GTK_STATE_FLAG_SELECTED, &color);
  gtk_widget_override_background_color (eventbox, 0, &color);

  gtk_style_context_get_color (context, GTK_STATE_FLAG_SELECTED, &color);
  gtk_widget_override_color (label, 0, &color);

  /* This label is displayed in a treeview cell displaying
   * an accelerator when the cell is clicked to change the 
   * acelerator.
   */
  gtk_label_set_text (GTK_LABEL (label), _("New accelerator…"));

  gtk_container_add (GTK_CONTAINER (eventbox), label);
  
  g_object_set_data_full (G_OBJECT (priv->edit_widget), "gtk-cell-renderer-text",
                          g_strdup (path), g_free);
  
  gtk_widget_show_all (priv->edit_widget);

  gtk_device_grab_add (priv->grab_widget, pointer, TRUE);

  g_signal_connect (priv->edit_widget, "unrealize",
                    G_CALLBACK (ungrab_stuff), accel);
  
  return GTK_CELL_EDITABLE (priv->edit_widget);
}
