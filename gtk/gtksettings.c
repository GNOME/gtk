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
#include	"gtksettings.h"



enum {
  PROP_0,
  PROP_DOUBLE_CLICK_TIMEOUT,
  PROP_BELL_PITCH,
  PROP_BELL_DURATION,
  PROP_BELL_PERCENT
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
static GQuark		 quark_property_id = 0;
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
  GObjectClass *gobject_class = G_OBJECT_GET_CLASS (settings);
  guint i;
  
  g_datalist_init (&settings->queued_settings);
  object_list = g_slist_prepend (object_list, settings);

  /* build up property array for all yet existing properties and queue
   * notification for them (at least notification for internal properties
   * will instantly be caught)
   */
  settings->property_values = g_new0 (GValue, class_n_properties);
  for (i = 0; i < class_n_properties; i++)
    {
      GParamSpec *pspec = gobject_class->property_specs[i]; // FIXME: g_object_list_properties(this_class_type)

      g_value_init (settings->property_values + i, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, settings->property_values + i);
      g_object_notify (G_OBJECT (settings), pspec->name);
    }
}

static void
gtk_settings_class_init (GtkSettingsClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);

  gobject_class->constructor = gtk_settings_constructor;
  gobject_class->finalize = gtk_settings_finalize;
  gobject_class->get_property = gtk_settings_get_property;
  gobject_class->set_property = gtk_settings_set_property;
  gobject_class->notify = gtk_settings_notify;

  quark_property_parser = g_quark_from_static_string ("gtk-rc-property-parser");
  quark_property_id = g_quark_try_string ("GObject-property-id");
  g_assert (quark_property_id != 0);	/* special quarks from GObjectClass */

  g_assert (PROP_DOUBLE_CLICK_TIMEOUT ==
	    settings_install_property_parser (class,
					      g_param_spec_int ("double-click-timeout", "Double Click Timeout", NULL,
								1, G_MAXINT, 1,
								G_PARAM_READWRITE),
					      NULL));
  g_assert (PROP_BELL_PITCH ==
	    settings_install_property_parser (class,
					      g_param_spec_int ("bell-pitch", "Bell Pitch", NULL,
								0, G_MAXINT, 440,
								G_PARAM_READWRITE),
					      NULL));
  g_assert (PROP_BELL_DURATION ==
	    settings_install_property_parser (class,
					      g_param_spec_int ("bell_duration", "Bell Duration", NULL,
								1, G_MAXINT, 250,
								G_PARAM_READWRITE),
					      NULL));
  g_assert (PROP_BELL_PERCENT ==
	    settings_install_property_parser (class,
					      g_param_spec_float ("bell_percent", "Bell Percent", NULL,
								  0, 100, 80,
								  G_PARAM_READWRITE),
					      NULL));
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
gtk_settings_get_global (void)
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
  
  g_value_copy (settings->property_values + property_id - 1, value);
}

static void
gtk_settings_notify (GObject    *object,
		     GParamSpec *pspec)
{
  guint property_id = GPOINTER_TO_UINT (g_param_spec_get_qdata ((pspec), quark_property_id));

#if 1
  GValue tmp_value = { 0, };
  gchar *contents;
  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (object, pspec->name, &tmp_value);
  contents = g_strdup_value_contents (&tmp_value);

  switch (property_id)
    {
    case PROP_DOUBLE_CLICK_TIMEOUT:
      g_print ("settings-notify: %s = \"%s\"\n", pspec->name, contents);
      break;
    case PROP_BELL_PITCH:
      g_print ("settings-notify: %s = \"%s\"\n", pspec->name, contents);
      break;
    case PROP_BELL_DURATION:
      g_print ("settings-notify: %s = \"%s\"\n", pspec->name, contents);
      break;
    case PROP_BELL_PERCENT:
      g_print ("settings-notify: %s = \"%s\"\n", pspec->name, contents);
      break;
    }

  g_free (contents);
  g_value_unset (&tmp_value);
#endif
}

static void
apply_queued_setting (GtkSettings      *data,
		      GParamSpec       *pspec,
		      GtkSettingsValue *qvalue)
{
  gboolean warn_convert = TRUE;

  if (g_value_type_transformable (G_VALUE_TYPE (&qvalue->value), G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
      GValue tmp_value = { 0, };

      warn_convert = FALSE;
      g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      if (g_param_value_convert (pspec, &qvalue->value, &tmp_value, TRUE))
	g_object_set_property (G_OBJECT (data), pspec->name, &tmp_value);
      else
	{
	  gchar *debug = g_strdup_value_contents (&tmp_value);

	  g_message ("%s: rc-value `%s' for rc-property \"%s\" of type `%s' has invalid contents \"%s\"",
		     qvalue->origin,
		     G_VALUE_TYPE_NAME (&qvalue->value),
		     pspec->name,
		     g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		     debug);
	  g_free (debug);
	}
      g_value_unset (&tmp_value);
    }
  else
    {
      GtkRcPropertyParser parser = g_param_spec_get_qdata (pspec, quark_property_parser);

      if (parser)
	{
	  GValue tmp_value = { 0, };

	  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
	  if (parser (pspec, g_value_get_boxed (&qvalue->value), &tmp_value))
	    {
	      warn_convert = FALSE;
	      g_object_set_property (G_OBJECT (data), pspec->name, &tmp_value);
	    }
	  g_value_unset (&tmp_value);
	}
    }

  if (warn_convert)
    g_message ("%s: unable to convert rc-value of type `%s' to rc-property \"%s\" of type `%s'",
	       qvalue->origin,
	       G_VALUE_TYPE_NAME (&qvalue->value),
	       pspec->name,
	       g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
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
	g_warning (G_STRLOC ": parser needs to be specified for property \"%s\" of type `%s'",
		   pspec->name, g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      return 0;
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
_gtk_rc_property_parser_for_type (GType type)
{
  if (type == GTK_TYPE_GDK_COLOR)
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
gtk_settings_install_property (GtkSettings *settings,
			       GParamSpec  *pspec)
{
  GtkRcPropertyParser parser = NULL;

  g_return_if_fail (GTK_IS_SETTINGS (settings));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  parser = _gtk_rc_property_parser_for_type (G_PARAM_SPEC_VALUE_TYPE (pspec));

  settings_install_property_parser (GTK_SETTINGS_GET_CLASS (settings), pspec, parser);
}

void
gtk_settings_install_property_parser (GtkSettings        *settings,
				      GParamSpec         *pspec,
				      GtkRcPropertyParser parser)
{
  g_return_if_fail (GTK_IS_SETTINGS (settings));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (parser != NULL);
  
  settings_install_property_parser (GTK_SETTINGS_GET_CLASS (settings), pspec, parser);
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
gtk_settings_set_property_value  (GtkSettings            *settings,
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
  name_quark = g_quark_try_string (name);
  if (name_quark)
    g_free (name);
  else
    name_quark = g_quark_from_string (name);

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
  g_return_val_if_fail (G_VALUE_HOLDS (property_value, GTK_TYPE_GDK_COLOR), FALSE);

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
