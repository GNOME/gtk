#ifndef __GTKMARSHAL_H__
#define __GTKMARSHAL_H__ 1

#include "gtktypeutils.h"
#include "gtkobject.h"

#define gtk_marshal_NONE__POINTER_POINTER gtk_marshal_NONE__POINTER_POINTER
void gtk_marshal_NONE__POINTER_POINTER (GtkObject * object,
					GtkSignalFunc func,
					gpointer func_data,
					GtkArg * args);

#define gtk_marshal_INT__POINTER_CHAR_CHAR gtk_marshal_INT__POINTER_CHAR_CHAR
void gtk_marshal_INT__POINTER_CHAR_CHAR (GtkObject * object,
					 GtkSignalFunc func,
					 gpointer func_data,
					 GtkArg * args);

#define gtk_marshal_NONE__POINTER gtk_marshal_NONE__POINTER
void gtk_marshal_NONE__POINTER (GtkObject * object,
				GtkSignalFunc func,
				gpointer func_data,
				GtkArg * args);

#define gtk_marshal_INT__POINTER gtk_marshal_INT__POINTER
void gtk_marshal_INT__POINTER (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args);

void gtk_marshal_NONE__UINT (GtkObject * object,
			     GtkSignalFunc func,
			     gpointer func_data,
			     GtkArg * args);

#define gtk_marshal_NONE__BOXED gtk_marshal_NONE__POINTER
#endif
