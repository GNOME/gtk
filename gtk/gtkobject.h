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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_OBJECT_H__
#define __GTK_OBJECT_H__


#include <gtk/gtkarg.h>
#include <gtk/gtkenums.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkdebug.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/* Macro for casting a pointer to a GtkObject or GtkObjectClass pointer.
 * The second portion of the ?: statments are just in place to offer
 * descriptive warning message.
 */
#define GTK_OBJECT(object)		( \
  GTK_IS_OBJECT (object) ? \
    (GtkObject*) (object) : \
    (GtkObject*) gtk_type_check_object_cast ((GtkTypeObject*) (object), GTK_TYPE_OBJECT) \
)
#define GTK_OBJECT_CLASS(klass)		( \
  GTK_IS_OBJECT_CLASS (klass) ? \
    (GtkObjectClass*) (klass) : \
    (GtkObjectClass*) gtk_type_check_class_cast ((GtkTypeClass*) (klass), GTK_TYPE_OBJECT) \
)

/* Macro for testing whether `object' and `klass' are of type GTK_TYPE_OBJECT.
 */
#define GTK_IS_OBJECT(object)		( \
  (object) != NULL && \
  GTK_IS_OBJECT_CLASS (((GtkObject*) (object))->klass) \
)
#define GTK_IS_OBJECT_CLASS(klass)	( \
  (klass) != NULL && \
  GTK_FUNDAMENTAL_TYPE (((GtkObjectClass*) (klass))->type) == GTK_TYPE_OBJECT \
)

/* Macros for extracting various fields from GtkObject and GtkObjectClass.
 */
#define GTK_OBJECT_TYPE(obj)		  (GTK_OBJECT (obj)->klass->type)
#define GTK_OBJECT_SIGNALS(obj)		  (GTK_OBJECT (obj)->klass->signals)
#define GTK_OBJECT_NSIGNALS(obj)	  (GTK_OBJECT (obj)->klass->nsignals)

/* GtkObject only uses the first 4 bits of the flags field.
 * Derived objects may use the remaining bits. Though this
 * is a kinda nasty break up, it does make the size of
 * derived objects smaller.
 */
typedef enum
{
  GTK_DESTROYED		= 1 << 0,
  GTK_FLOATING		= 1 << 1,
  GTK_CONNECTED		= 1 << 2,
  GTK_CONSTRUCTED	= 1 << 3
} GtkObjectFlags;

/* Macros for extracting the object_flags from GtkObject.
 */
#define GTK_OBJECT_FLAGS(obj)		  (GTK_OBJECT (obj)->flags)
#define GTK_OBJECT_DESTROYED(obj)	  ((GTK_OBJECT_FLAGS (obj) & GTK_DESTROYED) != 0)
#define GTK_OBJECT_FLOATING(obj)	  ((GTK_OBJECT_FLAGS (obj) & GTK_FLOATING) != 0)
#define GTK_OBJECT_CONNECTED(obj)	  ((GTK_OBJECT_FLAGS (obj) & GTK_CONNECTED) != 0)
#define GTK_OBJECT_CONSTRUCTED(obj)	  ((GTK_OBJECT_FLAGS (obj) & GTK_CONSTRUCTED) != 0)

/* Macros for setting and clearing bits in the object_flags field of GtkObject.
 */
#define GTK_OBJECT_SET_FLAGS(obj,flag)	  G_STMT_START{ (GTK_OBJECT_FLAGS (obj) |= (flag)); }G_STMT_END
#define GTK_OBJECT_UNSET_FLAGS(obj,flag)  G_STMT_START{ (GTK_OBJECT_FLAGS (obj) &= ~(flag)); }G_STMT_END

/* GtkArg flag bits for gtk_object_add_arg_type
 */
typedef enum
{
  GTK_ARG_READABLE	 = 1 << 0,
  GTK_ARG_WRITABLE	 = 1 << 1,
  GTK_ARG_CONSTRUCT	 = 1 << 2,
  GTK_ARG_CONSTRUCT_ONLY = 1 << 3,
  GTK_ARG_CHILD_ARG	 = 1 << 4,
  GTK_ARG_MASK		 = 0x1f,
  
  /* aliases
   */
  GTK_ARG_READWRITE	 = GTK_ARG_READABLE | GTK_ARG_WRITABLE
} GtkArgFlags;

typedef struct _GtkObjectClass	GtkObjectClass;


/* The GtkObject structure is the base of the Gtk+ objects hierarchy,
 * it ``inherits'' from the GtkTypeObject by mirroring its fields,
 * which must always be kept in sync completely. The GtkObject defines
 * the few basic items that all derived classes contain.
 */
struct _GtkObject
{
  /* GtkTypeObject related fields: */
  GtkObjectClass *klass;
  
  
  /* 32 bits of flags. GtkObject only uses 4 of these bits and
   *  GtkWidget uses the rest. This is done because structs are
   *  aligned on 4 or 8 byte boundaries. If a new bitfield were
   *  used in GtkWidget much space would be wasted.
   */
  guint32 flags;
  
  /* reference count.
   * refer to the file docs/refcounting.txt on this issue.
   */
  guint ref_count;
  
  /* A list of keyed data pointers, used for e.g. the list of signal
   * handlers or an object's user_data.
   */
  GData *object_data;
};

/* The GtkObjectClass is the base of the Gtk+ objects classes hierarchy,
 * it ``inherits'' from the GtkTypeClass by mirroring its fields, which
 * must always be kept in sync completely. The GtkObjectClass defines
 * the basic necessities for the object inheritance mechanism to work.
 * Namely, the `signals' and `nsignals' fields as well as the function
 * pointers, required to end an object's lifetime.
 */
struct _GtkObjectClass
{
  /* GtkTypeClass fields: */
  GtkType type;
  
  
  /* The signals this object class handles. "signals" is an
   *  array of signal ID's.
   */
  guint *signals;
  
  /* The number of signals listed in "signals".
   */
  guint nsignals;
  
  /* The number of arguments per class.
   */
  guint n_args;
  GSList *construct_args;
  
  /* Non overridable class methods to set and get per class arguments */
  void (*set_arg) (GtkObject *object,
		   GtkArg    *arg,
		   guint      arg_id);
  void (*get_arg) (GtkObject *object,
		   GtkArg    *arg,
		   guint      arg_id);
  
  /* The functions that will end an objects life time. In one way ore
   *  another all three of them are defined for all objects. If an
   *  object class overrides one of the methods in order to perform class
   *  specific destruction then it must still invoke its superclass'
   *  implementation of the method after it is finished with its
   *  own cleanup. (See the destroy function for GtkWidget for
   *  an example of how to do this).
   */
  void (* shutdown) (GtkObject *object);
  void (* destroy)  (GtkObject *object);
  
  void (* finalize) (GtkObject *object);
};



/* Application-level methods */

GtkType	gtk_object_get_type		(void);

/* Append a user defined signal without default handler to a class. */
guint	gtk_object_class_user_signal_new  (GtkObjectClass     *klass,
					   const gchar	      *name,
					   GtkSignalRunType    signal_flags,
					   GtkSignalMarshaller marshaller,
					   GtkType	       return_val,
					   guint	       nparams,
					   ...);
guint	gtk_object_class_user_signal_newv (GtkObjectClass     *klass,
					   const gchar	      *name,
					   GtkSignalRunType    signal_flags,
					   GtkSignalMarshaller marshaller,
					   GtkType	       return_val,
					   guint	       nparams,
					   GtkType	      *params);
GtkObject*	gtk_object_new		  (GtkType	       type,
					   const gchar	      *first_arg_name,
					   ...);
GtkObject*	gtk_object_newv		  (GtkType	       object_type,
					   guint	       n_args,
					   GtkArg	      *args);
void gtk_object_default_construct         (GtkObject	      *object);
void gtk_object_constructed		  (GtkObject	      *object);
void gtk_object_sink	  (GtkObject	    *object);
void gtk_object_ref	  (GtkObject	    *object);
void gtk_object_unref	  (GtkObject	    *object);
void gtk_object_weakref	  (GtkObject	    *object,
			   GtkDestroyNotify  notify,
			   gpointer	     data);
void gtk_object_weakunref (GtkObject	    *object,
			   GtkDestroyNotify  notify,
			   gpointer	     data);
void gtk_object_destroy	  (GtkObject *object);

/* gtk_object_getv() sets an arguments type and value, or just
 * its type to GTK_TYPE_INVALID.
 * if GTK_FUNDAMENTAL_TYPE (arg->type) == GTK_TYPE_STRING, it's
 * the callers response to do a g_free (GTK_VALUE_STRING (arg));
 */
void	gtk_object_getv		(GtkObject	*object,
				 guint		n_args,
				 GtkArg		*args);
/* gtk_object_get() sets the variable values pointed to by the adresses
 * passed after the argument names according to the arguments value.
 * if GTK_FUNDAMENTAL_TYPE (arg->type) == GTK_TYPE_STRING, it's
 * the callers response to do a g_free (retrived_value);
 */
void	gtk_object_get		(GtkObject	*object,
				 const gchar	*first_arg_name,
				 ...);

/* gtk_object_set() takes a variable argument list of the form:
 * (..., gchar *arg_name, ARG_VALUES, [repeatedly name/value pairs,] NULL)
 * where ARG_VALUES type depend on the argument and can consist of
 * more than one c-function argument.
 */
void	gtk_object_set		(GtkObject	*object,
				 const gchar	*first_arg_name,
				 ...);
void	gtk_object_setv		(GtkObject	*object,
				 guint		n_args,
				 GtkArg		*args);

/* Allocate a GtkArg array of size nargs that hold the
 * names and types of the args that can be used with
 * gtk_object_set/gtk_object_get. if (arg_flags!=NULL),
 * (*arg_flags) will be set to point to a newly allocated
 * guint array that holds the flags of the args.
 * It is the callers response to do a
 * g_free (returned_args); g_free (*arg_flags).
 */
GtkArg* gtk_object_query_args	(GtkType	  class_type,
				 guint32	**arg_flags,
				 guint		 *n_args);

/* Set 'data' to the "object_data" field of the object. The
 *  data is indexed by the "key". If there is already data
 *  associated with "key" then the new data will replace it.
 *  If 'data' is NULL then this call is equivalent to
 *  'gtk_object_remove_data'.
 *  The gtk_object_set_data_full variant acts just the same,
 *  but takes an additional argument which is a function to
 *  be called when the data is removed.
 *  `gtk_object_remove_data' is equivalent to the above,
 *  where 'data' is NULL
 *  `gtk_object_get_data' gets the data associated with "key".
 */
void	 gtk_object_set_data	     (GtkObject	     *object,
				      const gchar    *key,
				      gpointer	      data);
void	 gtk_object_set_data_full    (GtkObject	     *object,
				      const gchar    *key,
				      gpointer	      data,
				      GtkDestroyNotify destroy);
void	 gtk_object_remove_data	     (GtkObject	     *object,
				      const gchar    *key);
gpointer gtk_object_get_data	     (GtkObject	     *object,
				      const gchar    *key);
void	 gtk_object_remove_no_notify (GtkObject	     *object,
				      const gchar    *key);

/* Set/get the "user_data" object data field of "object". It should
 *  be noted that these functions are no different than calling
 *  `gtk_object_set_data'/`gtk_object_get_data' with a key of "user_data".
 *  They are merely provided as a convenience.
 */
void	 gtk_object_set_user_data (GtkObject	*object,
				   gpointer	 data);
gpointer gtk_object_get_user_data (GtkObject	*object);


/* Object-level methods */

/* Append "signals" to those already defined in "class". */
void	gtk_object_class_add_signals	(GtkObjectClass	*klass,
					 guint		*signals,
					 guint		 nsignals);
/* the `arg_name' argument needs to be a const static string */
void	gtk_object_add_arg_type		(const gchar	*arg_name,
					 GtkType	 arg_type,
					 guint		 arg_flags,
					 guint		 arg_id);

/* Object data method variants that operate on key ids. */
void gtk_object_set_data_by_id		(GtkObject	 *object,
					 GQuark		  data_id,
					 gpointer	  data);
void gtk_object_set_data_by_id_full	(GtkObject	 *object,
					 GQuark		  data_id,
					 gpointer	  data,
					 GtkDestroyNotify destroy);
gpointer gtk_object_get_data_by_id	(GtkObject	 *object,
					 GQuark		  data_id);
void  gtk_object_remove_data_by_id	(GtkObject	 *object,
					 GQuark		  data_id);
void  gtk_object_remove_no_notify_by_id	(GtkObject	 *object,
					 GQuark		  key_id);
#define	gtk_object_data_try_key	    g_quark_try_string
#define	gtk_object_data_force_id    g_quark_from_string


/* Non-public methods */

void	gtk_object_arg_set	(GtkObject   *object,
				 GtkArg	     *arg,
				 GtkArgInfo  *info);
void	gtk_object_arg_get	(GtkObject   *object,
				 GtkArg	     *arg,
				 GtkArgInfo  *info);
gchar*	gtk_object_args_collect (GtkType      object_type,
				 GSList	    **arg_list_p,
				 GSList	    **info_list_p,
				 const gchar *first_arg_name,
				 va_list      var_args);
gchar*	gtk_object_arg_get_info (GtkType      object_type,
				 const gchar *arg_name,
				 GtkArgInfo **info_p);
void	gtk_trace_referencing	(GtkObject   *object,
				 const gchar *func,
				 guint	      dummy,
				 guint	      line,
				 gboolean     do_ref);
#ifdef	G_ENABLE_DEBUG
#  define gtk_object_ref(o)   G_STMT_START{gtk_trace_referencing((o),G_GNUC_PRETTY_FUNCTION,0,__LINE__,1);}G_STMT_END
#  define gtk_object_unref(o) G_STMT_START{gtk_trace_referencing((o),G_GNUC_PRETTY_FUNCTION,0,__LINE__,0);}G_STMT_END
#endif	/* G_ENABLE_DEBUG */








#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_OBJECT_H__ */
