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
#ifndef __GTK_RC_DATA_H__
#define __GTK_RC_DATA_H__

#include	<gtk/gtkrc.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* -- type macros --- */
#define GTK_TYPE_RC_DATA            (gtk_rc_data_get_type ())
#define GTK_RC_DATA(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_RC_DATA, GtkRCData))
#define GTK_RC_DATA_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_RC_DATA, GtkRCDataClass))
#define GTK_IS_RC_DATA(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_RC_DATA))
#define GTK_IS_RC_DATA_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_RC_DATA))
#define GTK_RC_DATA_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_RC_DATA, GtkRCDataClass))


/* --- typedefs --- */
typedef struct _GtkRCData      GtkRCData;
typedef struct _GtkRCDataClass GtkRCDataClass;
typedef struct _GtkRCDataValue GtkRCDataValue;


/* --- structures --- */
struct _GtkRCData
{
  GObject parent_instance;

  GData  *qvalues;
  GValue *pvalues;
};
struct _GtkRCDataClass
{
  GObjectClass parent_class;
  
};
struct _GtkRCDataValue
{
  gchar  *location;	/* rc-file location */
  GValue  rc_value;	/* rc-file value */
};


/* --- functions --- */
GType		gtk_rc_data_get_type		(void);
GtkRCData*	gtk_rc_data_get_global		(void);	/* singleton */
void		gtk_rc_data_install_property	(GParamSpec     *pspec);

/*< private >*/
void		gtk_rc_data_set_string_property	(const gchar	*name,
						 const gchar	*v_string,
						 const gchar    *location);
void		gtk_rc_data_set_long_property	(const gchar	*name,
						 glong		 v_long,
						 const gchar    *location);
void		gtk_rc_data_set_double_property	(const gchar	*name,
						 gdouble	 v_double,
						 const gchar    *location);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_RC_DATA_H__ */
