/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <math.h>
#include <string.h>
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gdk/gdkkeysyms.h"
#include "gtkclipboard.h"
#include <pango/pango.h>
#include "gtkimagemenuitem.h"
#include "gtkintl.h"
#include "gtkseparatormenuitem.h"
#include "gtkmenuitem.h"
#include "gtknotebook.h"
#include "gtkstock.h"
#include "gtkbindings.h"

struct _GtkLabelSelectionInfo
{
  GdkWindow *window;
  gint selection_anchor;
  gint selection_end;
  GdkGC *cursor_gc;
  GtkWidget *popup_menu;
};

enum {
  MOVE_CURSOR,
  COPY_CLIPBOARD,
  POPULATE_POPUP,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ATTRIBUTES,
  PROP_USE_MARKUP,
  PROP_USE_UNDERLINE,
  PROP_JUSTIFY,
  PROP_PATTERN,
  PROP_WRAP,
  PROP_SELECTABLE,
  PROP_MNEMONIC_KEYVAL,
  PROP_MNEMONIC_WIDGET,
  PROP_CURSOR_POSITION,
  PROP_SELECTION_BOUND
};

static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_label_class_init        (GtkLabelClass    *klass);
static void gtk_label_init              (GtkLabel         *label);
static void gtk_label_set_property      (GObject          *object,
					 guint             prop_id,
					 const GValue     *value,
					 GParamSpec       *pspec);
static void gtk_label_get_property      (GObject          *object,
					 guint             prop_id,
					 GValue           *value,
					 GParamSpec       *pspec);
static void gtk_label_destroy           (GtkObject        *object);
static void gtk_label_finalize          (GObject          *object);
static void gtk_label_size_request      (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_label_size_allocate     (GtkWidget        *widget,
                                         GtkAllocation    *allocation);
static void gtk_label_state_changed     (GtkWidget        *widget,
                                         GtkStateType      state);
static void gtk_label_style_set         (GtkWidget        *widget,
					 GtkStyle         *previous_style);
static void gtk_label_direction_changed (GtkWidget        *widget,
					 GtkTextDirection  previous_dir);
static gint gtk_label_expose            (GtkWidget        *widget,
					 GdkEventExpose   *event);

static void gtk_label_realize           (GtkWidget        *widget);
static void gtk_label_unrealize         (GtkWidget        *widget);
static void gtk_label_map               (GtkWidget        *widget);
static void gtk_label_unmap             (GtkWidget        *widget);
static gint gtk_label_button_press      (GtkWidget        *widget,
                                         GdkEventButton   *event);
static gint gtk_label_button_release    (GtkWidget        *widget,
                                         GdkEventButton   *event);
static gint gtk_label_motion            (GtkWidget        *widget,
                                         GdkEventMotion   *event);


static void gtk_label_set_text_internal          (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_label_internal         (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_use_markup_internal    (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_use_underline_internal (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_attributes_internal    (GtkLabel      *label,
						  PangoAttrList *attrs);
static void gtk_label_set_uline_text_internal    (GtkLabel      *label,
						  const gchar   *str);
static void gtk_label_set_pattern_internal       (GtkLabel      *label,
				                  const gchar   *pattern);
static void set_markup                           (GtkLabel      *label,
						  const gchar   *str,
						  gboolean       with_uline);
static void gtk_label_recalculate                (GtkLabel      *label);
static void gtk_label_hierarchy_changed          (GtkWidget     *widget,
						  GtkWidget     *old_toplevel);

static void gtk_label_create_window       (GtkLabel *label);
static void gtk_label_destroy_window      (GtkLabel *label);
static void gtk_label_clear_layout        (GtkLabel *label);
static void gtk_label_ensure_layout       (GtkLabel *label);
static void gtk_label_select_region_index (GtkLabel *label,
                                           gint      anchor_index,
                                           gint      end_index);

static gboolean gtk_label_mnemonic_activate (GtkWidget         *widget,
					     gboolean           group_cycling);
static void     gtk_label_setup_mnemonic    (GtkLabel          *label,
					     guint              last_key);
static gboolean gtk_label_focus             (GtkWidget         *widget,
					     GtkDirectionType   direction);

/* For selectable lables: */
static void gtk_label_move_cursor        (GtkLabel        *label,
					  GtkMovementStep  step,
					  gint             count,
					  gboolean         extend_selection);
static void gtk_label_copy_clipboard     (GtkLabel        *label);
static void gtk_label_select_all         (GtkLabel        *label);
static void gtk_label_do_popup           (GtkLabel        *label,
					  GdkEventButton  *event);

static gint gtk_label_move_forward_word  (GtkLabel        *label,
					  gint             start);
static gint gtk_label_move_backward_word (GtkLabel        *label,
					  gint             start);

static GtkMiscClass *parent_class = NULL;


GtkType
gtk_label_get_type (void)
{
  static GtkType label_type = 0;
  
  if (!label_type)
    {
      static const GTypeInfo label_info =
      {
	sizeof (GtkLabelClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_label_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkLabel),
	32,             /* n_preallocs */
	(GInstanceInitFunc) gtk_label_init,
      };

      label_type = g_type_register_static (GTK_TYPE_MISC, "GtkLabel", &label_info, 0);
    }
  
  return label_type;
}

static void
add_move_binding (GtkBindingSet  *binding_set,
		  guint           keyval,
		  guint           modmask,
		  GtkMovementStep step,
		  gint            count)
{
  g_return_if_fail ((modmask & GDK_SHIFT_MASK) == 0);
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
				"move_cursor", 3,
				GTK_TYPE_ENUM, step,
				G_TYPE_INT, count,
                                G_TYPE_BOOLEAN, FALSE);

  /* Selection-extending version */
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | GDK_SHIFT_MASK,
				"move_cursor", 3,
				GTK_TYPE_ENUM, step,
				G_TYPE_INT, count,
                                G_TYPE_BOOLEAN, TRUE);
}

static void
gtk_label_class_init (GtkLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet *binding_set;

  parent_class = gtk_type_class (GTK_TYPE_MISC);
  
  gobject_class->set_property = gtk_label_set_property;
  gobject_class->get_property = gtk_label_get_property;
  gobject_class->finalize = gtk_label_finalize;

  object_class->destroy = gtk_label_destroy;
  
  widget_class->size_request = gtk_label_size_request;
  widget_class->size_allocate = gtk_label_size_allocate;
  widget_class->state_changed = gtk_label_state_changed;
  widget_class->style_set = gtk_label_style_set;
  widget_class->direction_changed = gtk_label_direction_changed;
  widget_class->expose_event = gtk_label_expose;
  widget_class->realize = gtk_label_realize;
  widget_class->unrealize = gtk_label_unrealize;
  widget_class->map = gtk_label_map;
  widget_class->unmap = gtk_label_unmap;
  widget_class->button_press_event = gtk_label_button_press;
  widget_class->button_release_event = gtk_label_button_release;
  widget_class->motion_notify_event = gtk_label_motion;
  widget_class->hierarchy_changed = gtk_label_hierarchy_changed;
  widget_class->mnemonic_activate = gtk_label_mnemonic_activate;
  widget_class->focus = gtk_label_focus;

  class->move_cursor = gtk_label_move_cursor;
  class->copy_clipboard = gtk_label_copy_clipboard;
  
  signals[MOVE_CURSOR] = 
      gtk_signal_new ("move_cursor",
                      GTK_RUN_LAST | GTK_RUN_ACTION,
                      GTK_CLASS_TYPE (object_class),
                      GTK_SIGNAL_OFFSET (GtkLabelClass, move_cursor),
                      _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
                      GTK_TYPE_NONE, 3, GTK_TYPE_MOVEMENT_STEP, GTK_TYPE_INT, GTK_TYPE_BOOL);
  
  signals[COPY_CLIPBOARD] =
    gtk_signal_new ("copy_clipboard",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkLabelClass, copy_clipboard),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);
  
  signals[POPULATE_POPUP] =
    gtk_signal_new ("populate_popup",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkLabelClass, populate_popup),
		    _gtk_marshal_VOID__OBJECT,
		    GTK_TYPE_NONE, 1, GTK_TYPE_MENU);

  g_object_class_install_property (G_OBJECT_CLASS(object_class),
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        _("Label"),
                                                        _("The text of the label."),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_boxed ("attributes",
						       _("Attributes"),
						       _("A list of style attributes to apply to the text of the label."),
						       PANGO_TYPE_ATTR_LIST,
						       G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use_markup",
							 _("Use markup"),
							 _("The text of the label includes XML markup. See pango_parse_markup()."),
                                                        FALSE,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use_underline",
							 _("Use underline"),
							 _("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                                                        FALSE,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_JUSTIFY,
                                   g_param_spec_enum ("justify",
                                                      _("Justification"),
                                                      _("The alignment of the lines in the text of the label relative to each other. This does NOT affect the alignment of the label within its allocation. See GtkMisc::xalign for that."),
						      GTK_TYPE_JUSTIFICATION,
						      GTK_JUSTIFY_LEFT,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PATTERN,
                                   g_param_spec_string ("pattern",
                                                        _("Pattern"),
                                                        _("A string with _ characters in positions correspond to characters in the text to underline."),
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
                                                        _("Line wrap"),
                                                        _("If set, wrap lines if the text becomes too wide."),
                                                        FALSE,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTABLE,
                                   g_param_spec_boolean ("selectable",
                                                        _("Selectable"),
                                                        _("Whether the label text can be selected with the mouse."),
                                                        FALSE,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MNEMONIC_KEYVAL,
                                   g_param_spec_uint ("mnemonic_keyval",
						      _("Mnemonic key"),
						      _("The mnemonic accelerator key for this label."),
						      0,
						      G_MAXUINT,
						      GDK_VoidSymbol,
						      G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_MNEMONIC_WIDGET,
                                   g_param_spec_object ("mnemonic_widget",
							_("Mnemonic widget"),
							_("The widget to be activated when the label's mnemonic "
							  "key is pressed."),
							GTK_TYPE_WIDGET,
							G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_POSITION,
                                   g_param_spec_int ("cursor_position",
                                                     _("Cursor Position"),
                                                     _("The current position of the insertion cursor in chars."),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTION_BOUND,
                                   g_param_spec_int ("selection_bound",
                                                     _("Selection Bound"),
                                                     _("The position of the opposite end of the selection from the cursor in chars."),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  
  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (class);

  /* Moving the insertion point */
  add_move_binding (binding_set, GDK_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (binding_set, GDK_KP_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KP_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_f, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_b, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KP_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KP_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);
  
  add_move_binding (binding_set, GDK_a, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_PARAGRAPH_ENDS, -1);

  add_move_binding (binding_set, GDK_e, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_PARAGRAPH_ENDS, 1);

  add_move_binding (binding_set, GDK_f, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_b, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
  
  add_move_binding (binding_set, GDK_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  /* copy */
  gtk_binding_entry_add_signal (binding_set, GDK_c, GDK_CONTROL_MASK,
				"copy_clipboard", 0);
}

static void 
gtk_label_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkLabel *label;
  guint last_keyval;

  label = GTK_LABEL (object);
  last_keyval = label->mnemonic_keyval;
  
  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_label_set_label (label, g_value_get_string (value));
      break;
    case PROP_ATTRIBUTES:
      gtk_label_set_attributes (label, g_value_get_boxed (value));
      break;
    case PROP_USE_MARKUP:
      gtk_label_set_use_markup (label, g_value_get_boolean (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_label_set_use_underline (label, g_value_get_boolean (value));
      break;
    case PROP_JUSTIFY:
      gtk_label_set_justify (label, g_value_get_enum (value));
      break;
    case PROP_PATTERN:
      gtk_label_set_pattern (label, g_value_get_string (value));
      break;
    case PROP_WRAP:
      gtk_label_set_line_wrap (label, g_value_get_boolean (value));
      break;	  
    case PROP_SELECTABLE:
      gtk_label_set_selectable (label, g_value_get_boolean (value));
      break;	  
    case PROP_MNEMONIC_WIDGET:
      gtk_label_set_mnemonic_widget (label, (GtkWidget*) g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_label_get_property (GObject     *object,
			guint        prop_id,
			GValue      *value,
			GParamSpec  *pspec)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, label->label);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, label->attrs);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, label->use_markup);
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, label->use_underline);
      break;
    case PROP_JUSTIFY:
      g_value_set_enum (value, label->jtype);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, label->wrap);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_label_get_selectable (label));
      break;
    case PROP_MNEMONIC_KEYVAL:
      g_value_set_uint (value, label->mnemonic_keyval);
      break;
    case PROP_MNEMONIC_WIDGET:
      g_value_set_object (value, (GObject*) label->mnemonic_widget);
      break;
    case PROP_CURSOR_POSITION:
      if (label->select_info)
	{
	  gint offset = g_utf8_pointer_to_offset (label->label,
						  label->label + label->select_info->selection_end);
	  g_value_set_int (value, offset);
	}
      else
	g_value_set_int (value, 0);
      break;
    case PROP_SELECTION_BOUND:
      if (label->select_info)
	{
	  gint offset = g_utf8_pointer_to_offset (label->label,
						  label->label + label->select_info->selection_anchor);
	  g_value_set_int (value, offset);
	}
      else
	g_value_set_int (value, 0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_label_init (GtkLabel *label)
{
  GTK_WIDGET_SET_FLAGS (label, GTK_NO_WINDOW);
  
  label->label = NULL;

  label->jtype = GTK_JUSTIFY_LEFT;
  label->wrap = FALSE;

  label->use_underline = FALSE;
  label->use_markup = FALSE;
  
  label->mnemonic_keyval = GDK_VoidSymbol;
  label->layout = NULL;
  label->text = NULL;
  label->attrs = NULL;

  label->mnemonic_widget = NULL;
  label->mnemonic_window = NULL;
  
  gtk_label_set_text (label, "");
}

/**
 * gtk_label_new:
 * @str: The text of the label
 *
 * Creates a new label with the given text inside it. You can
 * pass %NULL to get an empty label widget.
 *
 * Return value: the new #GtkLabel
 **/
GtkWidget*
gtk_label_new (const gchar *str)
{
  GtkLabel *label;
  
  label = gtk_type_new (GTK_TYPE_LABEL);

  if (str && *str)
    gtk_label_set_text (label, str);
  
  return GTK_WIDGET (label);
}

/**
 * gtk_label_new_with_mnemonic:
 * @str: The text of the label, with an underscore in front of the
 *       mnemonic character
 *
 * Creates a new #GtkLabel, containing the text in @str.
 *
 * If characters in @str are preceded by an underscore, they are
 * underlined. If you need a literal underscore character in a label, use
 * '__' (two underscores). The first underlined character represents a 
 * keyboard accelerator called a mnemonic. The mnemonic key can be used 
 * to activate another widget, chosen automatically, or explicitly using
 * gtk_label_set_mnemonic_widget().
 * 
 * If gtk_label_set_mnemonic_widget()
 * is not called, then the first activatable ancestor of the #GtkLabel
 * will be chosen as the mnemonic widget. For instance, if the
 * label is inside a button or menu item, the button or menu item will
 * automatically become the mnemonic widget and be activated by
 * the mnemonic.
 *
 * Return value: the new #GtkLabel
 **/
GtkWidget*
gtk_label_new_with_mnemonic (const gchar *str)
{
  GtkLabel *label;
  
  label = gtk_type_new (GTK_TYPE_LABEL);

  if (str && *str)
    gtk_label_set_text_with_mnemonic (label, str);
  
  return GTK_WIDGET (label);
}

static gboolean
gtk_label_mnemonic_activate (GtkWidget *widget,
			     gboolean   group_cycling)
{
  GtkWidget *parent;

  if (GTK_LABEL (widget)->mnemonic_widget)
    return gtk_widget_mnemonic_activate (GTK_LABEL (widget)->mnemonic_widget, group_cycling);

  /* Try to find the widget to activate by traversing the
   * widget's ancestry.
   */
  parent = widget->parent;

  if (parent && GTK_IS_NOTEBOOK (parent))
    return FALSE;
  
  while (parent)
    {
      if (GTK_WIDGET_CAN_FOCUS (parent) ||
	  (!group_cycling && GTK_WIDGET_GET_CLASS (parent)->activate_signal) ||
          (parent->parent && GTK_IS_NOTEBOOK (parent->parent)) ||
	  (GTK_IS_MENU_ITEM (parent)))
	return gtk_widget_mnemonic_activate (parent, group_cycling);
      parent = parent->parent;
    }

  /* barf if there was nothing to activate */
  g_warning ("Couldn't find a target for a mnemonic activation.");
  gdk_beep ();
  
  return FALSE;
}

static void
gtk_label_setup_mnemonic (GtkLabel *label,
			  guint     last_key)
{
  GtkWidget *toplevel;

  if (last_key != GDK_VoidSymbol && label->mnemonic_window)
    {
      gtk_window_remove_mnemonic  (label->mnemonic_window,
				   last_key,
				   GTK_WIDGET (label));
      label->mnemonic_window = NULL;
    }
  
  if (label->mnemonic_keyval == GDK_VoidSymbol)
    return;
  
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (label));
  if (GTK_WIDGET_TOPLEVEL (toplevel))
    {
      gtk_window_add_mnemonic (GTK_WINDOW (toplevel),
			       label->mnemonic_keyval,
			       GTK_WIDGET (label));
      label->mnemonic_window = GTK_WINDOW (toplevel);
    }
}

static void
gtk_label_hierarchy_changed (GtkWidget *widget,
			     GtkWidget *old_toplevel)
{
  GtkLabel *label = GTK_LABEL (widget);
  
  gtk_label_setup_mnemonic (label, label->mnemonic_keyval);
}


/**
 * gtk_label_set_mnemonic_widget:
 * @label: a #GtkLabel
 * @widget: the target #GtkWidget 
 *
 * If the label has been set so that it has an mnemonic key (using
 * i.e.  gtk_label_set_markup_with_mnemonic(),
 * gtk_label_set_text_with_mnemonic(), gtk_label_new_with_mnemonic()
 * or the "use_underline" property) the label can be associated with a
 * widget that is the target of the mnemonic. When the label is inside
 * a widget (like a #GtkButton or a #GtkNotebook tab) it is
 * automatically associated with the correct widget, but sometimes
 * (i.e. when the target is a #GtkEntry next to the label) you need to
 * set it explicitly using this function.
 *
 * The target widget will be accelerated by emitting "mnemonic_activate" on it.
 * The default handler for this signal will activate the widget if there are no
 * mnemonic collisions and toggle focus between the colliding widgets otherwise.
 **/
void
gtk_label_set_mnemonic_widget (GtkLabel  *label,
			       GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  if (widget)
    g_return_if_fail (GTK_IS_WIDGET (widget));

  if (label->mnemonic_widget)
    gtk_widget_unref (label->mnemonic_widget);
  label->mnemonic_widget = widget;
  if (label->mnemonic_widget)
    gtk_widget_ref (label->mnemonic_widget);
  
  g_object_notify (G_OBJECT (label), "mnemonic_widget");
}

/**
 * gtk_label_get_mnemonic_widget:
 * @label: a #GtkLabel
 *
 * Retrieves the target of the mnemonic (keyboard shortcut) of this
 * label. See gtk_label_set_mnemonic_widget ().
 *
 * Return value: the target of the label's mnemonic, or %NULL if none
 *               has been set and the default algorithm will be used.
 **/
GtkWidget *
gtk_label_get_mnemonic_widget (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->mnemonic_widget;
}

/**
 * gtk_label_get_mnemonic_keyval:
 * @label: a #GtkLabel
 *
 * If the label has been set so that it has an mnemonic key this function
 * returns the keyval used for the mnemonic accelerator. If there is no
 * mnemonic set up it returns #GDK_VoidSymbol.
 *
 * Returns: GDK keyval usable for accelerators, or #GDK_VoidSymbol
 **/
guint
gtk_label_get_mnemonic_keyval (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);

  return label->mnemonic_keyval;
}

static void
gtk_label_set_text_internal (GtkLabel *label,
			     gchar    *str)
{
  g_free (label->text);
  
  label->text = str;

  gtk_label_select_region_index (label, 0, 0);
}

static void
gtk_label_set_label_internal (GtkLabel *label,
			      gchar    *str)
{
  g_free (label->label);
  
  label->label = str;

  g_object_notify (G_OBJECT (label), "label");
}

static void
gtk_label_set_use_markup_internal (GtkLabel *label,
				   gboolean  val)
{
  val = val != FALSE;
  if (label->use_markup != val)
    {
      g_object_notify (G_OBJECT (label), "use_markup");
      label->use_markup = val;
    }
}

static void
gtk_label_set_use_underline_internal (GtkLabel *label,
				      gboolean val)
{
  val = val != FALSE;
  if (label->use_underline != val)
    {
      g_object_notify (G_OBJECT (label), "use_underline");
      label->use_underline = val;
    }
}

static void
gtk_label_set_attributes_internal (GtkLabel      *label,
				   PangoAttrList *attrs)
{
  if (attrs)
    pango_attr_list_ref (attrs);
  
  if (label->attrs)
    pango_attr_list_unref (label->attrs);

  if (!label->use_markup && !label->use_underline)
    {
      pango_attr_list_ref (attrs);
      if (label->effective_attrs)
	pango_attr_list_unref (label->effective_attrs);
      label->effective_attrs = attrs;
    }

  label->attrs = attrs;
  g_object_notify (G_OBJECT (label), "attributes");
}


/* Calculates text, attrs and mnemonic_keyval from
 * label, use_underline and use_markup
 */
static void
gtk_label_recalculate (GtkLabel *label)
{
  if (label->use_markup)
    set_markup (label, label->label, label->use_underline);
  else
    {
      if (label->use_underline)
	gtk_label_set_uline_text_internal (label, label->label);
      else
	{
	  gtk_label_set_text_internal (label, g_strdup (label->label));
	  if (label->attrs)
	    pango_attr_list_ref (label->attrs);
	  if (label->effective_attrs)
	    pango_attr_list_unref (label->effective_attrs);
	  label->effective_attrs = label->attrs;
	}
    }

  if (!label->use_underline)
    {
      guint keyval = label->mnemonic_keyval;

      label->mnemonic_keyval = GDK_VoidSymbol;
      gtk_label_setup_mnemonic (label, keyval);
    }

  gtk_label_clear_layout (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * gtk_label_set_text:
 * @label: a #GtkLabel
 * @str: The text you want to set.
 *
 * Sets the text within the #GtkLabel widget.  It overwrites any text that
 * was there before.  
 *
 * This will also clear any previously set mnemonic accelerators.
 **/
void
gtk_label_set_text (GtkLabel    *label,
		    const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, FALSE);
  
  gtk_label_recalculate (label);
}

/**
 * gtk_label_set_attributes:
 * @label: a #GtkLabel
 * @attrs: a #PangoAttrList
 * 
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * label text. The attributes set with this function will be ignored
 * if label->use_underline or label->use_markup is %TRUE.
 **/
void
gtk_label_set_attributes (GtkLabel         *label,
                          PangoAttrList    *attrs)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_attributes_internal (label, attrs);
  
  gtk_label_clear_layout (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * gtk_label_get_attributes:
 * @label: a #GtkLabel
 *
 * Gets the attribute list that was set on the label using
 * gtk_label_set_attributes(), if any. This function does
 * not reflect attributes that come from the labels markup
 * (see gtk_label_set_markup()). If you want to get the
 * effective attributes for the label, use
 * pango_layout_get_attribute (gtk_label_get_layout (label)).
 *
 * Return value: the attribute list, or %NULL if none was set.
 **/
PangoAttrList *
gtk_label_get_attributes (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->attrs;
}

/**
 * gtk_label_set_label:
 * @label: a #GtkLabel
 * @str: the new text to set for the label
 *
 * Sets the text of the label. The label is interpreted as
 * including embedded underlines and/or Pango markup depending
 * on the values of label->use_underline and label->use_markup.
 **/
void
gtk_label_set_label (GtkLabel    *label,
		     const gchar *str)
{
  guint last_keyval;

  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  last_keyval = label->mnemonic_keyval;

  gtk_label_set_label_internal (label, g_strdup (str));
  gtk_label_recalculate (label);
  if (last_keyval != label->mnemonic_keyval)
    gtk_label_setup_mnemonic (label, last_keyval);
}

/**
 * gtk_label_get_label:
 * @label: a #GtkLabel
 *
 * Fetches the text from a label widget including any embedded
 * underlines indicating mnemonics and Pango markup. (See
 * gtk_label_get_text ()).
 *
 * Return value: the text of the label widget. This string is
 *   owned by the widget and must not be modified or freed.
 **/
G_CONST_RETURN gchar *
gtk_label_get_label (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->label;
}

static void
set_markup (GtkLabel    *label,
            const gchar *str,
            gboolean     with_uline)
{
  gchar *text = NULL;
  GError *error = NULL;
  PangoAttrList *attrs = NULL;
  gunichar accel_char = 0;

  if (!pango_parse_markup (str,
                           -1,
                           with_uline ? '_' : 0,
                           &attrs,
                           &text,
                           with_uline ? &accel_char : NULL,
                           &error))
    {
      g_warning ("Failed to set label from markup due to error parsing markup: %s",
                 error->message);
      g_error_free (error);
      return;
    }

  if (text)
    gtk_label_set_text_internal (label, text);

  if (attrs)
    {
      if (label->effective_attrs)
	pango_attr_list_unref (label->effective_attrs);
      label->effective_attrs = attrs;
    }

  if (accel_char != 0)
    label->mnemonic_keyval = gdk_keyval_to_lower (gdk_unicode_to_keyval (accel_char));
  else
    label->mnemonic_keyval = GDK_VoidSymbol;
}

/**
 * gtk_label_set_markup:
 * @label: a #GtkLabel
 * @str: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 * 
 * Parses @str which is marked up with the <link linkend="PangoMarkupFormat">Pango text markup language</link>,
 * setting the label's text and attribute list based on the parse results.
 **/
void
gtk_label_set_markup (GtkLabel    *label,
                      const gchar *str)
{  
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, FALSE);
  
  gtk_label_recalculate (label);
}

/**
 * gtk_label_set_markup_with_mnemonic:
 * @label: a #GtkLabel
 * @str: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 * 
 * Parses @str which is marked up with the <link linkend="PangoMarkupFormat">Pango text markup language</link>,
 * setting the label's text and attribute list based on the parse results.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 *
 * The mnemonic key can be used to activate another widget, chosen automatically,
 * or explicitly using gtk_label_set_mnemonic_widget().
 **/
void
gtk_label_set_markup_with_mnemonic (GtkLabel    *label,
				    const gchar *str)
{
  guint last_keyval;
  g_return_if_fail (GTK_IS_LABEL (label));

  last_keyval = label->mnemonic_keyval;
  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);
  gtk_label_setup_mnemonic (label, last_keyval);
}

/**
 * gtk_label_get_text:
 * @label: a #GtkLabel
 * 
 * Fetches the text from a label widget, as displayed on the
 * screen. This does not include any embedded underlines
 * indicating mnemonics or Pango markup. (See gtk_label_get_label())
 * 
 * Return value: the text in the label widget. This is the internal
 *   string used by the label, and must not be modified.
 **/
G_CONST_RETURN gchar *
gtk_label_get_text (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->text;
}

static PangoAttrList *
gtk_label_pattern_to_attrs (GtkLabel      *label,
			    const gchar   *pattern)
{
  const char *start;
  const char *p = label->text;
  const char *q = pattern;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  while (1)
    {
      while (*p && *q && *q != '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      start = p;
      while (*p && *q && *q == '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      
      if (p > start)
	{
	  PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_LOW);
	  attr->start_index = start - label->text;
	  attr->end_index = p - label->text;
	  
	  pango_attr_list_insert (attrs, attr);
	}
      else
	break;
    }

  return attrs;
}

static void
gtk_label_set_pattern_internal (GtkLabel    *label,
				const gchar *pattern)
{
  PangoAttrList *attrs;
  g_return_if_fail (GTK_IS_LABEL (label));
  
  attrs = gtk_label_pattern_to_attrs (label, pattern);

  if (label->effective_attrs)
    pango_attr_list_unref (label->effective_attrs);
  label->effective_attrs = attrs;
}

void
gtk_label_set_pattern (GtkLabel	   *label,
		       const gchar *pattern)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  gtk_label_set_pattern_internal (label, pattern);

  gtk_label_clear_layout (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}


/**
 * gtk_label_set_justify:
 * @label: a #GtkLabel
 * @jtype: a #GtkJustification
 *
 * Sets the alignment of the lines in the text of the label relative to
 * each other.  %GTK_JUSTIFY_LEFT is the default value when the
 * widget is first created with gtk_label_new(). If you instead want
 * to set the alignment of the label as a whole, use
 * gtk_misc_set_alignment() instead. gtk_label_set_justify() has no
 * effect on labels containing only a single line.
 **/
void
gtk_label_set_justify (GtkLabel        *label,
		       GtkJustification jtype)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);
  
  if ((GtkJustification) label->jtype != jtype)
    {
      label->jtype = jtype;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (label);
      
      g_object_notify (G_OBJECT (label), "justify");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_justify:
 * @label: a #GtkLabel
 *
 * Returns the justification of the label. See gtk_label_set_justify ().
 *
 * Return value: #GtkJustification
 **/
GtkJustification
gtk_label_get_justify (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), 0);

  return label->jtype;
}

/**
 * gtk_label_set_line_wrap:
 * @label: a #GtkLabel
 * @wrap: the setting
 *
 * Toggles line wrapping within the #GtkLabel widget.  %TRUE makes it break
 * lines if text exceeds the widget's size.  %FALSE lets the text get cut off
 * by the edge of the widget if it exceeds the widget size.
 **/
void
gtk_label_set_line_wrap (GtkLabel *label,
			 gboolean  wrap)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  wrap = wrap != FALSE;
  
  if (label->wrap != wrap)
    {
      label->wrap = wrap;
      g_object_notify (G_OBJECT (label), "wrap");
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_line_wrap:
 * @label: a #GtkLabel
 *
 * Returns whether lines in the label are automatically wrapped. See gtk_label_set_line_wrap ().
 *
 * Return value: %TRUE if the lines of the label are automatically wrapped.
 */
gboolean
gtk_label_get_line_wrap (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->wrap;
}

void
gtk_label_get (GtkLabel *label,
	       gchar   **str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);
  
  *str = label->text;
}

static void
gtk_label_destroy (GtkObject *object)
{
  GtkLabel *label = GTK_LABEL (object);

  gtk_label_set_mnemonic_widget (label, NULL);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_label_finalize (GObject *object)
{
  GtkLabel *label;
  
  g_return_if_fail (GTK_IS_LABEL (object));
  
  label = GTK_LABEL (object);
  
  g_free (label->label);
  g_free (label->text);

  if (label->layout)
    g_object_unref (G_OBJECT (label->layout));

  if (label->attrs)
    pango_attr_list_unref (label->attrs);

  if (label->effective_attrs)
    pango_attr_list_unref (label->effective_attrs);

  g_free (label->select_info);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_label_clear_layout (GtkLabel *label)
{
  if (label->layout)
    {
      g_object_unref (G_OBJECT (label->layout));
      label->layout = NULL;
    }
}

static void
gtk_label_ensure_layout (GtkLabel *label)
{
  GtkWidget *widget;
  PangoRectangle logical_rect;
  gint rwidth, rheight;

  widget = GTK_WIDGET (label);

  rwidth = label->misc.xpad * 2;
  rheight = label->misc.ypad * 2;

  if (!label->layout)
    {
      PangoAlignment align = PANGO_ALIGN_LEFT; /* Quiet gcc */

      label->layout = gtk_widget_create_pango_layout (widget, label->text);

      if (label->effective_attrs)
	pango_layout_set_attributes (label->layout, label->effective_attrs);
      
      switch (label->jtype)
	{
	case GTK_JUSTIFY_LEFT:
	  align = PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  align = PANGO_ALIGN_RIGHT;
	  break;
	case GTK_JUSTIFY_CENTER:
	  align = PANGO_ALIGN_CENTER;
	  break;
	case GTK_JUSTIFY_FILL:
	  /* FIXME: This just doesn't work to do this */
	  align = PANGO_ALIGN_LEFT;
	  pango_layout_set_justify (label->layout, TRUE);
	  break;
	default:
	  g_assert_not_reached();
	}

      pango_layout_set_alignment (label->layout, align);

      if (label->wrap)
	{
	  GtkWidgetAuxInfo *aux_info;
	  gint longest_paragraph;
	  gint width, height;
	  
	  aux_info = _gtk_widget_get_aux_info (widget, FALSE);
	  if (aux_info && aux_info->width > 0)
	    pango_layout_set_width (label->layout, aux_info->width * PANGO_SCALE);
	  else
	    {
	      pango_layout_set_width (label->layout, -1);
	      pango_layout_get_extents (label->layout, NULL, &logical_rect);

	      width = logical_rect.width;
	      
	      /* Try to guess a reasonable maximum width */
	      longest_paragraph = width;
	      
	      width = MIN (width,
			   PANGO_SCALE * gdk_string_width (gtk_style_get_font (GTK_WIDGET (label)->style),
							   "This long string gives a good enough length for any line to have."));
	      width = MIN (width,
			   PANGO_SCALE * (gdk_screen_width () + 1) / 2);
	      
	      pango_layout_set_width (label->layout, width);
	      pango_layout_get_extents (label->layout, NULL, &logical_rect);
	      height = logical_rect.height;
	      
	      /* Unfortunately, the above may leave us with a very unbalanced looking paragraph,
	       * so we try short search for a narrower width that leaves us with the same height
	       */
	      if (longest_paragraph > 0)
		{
		  gint nlines, perfect_width;
		  
		  nlines = pango_layout_get_line_count (label->layout);
		  perfect_width = (longest_paragraph + nlines - 1) / nlines;
		  
		  if (perfect_width < width)
		    {
		      pango_layout_set_width (label->layout, perfect_width);
		      pango_layout_get_extents (label->layout, NULL, &logical_rect);
		      
		      if (logical_rect.height <= height)
			width = perfect_width;
		      else
			{
			  gint mid_width = (perfect_width + width) / 2;
			  
			  if (mid_width > perfect_width)
			    {
			      pango_layout_set_width (label->layout, mid_width);
			      pango_layout_get_extents (label->layout, NULL, &logical_rect);
			      
			      if (logical_rect.height <= height)
				width = mid_width;
			    }
			}
		    }
		}
	      pango_layout_set_width (label->layout, width);
	    }
	}
      else		/* !label->wrap */
	pango_layout_set_width (label->layout, -1);
    }
}

static void
gtk_label_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkLabel *label;
  gint width, height;
  PangoRectangle logical_rect;
  GtkWidgetAuxInfo *aux_info;
  
  g_return_if_fail (GTK_IS_LABEL (widget));
  g_return_if_fail (requisition != NULL);
  
  label = GTK_LABEL (widget);

  /*  
   * If word wrapping is on, then the height requisition can depend
   * on:
   *
   *   - Any width set on the widget via gtk_widget_set_usize().
   *   - The padding of the widget (xpad, set by gtk_misc_set_padding)
   *
   * Instead of trying to detect changes to these quantities, if we
   * are wrapping, we just rewrap for each size request. Since
   * size requisitions are cached by the GTK+ core, this is not
   * expensive.
   */

  if (label->wrap)
    gtk_label_clear_layout (label);

  gtk_label_ensure_layout (label);

  width = label->misc.xpad * 2;
  height = label->misc.ypad * 2;

  pango_layout_get_extents (label->layout, NULL, &logical_rect);
  
  aux_info = _gtk_widget_get_aux_info (widget, FALSE);
  if (label->wrap && aux_info && aux_info->width > 0)
    width += aux_info->width;
  else 
    width += PANGO_PIXELS (logical_rect.width);
  
  height += PANGO_PIXELS (logical_rect.height);

  requisition->width = width;
  requisition->height = height;
}

static void
gtk_label_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  (* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

  if (label->select_info && label->select_info->window)
    {
      gdk_window_move_resize (label->select_info->window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }
}

static void
gtk_label_state_changed (GtkWidget   *widget,
                         GtkStateType prev_state)
{
  GtkLabel *label;
  
  label = GTK_LABEL (widget);

  if (label->select_info)
    gtk_label_select_region (label, 0, 0);

  if (GTK_WIDGET_CLASS (parent_class)->state_changed)
    GTK_WIDGET_CLASS (parent_class)->state_changed (widget, prev_state);
}

static void 
gtk_label_style_set (GtkWidget *widget,
		     GtkStyle  *previous_style)
{
  GtkLabel *label;
  
  g_return_if_fail (GTK_IS_LABEL (widget));
  
  label = GTK_LABEL (widget);

  /* We have to clear the layout, fonts etc. may have changed */
  gtk_label_clear_layout (label);
}

static void 
gtk_label_direction_changed (GtkWidget        *widget,
			     GtkTextDirection previous_dir)
{
  GtkLabel *label = GTK_LABEL (widget);

  if (label->layout)
    pango_layout_context_changed (label->layout);

  GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

#if 0
static void
gtk_label_paint_word (GtkLabel     *label,
		      gint          x,
		      gint          y,
		      GtkLabelWord *word,
		      GdkRectangle *area)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkLabelULine *uline;
  gchar *tmp_str;
  
  tmp_str = gdk_wcstombs (word->beginning);
  if (tmp_str)
    {
      gtk_paint_string (widget->style, widget->window, widget->state,
			area, widget, "label", 
			x + word->x,
			y + word->y,
			tmp_str);
      g_free (tmp_str);
    }
  
  for (uline = word->uline; uline; uline = uline->next)
    gtk_paint_hline (widget->style, widget->window, 
		     widget->state, area,
		     widget, "label", 
		     x + uline->x1, x + uline->x2, y + uline->y);
}
#endif

static void
get_layout_location (GtkLabel  *label,
                     gint      *xp,
                     gint      *yp)
{
  GtkMisc *misc;
  GtkWidget *widget;
  gfloat xalign;
  gint x, y;
  
  misc = GTK_MISC (label);
  widget = GTK_WIDGET (label);
  
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    xalign = misc->xalign;
  else
    xalign = 1.0 - misc->xalign;
  
  x = floor (widget->allocation.x + (gint)misc->xpad
             + ((widget->allocation.width - widget->requisition.width) * xalign)
             + 0.5);
  
  y = floor (widget->allocation.y + (gint)misc->ypad 
             + ((widget->allocation.height - widget->requisition.height) * misc->yalign)
             + 0.5);
  

  if (xp)
    *xp = x;

  if (yp)
    *yp = y;
}

static void
gtk_label_draw_cursor (GtkLabel  *label, gint xoffset, gint yoffset)
{
  if (label->select_info == NULL)
    return;
  
  if (GTK_WIDGET_DRAWABLE (label))
    {
      GtkWidget *widget = GTK_WIDGET (label);

      GtkTextDirection keymap_direction;
      GtkTextDirection widget_direction;
      PangoRectangle strong_pos, weak_pos;
      gboolean split_cursor;
      PangoRectangle *cursor1 = NULL;
      PangoRectangle *cursor2 = NULL;
      GdkRectangle cursor_location;
      GtkTextDirection dir1 = GTK_TEXT_DIR_NONE;
      GtkTextDirection dir2 = GTK_TEXT_DIR_NONE;
      GdkGC *gc1 = NULL;
      GdkGC *gc2 = NULL;

      keymap_direction =
	(gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
	GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;

      widget_direction = gtk_widget_get_direction (widget);

      gtk_label_ensure_layout (label);
      
      pango_layout_get_cursor_pos (label->layout, label->select_info->selection_end,
				   &strong_pos, &weak_pos);

      g_object_get (gtk_widget_get_settings (widget),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	{
	  gc1 = label->select_info->cursor_gc;
	  cursor1 = &strong_pos;

	  if (strong_pos.x != weak_pos.x ||
	      strong_pos.y != weak_pos.y)
	    {
	      dir1 = widget_direction;
	      dir2 = (widget_direction == GTK_TEXT_DIR_LTR) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;
	      
	      gc2 = widget->style->black_gc;
	      cursor2 = &weak_pos;
	    }
	}
      else
	{
	  gc1 = label->select_info->cursor_gc;
	  
	  if (keymap_direction == widget_direction)
	    cursor1 = &strong_pos;
	  else
	    cursor1 = &weak_pos;
	}
      
      cursor_location.x = xoffset + PANGO_PIXELS (cursor1->x);
      cursor_location.y = yoffset + PANGO_PIXELS (cursor1->y);
      cursor_location.width = 0;
      cursor_location.height = PANGO_PIXELS (cursor1->height);
      
      _gtk_draw_insertion_cursor (widget, widget->window, gc1,
				  &cursor_location, dir1);
      
      if (gc2)
	{
	  cursor_location.x = xoffset + PANGO_PIXELS (cursor2->x);
	  cursor_location.y = yoffset + PANGO_PIXELS (cursor2->y);
	  cursor_location.width = 0;
	  cursor_location.height = PANGO_PIXELS (cursor2->height);
	  
	  _gtk_draw_insertion_cursor (widget, widget->window, gc2,
				      &cursor_location, dir2);
	}
    }
}


static gint
gtk_label_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkLabel *label;
  gint x, y;
  
  g_return_val_if_fail (GTK_IS_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  label = GTK_LABEL (widget);

  gtk_label_ensure_layout (label);
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget) &&
      label->text && (*label->text != '\0'))
    {
      get_layout_location (label, &x, &y);
      
      gtk_paint_layout (widget->style,
                        widget->window,
                        GTK_WIDGET_STATE (widget),
			FALSE,
                        &event->area,
                        widget,
                        "label",
                        x, y,
                        label->layout);
      
      if (label->select_info &&
          (label->select_info->selection_anchor !=
           label->select_info->selection_end))
        {
          gint range[2];
          GdkRegion *clip;
	  GtkStateType state;
	  
          range[0] = label->select_info->selection_anchor;
          range[1] = label->select_info->selection_end;

          if (range[0] > range[1])
            {
              gint tmp = range[0];
              range[0] = range[1];
              range[1] = tmp;
            }

          clip = gdk_pango_layout_get_clip_region (label->layout,
                                                   x, y,
                                                   range,
                                                   1);

          /* FIXME should use gtk_paint, but it can't use a clip
           * region
           */

          gdk_gc_set_clip_region (widget->style->black_gc, clip);


	  state = GTK_STATE_SELECTED;
	  if (!GTK_WIDGET_HAS_FOCUS (widget))
	    state = GTK_STATE_ACTIVE;
	      
          gdk_draw_layout_with_colors (widget->window,
                                       widget->style->black_gc,
                                       x, y,
                                       label->layout,
                                       &widget->style->text[state],
                                       &widget->style->base[state]);

          gdk_gc_set_clip_region (widget->style->black_gc, NULL);
          gdk_region_destroy (clip);
        }
      else if (label->select_info && GTK_WIDGET_HAS_FOCUS (widget))
	gtk_label_draw_cursor (label, x, y);
    }

  return FALSE;
}

static void
gtk_label_set_uline_text_internal (GtkLabel    *label,
				   const gchar *str)
{
  guint accel_key = GDK_VoidSymbol;

  gchar *new_str;
  gchar *pattern;
  const gchar *src;
  gchar *dest, *pattern_dest;
  gboolean underscore;
      
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  /* Convert text to wide characters */

  new_str = g_new (gchar, strlen (str) + 1);
  pattern = g_new (gchar, g_utf8_strlen (str, -1) + 1);
  
  underscore = FALSE;

  if (str == NULL)
    str = "";
  
  src = str;
  dest = new_str;
  pattern_dest = pattern;
  
  while (*src)
    {
      gunichar c;
      gchar *next_src;

      c = g_utf8_get_char (src);
      if (c == (gunichar)-1)
	{
	  g_warning ("Invalid input string");
	  g_free (new_str);
	  g_free (pattern);
	  return;
	}
      next_src = g_utf8_next_char (src);
      
      if (underscore)
	{
	  if (c == '_')
	    *pattern_dest++ = ' ';
	  else
	    {
	      *pattern_dest++ = '_';
	      if (accel_key == GDK_VoidSymbol)
		accel_key = gdk_keyval_to_lower (c);
	    }

	  while (src < next_src)
	    *dest++ = *src++;
	  
	  underscore = FALSE;
	}
      else
	{
	  if (c == '_')
	    {
	      underscore = TRUE;
	      src = next_src;
	    }
	  else
	    {
	      while (src < next_src)
		*dest++ = *src++;
	  
	      *pattern_dest++ = ' ';
	    }
	}
    }
  *dest = 0;
  *pattern_dest = 0;
  
  gtk_label_set_text_internal (label, new_str);
  gtk_label_set_pattern_internal (label, pattern);
  
  g_free (pattern);

  label->mnemonic_keyval = accel_key;
}

guint      
gtk_label_parse_uline (GtkLabel    *label,
		       const gchar *str)
{
  guint keyval;
  guint orig_keyval;
  
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);
  g_return_val_if_fail (str != NULL, GDK_VoidSymbol);

  orig_keyval = label->mnemonic_keyval;
  
  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);

  keyval = label->mnemonic_keyval;
  label->mnemonic_keyval = GDK_VoidSymbol;
  
  gtk_label_setup_mnemonic (label, orig_keyval);
  
  return keyval;
}

/**
 * gtk_label_set_text_with_mnemonic:
 * @label: a #GtkLabel
 * @str: a string
 * 
 * Sets the label's text from the string @str.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 * The mnemonic key can be used to activate another widget, chosen automatically,
 * or explicitly using gtk_label_set_mnemonic_widget().
 **/
void
gtk_label_set_text_with_mnemonic (GtkLabel    *label,
				  const gchar *str)
{
  guint last_keyval;
  
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  last_keyval = label->mnemonic_keyval;
  
  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);

  gtk_label_setup_mnemonic (label, last_keyval);
}

static void
gtk_label_realize_cursor_gc (GtkLabel *label)
{
  GdkColor *cursor_color;
  GdkColor red = {0, 0xffff, 0x0000, 0x0000};

  if (label->select_info == NULL)
    return;
  
  if (label->select_info->cursor_gc)
    gdk_gc_unref (label->select_info->cursor_gc);

  gtk_widget_style_get (GTK_WIDGET (label), "cursor-color", &cursor_color, NULL);
  label->select_info->cursor_gc = gdk_gc_new (GTK_WIDGET (label)->window);
  if (cursor_color)
    gdk_gc_set_rgb_fg_color (label->select_info->cursor_gc, cursor_color);
  else
    gdk_gc_set_rgb_fg_color (label->select_info->cursor_gc, &red);
}

static void
gtk_label_realize (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  if (label->select_info)
    {
      gtk_label_create_window (label);
      gtk_label_realize_cursor_gc (label);
    }
}

static void
gtk_label_unrealize_cursor_gc (GtkLabel *label)
{
  if (label->select_info == NULL)
    return;
  
  if (label->select_info->cursor_gc)
    {
      gdk_gc_unref (label->select_info->cursor_gc);
      label->select_info->cursor_gc = NULL;
    }
}

static void
gtk_label_unrealize (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  if (label->select_info)
    {
      gtk_label_unrealize_cursor_gc (label);
      gtk_label_destroy_window (label);
    }
  
  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_label_map (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
  
  if (label->select_info)
    gdk_window_show (label->select_info->window);
}

static void
gtk_label_unmap (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  if (label->select_info)
    gdk_window_hide (label->select_info->window);
  
  (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void
window_to_layout_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    {
      *x += widget->allocation.x; /* go to widget->window */
      *x -= lx;                   /* go to layout */
    }

  if (y)
    {
      *y += widget->allocation.y; /* go to widget->window */
      *y -= ly;                   /* go to layout */
    }
}

#if 0
static void
layout_to_window_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    {
      *x += lx;                   /* go to widget->window */
      *x -= widget->allocation.x; /* go to selection window */
    }

  if (y)
    {
      *y += ly;                   /* go to widget->window */
      *y -= widget->allocation.y; /* go to selection window */
    }
}
#endif

static void
get_layout_index (GtkLabel *label,
                  gint      x,
                  gint      y,
                  gint     *index)
{
  gint trailing = 0;
  const gchar *cluster;
  const gchar *cluster_end;

  *index = 0;
  
  gtk_label_ensure_layout (label);
  
  window_to_layout_coords (label, &x, &y);

  x *= PANGO_SCALE;
  y *= PANGO_SCALE;
  
  pango_layout_xy_to_index (label->layout,
                            x, y,
                            index, &trailing);

  
  cluster = label->text + *index;
  cluster_end = cluster;
  while (trailing)
    {
      cluster_end = g_utf8_next_char (cluster_end);
      --trailing;
    }

  *index += (cluster_end - cluster);
}

static void
gtk_label_select_word (GtkLabel *label)
{
  gint min, max;
  
  gint start_index = gtk_label_move_backward_word (label, label->select_info->selection_end);
  gint end_index = gtk_label_move_forward_word (label, label->select_info->selection_end);

  min = MIN (label->select_info->selection_anchor,
	     label->select_info->selection_end);
  max = MAX (label->select_info->selection_anchor,
	     label->select_info->selection_end);

  min = MIN (min, start_index);
  max = MAX (max, end_index);

  gtk_label_select_region_index (label, min, max);
}

static gint
gtk_label_button_press (GtkWidget      *widget,
                        GdkEventButton *event)
{
  GtkLabel *label;
  gint index = 0;
  
  label = GTK_LABEL (widget);

  if (label->select_info == NULL)
    return FALSE;

  if (event->button == 1)
    {
      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);

      if (event->type == GDK_3BUTTON_PRESS)
	{
	  gtk_label_select_region_index (label, 0, strlen (label->label));
	  return TRUE;
	}
      
      if (event->type == GDK_2BUTTON_PRESS)
	{
	  gtk_label_select_word (label);
	  return TRUE;
	}
      
      get_layout_index (label, event->x, event->y, &index);
      
      if ((label->select_info->selection_anchor !=
	   label->select_info->selection_end) &&
	  (event->state & GDK_SHIFT_MASK))
	{
	  gint min, max;
	  
	  /* extend (same as motion) */
	  min = MIN (label->select_info->selection_anchor,
		     label->select_info->selection_end);
	  max = MAX (label->select_info->selection_anchor,
		     label->select_info->selection_end);
	  
	  min = MIN (min, index);
	  max = MAX (max, index);
	  
	  /* ensure the anchor is opposite index */
	  if (index == min)
	    {
	      gint tmp = min;
	      min = max;
	      max = tmp;
	    }
	  
	  gtk_label_select_region_index (label, min, max);
	}
      else
	{
	  if (event->type == GDK_3BUTTON_PRESS)
	      gtk_label_select_region_index (label, 0, strlen (label->label));
	  else if (event->type == GDK_2BUTTON_PRESS)
	      gtk_label_select_word (label);
	  else 
	    /* start a replacement */
	    gtk_label_select_region_index (label, index, index);
	}
  
      return TRUE;
    }
  else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      gtk_label_do_popup (label, event);

      return TRUE;
      
    }
  return FALSE;
}

static gint
gtk_label_button_release (GtkWidget      *widget,
                          GdkEventButton *event)

{
  GtkLabel *label;

  label = GTK_LABEL (widget);
  
  if (label->select_info == NULL)
    return FALSE;
  
  if (event->button != 1)
    return FALSE;
  
  /* The goal here is to return TRUE iff we ate the
   * button press to start selecting.
   */
  
  return TRUE;
}

static gint
gtk_label_motion (GtkWidget      *widget,
                  GdkEventMotion *event)
{
  GtkLabel *label;
  gint index;
  gint x, y;
  
  label = GTK_LABEL (widget);
  
  if (label->select_info == NULL)
    return FALSE;  

  if ((event->state & GDK_BUTTON1_MASK) == 0)
    return FALSE;

  gdk_window_get_pointer (label->select_info->window,
                          &x, &y, NULL);
  
  get_layout_index (label, x, y, &index);

  gtk_label_select_region_index (label,
                                 label->select_info->selection_anchor,
                                 index);
  
  return TRUE;
}

static void
gtk_label_create_window (GtkLabel *label)
{
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_assert (label->select_info);
  g_assert (GTK_WIDGET_REALIZED (label));
  
  if (label->select_info->window)
    return;
  
  widget = GTK_WIDGET (label);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.cursor = gdk_cursor_new (GDK_XTERM);
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_BUTTON_PRESS_MASK        |
    GDK_BUTTON_RELEASE_MASK      |
    GDK_BUTTON_MOTION_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR | GDK_WA_CURSOR;

  label->select_info->window = gdk_window_new (widget->window,
                                               &attributes, attributes_mask);
  gdk_window_set_user_data (label->select_info->window, widget);

  gdk_cursor_unref (attributes.cursor);
}

static void
gtk_label_destroy_window (GtkLabel *label)
{
  g_assert (label->select_info);

  if (label->select_info->window == NULL)
    return;
  
  gdk_window_set_user_data (label->select_info->window, NULL);
  gdk_window_destroy (label->select_info->window);
  label->select_info->window = NULL;
}

/**
 * gtk_label_set_selectable:
 * @label: a #GtkLabel
 * @setting: %TRUE to allow selecting text in the label
 *
 * Selectable labels allow the user to select text from the label, for
 * copy-and-paste.
 * 
 **/
void
gtk_label_set_selectable (GtkLabel *label,
                          gboolean  setting)
{
  gboolean old_setting;
  
  g_return_if_fail (GTK_IS_LABEL (label));
  
  setting = setting != FALSE;
  old_setting = label->select_info != NULL;
  
  if (setting)
    {
      if (label->select_info == NULL)
        {
          label->select_info = g_new0 (GtkLabelSelectionInfo, 1);

	  GTK_WIDGET_SET_FLAGS (label, GTK_CAN_FOCUS);
      
          if (GTK_WIDGET_REALIZED (label))
	    {
	      gtk_label_create_window (label);
	      gtk_label_realize_cursor_gc (label);
	    }

          if (GTK_WIDGET_MAPPED (label))
            gdk_window_show (label->select_info->window);
        }
    }
  else
    {
      if (label->select_info)
        {
          /* unselect, to give up the selection */
          gtk_label_select_region (label, 0, 0);
          
	  gtk_label_unrealize_cursor_gc (label);
	  
          if (label->select_info->window)
	    {
	      gtk_label_destroy_window (label);
	    }

          g_free (label->select_info);

          label->select_info = NULL;

	  GTK_WIDGET_UNSET_FLAGS (label, GTK_CAN_FOCUS);
        }
    }
  if (setting != old_setting)
    {
      g_object_freeze_notify (G_OBJECT (label));
      g_object_notify (G_OBJECT (label), "selectable");
      g_object_notify (G_OBJECT (label), "cursor_position");
      g_object_notify (G_OBJECT (label), "selection_bound");
      g_object_thaw_notify (G_OBJECT (label));
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_selectable:
 * @label: a #GtkLabel
 * 
 * Gets the value set by gtk_label_set_selectable().
 * 
 * Return value: %TRUE if the user can copy text from the label
 **/
gboolean
gtk_label_get_selectable (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->select_info != NULL;
}

static void
get_text_callback (GtkClipboard     *clipboard,
                   GtkSelectionData *selection_data,
                   guint             info,
                   gpointer          user_data_or_owner)
{
  GtkLabel *label;
  
  label = GTK_LABEL (user_data_or_owner);
  
  if ((label->select_info->selection_anchor !=
       label->select_info->selection_end) &&
      label->text)
    {
      gint start, end;
      gint len;
      
      start = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);

      len = strlen (label->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      gtk_selection_data_set_text (selection_data,
				   label->text + start,
				   end - start);
    }
}

static void
clear_text_callback (GtkClipboard     *clipboard,
                     gpointer          user_data_or_owner)
{
  GtkLabel *label;

  label = GTK_LABEL (user_data_or_owner);

  if (label->select_info)
    {
      label->select_info->selection_anchor = label->select_info->selection_end;
      
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

static void
gtk_label_select_region_index (GtkLabel *label,
                               gint      anchor_index,
                               gint      end_index)
{
  static const GtkTargetEntry targets[] = {
    { "STRING", 0, 0 },
    { "TEXT",   0, 0 }, 
    { "COMPOUND_TEXT", 0, 0 },
    { "UTF8_STRING", 0, 0 }
  };

  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->select_info)
    {
      GtkClipboard *clipboard;

      if (label->select_info->selection_anchor == anchor_index &&
	  label->select_info->selection_end == end_index)
	return;

      label->select_info->selection_anchor = anchor_index;
      label->select_info->selection_end = end_index;

      clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);      
      
      if (anchor_index != end_index)
        {
          gtk_clipboard_set_with_owner (clipboard,
                                        targets,
                                        G_N_ELEMENTS (targets),
                                        get_text_callback,
                                        clear_text_callback,
                                        G_OBJECT (label));
        }
      else
        {
          if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (label))
            gtk_clipboard_clear (clipboard);
        }

      gtk_widget_queue_draw (GTK_WIDGET (label));

      g_object_freeze_notify (G_OBJECT (label));
      g_object_notify (G_OBJECT (label), "cursor_position");
      g_object_notify (G_OBJECT (label), "selection_bound");
      g_object_thaw_notify (G_OBJECT (label));
    }
}

/**
 * gtk_label_select_region:
 * @label: a #GtkLabel
 * @start_offset: start offset (in characters not bytes)
 * @end_offset: end offset (in characters not bytes)
 *
 * Selects a range of characters in the label, if the label is selectable.
 * See gtk_label_set_selectable(). If the label is not selectable,
 * this function has no effect. If @start_offset or
 * @end_offset are -1, then the end of the label will be substituted.
 * 
 **/
void
gtk_label_select_region  (GtkLabel *label,
                          gint      start_offset,
                          gint      end_offset)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->text && label->select_info)
    {
      if (start_offset < 0)
        start_offset = g_utf8_strlen (label->text, -1);
      
      if (end_offset < 0)
        end_offset = g_utf8_strlen (label->text, -1);
      
      gtk_label_select_region_index (label,
                                     g_utf8_offset_to_pointer (label->text, start_offset) - label->text,
                                     g_utf8_offset_to_pointer (label->text, end_offset) - label->text);
    }
}

/**
 * gtk_label_get_selection_bounds:
 * @label: a #GtkLabel
 * @start: return location for start of selection, as a character offset
 * @end: return location for end of selection, as a character offset
 * 
 * Gets the selected range of characters in the label, returning %TRUE
 * if there's a selection.
 * 
 * Return value: %TRUE if selection is non-empty
 **/
gboolean
gtk_label_get_selection_bounds (GtkLabel  *label,
                                gint      *start,
                                gint      *end)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  if (label->select_info == NULL)
    {
      /* not a selectable label */
      if (start)
        *start = 0;
      if (end)
        *end = 0;

      return FALSE;
    }
  else
    {
      gint start_index, end_index;
      gint start_offset, end_offset;
      gint len;
      
      start_index = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end_index = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);

      len = strlen (label->text);

      if (end_index > len)
        end_index = len;

      if (start_index > len)
        start_index = len;
      
      start_offset = g_utf8_strlen (label->text, start_index);
      end_offset = g_utf8_strlen (label->text, end_index);

      if (start_offset > end_offset)
        {
          gint tmp = start_offset;
          start_offset = end_offset;
          end_offset = tmp;
        }
      
      if (start)
        *start = start_offset;

      if (end)
        *end = end_offset;

      return start_offset != end_offset;
    }
}


/**
 * gtk_label_get_layout:
 * @label: a #GtkLabel
 * 
 * Gets the #PangoLayout used to display the label.
 * The layout is useful to e.g. convert text positions to
 * pixel positions, in combination with gtk_label_get_layout_offsets().
 * The returned layout is owned by the label so need not be
 * freed by the caller.
 * 
 * Return value: the #PangoLayout for this label
 **/
PangoLayout*
gtk_label_get_layout (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  gtk_label_ensure_layout (label);

  return label->layout;
}

/**
 * gtk_label_get_layout_offsets:
 * @label: a #GtkLabel
 * @x: location to store X offset of layout, or %NULL
 * @y: location to store Y offset of layout, or %NULL
 *
 * Obtains the coordinates where the label will draw the #PangoLayout
 * representing the text in the label; useful to convert mouse events
 * into coordinates inside the #PangoLayout, e.g. to take some action
 * if some part of the label is clicked. Of course you will need to
 * create a #GtkEventBox to receive the events, and pack the label
 * inside it, since labels are a #GTK_NO_WINDOW widget. Remember
 * when using the #PangoLayout functions you need to convert to
 * and from pixels using PANGO_PIXELS() or #PANGO_SCALE.
 * 
 **/
void
gtk_label_get_layout_offsets (GtkLabel *label,
                              gint     *x,
                              gint     *y)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  get_layout_location (label, x, y);
}

/**
 * gtk_label_set_use_markup:
 * @label: a #GtkLabel
 * @setting: %TRUE if the label's text should be parsed for markup.
 *
 * Sets whether the text of the label contains markup in <link
 * linkend="PangoMarkupFormat">Pango's text markup
 * language</link>. See gtk_label_set_markup().
 **/
void
gtk_label_set_use_markup (GtkLabel *label,
			  gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_use_markup_internal (label, setting);
  gtk_label_recalculate (label);
}

/**
 * gtk_label_get_use_markup:
 * @label: a #GtkLabel
 *
 * Returns whether the label's text is interpreted as marked up with
 * the <link linkend="PangoMarkupFormat">Pango text markup
 * language</link>. See gtk_label_set_use_markup ().
 *
 * Return value: %TRUE if the label's text will be parsed for markup.
 **/
gboolean
gtk_label_get_use_markup (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  
  return label->use_markup;
}

/**
 * gtk_label_set_use_underline:
 * @label: a #GtkLabel
 * @setting: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text indicates the next character should be
 * used for the mnemonic accelerator key.
 */
void
gtk_label_set_use_underline (GtkLabel *label,
			     gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_use_underline_internal (label, setting);
  gtk_label_recalculate (label);
  if (label->use_underline)
    gtk_label_setup_mnemonic (label, label->mnemonic_keyval);
}

/**
 * gtk_label_get_use_underline:
 * @label: a #GtkLabel
 *
 * Returns whether an embedded underline in the label indicates a
 * mnemonic. See gtk_label_set_use_underline ().
 *
 * Return value: %TRUE whether an embedded underline in the label indicates
 *               the mnemonic accelerator keys.
 **/
gboolean
gtk_label_get_use_underline (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  
  return label->use_underline;
}

static gboolean
gtk_label_focus (GtkWidget         *widget,
		 GtkDirectionType   direction)
{
  /* We never want to be in the tab chain */
  return FALSE;
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static void
get_better_cursor (GtkLabel *label,
		   gint      index,
		   gint      *x,
		   gint      *y)
{
  GtkTextDirection keymap_direction =
    (gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
    GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;
  GtkTextDirection widget_direction = gtk_widget_get_direction (GTK_WIDGET (label));
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		"gtk-split-cursor", &split_cursor,
		NULL);

  gtk_label_ensure_layout (label);
  
  pango_layout_get_cursor_pos (label->layout, index,
			       &strong_pos, &weak_pos);

  if (split_cursor)
    {
      *x = strong_pos.x / PANGO_SCALE;
      *y = strong_pos.y / PANGO_SCALE;
    }
  else
    {
      if (keymap_direction == widget_direction)
	{
	  *x = strong_pos.x / PANGO_SCALE;
	  *y = strong_pos.y / PANGO_SCALE;
	}
      else
	{
	  *x = weak_pos.x / PANGO_SCALE;
	  *y = weak_pos.y / PANGO_SCALE;
	}
    }
}


static gint
gtk_label_move_logically (GtkLabel *label,
			  gint      start,
			  gint      count)
{
  gint offset = g_utf8_pointer_to_offset (label->label,
					  label->label + start);

  if (label->label)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;
      gint length;

      gtk_label_ensure_layout (label);
      
      length = g_utf8_strlen (label->label, -1);

      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);

      while (count > 0 && offset < length)
	{
	  do
	    offset++;
	  while (offset < length && !log_attrs[offset].is_cursor_position);
	  
	  count--;
	}
      while (count < 0 && offset > 0)
	{
	  do
	    offset--;
	  while (offset > 0 && !log_attrs[offset].is_cursor_position);
	  
	  count++;
	}
      
      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->label, offset) - label->label;
}

static gint
gtk_label_move_visually (GtkLabel *label,
			 gint      start,
			 gint      count)
{
  gint index;

  index = start;
  
  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      gtk_label_ensure_layout (label);

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	strong = TRUE;
      else
	{
	  GtkTextDirection keymap_direction =
	    (gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
	    GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;

	  strong = keymap_direction == gtk_widget_get_direction (GTK_WIDGET (label));
	}
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (label->layout, strong, index, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (label->layout, strong, index, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      if (new_index < 0 || new_index == G_MAXINT)
	break;

      index = new_index;
      
      while (new_trailing--)
	index = g_utf8_next_char (label->label + new_index) - label->label;
    }
  
  return index;
}

static gint
gtk_label_move_forward_word (GtkLabel *label,
			     gint      start)
{
  gint new_pos = g_utf8_pointer_to_offset (label->label,
					   label->label + start);
  gint length;

  length = g_utf8_strlen (label->label, -1);
  if (new_pos < length)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      gtk_label_ensure_layout (label);
      
      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);
      
      /* Find the next word end */
      new_pos++;
      while (new_pos < n_attrs && !log_attrs[new_pos].is_word_end)
	new_pos++;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->label, new_pos) - label->label;
}


static gint
gtk_label_move_backward_word (GtkLabel *label,
			      gint      start)
{
  gint new_pos = g_utf8_pointer_to_offset (label->label,
					   label->label + start);
  gint length;

  length = g_utf8_strlen (label->label, -1);
  
  if (new_pos > 0)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      gtk_label_ensure_layout (label);
      
      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);
      
      new_pos -= 1;

      /* Find the previous word beginning */
      while (new_pos > 0 && !log_attrs[new_pos].is_word_start)
	new_pos--;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->label, new_pos) - label->label;
}

static void
gtk_label_move_cursor (GtkLabel       *label,
		       GtkMovementStep step,
		       gint            count,
		       gboolean        extend_selection)
{
  gint new_pos;
  
  if (label->select_info == NULL)
    return;
  
  new_pos = label->select_info->selection_end;

  if (label->select_info->selection_end != label->select_info->selection_anchor &&
      !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
	{
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  {
	    gint end_x, end_y;
	    gint anchor_x, anchor_y;
	    gboolean end_is_left;
	    
	    get_better_cursor (label, label->select_info->selection_end, &end_x, &end_y);
	    get_better_cursor (label, label->select_info->selection_anchor, &anchor_x, &anchor_y);

	    end_is_left = (end_y < anchor_y) || (end_y == anchor_y && end_x < anchor_x);
	    
	    if (count < 0)
	      new_pos = end_is_left ? label->select_info->selection_end : label->select_info->selection_anchor;
	    else
	      new_pos = !end_is_left ? label->select_info->selection_end : label->select_info->selection_anchor;

	    break;
	  }
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	case GTK_MOVEMENT_WORDS:
	  if (count < 0)
	    new_pos = MIN (label->select_info->selection_end, label->select_info->selection_anchor);
	  else
	    new_pos = MAX (label->select_info->selection_end, label->select_info->selection_anchor);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  /* FIXME: Can do better here */
	  new_pos = count < 0 ? 0 : strlen (label->label);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	  break;
	}
    }
  else
    {
      switch (step)
	{
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	  new_pos = gtk_label_move_logically (label, new_pos, count);
	  break;
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  new_pos = gtk_label_move_visually (label, new_pos, count);
	  break;
	case GTK_MOVEMENT_WORDS:
	  while (count > 0)
	    {
	      new_pos = gtk_label_move_forward_word (label, new_pos);
	      count--;
	    }
	  while (count < 0)
	    {
	      new_pos = gtk_label_move_backward_word (label, new_pos);
	      count++;
	    }
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  /* FIXME: Can do better here */
	  new_pos = count < 0 ? 0 : strlen (label->label);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	  break;
	}
    }

  if (extend_selection)
    gtk_label_select_region_index (label,
				   label->select_info->selection_anchor,
				   new_pos);
  else
    gtk_label_select_region_index (label, new_pos, new_pos);
}

static void
gtk_label_copy_clipboard (GtkLabel *label)
{
  if (label->text && label->select_info)
    {
      gint start, end;
      gint len;
      
      start = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);

      len = strlen (label->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      if (start != end)
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				label->text + start, end - start);
    }
}

static void
gtk_label_select_all (GtkLabel *label)
{
  gtk_label_select_region_index (label, 0, strlen (label->label));
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
	     GtkLabel  *label)
{
  const gchar *signal = gtk_object_get_data (GTK_OBJECT (menuitem), "gtk-signal");
  gtk_signal_emit_by_name (GTK_OBJECT (label), signal);
}

static void
append_action_signal (GtkLabel     *label,
		      GtkWidget    *menu,
		      const gchar  *stock_id,
		      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);

  gtk_object_set_data (GTK_OBJECT (menuitem), "gtk-signal", (char *)signal);
  gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
		      GTK_SIGNAL_FUNC (activate_cb), label);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GtkLabel *label;
  label = GTK_LABEL (attach_widget);

  if (label->select_info)
    label->select_info->popup_menu = NULL;
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkLabel *label;
  GtkWidget *widget;
  GtkRequisition req;
  
  label = GTK_LABEL (user_data);  
  widget = GTK_WIDGET (label);

  if (label->select_info == NULL)
    return;
  
  g_return_if_fail (GTK_WIDGET_REALIZED (label));

  gdk_window_get_origin (widget->window, x, y);      

  gtk_widget_size_request (label->select_info->popup_menu, &req);
  
  *x += widget->allocation.width / 2;
  *y += widget->allocation.height;

  *x = CLAMP (*x, 0, MAX (0, gdk_screen_width () - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_height () - req.height));
}


static void
gtk_label_do_popup (GtkLabel       *label,
                    GdkEventButton *event)
{
  GtkWidget *menuitem;
  gboolean have_selection;

  if (label->select_info == NULL)
    return;
    
  if (label->select_info->popup_menu)
    gtk_widget_destroy (label->select_info->popup_menu);
  
  label->select_info->popup_menu = gtk_menu_new ();

  gtk_menu_attach_to_widget (GTK_MENU (label->select_info->popup_menu),
                             GTK_WIDGET (label),
                             popup_menu_detach);

  have_selection =
    label->select_info->selection_anchor != label->select_info->selection_end;


  append_action_signal (label, label->select_info->popup_menu, GTK_STOCK_CUT, "cut_clipboard",
                        FALSE);
  append_action_signal (label, label->select_info->popup_menu, GTK_STOCK_COPY, "copy_clipboard",
                        have_selection);
  append_action_signal (label, label->select_info->popup_menu, GTK_STOCK_PASTE, "paste_clipboard",
                        FALSE);
  
  menuitem = gtk_menu_item_new_with_label (_("Select All"));
  gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
			     GTK_SIGNAL_FUNC (gtk_label_select_all), label);
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (label->select_info->popup_menu), menuitem);

  menuitem = gtk_separator_menu_item_new ();
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (label->select_info->popup_menu), menuitem);
      
  menuitem = gtk_menu_item_new_with_label (_("Input Methods"));
  gtk_widget_show (menuitem);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), gtk_menu_new ());
  gtk_widget_set_sensitive (menuitem, FALSE);
  gtk_menu_shell_append (GTK_MENU_SHELL (label->select_info->popup_menu), menuitem);

  gtk_signal_emit (GTK_OBJECT (label),
                   signals[POPULATE_POPUP],
                   label->select_info->popup_menu);
  
  if (event)
    gtk_menu_popup (GTK_MENU (label->select_info->popup_menu), NULL, NULL,
                    NULL, NULL,
                    event->button, event->time);
  else
    gtk_menu_popup (GTK_MENU (label->select_info->popup_menu), NULL, NULL,
                    popup_position_func, label,
                    0, gtk_get_current_event_time ());
}
