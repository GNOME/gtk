#include "gtkmarshal.h"

typedef gboolean (*GtkSignal_BOOL__NONE) (GtkObject * object,
					  gpointer user_data);
void 
gtk_marshal_BOOL__NONE (GtkObject * object,
			GtkSignalFunc func,
			gpointer func_data,
			GtkArg * args)
{
  GtkSignal_BOOL__NONE rfunc;
  gboolean *return_val;
  return_val = GTK_RETLOC_BOOL (args[0]);
  rfunc = (GtkSignal_BOOL__NONE) func;
  *return_val = (*rfunc) (object,
			  func_data);
}

typedef gboolean (*GtkSignal_BOOL__POINTER) (GtkObject * object,
					     gpointer arg1,
					     gpointer user_data);
void 
gtk_marshal_BOOL__POINTER (GtkObject * object,
			   GtkSignalFunc func,
			   gpointer func_data,
			   GtkArg * args)
{
  GtkSignal_BOOL__POINTER rfunc;
  gboolean *return_val;
  return_val = GTK_RETLOC_BOOL (args[1]);
  rfunc = (GtkSignal_BOOL__POINTER) func;
  *return_val = (*rfunc) (object,
			  GTK_VALUE_POINTER (args[0]),
			  func_data);
}

typedef gboolean (*GtkSignal_BOOL__POINTER_POINTER_INT_INT) (GtkObject * object,
							     gpointer arg1,
							     gpointer arg2,
							     gint arg3,
							     gint arg4,
							gpointer user_data);
void 
gtk_marshal_BOOL__POINTER_POINTER_INT_INT (GtkObject * object,
					   GtkSignalFunc func,
					   gpointer func_data,
					   GtkArg * args)
{
  GtkSignal_BOOL__POINTER_POINTER_INT_INT rfunc;
  gboolean *return_val;
  return_val = GTK_RETLOC_BOOL (args[4]);
  rfunc = (GtkSignal_BOOL__POINTER_POINTER_INT_INT) func;
  *return_val = (*rfunc) (object,
			  GTK_VALUE_POINTER (args[0]),
			  GTK_VALUE_POINTER (args[1]),
			  GTK_VALUE_INT (args[2]),
			  GTK_VALUE_INT (args[3]),
			  func_data);
}

typedef gboolean (*GtkSignal_BOOL__POINTER_POINTER_POINTER_POINTER) (GtkObject * object,
							      gpointer arg1,
							      gpointer arg2,
							      gpointer arg3,
							      gpointer arg4,
							gpointer user_data);
void 
gtk_marshal_BOOL__POINTER_POINTER_POINTER_POINTER (GtkObject * object,
						   GtkSignalFunc func,
						   gpointer func_data,
						   GtkArg * args)
{
  GtkSignal_BOOL__POINTER_POINTER_POINTER_POINTER rfunc;
  gboolean *return_val;
  return_val = GTK_RETLOC_BOOL (args[4]);
  rfunc = (GtkSignal_BOOL__POINTER_POINTER_POINTER_POINTER) func;
  *return_val = (*rfunc) (object,
			  GTK_VALUE_POINTER (args[0]),
			  GTK_VALUE_POINTER (args[1]),
			  GTK_VALUE_POINTER (args[2]),
			  GTK_VALUE_POINTER (args[3]),
			  func_data);
}

typedef gint (*GtkSignal_INT__INT) (GtkObject * object,
				    gint arg1,
				    gpointer user_data);
void 
gtk_marshal_INT__INT (GtkObject * object,
		      GtkSignalFunc func,
		      gpointer func_data,
		      GtkArg * args)
{
  GtkSignal_INT__INT rfunc;
  gint *return_val;
  return_val = GTK_RETLOC_INT (args[1]);
  rfunc = (GtkSignal_INT__INT) func;
  *return_val = (*rfunc) (object,
			  GTK_VALUE_INT (args[0]),
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

typedef void (*GtkSignal_NONE__BOOL) (GtkObject * object,
				      gboolean arg1,
				      gpointer user_data);
void 
gtk_marshal_NONE__BOOL (GtkObject * object,
			GtkSignalFunc func,
			gpointer func_data,
			GtkArg * args)
{
  GtkSignal_NONE__BOOL rfunc;
  rfunc = (GtkSignal_NONE__BOOL) func;
  (*rfunc) (object,
	    GTK_VALUE_BOOL (args[0]),
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

typedef void (*GtkSignal_NONE__C_CALLBACK) (GtkObject * object,
					    gpointer arg1a,
					    gpointer arg1b,
					    gpointer user_data);
void 
gtk_marshal_NONE__C_CALLBACK (GtkObject * object,
			      GtkSignalFunc func,
			      gpointer func_data,
			      GtkArg * args)
{
  GtkSignal_NONE__C_CALLBACK rfunc;
  rfunc = (GtkSignal_NONE__C_CALLBACK) func;
  (*rfunc) (object,
	    GTK_VALUE_C_CALLBACK (args[0]).func,
	    GTK_VALUE_C_CALLBACK (args[0]).func_data,
	    func_data);
}

typedef void (*GtkSignal_NONE__C_CALLBACK_C_CALLBACK) (GtkObject * object,
						       gpointer arg1a,
						       gpointer arg1b,
						       gpointer arg2a,
						       gpointer arg2b,
						       gpointer user_data);
void 
gtk_marshal_NONE__C_CALLBACK_C_CALLBACK (GtkObject * object,
					 GtkSignalFunc func,
					 gpointer func_data,
					 GtkArg * args)
{
  GtkSignal_NONE__C_CALLBACK_C_CALLBACK rfunc;
  rfunc = (GtkSignal_NONE__C_CALLBACK_C_CALLBACK) func;
  (*rfunc) (object,
	    GTK_VALUE_C_CALLBACK (args[0]).func,
	    GTK_VALUE_C_CALLBACK (args[0]).func_data,
	    GTK_VALUE_C_CALLBACK (args[1]).func,
	    GTK_VALUE_C_CALLBACK (args[1]).func_data,
	    func_data);
}

typedef void (*GtkSignal_NONE__INT) (GtkObject * object,
				     gint arg1,
				     gpointer user_data);
void 
gtk_marshal_NONE__INT (GtkObject * object,
		       GtkSignalFunc func,
		       gpointer func_data,
		       GtkArg * args)
{
  GtkSignal_NONE__INT rfunc;
  rfunc = (GtkSignal_NONE__INT) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    func_data);
}

typedef void (*GtkSignal_NONE__INT_FLOAT) (GtkObject * object,
					   gint arg1,
					   gfloat arg2,
					   gpointer user_data);
void 
gtk_marshal_NONE__INT_FLOAT (GtkObject * object,
			     GtkSignalFunc func,
			     gpointer func_data,
			     GtkArg * args)
{
  GtkSignal_NONE__INT_FLOAT rfunc;
  rfunc = (GtkSignal_NONE__INT_FLOAT) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_FLOAT (args[1]),
	    func_data);
}

typedef void (*GtkSignal_NONE__INT_FLOAT_BOOL) (GtkObject * object,
						gint arg1,
						gfloat arg2,
						gboolean arg3,
						gpointer user_data);
void 
gtk_marshal_NONE__INT_FLOAT_BOOL (GtkObject * object,
				  GtkSignalFunc func,
				  gpointer func_data,
				  GtkArg * args)
{
  GtkSignal_NONE__INT_FLOAT_BOOL rfunc;
  rfunc = (GtkSignal_NONE__INT_FLOAT_BOOL) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_FLOAT (args[1]),
	    GTK_VALUE_BOOL (args[2]),
	    func_data);
}

typedef void (*GtkSignal_NONE__INT_INT) (GtkObject * object,
					 gint arg1,
					 gint arg2,
					 gpointer user_data);
void 
gtk_marshal_NONE__INT_INT (GtkObject * object,
			   GtkSignalFunc func,
			   gpointer func_data,
			   GtkArg * args)
{
  GtkSignal_NONE__INT_INT rfunc;
  rfunc = (GtkSignal_NONE__INT_INT) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_INT (args[1]),
	    func_data);
}

typedef void (*GtkSignal_NONE__INT_INT_POINTER) (GtkObject * object,
						 gint arg1,
						 gint arg2,
						 gpointer arg3,
						 gpointer user_data);
void 
gtk_marshal_NONE__INT_INT_POINTER (GtkObject * object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg * args)
{
  GtkSignal_NONE__INT_INT_POINTER rfunc;
  rfunc = (GtkSignal_NONE__INT_INT_POINTER) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_INT (args[1]),
	    GTK_VALUE_POINTER (args[2]),
	    func_data);
}

typedef void (*GtkSignal_NONE__NONE) (GtkObject * object,
				      gpointer user_data);
void 
gtk_marshal_NONE__NONE (GtkObject * object,
			GtkSignalFunc func,
			gpointer func_data,
			GtkArg * args)
{
  GtkSignal_NONE__NONE rfunc;
  rfunc = (GtkSignal_NONE__NONE) func;
  (*rfunc) (object,
	    func_data);
}

typedef void (*GtkSignal_NONE__POINTER_INT) (GtkObject * object,
					     gpointer arg1,
					     gint arg2,
					     gpointer user_data);
void 
gtk_marshal_NONE__POINTER_INT (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args)
{
  GtkSignal_NONE__POINTER_INT rfunc;
  rfunc = (GtkSignal_NONE__POINTER_INT) func;
  (*rfunc) (object,
	    GTK_VALUE_POINTER (args[0]),
	    GTK_VALUE_INT (args[1]),
	    func_data);
}

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

typedef void (*GtkSignal_NONE__POINTER_POINTER_POINTER) (GtkObject * object,
							 gpointer arg1,
							 gpointer arg2,
							 gpointer arg3,
							 gpointer user_data);
void 
gtk_marshal_NONE__POINTER_POINTER_POINTER (GtkObject * object,
					   GtkSignalFunc func,
					   gpointer func_data,
					   GtkArg * args)
{
  GtkSignal_NONE__POINTER_POINTER_POINTER rfunc;
  rfunc = (GtkSignal_NONE__POINTER_POINTER_POINTER) func;
  (*rfunc) (object,
	    GTK_VALUE_POINTER (args[0]),
	    GTK_VALUE_POINTER (args[1]),
	    GTK_VALUE_POINTER (args[2]),
	    func_data);
}

typedef void (*GtkSignal_NONE__POINTER_INT_INT) (GtkObject * object,
						 gpointer arg1,
						 gint arg2,
						 gint arg3,
						 gpointer user_data);
void 
gtk_marshal_NONE__POINTER_INT_INT (GtkObject * object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg * args)
{
  GtkSignal_NONE__POINTER_INT_INT rfunc;
  rfunc = (GtkSignal_NONE__POINTER_INT_INT) func;
  (*rfunc) (object,
	    GTK_VALUE_POINTER (args[0]),
	    GTK_VALUE_INT (args[1]),
	    GTK_VALUE_INT (args[2]),
	    func_data);
}

typedef void (*GtkSignal_NONE__POINTER_INT_POINTER) (GtkObject * object,
						     gpointer arg1,
						     gint arg2,
						     gpointer arg3,
						     gpointer user_data);
void 
gtk_marshal_NONE__POINTER_INT_POINTER (GtkObject * object,
				       GtkSignalFunc func,
				       gpointer func_data,
				       GtkArg * args)
{
  GtkSignal_NONE__POINTER_INT_POINTER rfunc;
  rfunc = (GtkSignal_NONE__POINTER_INT_POINTER) func;
  (*rfunc) (object,
	    GTK_VALUE_POINTER (args[0]),
	    GTK_VALUE_INT (args[1]),
	    GTK_VALUE_POINTER (args[2]),
	    func_data);
}

typedef void (*GtkSignal_NONE__INT_POINTER_INT_INT_INT_POINTER) (GtkObject * object,
								 gint arg1,
							      gpointer arg2,
								 gint arg3,
								 gint arg4,
								 gint arg5,
							      gpointer arg6,
							gpointer user_data);
void 
gtk_marshal_NONE__INT_POINTER_INT_INT_INT_POINTER (GtkObject * object,
						   GtkSignalFunc func,
						   gpointer func_data,
						   GtkArg * args)
{
  GtkSignal_NONE__INT_POINTER_INT_INT_INT_POINTER rfunc;
  rfunc = (GtkSignal_NONE__INT_POINTER_INT_INT_INT_POINTER) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_POINTER (args[1]),
	    GTK_VALUE_INT (args[2]),
	    GTK_VALUE_INT (args[3]),
	    GTK_VALUE_INT (args[4]),
	    GTK_VALUE_POINTER (args[5]),
	    func_data);
}

typedef void (*GtkSignal_NONE__INT_POINTER_INT_INT_INT) (GtkObject * object,
							 gint arg1,
							 gpointer arg2,
							 gint arg3,
							 gint arg4,
							 gint arg5,
							 gpointer user_data);
void 
gtk_marshal_NONE__INT_POINTER_INT_INT_INT (GtkObject * object,
					   GtkSignalFunc func,
					   gpointer func_data,
					   GtkArg * args)
{
  GtkSignal_NONE__INT_POINTER_INT_INT_INT rfunc;
  rfunc = (GtkSignal_NONE__INT_POINTER_INT_INT_INT) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_POINTER (args[1]),
	    GTK_VALUE_INT (args[2]),
	    GTK_VALUE_INT (args[3]),
	    GTK_VALUE_INT (args[4]),
	    func_data);
}

typedef void (*GtkSignal_NONE__INT_POINTER) (GtkObject * object,
					     gint arg1,
					     gpointer arg2,
					     gpointer user_data);
void 
gtk_marshal_NONE__INT_POINTER (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args)
{
  GtkSignal_NONE__INT_POINTER rfunc;
  rfunc = (GtkSignal_NONE__INT_POINTER) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_POINTER (args[1]),
	    func_data);
}
