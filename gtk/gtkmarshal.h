#ifndef __GTKMARSHAL_H__
#define __GTKMARSHAL_H__ 1

#include <gtk/gtktypeutils.h>
#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C"
{
#pragma }
#endif				/* __cplusplus */

#define gtk_signal_default_marshaller gtk_marshal_NONE__NONE

#define gtk_marshal_BOOL__GDK_EVENT gtk_marshal_BOOL__POINTER

  void gtk_marshal_BOOL__POINTER (GtkObject * object,
				  GtkSignalFunc func,
				  gpointer func_data,
				  GtkArg * args);

  void gtk_marshal_BOOL__NONE (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args);

  void gtk_marshal_BOOL__POINTER_POINTER_INT_INT (GtkObject * object,
						  GtkSignalFunc func,
						  gpointer func_data,
						  GtkArg * args);

#define gtk_marshal_ENUM__ENUM gtk_marshal_INT__INT

  void gtk_marshal_INT__INT (GtkObject * object,
			     GtkSignalFunc func,
			     gpointer func_data,
			     GtkArg * args);

  void gtk_marshal_INT__POINTER (GtkObject * object,
				 GtkSignalFunc func,
				 gpointer func_data,
				 GtkArg * args);

  void gtk_marshal_INT__POINTER_CHAR_CHAR (GtkObject * object,
					   GtkSignalFunc func,
					   gpointer func_data,
					   GtkArg * args);

#define gtk_marshal_NONE__BOXED gtk_marshal_NONE__POINTER

  void gtk_marshal_NONE__POINTER (GtkObject * object,
				  GtkSignalFunc func,
				  gpointer func_data,
				  GtkArg * args);

#define gtk_marshal_NONE__C_CALLBACK_C_CALLBACK gtk_marshal_NONE__POINTER_POINTER

  void gtk_marshal_NONE__POINTER_POINTER (GtkObject * object,
					  GtkSignalFunc func,
					  gpointer func_data,
					  GtkArg * args);

#define gtk_marshal_NONE__ENUM gtk_marshal_NONE__INT

  void gtk_marshal_NONE__INT (GtkObject * object,
			      GtkSignalFunc func,
			      gpointer func_data,
			      GtkArg * args);

#define gtk_marshal_NONE__ENUM_FLOAT gtk_marshal_NONE__INT_FLOAT

  void gtk_marshal_NONE__INT_FLOAT (GtkObject * object,
				    GtkSignalFunc func,
				    gpointer func_data,
				    GtkArg * args);

#define gtk_marshal_NONE__ENUM_FLOAT_BOOL gtk_marshal_NONE__INT_FLOAT_BOOL

  void gtk_marshal_NONE__INT_FLOAT_BOOL (GtkObject * object,
					 GtkSignalFunc func,
					 gpointer func_data,
					 GtkArg * args);

  void gtk_marshal_NONE__INT_INT (GtkObject * object,
				  GtkSignalFunc func,
				  gpointer func_data,
				  GtkArg * args);

  void gtk_marshal_NONE__INT_INT_POINTER (GtkObject * object,
					  GtkSignalFunc func,
					  gpointer func_data,
					  GtkArg * args);

  void gtk_marshal_NONE__NONE (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args);

#define gtk_marshal_NONE__OBJECT gtk_marshal_NONE__POINTER

  void gtk_marshal_NONE__POINTER_INT (GtkObject * object,
				      GtkSignalFunc func,
				      gpointer func_data,
				      GtkArg * args);

  void gtk_marshal_NONE__POINTER_POINTER_POINTER (GtkObject * object,
						  GtkSignalFunc func,
						  gpointer func_data,
						  GtkArg * args);

#define gtk_marshal_NONE__POINTER_UINT_ENUM gtk_marshal_NONE__POINTER_INT_INT

  void gtk_marshal_NONE__POINTER_INT_INT (GtkObject * object,
					  GtkSignalFunc func,
					  gpointer func_data,
					  GtkArg * args);

#define gtk_marshal_NONE__STRING gtk_marshal_NONE__POINTER

#define gtk_marshal_NONE__STRING_INT_POINTER gtk_marshal_NONE__POINTER_INT_POINTER

  void gtk_marshal_NONE__POINTER_INT_POINTER (GtkObject * object,
					      GtkSignalFunc func,
					      gpointer func_data,
					      GtkArg * args);

#define gtk_marshal_NONE__STYLE gtk_marshal_NONE__POINTER

#define gtk_marshal_NONE__UINT gtk_marshal_NONE__INT

#define gtk_marshal_NONE__UINT_POINTER_UINT_ENUM_ENUM_POINTER gtk_marshal_NONE__INT_POINTER_INT_INT_INT_POINTER

  void gtk_marshal_NONE__INT_POINTER_INT_INT_INT_POINTER (GtkObject * object,
							  GtkSignalFunc func,
							  gpointer func_data,
							  GtkArg * args);

#define gtk_marshal_NONE__POINTER_UINT_UINT gtk_marshal_NONE__POINTER_INT_INT

#define gtk_marshal_NONE__UINT_POINTER_UINT_UINT_ENUM gtk_marshal_NONE__INT_POINTER_INT_INT_INT

  void gtk_marshal_NONE__INT_POINTER_INT_INT_INT (GtkObject * object,
						  GtkSignalFunc func,
						  gpointer func_data,
						  GtkArg * args);

#define gtk_marshal_NONE__C_CALLBACK gtk_marshal_NONE__POINTER

  void gtk_marshal_NONE__BOOL (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args);

#define gtk_marshal_NONE__POINTER_UINT gtk_marshal_NONE__POINTER_INT

#define gtk_marshal_NONE__POINTER_STRING_STRING gtk_marshal_NONE__POINTER_POINTER_POINTER

#define gtk_marshal_BOOL__POINTER_STRING_STRING_POINTER gtk_marshal_BOOL__POINTER_POINTER_POINTER_POINTER

  void gtk_marshal_BOOL__POINTER_POINTER_POINTER_POINTER (GtkObject * object,
							  GtkSignalFunc func,
							  gpointer func_data,
							  GtkArg * args);

#define gtk_marshal_NONE__UINT_STRING gtk_marshal_NONE__INT_POINTER

  void gtk_marshal_NONE__INT_POINTER (GtkObject * object,
				      GtkSignalFunc func,
				      gpointer func_data,
				      GtkArg * args);


#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTKMARSHAL_H__ */
