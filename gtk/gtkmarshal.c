#include "gtkmarshal.h"

typedef void (*GtkSignal_NONE__POINTER_POINTER) (GtkObject * object,
						 gpointer arg1,
						 gpointer arg2,
						 gpointer user_data);
void 
gtk_marshal_NONE__POINTER_POINTER (GtkObject * object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg * args)
{
  GtkSignal_NONE__POINTER_POINTER rfunc;
  rfunc = (GtkSignal_NONE__POINTER_POINTER) func;
  (*rfunc) (object,
	    GTK_VALUE_POINTER (args[0]),
	    GTK_VALUE_POINTER (args[1]),
	    func_data);
}

typedef gint (*GtkSignal_INT__POINTER_CHAR_CHAR) (GtkObject * object,
						  gpointer arg1,
						  char arg2,
						  char arg3,
						  gpointer user_data);
void 
gtk_marshal_INT__POINTER_CHAR_CHAR (GtkObject * object,
				    GtkSignalFunc func,
				    gpointer func_data,
				    GtkArg * args)
{
  GtkSignal_INT__POINTER_CHAR_CHAR rfunc;
  gint *return_val;
  return_val = GTK_RETLOC_INT (args[3]);
  rfunc = (GtkSignal_INT__POINTER_CHAR_CHAR) func;
  *return_val = (*rfunc) (object,
			  GTK_VALUE_POINTER (args[0]),
			  GTK_VALUE_CHAR (args[1]),
			  GTK_VALUE_CHAR (args[2]),
			  func_data);
}

typedef void (*GtkSignal_NONE__POINTER) (GtkObject * object,
					 gpointer arg1,
					 gpointer user_data);
void 
gtk_marshal_NONE__POINTER (GtkObject * object,
			   GtkSignalFunc func,
			   gpointer func_data,
			   GtkArg * args)
{
  GtkSignal_NONE__POINTER rfunc;
  rfunc = (GtkSignal_NONE__POINTER) func;
  (*rfunc) (object,
	    GTK_VALUE_POINTER (args[0]),
	    func_data);
}

typedef gint (*GtkSignal_INT__POINTER) (GtkObject * object,
					gpointer arg1,
					gpointer user_data);
void 
gtk_marshal_INT__POINTER (GtkObject * object,
			  GtkSignalFunc func,
			  gpointer func_data,
			  GtkArg * args)
{
  GtkSignal_INT__POINTER rfunc;
  gint *return_val;
  return_val = GTK_RETLOC_INT (args[1]);
  rfunc = (GtkSignal_INT__POINTER) func;
  *return_val = (*rfunc) (object,
			  GTK_VALUE_POINTER (args[0]),
			  func_data);
}

typedef void (*GtkSignal_NONE__UINT) (GtkObject * object,
				      guint arg1,
				      gpointer user_data);
void 
gtk_marshal_NONE__UINT (GtkObject * object,
			GtkSignalFunc func,
			gpointer func_data,
			GtkArg * args)
{
  GtkSignal_NONE__UINT rfunc;
  rfunc = (GtkSignal_NONE__UINT) func;
  (*rfunc) (object,
	    GTK_VALUE_UINT (args[0]),
	    func_data);
}
