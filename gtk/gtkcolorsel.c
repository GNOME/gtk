/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <math.h>
#include "gtkcolorsel.h"


#define DEGTORAD(a) (2.0*M_PI*a/360.0)
#define SQR(a) (a*a)

#define TIMER_DELAY 300

#define CIRCLE_RADIUS 65

#define WHEEL_WIDTH   2*CIRCLE_RADIUS+2
#define WHEEL_HEIGHT  2*CIRCLE_RADIUS+2

#define VALUE_WIDTH   32
#define VALUE_HEIGHT  WHEEL_HEIGHT

#define SAMPLE_WIDTH  WHEEL_WIDTH+VALUE_WIDTH+5
#define SAMPLE_HEIGHT 28


enum
{
  COLOR_CHANGED,
  LAST_SIGNAL
};

enum
{
  RGB_INPUTS     = 1 << 0,
  HSV_INPUTS     = 1 << 1,
  OPACITY_INPUTS = 1 << 2
};

enum
{
  SCALE,
  ENTRY,
  BOTH
};

enum
{
  HUE,
  SATURATION,
  VALUE,
  RED,
  GREEN,
  BLUE,
  OPACITY,
  NUM_CHANNELS
};

typedef struct
{
  gchar *label;
  gfloat lower, upper, step_inc, page_inc;
  GtkSignalFunc updater;
} scale_val_type;


#define HSV_TO_RGB()  gtk_color_selection_hsv_to_rgb( \
                        colorsel->values[HUE], \
                        colorsel->values[SATURATION], \
                        colorsel->values[VALUE], \
                        &colorsel->values[RED], \
                        &colorsel->values[GREEN], \
                        &colorsel->values[BLUE])

#define RGB_TO_HSV()  gtk_color_selection_rgb_to_hsv( \
                        colorsel->values[RED], \
                        colorsel->values[GREEN], \
                        colorsel->values[BLUE], \
                        &colorsel->values[HUE], \
                        &colorsel->values[SATURATION], \
                        &colorsel->values[VALUE])


static void gtk_color_selection_hsv_updater       (GtkWidget         *widget,
                                                   gpointer           data);
static void gtk_color_selection_rgb_updater       (GtkWidget         *widget,
                                                   gpointer           data);
static void gtk_color_selection_opacity_updater   (GtkWidget         *widget,
                                                   gpointer           data);
static void gtk_color_selection_realize           (GtkWidget         *widget);
static void gtk_color_selection_destroy           (GtkObject         *object);
static void gtk_color_selection_color_changed     (GtkColorSelection *colorsel);
static void gtk_color_selection_update_input      (GtkWidget         *scale,
                                                   GtkWidget         *entry,
                                                   gdouble            value);
static void gtk_color_selection_update_inputs     (GtkColorSelection *colorsel,
                                                   gint               inputs,
                                                   gint               which);
static void gtk_color_selection_update_value      (GtkColorSelection *colorsel,
                                                   gint               y);
static void gtk_color_selection_update_wheel      (GtkColorSelection *colorsel,
                                                   gint               x,
                                                   gint               y);
static void gtk_color_selection_value_resize      (GtkWidget          *widget,
                                                   gpointer            data);
static gint gtk_color_selection_value_events      (GtkWidget          *area,
                                                   GdkEvent           *event);
static gint gtk_color_selection_value_timeout     (GtkColorSelection  *colorsel);
static void gtk_color_selection_wheel_resize      (GtkWidget          *widget,
                                                   gpointer            data);
static gint gtk_color_selection_wheel_events      (GtkWidget          *area,
                                                   GdkEvent           *event);
static gint gtk_color_selection_wheel_timeout     (GtkColorSelection  *colorsel);
static void gtk_color_selection_sample_resize     (GtkWidget          *widget,
                                                   gpointer            data);
static void gtk_color_selection_drop_handle       (GtkWidget          *widget,
						   GdkEvent           *event);
static void gtk_color_selection_drag_handle       (GtkWidget          *widget,
						   GdkEvent           *event);
static void gtk_color_selection_draw_wheel_marker (GtkColorSelection  *colorsel);
static void gtk_color_selection_draw_wheel_frame  (GtkColorSelection  *colorsel);
static void gtk_color_selection_draw_value_marker (GtkColorSelection  *colorsel);
static void gtk_color_selection_draw_value_bar    (GtkColorSelection  *colorsel,
                                                   gint                resize);
static void gtk_color_selection_draw_wheel        (GtkColorSelection  *colorsel,
                                                   gint                resize);
static void gtk_color_selection_draw_sample       (GtkColorSelection  *colorsel,
                                                   gint                resize);

static gint gtk_color_selection_eval_wheel        (gint     x, gint     y,
					           gdouble cx, gdouble cy,
					           gdouble *h, gdouble *s);

static void gtk_color_selection_hsv_to_rgb        (gdouble  h, gdouble  s, gdouble  v,
				                   gdouble *r, gdouble *g, gdouble *b);
static void gtk_color_selection_rgb_to_hsv        (gdouble  r, gdouble  g, gdouble  b,
				                   gdouble *h, gdouble *s, gdouble *v);

static void gtk_color_selection_dialog_destroy    (GtkObject *object);


static GtkVBoxClass *color_selection_parent_class = NULL;
static GtkWindowClass *color_selection_dialog_parent_class = NULL;


static gint color_selection_signals[LAST_SIGNAL] = {0};


#define SF GtkSignalFunc


scale_val_type scale_vals[NUM_CHANNELS] =
{
  {"Hue:",        0.0, 360.0, 1.00, 10.00, (SF) gtk_color_selection_hsv_updater},
  {"Saturation:", 0.0,   1.0, 0.01,  0.01, (SF) gtk_color_selection_hsv_updater},
  {"Value:",      0.0,   1.0, 0.01,  0.01, (SF) gtk_color_selection_hsv_updater},
  {"Red:",        0.0,   1.0, 0.01,  0.01, (SF) gtk_color_selection_rgb_updater},
  {"Green:",      0.0,   1.0, 0.01,  0.01, (SF) gtk_color_selection_rgb_updater},
  {"Blue:",       0.0,   1.0, 0.01,  0.01, (SF) gtk_color_selection_rgb_updater},
  {"Opacity:",    0.0,   1.0, 0.01,  0.01, (SF) gtk_color_selection_opacity_updater}
};

guint
gtk_color_selection_get_type ()
{
  static guint color_selection_type = 0;

  if (!color_selection_type)
    {
      GtkTypeInfo colorsel_info =
      {
	"color selection widget",
	sizeof (GtkColorSelection),
	sizeof (GtkColorSelectionClass),
	(GtkClassInitFunc) gtk_color_selection_class_init,
	(GtkObjectInitFunc) gtk_color_selection_init,
      };

      color_selection_type = gtk_type_unique (gtk_vbox_get_type (), &colorsel_info);
    }

  return color_selection_type;
}

void
gtk_color_selection_class_init (GtkColorSelectionClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  color_selection_parent_class = gtk_type_class (gtk_vbox_get_type ());

  color_selection_signals[COLOR_CHANGED] =
     gtk_signal_new ("color_changed",
	             GTK_RUN_FIRST,
                     object_class->type,
		     GTK_SIGNAL_OFFSET (GtkColorSelectionClass, color_changed),
                     gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, color_selection_signals, LAST_SIGNAL);

  object_class->destroy = gtk_color_selection_destroy;

  widget_class->realize = gtk_color_selection_realize;
}

void
gtk_color_selection_init (GtkColorSelection *colorsel)
{
  GtkWidget *frame, *hbox, *vbox, *hbox2, *label, *table;
  GtkObject *adj;
  gint old_mask, n;
  gchar txt[32];

  for (n = RED; n <= OPACITY; n++)
    colorsel->values[n] = 1.0;

  RGB_TO_HSV ();

  for (n = HUE; n <= OPACITY; n++)
    colorsel->old_values[n] = colorsel->values[n];

  colorsel->wheel_gc = NULL;
  colorsel->value_gc = NULL;
  colorsel->sample_gc = NULL;
  colorsel->wheel_buf = NULL;
  colorsel->value_buf = NULL;
  colorsel->sample_buf = NULL;

  colorsel->use_opacity = FALSE;
  colorsel->timer_active = FALSE;
  colorsel->policy = GTK_UPDATE_CONTINUOUS;

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_border_width (GTK_CONTAINER (hbox), 5);
  gtk_container_add (GTK_CONTAINER (colorsel), hbox);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);
  gtk_widget_show (vbox);

  hbox2 = gtk_hbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (vbox), hbox2);
  gtk_widget_show (hbox2);

  colorsel->wheel_area = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_widget_set_events (colorsel->wheel_area,
			 old_mask |
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_BUTTON_MOTION_MASK |
			 GDK_POINTER_MOTION_HINT_MASK);
  gtk_preview_size (GTK_PREVIEW (colorsel->wheel_area), WHEEL_WIDTH, WHEEL_HEIGHT);
  gtk_preview_set_expand (GTK_PREVIEW (colorsel->wheel_area), TRUE);
  gtk_container_add (GTK_CONTAINER (hbox2), colorsel->wheel_area);
  gtk_widget_show (colorsel->wheel_area);

  old_mask = gtk_widget_get_events (colorsel->wheel_area);

  gtk_signal_connect (GTK_OBJECT (colorsel->wheel_area), "event",
    (SF) gtk_color_selection_wheel_events, (gpointer) colorsel->wheel_area);
  gtk_signal_connect_after (GTK_OBJECT (colorsel->wheel_area), "expose_event",
    (SF) gtk_color_selection_wheel_events, (gpointer) colorsel->wheel_area);
  gtk_signal_connect_after (GTK_OBJECT (colorsel->wheel_area), "size_allocate",
    (SF) gtk_color_selection_wheel_resize, (gpointer) colorsel->wheel_area);
  gtk_object_set_data (GTK_OBJECT (colorsel->wheel_area), "_GtkColorSelection", (gpointer) colorsel);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_border_width (GTK_CONTAINER (frame), 0);
  gtk_box_pack_start (GTK_BOX (hbox2), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  colorsel->value_area = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (colorsel->value_area), VALUE_WIDTH, VALUE_HEIGHT);
  gtk_preview_set_expand (GTK_PREVIEW (colorsel->value_area), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), colorsel->value_area);
  gtk_widget_show (colorsel->value_area);

  old_mask = gtk_widget_get_events (colorsel->value_area);
  gtk_widget_set_events (colorsel->value_area,
			 old_mask |
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_BUTTON_MOTION_MASK |
			 GDK_POINTER_MOTION_HINT_MASK);

  gtk_signal_connect_after (GTK_OBJECT (colorsel->value_area), "expose_event",
    (SF) gtk_color_selection_value_events, (gpointer) colorsel->value_area);
  gtk_signal_connect_after (GTK_OBJECT (colorsel->value_area), "size_allocate",
    (SF) gtk_color_selection_value_resize, (gpointer) colorsel->value_area);
  gtk_signal_connect (GTK_OBJECT (colorsel->value_area), "event",
    (SF) gtk_color_selection_value_events, (gpointer) colorsel->value_area);
  gtk_object_set_data (GTK_OBJECT (colorsel->value_area), "_GtkColorSelection", (gpointer) colorsel);

  /* New/old color samples */
  /* ===================== */

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

/*  colorsel->sample_area_eb = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (frame), colorsel->sample_area_eb);
  gtk_widget_show (colorsel->sample_area_eb); */

  colorsel->sample_area = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (colorsel->sample_area), SAMPLE_WIDTH, SAMPLE_HEIGHT);
  gtk_preview_set_expand (GTK_PREVIEW (colorsel->sample_area), TRUE);
  gtk_container_add (GTK_CONTAINER (frame),
		     colorsel->sample_area);
  gtk_widget_set_events(colorsel->sample_area,
		gtk_widget_get_events(colorsel->sample_area)
		| GDK_BUTTON_MOTION_MASK
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_ENTER_NOTIFY_MASK
		| GDK_LEAVE_NOTIFY_MASK);
  gtk_widget_show (colorsel->sample_area);

  gtk_signal_connect_after (GTK_OBJECT (colorsel->sample_area),
			    "size_allocate",
			    GTK_SIGNAL_FUNC (gtk_color_selection_sample_resize),
			    colorsel->sample_area);
  gtk_object_set_data (GTK_OBJECT (colorsel->sample_area), "_GtkColorSelection", (gpointer) colorsel);

  table = gtk_table_new (NUM_CHANNELS, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, TRUE, 0);

  for (n = HUE; n <= OPACITY; n++)
    {
      label = gtk_label_new (scale_vals[n].label);
      gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
      gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, n, n + 1);

      adj = gtk_adjustment_new (colorsel->values[n], scale_vals[n].lower,
				scale_vals[n].upper, scale_vals[n].step_inc,
                                scale_vals[n].page_inc, 0.0);
      colorsel->scales[n] = gtk_hscale_new (GTK_ADJUSTMENT (adj));
      gtk_widget_set_usize (colorsel->scales[n], 128, 0);
      gtk_scale_set_value_pos (GTK_SCALE (colorsel->scales[n]), GTK_POS_TOP);

      gtk_range_set_update_policy (GTK_RANGE (colorsel->scales[n]), colorsel->policy);
      gtk_scale_set_draw_value (GTK_SCALE (colorsel->scales[n]), FALSE);
      gtk_scale_set_digits (GTK_SCALE (colorsel->scales[n]), 2);
      gtk_table_attach_defaults (GTK_TABLE (table), colorsel->scales[n], 1, 2, n, n + 1);

      colorsel->entries[n] = gtk_entry_new ();
      gtk_widget_set_usize (colorsel->entries[n], 40, 0);
      sprintf (txt, "%.2f", colorsel->values[n]);
      gtk_entry_set_text (GTK_ENTRY (colorsel->entries[n]), txt);
      gtk_table_attach_defaults (GTK_TABLE (table), colorsel->entries[n], 2, 3, n, n + 1);

      if (n != OPACITY)
	{
	  gtk_widget_show (label);
	  gtk_widget_show (colorsel->scales[n]);
	  gtk_widget_show (colorsel->entries[n]);
	}

      gtk_signal_connect_object (GTK_OBJECT (adj), "value_changed",
                                 scale_vals[n].updater, (gpointer) colorsel->scales[n]);
      gtk_object_set_data (GTK_OBJECT (colorsel->scales[n]), "_GtkColorSelection", (gpointer) colorsel);
      gtk_object_set_data (GTK_OBJECT (colorsel->scales[n]), "_ValueIndex", (gpointer) n);
      gtk_signal_connect_object (GTK_OBJECT (colorsel->entries[n]), "changed",
                                 scale_vals[n].updater, (gpointer) colorsel->entries[n]);
      gtk_object_set_data (GTK_OBJECT (colorsel->entries[n]), "_GtkColorSelection", (gpointer) colorsel);
      gtk_object_set_data (GTK_OBJECT (colorsel->entries[n]), "_ValueIndex", (gpointer) n);
    }

  colorsel->opacity_label = label;

  gtk_widget_show (table);
  gtk_widget_show (hbox);
}

GtkWidget *
gtk_color_selection_new (void)
{
  GtkColorSelection *colorsel;

  colorsel = gtk_type_new (gtk_color_selection_get_type ());

  return GTK_WIDGET (colorsel);
}

void
gtk_color_selection_set_update_policy (GtkColorSelection *colorsel,
				       GtkUpdateType      policy)
{
  gint n;

  g_return_if_fail (colorsel != NULL);

  if (policy != colorsel->policy)
    {
      colorsel->policy = policy;

      for (n = 0; n < NUM_CHANNELS; n++)
	gtk_range_set_update_policy (GTK_RANGE (colorsel->scales[n]), policy);
    }
}


void
gtk_color_selection_set_color (GtkColorSelection *colorsel,
			       gdouble           *color)
{
  gint n, i = 0;

  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));

  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (colorsel)))
    gtk_color_selection_draw_wheel_marker (colorsel);

  for (n = RED; n <= BLUE; n++)
    {
      colorsel->old_values[n] = colorsel->values[n];
      colorsel->values[n] = color[i++];
    }

  if (colorsel->use_opacity == TRUE)
    {
      colorsel->old_values[OPACITY] = colorsel->values[OPACITY];
      colorsel->values[OPACITY] = color[i];
    }

  RGB_TO_HSV ();

  gtk_color_selection_update_inputs (colorsel, RGB_INPUTS | HSV_INPUTS | OPACITY_INPUTS, BOTH);

  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (colorsel)))
    {
      gtk_color_selection_draw_value_bar (colorsel, FALSE);
      gtk_color_selection_draw_sample (colorsel, FALSE);
      gtk_color_selection_draw_wheel_marker (colorsel);
    }
}

void
gtk_color_selection_get_color (GtkColorSelection *colorsel,
			       gdouble           *color)
{
  gint n, i = 0;

  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));

  for (n = RED; n <= BLUE; n++)
    color[i++] = colorsel->values[n];
  if (colorsel->use_opacity == TRUE)
    color[i] = colorsel->values[OPACITY];
}

static void
gtk_color_selection_realize (GtkWidget         *widget)
{
  GtkColorSelection *colorsel;
  gchar *type_accept_list[] = {"application/x-color"};

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (widget));

  colorsel = GTK_COLOR_SELECTION (widget);

  if (GTK_WIDGET_CLASS (color_selection_parent_class)->realize)
    (*GTK_WIDGET_CLASS (color_selection_parent_class)->realize) (widget);

  gtk_widget_dnd_drag_set (colorsel->sample_area,
			   1, type_accept_list, 1);
  gtk_widget_dnd_drop_set (colorsel->sample_area,
			   1, type_accept_list, 1, 0);
  gtk_signal_connect_after (GTK_OBJECT (colorsel->sample_area),
			    "drop_data_available_event",
			    GTK_SIGNAL_FUNC (gtk_color_selection_drop_handle),
			    NULL);
  gtk_signal_connect_after (GTK_OBJECT (colorsel->sample_area),
			    "drag_request_event",
			    GTK_SIGNAL_FUNC (gtk_color_selection_drag_handle),
			    NULL);
}

static void
gtk_color_selection_destroy (GtkObject *object)
{
  GtkColorSelection *colorsel;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (object));

  colorsel = GTK_COLOR_SELECTION (object);

  if (colorsel->wheel_buf != NULL)
    g_free (colorsel->wheel_buf);
  if (colorsel->value_buf != NULL)
    g_free (colorsel->value_buf);
  if (colorsel->sample_buf != NULL)
    g_free (colorsel->sample_buf);

  if (GTK_OBJECT_CLASS (color_selection_parent_class)->destroy)
    (*GTK_OBJECT_CLASS (color_selection_parent_class)->destroy) (object);
}

static void
gtk_color_selection_color_changed (GtkColorSelection *colorsel)
{
  gtk_signal_emit (GTK_OBJECT (colorsel), color_selection_signals[COLOR_CHANGED]);
}

static void
gtk_color_selection_update_input (GtkWidget *scale,
                                  GtkWidget *entry,
                                  gdouble    value)
{
  GtkAdjustment *adj;
  gchar txt[32];

  if (scale != NULL)
    {
      adj = gtk_range_get_adjustment (GTK_RANGE (scale));
      adj->value = (gfloat) value;
      gtk_signal_handler_block_by_data (GTK_OBJECT (adj), (gpointer) scale);
      gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
      gtk_range_slider_update (GTK_RANGE (scale));
      gtk_signal_handler_unblock_by_data (GTK_OBJECT (adj), (gpointer) scale);
    }

  if (entry != NULL)
    {
      gtk_signal_handler_block_by_data (GTK_OBJECT (entry), (gpointer) entry);
      sprintf (txt, "%.2f", value);
      gtk_entry_set_text (GTK_ENTRY (entry), txt);
      gtk_signal_handler_unblock_by_data (GTK_OBJECT (entry), (gpointer) entry);
    }
}

static void
gtk_color_selection_update_inputs (GtkColorSelection *colorsel,
                                   gint               inputs,
                                   gint               which)
{
  gint n;

  switch (which)
    {
    case SCALE:
      if ((inputs & RGB_INPUTS) != 0)
	for (n = RED; n <= BLUE; n++)
	  gtk_color_selection_update_input (colorsel->scales[n], NULL,
					    colorsel->values[n]);
      if ((inputs & HSV_INPUTS) != 0)
	for (n = HUE; n <= VALUE; n++)
	  gtk_color_selection_update_input (colorsel->scales[n], NULL,
					    colorsel->values[n]);
      if ((inputs & OPACITY_INPUTS) != 0)
	gtk_color_selection_update_input(colorsel->scales[OPACITY], NULL,
					 colorsel->values[OPACITY]);
      break;
    case ENTRY:
      if ((inputs & RGB_INPUTS) != 0)
	for (n = RED; n <= BLUE; n++)
	  gtk_color_selection_update_input (NULL, colorsel->entries[n], colorsel->values[n]);
      if ((inputs & HSV_INPUTS) != 0)
	for (n = HUE; n <= VALUE; n++)
	  gtk_color_selection_update_input (NULL, colorsel->entries[n], colorsel->values[n]);
      if ((inputs & OPACITY_INPUTS) != 0)
	gtk_color_selection_update_input(NULL, colorsel->entries[OPACITY], colorsel->values[OPACITY]);
      break;
    default:
      if ((inputs & RGB_INPUTS) != 0)
	for (n = RED; n <= BLUE; n++)
	  gtk_color_selection_update_input (colorsel->scales[n], colorsel->entries[n],
                                            colorsel->values[n]);
      if ((inputs & HSV_INPUTS) != 0)
	for (n = HUE; n <= VALUE; n++)
	  gtk_color_selection_update_input (colorsel->scales[n], colorsel->entries[n],
                                            colorsel->values[n]);
      if ((inputs & OPACITY_INPUTS) != 0)
	gtk_color_selection_update_input(colorsel->scales[OPACITY], colorsel->entries[OPACITY],
					 colorsel->values[OPACITY]);
      break;
    }
}

static void
gtk_color_selection_hsv_updater (GtkWidget *widget,
                                 gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkAdjustment *adj;
  gdouble newvalue;
  gint i, which = SCALE;

  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (widget)))
    {
      colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (widget), "_GtkColorSelection");
      i = (gint) gtk_object_get_data (GTK_OBJECT (widget), "_ValueIndex");

      if (GTK_IS_SCALE (widget))
	{
	  adj = gtk_range_get_adjustment (GTK_RANGE (GTK_SCALE (widget)));
	  newvalue = (gdouble) adj->value;
	  which = ENTRY;
	}
      else
	newvalue = (gdouble) atof (gtk_entry_get_text (GTK_ENTRY (widget)));

      if (i == VALUE)
	{
	  gtk_color_selection_draw_value_marker (colorsel);
	  colorsel->values[i] = newvalue;

	  HSV_TO_RGB ();

	  gtk_color_selection_draw_value_marker (colorsel);
	}
      else
	{
	  gtk_color_selection_draw_wheel_marker (colorsel);
	  colorsel->values[i] = newvalue;

	  HSV_TO_RGB ();

	  gtk_color_selection_draw_wheel_marker (colorsel);
	  gtk_color_selection_draw_value_bar (colorsel, FALSE);
	}

      gtk_color_selection_draw_sample (colorsel, FALSE);
      gtk_color_selection_color_changed (colorsel);
      gtk_color_selection_update_inputs (colorsel, HSV_INPUTS, which);
      gtk_color_selection_update_inputs (colorsel, RGB_INPUTS, BOTH);
    }
}

static void
gtk_color_selection_rgb_updater (GtkWidget *widget,
                                 gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkAdjustment *adj;
  gdouble newvalue;
  gint i, which = SCALE;

  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (widget)))
    {
      colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (widget), "_GtkColorSelection");
      i = (gint) gtk_object_get_data (GTK_OBJECT (widget), "_ValueIndex");

      if (GTK_IS_SCALE (widget))
	{
	  adj = gtk_range_get_adjustment (GTK_RANGE (GTK_SCALE (widget)));
	  newvalue = (gdouble) adj->value;
	  which = ENTRY;
	}
      else
	newvalue = (gdouble) atof (gtk_entry_get_text (GTK_ENTRY (widget)));

      gtk_color_selection_draw_wheel_marker (colorsel);

      colorsel->values[i] = newvalue;
      RGB_TO_HSV ();

      gtk_color_selection_draw_wheel_marker (colorsel);
      gtk_color_selection_draw_value_bar (colorsel, FALSE);
      gtk_color_selection_draw_sample (colorsel, FALSE);
      gtk_color_selection_color_changed (colorsel);
      gtk_color_selection_update_inputs (colorsel, RGB_INPUTS, which);
      gtk_color_selection_update_inputs (colorsel, HSV_INPUTS, BOTH);
    }
}

static void
gtk_color_selection_opacity_updater (GtkWidget *widget,
                                     gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkAdjustment *adj;

  colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (widget), "_GtkColorSelection");

  if (GTK_IS_SCALE (widget))
    {
      adj = gtk_range_get_adjustment (GTK_RANGE (widget));
      colorsel->values[OPACITY] = (gdouble) adj->value;
      gtk_color_selection_update_input (NULL, colorsel->entries[OPACITY], colorsel->values[OPACITY]);
    }
  else
    {
      colorsel->values[OPACITY] = (gdouble) atof (gtk_entry_get_text (GTK_ENTRY (widget)));
      gtk_color_selection_update_input (colorsel->scales[OPACITY], NULL, colorsel->values[OPACITY]);
    }

  gtk_color_selection_draw_sample (colorsel, FALSE);
  gtk_color_selection_color_changed (colorsel);
}

void
gtk_color_selection_set_opacity (GtkColorSelection *colorsel,
                                 gint               use_opacity)
{
  g_return_if_fail (colorsel != NULL);

  colorsel->use_opacity = use_opacity;

  if (use_opacity == FALSE && GTK_WIDGET_VISIBLE (colorsel->scales[OPACITY]))
    {
      gtk_widget_hide (colorsel->opacity_label);
      gtk_widget_hide (colorsel->scales[OPACITY]);
      gtk_widget_hide (colorsel->entries[OPACITY]);
    }
  else if (use_opacity == TRUE && !GTK_WIDGET_VISIBLE (colorsel->scales[OPACITY]))
    {
      gtk_widget_show (colorsel->opacity_label);
      gtk_widget_show (colorsel->scales[OPACITY]);
      gtk_widget_show (colorsel->entries[OPACITY]);
    }

  if (GTK_WIDGET_DRAWABLE (colorsel->sample_area))
    gtk_color_selection_draw_sample (colorsel, FALSE);
}

static void
gtk_color_selection_value_resize (GtkWidget *widget,
                                  gpointer   data)
{
  GtkColorSelection *colorsel;

  colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (widget), "_GtkColorSelection");
  gtk_color_selection_draw_value_bar (colorsel, TRUE);
}

static void
gtk_color_selection_wheel_resize (GtkWidget *widget,
                                  gpointer   data)
{
  GtkColorSelection *colorsel;

  colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (widget), "_GtkColorSelection");
  gtk_color_selection_draw_wheel (colorsel, TRUE);
}

static void
gtk_color_selection_sample_resize (GtkWidget *widget,
                                   gpointer   data)
{
  GtkColorSelection *colorsel;

  colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (widget), "_GtkColorSelection");
  gtk_color_selection_draw_sample (colorsel, TRUE);
}

static void
gtk_color_selection_drop_handle (GtkWidget *widget, GdkEvent *event)
{
  int i;
  GtkColorSelection *w;
  gdouble *newbuf;
  g_print("Handling drop in color selection\n");
  gtk_color_selection_set_color(GTK_COLOR_SELECTION(widget),
				event->dropdataavailable.data);
  g_free(event->dropdataavailable.data);
  g_free(event->dropdataavailable.data_type);
}

static void
gtk_color_selection_drag_handle (GtkWidget *widget, GdkEvent *event)
{
  g_print("Handling drag in color selector\n");
  gtk_widget_dnd_data_set(widget, event, GTK_COLOR_SELECTION(widget)->values,
			  sizeof(GTK_COLOR_SELECTION(widget)->values));
}

static void
gtk_color_selection_draw_wheel_marker (GtkColorSelection *colorsel)
{
  gint xpos, ypos;

  gdk_gc_set_function (colorsel->wheel_gc, GDK_INVERT);

  xpos = (gint) ((-(gdouble) (colorsel->wheel_area->allocation.width) / 2.0) *
		 colorsel->values[SATURATION] * cos (DEGTORAD ((colorsel->values[HUE] - 90)))) +
                 (colorsel->wheel_area->allocation.width >> 1) - 4;
  ypos = (gint) (((gdouble) (colorsel->wheel_area->allocation.height) / 2.0) *
		 colorsel->values[SATURATION] * sin (DEGTORAD ((colorsel->values[HUE] - 90)))) +
                 (colorsel->wheel_area->allocation.height >> 1) - 4;

  gdk_draw_arc (colorsel->wheel_area->window, colorsel->wheel_gc, FALSE, xpos, ypos, 8, 8, 0, 360 * 64);
}

static void
gtk_color_selection_draw_value_marker (GtkColorSelection *colorsel)
{
  gint y;

  gdk_gc_set_function (colorsel->value_gc, GDK_INVERT);

  y = (gint) ((gdouble) (colorsel->value_area->allocation.height) * (1.0 - colorsel->values[VALUE]));
  gdk_draw_line (colorsel->value_area->window, colorsel->value_gc,
                 0, y, colorsel->value_area->allocation.width, y);
}

static void
gtk_color_selection_update_value (GtkColorSelection *colorsel,
                                  gint               y)
{
  gtk_color_selection_draw_value_marker (colorsel);

  if (y < 0)
    y = 0;
  else if (y > colorsel->value_area->allocation.height - 1)
    y = colorsel->value_area->allocation.height - 1;

  colorsel->values[VALUE] = 1.0 - (gdouble) y / (gdouble) (colorsel->value_area->allocation.height);

  HSV_TO_RGB ();

  gtk_color_selection_draw_value_marker (colorsel);
  gtk_color_selection_draw_sample (colorsel, FALSE);
  gtk_color_selection_update_input (colorsel->scales[VALUE], colorsel->entries[VALUE],
				    colorsel->values[VALUE]);
  gtk_color_selection_update_inputs (colorsel, RGB_INPUTS, BOTH);
}

static void
gtk_color_selection_update_wheel (GtkColorSelection *colorsel,
                                  gint               x,
                                  gint               y)
{
  gdouble wid, heig;
  gint res;

  gtk_color_selection_draw_wheel_marker (colorsel);

  wid = (gdouble) (colorsel->wheel_area->allocation.width) / 2.0;
  heig = (gdouble) (colorsel->wheel_area->allocation.height) / 2.0;

  res = gtk_color_selection_eval_wheel (x, y, wid, heig, &colorsel->values[HUE],
                                        &colorsel->values[SATURATION]);

  HSV_TO_RGB ();

  gtk_color_selection_draw_wheel_marker (colorsel);
  gtk_color_selection_draw_value_bar (colorsel, FALSE);
  gtk_color_selection_draw_sample (colorsel, FALSE);
  gtk_color_selection_update_inputs (colorsel, RGB_INPUTS | HSV_INPUTS, BOTH);
}

static gint
gtk_color_selection_value_timeout (GtkColorSelection *colorsel)
{
  gint x, y;

  gdk_window_get_pointer (colorsel->value_area->window, &x, &y, NULL);
  gtk_color_selection_update_value (colorsel, y);
  gtk_color_selection_color_changed (colorsel);

  return (TRUE);
}

static gint
gtk_color_selection_value_events (GtkWidget *area,
                                  GdkEvent  *event)
{
  GtkColorSelection *colorsel;
  gint y;

  colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (area), "_GtkColorSelection");

  switch (event->type)
    {
    case GDK_MAP:
      gtk_color_selection_draw_value_marker (colorsel);
      break;
    case GDK_EXPOSE:
      if (colorsel->value_gc == NULL)
	colorsel->value_gc = gdk_gc_new (colorsel->value_area->window);
      gtk_color_selection_draw_value_marker (colorsel);
      break;
    case GDK_BUTTON_PRESS:
      gtk_grab_add (area);
      gtk_color_selection_update_value (colorsel, event->button.y);
      gtk_color_selection_color_changed (colorsel);
      break;
    case GDK_BUTTON_RELEASE:
      gtk_grab_remove (area);
      if (colorsel->timer_active == TRUE)
	gtk_timeout_remove (colorsel->timer_tag);
      colorsel->timer_active = FALSE;

      y = event->button.y;
      if (event->button.window != area->window)
	gdk_window_get_pointer (area->window, NULL, &y, NULL);

      gtk_color_selection_update_value (colorsel, y);
      gtk_color_selection_color_changed (colorsel);
      break;
    case GDK_MOTION_NOTIFY:
      y = event->motion.y;

      if (event->motion.is_hint || (event->motion.window != area->window))
	gdk_window_get_pointer (area->window, NULL, &y, NULL);

      switch (colorsel->policy)
	{
	case GTK_UPDATE_CONTINUOUS:
	  gtk_color_selection_update_value (colorsel, y);
	  gtk_color_selection_color_changed (colorsel);
	  break;
	case GTK_UPDATE_DELAYED:
	  if (colorsel->timer_active == TRUE)
	    gtk_timeout_remove (colorsel->timer_tag);

	  colorsel->timer_tag = gtk_timeout_add (TIMER_DELAY,
						 (GtkFunction) gtk_color_selection_value_timeout,
						 (gpointer) colorsel);
	  colorsel->timer_active = TRUE;
	  break;
	default:
	  break;
	}
      break;
    default:
      break;
    }

  return (FALSE);
}

static gint
gtk_color_selection_wheel_timeout (GtkColorSelection *colorsel)
{
  gint x, y;

  gdk_window_get_pointer (colorsel->wheel_area->window, &x, &y, NULL);
  gtk_color_selection_update_wheel (colorsel, x, y);
  gtk_color_selection_color_changed (colorsel);

  return (TRUE);
}

static gint
gtk_color_selection_wheel_events (GtkWidget *area,
                                  GdkEvent  *event)
{
  GtkColorSelection *colorsel;
  gint x, y;

  colorsel = (GtkColorSelection*) gtk_object_get_data (GTK_OBJECT (area), "_GtkColorSelection");

  switch (event->type)
    {
    case GDK_MAP:
      gtk_color_selection_draw_wheel (colorsel, TRUE);
      gtk_color_selection_draw_wheel_marker (colorsel);
      gtk_color_selection_draw_sample (colorsel, TRUE);
      break;
    case GDK_EXPOSE:
      if (colorsel->wheel_gc == NULL)
	colorsel->wheel_gc = gdk_gc_new (colorsel->wheel_area->window);
      if (colorsel->sample_gc == NULL)
	colorsel->sample_gc = gdk_gc_new (colorsel->sample_area->window);
      if (colorsel->value_gc == NULL)
	colorsel->value_gc = gdk_gc_new (colorsel->value_area->window);
      gtk_color_selection_draw_wheel_marker (colorsel);
      gtk_color_selection_draw_wheel_frame (colorsel);
      break;
    case GDK_BUTTON_PRESS:
      gtk_grab_add (area);
      gtk_color_selection_update_wheel (colorsel, event->button.x, event->button.y);
      gtk_color_selection_color_changed (colorsel);
      break;
    case GDK_BUTTON_RELEASE:
      gtk_grab_remove (area);
      if (colorsel->timer_active == TRUE)
	gtk_timeout_remove (colorsel->timer_tag);
      colorsel->timer_active = FALSE;

      x = event->button.x;
      y = event->button.y;

      if (event->button.window != area->window)
	gdk_window_get_pointer (area->window, &x, &y, NULL);

      gtk_color_selection_update_wheel (colorsel, x, y);
      gtk_color_selection_color_changed (colorsel);
      break;
    case GDK_MOTION_NOTIFY:
      x = event->motion.x;
      y = event->motion.y;

      if (event->motion.is_hint || (event->motion.window != area->window))
	gdk_window_get_pointer (area->window, &x, &y, NULL);

      switch (colorsel->policy)
	{
	case GTK_UPDATE_CONTINUOUS:
	  gtk_color_selection_update_wheel (colorsel, x, y);
	  gtk_color_selection_color_changed (colorsel);
	  break;
	case GTK_UPDATE_DELAYED:
	  if (colorsel->timer_active == TRUE)
	    gtk_timeout_remove (colorsel->timer_tag);
	  colorsel->timer_tag = gtk_timeout_add (TIMER_DELAY,
						 (GtkFunction) gtk_color_selection_wheel_timeout,
						 (gpointer) colorsel);
	  colorsel->timer_active = TRUE;
	  break;
	default:
	  break;
	}
      break;
    default:
      break;
    }

  return (FALSE);
}

static void
gtk_color_selection_draw_value_bar (GtkColorSelection *colorsel,
                                    gint               resize)
{
  gint x, y, i, wid, heig, n;
  gdouble sv, v, c[3];
  guchar rc[3];

  wid = colorsel->value_area->allocation.width;
  heig = colorsel->value_area->allocation.height;

  if (resize)
    {
      if (colorsel->value_buf != NULL)
	g_free (colorsel->value_buf);

      colorsel->value_buf = g_new(guchar, 3 * wid);
    }

  v = 1.0;
  sv = 1.0 / (gdouble) (heig - 1);

  for (y = 0; y < heig; y++)
    {
      i = 0;

      gtk_color_selection_hsv_to_rgb (colorsel->values[HUE],colorsel->values[SATURATION],v,
                                      &c[0], &c[1], &c[2]);

      for (n = 0; n < 3; n++)
	rc[n] = (guchar) (255.0 * c[n]);

      for (x = 0; x < wid; x++)
	{
	  for (n = 0; n < 3; n++)
	    colorsel->value_buf[i++] = rc[n];
	}

      gtk_preview_draw_row (GTK_PREVIEW (colorsel->value_area), colorsel->value_buf, 0, y, wid);
      v -= sv;
    }

  gtk_widget_draw (colorsel->value_area, NULL);
}

static void
gtk_color_selection_draw_wheel_frame (GtkColorSelection *colorsel)
{
  GtkStyle *style;
  gint w, h;

  style = gtk_widget_get_style (GTK_WIDGET (colorsel));

  w = colorsel->wheel_area->allocation.width;
  h = colorsel->wheel_area->allocation.height;

  gdk_draw_arc (colorsel->wheel_area->window, style->black_gc,
		FALSE, 1, 1, w - 1, h - 1, 30 * 64, 180 * 64);
  gdk_draw_arc (colorsel->wheel_area->window, style->mid_gc[GTK_STATE_NORMAL],
		FALSE, 0, 0, w, h, 30 * 64, 180 * 64);

  gdk_draw_arc (colorsel->wheel_area->window, style->bg_gc[GTK_STATE_NORMAL],
		FALSE, 1, 1, w - 1, h - 1, 210 * 64, 180 * 64);
  gdk_draw_arc (colorsel->wheel_area->window, style->light_gc[GTK_STATE_NORMAL],
		FALSE, 0, 0, w, h, 210 * 64, 180 * 64);
}

static void
gtk_color_selection_draw_wheel (GtkColorSelection *colorsel,
                                gint               resize)
{
  gint x, y, i, wid, heig, n;
  gdouble cx, cy, h, s, c[3];
  guchar bg[3];
  GtkStyle *style = gtk_widget_get_style (GTK_WIDGET (colorsel));

  wid = colorsel->wheel_area->allocation.width;
  heig = colorsel->wheel_area->allocation.height;

  if (resize)
    {
      if (colorsel->wheel_buf != NULL)
	g_free (colorsel->wheel_buf);

      colorsel->wheel_buf = g_new(guchar, 3 * wid);
    }

  cx = (gdouble) (wid) / 2.0;
  cy = (gdouble) (heig) / 2.0;

  bg[0] = style->bg[GTK_STATE_NORMAL].red >> 8;
  bg[1] = style->bg[GTK_STATE_NORMAL].green >> 8;
  bg[2] = style->bg[GTK_STATE_NORMAL].blue >> 8;

  for (y = 0; y < heig; y++)
    {
      i = 0;
      for (x = 0; x < wid; x++)
	{
	  if (gtk_color_selection_eval_wheel (x, y, cx, cy, &h, &s) == TRUE)
	    {
	      for (n = 0; n < 3; n++)
		colorsel->wheel_buf[i++] = bg[n];
	    }
	  else
	    {
	      gtk_color_selection_hsv_to_rgb (h, s, 1.0, &c[0], &c[1], &c[2]);
	      for (n = 0; n < 3; n++)
		colorsel->wheel_buf[i++] = (guchar) (255.0 * c[n]);
	    }
	}

      gtk_preview_draw_row (GTK_PREVIEW (colorsel->wheel_area), colorsel->wheel_buf, 0, y, wid);
    }

  gtk_widget_draw (colorsel->wheel_area, NULL);
}

static void
gtk_color_selection_draw_sample (GtkColorSelection *colorsel,
                                 gint               resize)
{
  gint x, y, i, wid, heig, f, half, n;
  guchar c[3 * 2], cc[3 * 4], *cp = c;
  gdouble o, oldo;

  wid = colorsel->sample_area->allocation.width;
  heig = colorsel->sample_area->allocation.height;
  half = wid >> 1;

  if (resize)
    {
      if (colorsel->sample_buf != NULL)
	g_free (colorsel->sample_buf);

      colorsel->sample_buf = g_new(guchar, 3 * wid);
    }

  i = RED;
  for (n = 0; n < 3; n++)
    {
      c[n] = (guchar) (255.0 * colorsel->old_values[i]);
      c[n + 3] = (guchar) (255.0 * colorsel->values[i++]);
    }

  if (colorsel->use_opacity == TRUE)
    {
      o = colorsel->values[OPACITY];
      oldo = colorsel->old_values[OPACITY];

      for (n = 0; n < 3; n++)
	{
	  cc[n] = (guchar) ((1.0 - oldo) * 192 + (oldo * (gdouble) c[n]));
	  cc[n + 3] = (guchar) ((1.0 - oldo) * 128 + (oldo * (gdouble) c[n]));
	  cc[n + 6] = (guchar) ((1.0 - o) * 192 + (o * (gdouble) c[n + 3]));
	  cc[n + 9] = (guchar) ((1.0 - o) * 128 + (o * (gdouble) c[n + 3]));
	}
      cp = cc;
    }

  for (y = 0; y < heig; y++)
    {
      i = 0;
      for (x = 0; x < wid; x++)
	{
	  if (colorsel->use_opacity)
	    {
	      f = 3 * (((x % 32) < 16) ^ ((y % 32) < 16));
	      f += (x > half) * 6;
	    }
	  else
	    f = (x > half) * 3;

	  for (n = 0; n < 3; n++)
	    colorsel->sample_buf[i++] = cp[n + f];
	}

      gtk_preview_draw_row (GTK_PREVIEW (colorsel->sample_area), colorsel->sample_buf, 0, y, wid);
    }

  gtk_widget_draw (colorsel->sample_area, NULL);
}

static gint
gtk_color_selection_eval_wheel (gint     x,  gint     y,
				gdouble  cx, gdouble  cy,
				gdouble *h,  gdouble *s)
{
  gdouble d, r, rx, ry, l;

  rx = (gdouble) x - cx;
  ry = (gdouble) y - cy;

  d = (SQR (cy) * SQR (rx) + SQR (cx) * SQR (ry) - SQR (cx) * SQR (cy));

  r = sqrt (SQR (rx) + SQR (ry));

  if (r != 0.0)
    *h = atan2 (rx / r, ry / r);
  else
    *h = 0.0;

  l = sqrt (SQR ((cx * cos (*h + 0.5 * M_PI))) + SQR ((cy * sin (*h + 0.5 * M_PI))));
  *s = r / l;
  *h = 360.0 * (*h) / (2.0 * M_PI) + 180;

  if (*s == 0.0)
    *s = 0.00001;
  else if (*s > 1.0)
    *s = 1.0;

  return ((d > 0.0));
}

static void
gtk_color_selection_hsv_to_rgb (gdouble  h, gdouble  s, gdouble  v,
				gdouble *r, gdouble *g, gdouble *b)
{
  gint i;
  gdouble f, w, q, t;

  if (s == 0.0)
    s = 0.000001;

  if (h == -1.0)
    {
      *r = v;
      *g = v;
      *b = v;
    }
  else
    {
      if (h == 360.0)
	h = 0.0;
      h = h / 60.0;
      i = (gint) h;
      f = h - i;
      w = v * (1.0 - s);
      q = v * (1.0 - (s * f));
      t = v * (1.0 - (s * (1.0 - f)));

      switch (i)
	{
	case 0:
	  *r = v;
	  *g = t;
	  *b = w;
	  break;
	case 1:
	  *r = q;
	  *g = v;
	  *b = w;
	  break;
	case 2:
	  *r = w;
	  *g = v;
	  *b = t;
	  break;
	case 3:
	  *r = w;
	  *g = q;
	  *b = v;
	  break;
	case 4:
	  *r = t;
	  *g = w;
	  *b = v;
	  break;
	case 5:
	  *r = v;
	  *g = w;
	  *b = q;
	  break;
	}
    }
}

static void
gtk_color_selection_rgb_to_hsv (gdouble  r, gdouble  g, gdouble  b,
				gdouble *h, gdouble *s, gdouble *v)
{
  double max, min, delta;

  max = r;
  if (g > max)
    max = g;
  if (b > max)
    max = b;

  min = r;
  if (g < min)
    min = g;
  if (b < min)
    min = b;

  *v = max;

  if (max != 0.0)
    *s = (max - min) / max;
  else
    *s = 0.0;

  if (*s == 0.0)
    *h = -1.0;
  else
    {
      delta = max - min;

      if (r == max)
	*h = (g - b) / delta;
      else if (g == max)
	*h = 2.0 + (b - r) / delta;
      else if (b == max)
	*h = 4.0 + (r - g) / delta;

      *h = *h * 60.0;

      if (*h < 0.0)
	*h = *h + 360;
    }
}

/***************************/
/* GtkColorSelectionDialog */
/***************************/

guint
gtk_color_selection_dialog_get_type ()
{
  static guint color_selection_dialog_type = 0;

  if (!color_selection_dialog_type)
    {
      GtkTypeInfo colorsel_diag_info =
      {
	"color selection dialog",
	sizeof (GtkColorSelectionDialog),
	sizeof (GtkColorSelectionDialogClass),
	(GtkClassInitFunc) gtk_color_selection_dialog_class_init,
	(GtkObjectInitFunc) gtk_color_selection_dialog_init,
      };

      color_selection_dialog_type = gtk_type_unique (gtk_window_get_type (), &colorsel_diag_info);
    }

  return color_selection_dialog_type;
}

void
gtk_color_selection_dialog_class_init (GtkColorSelectionDialogClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  color_selection_dialog_parent_class = gtk_type_class (gtk_window_get_type ());

  object_class->destroy = gtk_color_selection_dialog_destroy;
}

void
gtk_color_selection_dialog_init (GtkColorSelectionDialog *colorseldiag)
{
  GtkWidget *action_area, *frame;

  colorseldiag->main_vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_border_width (GTK_CONTAINER (colorseldiag), 10);
  gtk_container_add (GTK_CONTAINER (colorseldiag), colorseldiag->main_vbox);
  gtk_widget_show (colorseldiag->main_vbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (colorseldiag->main_vbox), frame);
  gtk_widget_show (frame);

  colorseldiag->colorsel = gtk_color_selection_new ();
  gtk_container_add (GTK_CONTAINER (frame), colorseldiag->colorsel);
  gtk_widget_show (colorseldiag->colorsel);

  action_area = gtk_hbox_new (TRUE, 10);
  gtk_box_pack_end (GTK_BOX (colorseldiag->main_vbox), action_area, FALSE, FALSE, 0);
  gtk_widget_show (action_area);

  colorseldiag->ok_button = gtk_button_new_with_label ("OK");
  GTK_WIDGET_SET_FLAGS (colorseldiag->ok_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (action_area), colorseldiag->ok_button, TRUE, TRUE, 0);
  gtk_widget_grab_default (colorseldiag->ok_button);
  gtk_widget_show (colorseldiag->ok_button);

  colorseldiag->cancel_button = gtk_button_new_with_label ("Cancel");
  GTK_WIDGET_SET_FLAGS (colorseldiag->cancel_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (action_area), colorseldiag->cancel_button, TRUE, TRUE, 0);
  gtk_widget_show (colorseldiag->cancel_button);

  colorseldiag->help_button = gtk_button_new_with_label ("Help");
  GTK_WIDGET_SET_FLAGS (colorseldiag->help_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (action_area), colorseldiag->help_button, TRUE, TRUE, 0);
  gtk_widget_show (colorseldiag->help_button);
}

GtkWidget *
gtk_color_selection_dialog_new (const gchar *title)
{
  GtkColorSelectionDialog *colorseldiag;

  colorseldiag = gtk_type_new (gtk_color_selection_dialog_get_type ());
  gtk_window_set_title (GTK_WINDOW (colorseldiag), title);

  return GTK_WIDGET (colorseldiag);
}

static void
gtk_color_selection_dialog_destroy (GtkObject *object)
{
  GtkColorSelectionDialog *colorseldiag;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION_DIALOG (object));

  colorseldiag = GTK_COLOR_SELECTION_DIALOG (object);


  if (GTK_OBJECT_CLASS (color_selection_dialog_parent_class)->destroy)
    (*GTK_OBJECT_CLASS (color_selection_dialog_parent_class)->destroy) (object);
}
