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
#include	"gtkrcdata.h"



/* --- prototypes --- */
static void	gtk_rc_data_init		(GtkRCData		*data);
static void	gtk_rc_data_class_init		(GtkRCDataClass		*class);
static GObject*	gtk_rc_data_constructor		(GType			 type,
						 guint			 n_construct_properties,
						 GObjectConstructParam	*construct_properties);
static void	gtk_rc_data_get_property	(GObject		*object,
						 guint			 property_id,
						 GValue			*value,
						 GParamSpec		*pspec,
						 const gchar		*trailer);
static void	gtk_rc_data_set_property	(GObject		*object,
						 guint			 property_id,
						 const GValue		*value,
						 GParamSpec		*pspec,
						 const gchar		*trailer);
static void	gtk_rc_data_notify		(GObject		*object,
						 GParamSpec		*pspec);


/* --- variables --- */
static gpointer		 parent_class = NULL;
static GtkRCData	*the_singleton = NULL;


/* --- functions --- */
GType
gtk_rc_data_get_type (void)
{
  static GType rc_data_type = 0;

  if (!rc_data_type)
    {
      static const GTypeInfo rc_data_info =
      {
	sizeof (GtkRCDataClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_rc_data_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkRCData),
	0,              /* n_preallocs */
	(GInstanceInitFunc) gtk_rc_data_init,
      };

      rc_data_type = g_type_register_static (G_TYPE_OBJECT, "GtkRCData", &rc_data_info, 0);
    }

  return rc_data_type;
}

static void
gtk_rc_data_init (GtkRCData *data)
{
  g_datalist_init (&data->qvalues);
  data->pvalues = NULL;
}

static void
gtk_rc_data_class_init (GtkRCDataClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->constructor = gtk_rc_data_constructor;
  gobject_class->get_property = gtk_rc_data_get_property;
  gobject_class->set_property = gtk_rc_data_set_property;
  gobject_class->notify = gtk_rc_data_notify;
}

static GObject*
gtk_rc_data_constructor	(GType			type,
			 guint		        n_construct_properties,
			 GObjectConstructParam *construct_properties)
{
  GObject *object;

  if (!the_singleton)
    {
      object = G_OBJECT_CLASS (parent_class)->constructor (type,
							   n_construct_properties,
							   construct_properties);
      the_singleton = GTK_RC_DATA (g_object_ref (object));
    }
  else
    object = g_object_ref (G_OBJECT (the_singleton));

  return object;
}

GtkRCData*
gtk_rc_data_get_global (void)
{
  if (!the_singleton)
    g_object_new (GTK_TYPE_RC_DATA, NULL);

  return the_singleton;	/* we don't add a reference count here, we'd be rc_data_ref_global() if we did */
}

static void
set_property (GtkRCData	     *data,
	      GParamSpec     *pspec,
	      GtkRCDataValue *dvalue)
{
  if (g_value_types_exchangable (G_VALUE_TYPE (&dvalue->rc_value), G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
      GValue tmp_value = { 0, };

      g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_value_convert (&dvalue->rc_value, &tmp_value);
      g_param_value_validate (pspec, &tmp_value);
      g_object_set_property (G_OBJECT (data), pspec->name, &tmp_value);
      g_value_unset (&tmp_value);
    }
  else
    g_message ("%s: unable to convert rc-value of type `%s' for rc-property \"%s\" of type `%s'",
	       dvalue->location,
	       G_VALUE_TYPE_NAME (&dvalue->rc_value),
	       pspec->name,
	       g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
}

static void
gtk_rc_data_set_property (GObject      *object,
			  guint	        property_id,
			  const GValue *value,
			  GParamSpec   *pspec,
			  const gchar  *trailer)
{
  GtkRCData *data = GTK_RC_DATA (object);

  g_value_copy (value, data->pvalues + property_id - 1);
}

static void
gtk_rc_data_get_property (GObject     *object,
			  guint	       property_id,
			  GValue      *value,
			  GParamSpec  *pspec,
			  const gchar *trailer)
{
  GtkRCData *data = GTK_RC_DATA (object);

  g_value_copy (data->pvalues + property_id - 1, value);
}

static void
gtk_rc_data_notify (GObject    *object,
		    GParamSpec *pspec)
{
  GValue tmp_value = { 0, };

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (object, pspec->name, &tmp_value);
  g_print ("rc-file property \"%s\" set to %p\n",
	   pspec->name,
	   tmp_value.data[0].v_pointer);
  g_value_unset (&tmp_value);
}

void
gtk_rc_data_install_property (GParamSpec *pspec)
{
  GtkRCData *data = gtk_rc_data_get_global ();

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

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
      g_warning (G_STRLOC ": property type `%s' is not supported for rc-data property \"%s\"",
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)), pspec->name);
      return;
    }
  g_object_freeze_notify (G_OBJECT (data));
  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (data), pspec->name))
    {
      static guint n_properties = 0;
      GtkRCDataValue *dvalue;

      g_object_class_install_property (G_OBJECT_GET_CLASS (data), ++n_properties, pspec);
      data->pvalues = g_renew (GValue, data->pvalues, n_properties);
      data->pvalues[n_properties - 1].g_type = 0;
      g_value_init (data->pvalues + n_properties - 1, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, data->pvalues + n_properties - 1);
      g_object_notify (G_OBJECT (data), pspec->name);

      dvalue = g_datalist_get_data (&data->qvalues, pspec->name);
      if (dvalue)
	set_property (data, pspec, dvalue);
    }
  else
    g_warning (G_STRLOC ": an rc-data property \"%s\" already exists",
	       pspec->name);
  g_object_thaw_notify (G_OBJECT (data));
}

static void
free_value (gpointer data)
{
  GtkRCDataValue *dvalue = data;

  g_value_unset (&dvalue->rc_value);
  g_free (dvalue->location);
  g_free (dvalue);
}

static void
set_value (GtkRCData   *data,
	   const gchar *vname,
	   GValue      *value,
	   const gchar *location)
{
  GtkRCDataValue *dvalue;
  GParamSpec *pspec;
  gchar *name = g_strdup (vname);

  g_strcanon (name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');

  dvalue = g_datalist_get_data (&data->qvalues, name);
  if (!dvalue)
    {
      dvalue = g_new0 (GtkRCDataValue, 1);
      g_datalist_set_data_full (&data->qvalues, name, dvalue, free_value);
    }
  else
    g_value_unset (&dvalue->rc_value);
  g_free (dvalue->location);
  dvalue->location = g_strdup (location);
  g_value_init (&dvalue->rc_value, G_VALUE_TYPE (value));
  g_value_copy (value, &dvalue->rc_value);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (data), name);
  if (pspec)
    set_property (data, pspec, dvalue);
  g_free (name);
}

void
gtk_rc_data_set_string_property (const gchar *name,
				 const gchar *v_string,
				 const gchar *location)
{
  GtkRCData *data = gtk_rc_data_get_global ();
  GValue value = { 0, };

  g_return_if_fail (name != NULL);
  g_return_if_fail (v_string != NULL);

  g_object_freeze_notify (G_OBJECT (data));
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, v_string);
  set_value (data, name, &value, location);
  g_value_unset (&value);
  g_object_thaw_notify (G_OBJECT (data));
}

void
gtk_rc_data_set_long_property (const gchar *name,
			       glong	    v_long,
			       const gchar *location)
{
  GtkRCData *data = gtk_rc_data_get_global ();
  GValue value = { 0, };

  g_return_if_fail (name != NULL);

  g_object_freeze_notify (G_OBJECT (data));
  g_value_init (&value, G_TYPE_LONG);
  g_value_set_long (&value, v_long);
  set_value (data, name, &value, location);
  g_value_unset (&value);
  g_object_thaw_notify (G_OBJECT (data));
}

void
gtk_rc_data_set_double_property (const gchar *name,
				 gdouble      v_double,
				 const gchar *location)
{
  GtkRCData *data = gtk_rc_data_get_global ();
  GValue value = { 0, };

  g_return_if_fail (name != NULL);

  g_object_freeze_notify (G_OBJECT (data));
  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, v_double);
  set_value (data, name, &value, location);
  g_value_unset (&value);
  g_object_thaw_notify (G_OBJECT (data));
}
