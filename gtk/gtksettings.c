/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "gtksettings.h"
#include "gtkrc.h"
#include "gtkintl.h"
#include "gtkwidget.h"

enum {
  PROP_0,
  PROP_DOUBLE_CLICK_TIME,
  PROP_CURSOR_BLINK,
  PROP_CURSOR_BLINK_TIME,
  PROP_SPLIT_CURSOR,
  PROP_THEME_NAME,
  PROP_KEY_THEME_NAME,
  PROP_MENU_BAR_ACCEL,
  PROP_DND_DRAG_THRESHOLD,
  PROP_FONT_NAME
};


/* --- prototypes --- */
static void	gtk_settings_init		 (GtkSettings		*settings);
static void	gtk_settings_class_init		 (GtkSettingsClass	*class);
static void	gtk_settings_finalize		 (GObject		*object);
static GObject*	gtk_settings_constructor	 (GType			 type,
						  guint			 n_construct_properties,
						  GObjectConstructParam	*construct_properties);
static void	gtk_settings_get_property	 (GObject		*object,
						  guint			 property_id,
						  GValue		*value,
						  GParamSpec		*pspec);
static void	gtk_settings_set_property	 (GObject		*object,
						  guint			 property_id,
						  const GValue		*value,
						  GParamSpec		*pspec);
static void	gtk_settings_notify		 (GObject		*object,
						  GParamSpec		*pspec);
static guint	settings_install_property_parser (GtkSettingsClass      *class,
						  GParamSpec            *pspec,
						  GtkRcPropertyParser    parser);


/* --- variables --- */
static gpointer		 parent_class = NULL;
static GtkSettings	*the_singleton = NULL;
static GQuark		 quark_property_parser = 0;
static GSList           *object_list = NULL;
static guint		 class_n_properties = 0;


/* --- functions --- */
GType
gtk_settings_get_type (void)
{
  static GType settings_type = 0;
  
  if (!settings_type)
    {
      static const GTypeInfo settings_info =
      {
	sizeof (GtkSettingsClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_settings_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkSettings),
	0,              /* n_preallocs */
	(GInstanceInitFunc) gtk_settings_init,
      };
      
      settings_type = g_type_register_static (G_TYPE_OBJECT, "GtkSettings", &settings_info, 0);
    }
  
  return settings_type;
}

static void
gtk_settings_init (GtkSettings *settings)
{
  GParamSpec **pspecs, **p;
  guint i = 0;
  
  g_datalist_init (&settings->queued_settings);
  object_list = g_slist_prepend (object_list, settings);

  /* build up property array for all yet existing properties and queue
   * notification for them (at least notification for internal properties
   * will instantly be caught)
   */
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), NULL);
  for (p = pspecs; *p; p++)
    if ((*p)->owner_type == G_OBJECT_TYPE (settings))
      i++;
  settings->property_values = g_new0 (GValue, i);
  i = 0;
  g_object_freeze_notify (G_OBJECT (settings));
  for (p = pspecs; *p; p++)
    {
      GParamSpec *pspec = *p;

      if (pspec->owner_type != G_OBJECT_TYPE (settings))
	continue;
      g_value_init (settings->property_values + i, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, settings->property_values + i);
      g_object_notify (G_OBJECT (settings), pspec->name);
      i++;
    }
  g_object_thaw_notify (G_OBJECT (settings));
  g_free (pspecs);
}

static void
gtk_settings_class_init (GtkSettingsClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  guint result;
  
  parent_class = g_type_class_peek_parent (class);

  gobject_class->constructor = gtk_settings_constructor;
  gobject_class->finalize = gtk_settings_finalize;
  gobject_class->get_property = gtk_settings_get_property;
  gobject_class->set_property = gtk_settings_set_property;
  gobject_class->notify = gtk_settings_notify;

  quark_property_parser = g_quark_from_static_string ("gtk-rc-property-parser");

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-double-click-time",
                                                               _("Double Click Time"),
                                                               _("Maximum time allowed between two clicks for them to be considered a double click (in milliseconds)"),
                                                               0, G_MAXINT, 250,
                                                               G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_DOUBLE_CLICK_TIME);
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-cursor-blink",
								   _("Cursor Blink"),
								   _("Whether the cursor should blink"),
								   TRUE,
								   G_PARAM_READWRITE),
					     NULL);
  g_assert (result == PROP_CURSOR_BLINK);
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-blink-time",
                                                               _("Cursor Blink Time"),
                                                               _("Length of the cursor blink cycle, in milleseconds"),
                                                               100, G_MAXINT, 1200,
                                                               G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_CURSOR_BLINK_TIME);
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-split-cursor",
								   _("Split Cursor"),
								   _("Whether two cursors should be displayed for mixed left-to-right and right-to-left text"),
								   TRUE,
								   G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_SPLIT_CURSOR);
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-theme-name",
								   _("Theme Name"),
								   _("Name of theme RC file to load"),
								  "Default",
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_THEME_NAME);
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-key-theme-name",
								  _("Key Theme Name"),
								  _("Name of key theme RC file to load"),
								  NULL,
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_KEY_THEME_NAME);    

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-menu-bar-accel",
                                                                  _("Menu bar accelerator"),
                                                                  _("Keybinding to activate the menu bar"),
                                                                  "F10",
                                                                  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_MENU_BAR_ACCEL);

  result = settings_install_property_parser (class,
					     g_param_spec_int ("gtk-dnd-drag-threshold",
							       _("Drag threshold"),
							       _("Number of pixels the cursor can move before dragging"),
							       1, G_MAXINT, 8,
                                                               G_PARAM_READWRITE),
					     NULL);
  g_assert (result == PROP_DND_DRAG_THRESHOLD);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-font-name",
								   _("Font Name"),
								   _("Name of default font to use"),
								  "Sans 10",
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_FONT_NAME);
 
}

static void
gtk_settings_finalize (GObject *object)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  guint i;

  object_list = g_slist_remove (object_list, settings);

  for (i = 0; i < class_n_properties; i++)
    g_value_unset (settings->property_values + i);
  g_free (settings->property_values);

  g_datalist_clear (&settings->queued_settings);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GObject*
gtk_settings_constructor (GType			 type,
			  guint		         n_construct_properties,
			  GObjectConstructParam *construct_properties)
{
  GObject *object;

  /* currently we're a singleton, that might change with multiple display support though */
  if (!the_singleton)
    {
      object = G_OBJECT_CLASS (parent_class)->constructor (type,
							   n_construct_properties,
							   construct_properties);
      the_singleton = GTK_SETTINGS (g_object_ref (object));
    }
  else
    object = g_object_ref (G_OBJECT (the_singleton));

  return object;
}

GtkSettings*
gtk_settings_get_default (void)
{
  if (!the_singleton)
    g_object_new (GTK_TYPE_SETTINGS, NULL);
  
  return the_singleton;	/* we don't add a reference count here, we'd be settings_ref_global() if we did */
}

static void
gtk_settings_set_property (GObject      *object,
			   guint	 property_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  
  g_value_copy (value, settings->property_values + property_id - 1);
}

static void
gtk_settings_get_property (GObject     *object,
			   guint	property_id,
			   GValue      *value,
			   GParamSpec  *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GType value_type = G_VALUE_TYPE (value);
  GType fundamental_type = G_TYPE_FUNDAMENTAL (value_type);

  /* For enums and strings, we need to get the value as a string,
   * not as an int, since we support using names/nicks as the setting
   * value.
   */
  if ((g_value_type_transformable (G_TYPE_INT, value_type) &&
       !(fundamental_type == G_TYPE_ENUM || fundamental_type == G_TYPE_FLAGS)) ||
      g_value_type_transformable (G_TYPE_STRING, G_VALUE_TYPE (value)) ||
      g_value_type_transformable (GDK_TYPE_COLOR, G_VALUE_TYPE (value)))
    {
      if (gdk_setting_get (pspec->name, value))
        g_param_value_validate (pspec, value);
      else
        g_value_copy (settings->property_values + property_id - 1, value);
    }
  else
    {
      GValue val = { 0, };

      /* Try to get xsetting as a string and parse it. */
      
      g_value_init (&val, G_TYPE_STRING);

      if (!gdk_setting_get (pspec->name, &val))
        {
          g_value_copy (settings->property_values + property_id - 1, value);
        }
      else
        {
          GValue tmp_value = { 0, };
          GValue gstring_value = { 0, };
          GtkRcPropertyParser parser = g_param_spec_get_qdata (pspec,
                                                               quark_property_parser);
          
          g_value_init (&gstring_value, G_TYPE_GSTRING);

          g_value_set_boxed (&gstring_value,
                             g_string_new (g_value_get_string (&val)));

          g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));

          if (parser && _gtk_settings_parse_convert (parser, &gstring_value,
                                                     pspec, &tmp_value))
            {
              g_value_copy (&tmp_value, value);
              g_param_value_validate (pspec, value);
            }
          else
            {
              g_value_copy (settings->property_values + property_id - 1, value);
            }

          g_value_unset (&gstring_value);
          g_value_unset (&tmp_value);
        }

      g_value_unset (&val);
    }
}

static void
gtk_settings_notify (GObject    *object,
		     GParamSpec *pspec)
{
  guint property_id = pspec->param_id;
  gint double_click_time;
  
#if 1
  GValue tmp_value = { 0, };
  gchar *contents;
  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (object, pspec->name, &tmp_value);
  contents = g_strdup_value_contents (&tmp_value);

  switch (property_id)
    {
    case PROP_DOUBLE_CLICK_TIME:
      g_object_get (object, pspec->name, &double_click_time, NULL);
      gdk_set_double_click_time (double_click_time);
      break;
    }

  g_free (contents);
  g_value_unset (&tmp_value);
#endif
}

gboolean
_gtk_settings_parse_convert (GtkRcPropertyParser parser,
			     const GValue       *src_value,
			     GParamSpec         *pspec,
			     GValue	        *dest_value)
{
  gboolean success = FALSE;

  g_return_val_if_fail (G_VALUE_HOLDS (dest_value, G_PARAM_SPEC_VALUE_TYPE (pspec)), FALSE);

  if (parser)
    {
      GString *gstring;
      gboolean free_gstring = TRUE;
      
      if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
	{
	  gstring = g_value_get_boxed (src_value);
	  free_gstring = FALSE;
	}
      else if (G_VALUE_HOLDS_LONG (src_value))
	{
	  gstring = g_string_new ("");
	  g_string_append_printf (gstring, "%ld", g_value_get_long (src_value));
	}
      else if (G_VALUE_HOLDS_DOUBLE (src_value))
	{
	  gstring = g_string_new ("");
	  g_string_append_printf (gstring, "%f", g_value_get_double (src_value));
	}
      else if (G_VALUE_HOLDS_STRING (src_value))
	{
	  gchar *tstr = g_strescape (g_value_get_string (src_value), NULL);
	  
	  gstring = g_string_new ("\"");
	  g_string_append (gstring, tstr);
	  g_string_append_c (gstring, '\"');
	  g_free (tstr);
	}
      else
	{
	  g_return_val_if_fail (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING), FALSE);
	  gstring = NULL; /* silence compiler */
	}

      success = (parser (pspec, gstring, dest_value) &&
		 !g_param_value_validate (pspec, dest_value));

      if (free_gstring)
	g_string_free (gstring, TRUE);
    }
  else if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
    {
      if (G_VALUE_HOLDS (dest_value, G_TYPE_STRING))
	{
	  GString *gstring = g_value_get_boxed (src_value);

	  g_value_set_string (dest_value, gstring ? gstring->str : NULL);
	  success = !g_param_value_validate (pspec, dest_value);
	}
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (src_value), G_VALUE_TYPE (dest_value)))
    success = g_param_value_convert (pspec, src_value, dest_value, TRUE);

  return success;
}

static void
apply_queued_setting (GtkSettings      *data,
		      GParamSpec       *pspec,
		      GtkSettingsValue *qvalue)
{
  GValue tmp_value = { 0, };
  GtkRcPropertyParser parser = g_param_spec_get_qdata (pspec, quark_property_parser);

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (_gtk_settings_parse_convert (parser, &qvalue->value,
				   pspec, &tmp_value))
    g_object_set_property (G_OBJECT (data), pspec->name, &tmp_value);
  else
    {
      gchar *debug = g_strdup_value_contents (&qvalue->value);
      
      g_message ("%s: failed to retrieve property `%s' of type `%s' from rc file value \"%s\" of type `%s'",
		 qvalue->origin,
		 pspec->name,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		 debug,
		 G_VALUE_TYPE_NAME (&tmp_value));
      g_free (debug);
    }
  g_value_unset (&tmp_value);
}

static guint
settings_install_property_parser (GtkSettingsClass   *class,
				  GParamSpec         *pspec,
				  GtkRcPropertyParser parser)
{
  GSList *node, *next;

  switch (G_TYPE_FUNDAMENTAL (G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
    case G_TYPE_BOOLEAN:
    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
    case G_TYPE_UINT:
    case G_TYPE_INT:
    case G_TYPE_ULONG:
    case G_TYPE_LONG:
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
    case G_TYPE_STRING:
      break;
    default:
      if (!parser)
        {
          g_warning (G_STRLOC ": parser needs to be specified for property \"%s\" of type `%s'",
                     pspec->name, g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
          return 0;
        }
    }
  if (g_object_class_find_property (G_OBJECT_CLASS (class), pspec->name))
    {
      g_warning (G_STRLOC ": an rc-data property \"%s\" already exists",
		 pspec->name);
      return 0;
    }
  
  for (node = object_list; node; node = node->next)
    g_object_freeze_notify (node->data);

  g_object_class_install_property (G_OBJECT_CLASS (class), ++class_n_properties, pspec);
  g_param_spec_set_qdata (pspec, quark_property_parser, parser);

  for (node = object_list; node; node = node->next)
    {
      GtkSettings *settings = node->data;
      GtkSettingsValue *qvalue;
      
      settings->property_values = g_renew (GValue, settings->property_values, class_n_properties);
      settings->property_values[class_n_properties - 1].g_type = 0;
      g_value_init (settings->property_values + class_n_properties - 1, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, settings->property_values + class_n_properties - 1);
      g_object_notify (G_OBJECT (settings), pspec->name);
      
      qvalue = g_datalist_get_data (&settings->queued_settings, pspec->name);
      if (qvalue)
	apply_queued_setting (settings, pspec, qvalue);
    }

  for (node = object_list; node; node = next)
    {
      next = node->next;
      g_object_thaw_notify (node->data);
    }

  return class_n_properties;
}

GtkRcPropertyParser
_gtk_rc_property_parser_from_type (GType type)
{
  if (type == GDK_TYPE_COLOR)
    return gtk_rc_property_parse_color;
  else if (type == GTK_TYPE_REQUISITION)
    return gtk_rc_property_parse_requisition;
  else if (type == GTK_TYPE_BORDER)
    return gtk_rc_property_parse_border;
  else if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_ENUM && G_TYPE_IS_DERIVED (type))
    return gtk_rc_property_parse_enum;
  else if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_FLAGS && G_TYPE_IS_DERIVED (type))
    return gtk_rc_property_parse_flags;
  else
    return NULL;
}

void
gtk_settings_install_property (GParamSpec  *pspec)
{
  GtkRcPropertyParser parser;

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  parser = _gtk_rc_property_parser_from_type (G_PARAM_SPEC_VALUE_TYPE (pspec));

  settings_install_property_parser (gtk_type_class (GTK_TYPE_SETTINGS), pspec, parser);
}

void
gtk_settings_install_property_parser (GParamSpec         *pspec,
				      GtkRcPropertyParser parser)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (parser != NULL);
  
  settings_install_property_parser (gtk_type_class (GTK_TYPE_SETTINGS), pspec, parser);
}

static void
free_value (gpointer data)
{
  GtkSettingsValue *qvalue = data;
  
  g_value_unset (&qvalue->value);
  g_free (qvalue->origin);
  g_free (qvalue);
}

void
gtk_settings_set_property_value (GtkSettings            *settings,
				 const gchar            *prop_name,
				 const GtkSettingsValue *new_value)
{
  GtkSettingsValue *qvalue;
  GParamSpec *pspec;
  gchar *name;
  GQuark name_quark;

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (prop_name != NULL);
  g_return_if_fail (new_value != NULL);
  g_return_if_fail (new_value->origin != NULL);

  if (!G_VALUE_HOLDS_LONG (&new_value->value) &&
      !G_VALUE_HOLDS_DOUBLE (&new_value->value) &&
      !G_VALUE_HOLDS_STRING (&new_value->value) &&
      !G_VALUE_HOLDS (&new_value->value, G_TYPE_GSTRING))
    {
      g_warning (G_STRLOC ": value type invalid");
      return;
    }
  
  name = g_strdup (prop_name);
  g_strcanon (name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');
  name_quark = g_quark_from_string (name);
  g_free (name);

  qvalue = g_datalist_id_get_data (&settings->queued_settings, name_quark);
  if (!qvalue)
    {
      qvalue = g_new0 (GtkSettingsValue, 1);
      g_datalist_id_set_data_full (&settings->queued_settings, name_quark, qvalue, free_value);
    }
  else
    {
      g_free (qvalue->origin);
      g_value_unset (&qvalue->value);
    }
  qvalue->origin = g_strdup (new_value->origin);
  g_value_init (&qvalue->value, G_VALUE_TYPE (&new_value->value));
  g_value_copy (&new_value->value, &qvalue->value);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), g_quark_to_string (name_quark));
  if (pspec)
    apply_queued_setting (settings, pspec, qvalue);
}

void
gtk_settings_set_string_property (GtkSettings *settings,
				  const gchar *name,
				  const gchar *v_string,
				  const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (v_string != NULL);
  g_return_if_fail (origin != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_STRING);
  g_value_set_static_string (&svalue.value, v_string);
  gtk_settings_set_property_value (settings, name, &svalue);
  g_value_unset (&svalue.value);
}

void
gtk_settings_set_long_property (GtkSettings *settings,
				const gchar *name,
				glong	     v_long,
				const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };
  
  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (origin != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_LONG);
  g_value_set_long (&svalue.value, v_long);
  gtk_settings_set_property_value (settings, name, &svalue);
  g_value_unset (&svalue.value);
}

void
gtk_settings_set_double_property (GtkSettings *settings,
				  const gchar *name,
				  gdouble      v_double,
				  const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (origin != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_DOUBLE);
  g_value_set_double (&svalue.value, v_double);
  gtk_settings_set_property_value (settings, name, &svalue);
  g_value_unset (&svalue.value);
}

gboolean
gtk_rc_property_parse_color (const GParamSpec *pspec,
			     const GString    *gstring,
			     GValue           *property_value)
{
  GdkColor color = { 0, 0, 0, 0, };
  GScanner *scanner;
  gboolean success;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS (property_value, GDK_TYPE_COLOR), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);
  if (gtk_rc_parse_color (scanner, &color) == G_TOKEN_NONE &&
      g_scanner_get_next_token (scanner) == G_TOKEN_EOF)
    {
      g_value_set_boxed (property_value, &color);
      success = TRUE;
    }
  else
    success = FALSE;
  g_scanner_destroy (scanner);

  return success;
}

gboolean
gtk_rc_property_parse_enum (const GParamSpec *pspec,
			    const GString    *gstring,
			    GValue           *property_value)
{
  gboolean need_closing_brace = FALSE, success = FALSE;
  GScanner *scanner;
  GEnumValue *enum_value = NULL;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_ENUM (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  /* we just want to parse _one_ value, but for consistency with flags parsing
   * we support optional paranthesis
   */
  g_scanner_get_next_token (scanner);
  if (scanner->token == '(')
    {
      need_closing_brace = TRUE;
      g_scanner_get_next_token (scanner);
    }
  if (scanner->token == G_TOKEN_IDENTIFIER)
    {
      GEnumClass *class = G_PARAM_SPEC_ENUM (pspec)->enum_class;
      
      enum_value = g_enum_get_value_by_name (class, scanner->value.v_identifier);
      if (!enum_value)
	enum_value = g_enum_get_value_by_nick (class, scanner->value.v_identifier);
      if (enum_value)
	{
	  g_value_set_enum (property_value, enum_value->value);
	  success = TRUE;
	}
    }
  else if (scanner->token == G_TOKEN_INT)
    {
      g_value_set_enum (property_value, scanner->value.v_int);
      success = TRUE;
    }
  if (need_closing_brace && g_scanner_get_next_token (scanner) != ')')
    success = FALSE;
  if (g_scanner_get_next_token (scanner) != G_TOKEN_EOF)
    success = FALSE;

  g_scanner_destroy (scanner);

  return success;
}

static guint
parse_flags_value (GScanner    *scanner,
		   GFlagsClass *class,
		   guint       *number)
{
  g_scanner_get_next_token (scanner);
  if (scanner->token == G_TOKEN_IDENTIFIER)
    {
      GFlagsValue *flags_value;

      flags_value = g_flags_get_value_by_name (class, scanner->value.v_identifier);
      if (!flags_value)
	flags_value = g_flags_get_value_by_nick (class, scanner->value.v_identifier);
      if (flags_value)
	{
	  *number |= flags_value->value;
	  return G_TOKEN_NONE;
	}
    }
  else if (scanner->token == G_TOKEN_INT)
    {
      *number |= scanner->value.v_int;
      return G_TOKEN_NONE;
    }
  return G_TOKEN_IDENTIFIER;
}

gboolean
gtk_rc_property_parse_flags (const GParamSpec *pspec,
			     const GString    *gstring,
			     GValue           *property_value)
{
  GFlagsClass *class;
  gboolean success = FALSE;
  GScanner *scanner;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_FLAGS (property_value), FALSE);

  class = G_PARAM_SPEC_FLAGS (pspec)->flags_class;
  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  /* parse either a single flags value or a "\( ... [ \| ... ] \)" compound */
  if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER ||
      scanner->next_token == G_TOKEN_INT)
    {
      guint token, flags_value = 0;
      
      token = parse_flags_value (scanner, class, &flags_value);

      if (token == G_TOKEN_NONE && g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
	{
	  success = TRUE;
	  g_value_set_flags (property_value, flags_value);
	}
      
    }
  else if (g_scanner_get_next_token (scanner) == '(')
    {
      guint token, flags_value = 0;

      /* parse first value */
      token = parse_flags_value (scanner, class, &flags_value);

      /* parse nth values, preceeded by '|' */
      while (token == G_TOKEN_NONE && g_scanner_get_next_token (scanner) == '|')
	token = parse_flags_value (scanner, class, &flags_value);

      /* done, last token must have closed expression */
      if (token == G_TOKEN_NONE && scanner->token == ')' &&
	  g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
	{
	  g_value_set_flags (property_value, flags_value);
	  success = TRUE;
	}
    }
  g_scanner_destroy (scanner);

  return success;
}

static gboolean
get_braced_int (GScanner *scanner,
		gboolean  first,
		gboolean  last,
		gint     *value)
{
  if (first)
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != '{')
	return FALSE;
    }

  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_INT)
    return FALSE;

  *value = scanner->value.v_int;

  if (last)
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != '}')
	return FALSE;
    }
  else
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != ',')
	return FALSE;
    }

  return TRUE;
}

gboolean
gtk_rc_property_parse_requisition  (const GParamSpec *pspec,
				    const GString    *gstring,
				    GValue           *property_value)
{
  GtkRequisition requisition;
  GScanner *scanner;
  gboolean success = FALSE;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  if (get_braced_int (scanner, TRUE, FALSE, &requisition.width) &&
      get_braced_int (scanner, FALSE, TRUE, &requisition.height))
    {
      g_value_set_boxed (property_value, &requisition);
      success = TRUE;
    }

  g_scanner_destroy (scanner);

  return success;
}

gboolean
gtk_rc_property_parse_border (const GParamSpec *pspec,
			      const GString    *gstring,
			      GValue           *property_value)
{
  GtkBorder border;
  GScanner *scanner;
  gboolean success = FALSE;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  if (get_braced_int (scanner, TRUE, FALSE, &border.left) &&
      get_braced_int (scanner, FALSE, FALSE, &border.right) &&
      get_braced_int (scanner, FALSE, FALSE, &border.top) &&
      get_braced_int (scanner, FALSE, TRUE, &border.bottom))
    {
      g_value_set_boxed (property_value, &border);
      success = TRUE;
    }

  g_scanner_destroy (scanner);

  return success;
}

void
_gtk_settings_handle_event (GdkEventSetting *event)
{
  GtkSettings *settings = gtk_settings_get_default ();
  
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (settings), event->name))
    g_object_notify (G_OBJECT (settings), event->name);
}
