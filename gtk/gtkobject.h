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
#ifndef __GTK_OBJECT_H__
#define __GTK_OBJECT_H__


#include <gtk/gtkenums.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkdebug.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */



/* The debugging versions of the casting macros make sure the cast is "ok"
 *  before proceeding, but they are definately slower than their less
 *  careful counterparts as they involve no less than 3 function calls.
 */
#ifdef GTK_NO_CHECK_CASTS

#define GTK_CHECK_CAST(obj,cast_type,cast)	   ((cast*) (obj))
#define GTK_CHECK_CLASS_CAST(klass,cast_type,cast) ((cast*) (klass))

#else /* !GTK_NO_CHECK_CASTS */

#define GTK_CHECK_CAST(obj,cast_type,cast) \
  ((cast*) gtk_object_check_cast ((GtkObject*) (obj), (cast_type)))

#define GTK_CHECK_CLASS_CAST(klass,cast_type,cast) \
  ((cast*) gtk_object_check_class_cast ((GtkObjectClass*) (klass), (cast_type)))

#endif /* GTK_NO_CHECK_CASTS */


/* Determines whether `obj' and `klass' are a type of `otype'.
 */
#define GTK_CHECK_TYPE(obj,otype)	   ( \
  gtk_type_is_a (((GtkObject*) (obj))->klass->type, (otype)) \
)
#define GTK_CHECK_CLASS_TYPE(klass,otype)  ( \
  gtk_type_is_a (((GtkObjectClass*) (klass))->type, (otype)) \
)

/* Macro for casting a pointer to a GtkObject or GtkObjectClass pointer.
 * The second portion of the ?: statments are just in place to offer
 * descriptive warning message.
 */
#define GTK_OBJECT(object)		( \
  GTK_IS_OBJECT (object) ? \
    (GtkObject*) (object) : \
    (GtkObject*) gtk_object_check_cast ((GtkObject*) (object), GTK_TYPE_OBJECT) \
)
#define GTK_OBJECT_CLASS(klass)		( \
  GTK_IS_OBJECT_CLASS (klass) ? \
    (GtkObjectClass*) (klass) : \
    (GtkObjectClass*) gtk_object_check_class_cast ((GtkObjectClass*) (klass), GTK_TYPE_OBJECT) \
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
enum
{
  GTK_DESTROYED		= 1 << 0,
  GTK_FLOATING		= 1 << 1,
  GTK_CONNECTED		= 1 << 2,
  GTK_RESERVED_2	= 1 << 3,
  GTK_OBJECT_FLAG_LAST	= GTK_RESERVED_2
};

/* Macros for extracting the object_flags from GtkObject.
 */
#define GTK_OBJECT_FLAGS(obj)		  (GTK_OBJECT (obj)->flags)
#define GTK_OBJECT_DESTROYED(obj)	  (GTK_OBJECT_FLAGS (obj) & GTK_DESTROYED)
#define GTK_OBJECT_FLOATING(obj)	  (GTK_OBJECT_FLAGS (obj) & GTK_FLOATING)
#define GTK_OBJECT_CONNECTED(obj)	  (GTK_OBJECT_FLAGS (obj) & GTK_CONNECTED)

/* Macros for setting and clearing bits in the object_flags field of GtkObject.
 */
#define GTK_OBJECT_SET_FLAGS(obj,flag)	  G_STMT_START{ (GTK_OBJECT_FLAGS (obj) |= (flag)); }G_STMT_END
#define GTK_OBJECT_UNSET_FLAGS(obj,flag)  G_STMT_START{ (GTK_OBJECT_FLAGS (obj) &= ~(flag)); }G_STMT_END

/* GtkArg flag bits for gtk_object_add_arg_type
 */
enum
{
  GTK_ARG_READABLE	= 1 << 0,
  GTK_ARG_WRITABLE	= 1 << 1,
  GTK_ARG_CONSTRUCT	= 1 << 2,
  /* aliases
   */
  GTK_ARG_READWRITE	= GTK_ARG_READABLE | GTK_ARG_WRITABLE
};


typedef struct _GtkObjectClass	GtkObjectClass;


/* GtkObject is the base of the object hierarchy. It defines
 *  the few basic items that all derived classes contain.
 */
struct _GtkObject
{
  /* A pointer to the objects class. This will actually point to
   *  the derived objects class struct (which will be derived from
   *  GtkObjectClass).
   */
  GtkObjectClass *klass;
  
  /* 32 bits of flags. GtkObject only uses 4 of these bits and
   *  GtkWidget uses the rest. This is done because structs are
   *  aligned on 4 or 8 byte boundaries. If a new bitfield were
   *  used in GtkWidget much space would be wasted.
   */
  guint32 flags;
  
  /* reference count.
   * refer to the file REFCOUNTING on this issue.
   */
  guint ref_count;
  
  /* The list of signal handlers and other data
   *  fields for this object.
   */
  gpointer object_data;
};

/* GtkObjectClass is the base of the class hierarchy. It defines
 *  the basic necessities for the class mechanism to work. Namely,
 *  the "type", "signals" and "nsignals" fields.
 */
struct _GtkObjectClass
{
  /* The type identifier for the objects class. There is
   *  one unique identifier per class.
   */
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
  
  /* The functions that will end an objects life time. In one way ore
   *  another all three of them are defined for all objects. If an
   *  object class overrides one of the methods in order to perform class
   *  specific destruction then it must still invoke its superclass'
   *  implementation of the method after it is finished with its
   *  own cleanup. (See the destroy function for GtkWidget for
   *  an example of how to do this).
   */
  void (* shutdown) (GtkObject *object);
  void (* destroy) (GtkObject *object);
  
  void (* finalize) (GtkObject *object);
};


/* For the purpose of user signals we need the signal function
 * and signal marshaller signatures already in this place.
 */
#define GTK_SIGNAL_FUNC(f)  ((GtkSignalFunc) f)

typedef void (*GtkSignalFunc)	    (void);
typedef void (*GtkSignalMarshaller) (GtkObject	    *object,
				     GtkSignalFunc   func,
				     gpointer	     func_data,
				     GtkArg	    *args);


/* Get the type identifier for GtkObject's.
 */
GtkType	gtk_object_get_type		(void);

/* Append "signals" to those already defined in "class".
 */
void	gtk_object_class_add_signals	(GtkObjectClass	*klass,
					 guint		*signals,
					 guint		 nsignals);

/* Append a user defined signal without default handler to a class.
 */
guint	gtk_object_class_add_user_signal (GtkObjectClass     *klass,
					  const gchar	     *name,
					  GtkSignalMarshaller marshaller,
					  GtkType	      return_val,
					  guint		      nparams,
					  ...);

GtkObject*	gtk_object_new		(GtkType	type,
					 ...);

GtkObject*	gtk_object_newv		(GtkType	type,
					 guint		nargs,
					 GtkArg		*args);
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
 * if arg->type == GTK_TYPE_STRING, it's the callers response to
 * do a g_free (GTK_VALUE_STRING (arg));
 */
void	gtk_object_getv		(GtkObject	*object,
				 guint		nargs,
				 GtkArg		*args);

/* gtk_object_set() takes a variable argument list of the form:
 * (..., gchar *arg_name, ARG_VALUES, [repeatedly name/value pairs,] NULL)
 * where ARG_VALUES type depend on the argument and can consist of
 * more than one c-function argument.
 */
void	gtk_object_set		(GtkObject	*object,
				 ...);

void	gtk_object_setv		(GtkObject	*object,
				 guint		nargs,
				 GtkArg		*args);

/* Allocate a GtkArg array of size nargs that hold the
 * names and types of the args that can be used with
 * gtk_object_set/gtk_object_get. if (arg_flags!=NULL),
 * (*arg_flags) will be set to point to a newly allocated
 * guint array that holds the flags of the args.
 * It is the callers response to do a
 * g_free (returned_args); g_free (*acess_masks).
 */
GtkArg* gtk_object_query_args	(GtkType	class_type,
				 guint32	**arg_flags,
				 guint		*nargs);

void	gtk_object_add_arg_type	(const gchar	*arg_name,
				 GtkType	arg_type,
				 guint		arg_flags,
				 guint		arg_id);

GtkType	gtk_object_get_arg_type	(const gchar	*arg_name);

/* Set 'data' to the "object_data" field of the object. The
 *  data is indexed by the "key". If there is already data
 *  associated with "key" then the new data will replace it.
 *  If 'data' is NULL then this call is equivalent to
 *  'gtk_object_remove_data'.
 */
void gtk_object_set_data       (GtkObject   *object,
				const gchar *key,
				gpointer     data);

/* Like gtk_object_set_data, but takes an additional argument
 * which is a function to be called when the data is removed.
 */
void gtk_object_set_data_full	    (GtkObject	 *object,
				     const gchar *key,
				     gpointer	  data,
				     GtkDestroyNotify destroy);

/* Get the data associated with "key".
 */
gpointer gtk_object_get_data	   (GtkObject	*object,
				    const gchar *key);

/* Remove the data associated with "key". This call is
 *  equivalent to 'gtk_object_set_data' where 'data' is NULL.
 */
void gtk_object_remove_data	  (GtkObject   *object,
				   const gchar *key);

/* Object data functions that operate on key ids.
 *  These functions are meant for *internal* use only.
 */
void gtk_object_set_data_by_id	    (GtkObject	     *object,
				     guint	      data_id,
				     gpointer	      data);
void gtk_object_set_data_by_id_full (GtkObject	     *object,
				     guint	      data_id,
				     gpointer	      data,
				     GtkDestroyNotify destroy);
gpointer gtk_object_get_data_by_id  (GtkObject	     *object,
				     guint	      data_id);
void  gtk_object_remove_data_by_id  (GtkObject	     *object,
				     guint	      data_id);
guint gtk_object_data_try_key	    (const gchar     *key);
guint gtk_object_data_force_id	    (const gchar     *key);

/* Set the "user_data" object data field of "object". It should
 *  be noted that this is no different than calling 'gtk_object_set_data'
 *  with a key of "user_data". It is merely provided as a convenience.
 */
void gtk_object_set_user_data (GtkObject *object,
			       gpointer	  data);

/* Get the "user_data" object data field of "object". It should
 *  be noted that this is no different than calling 'gtk_object_get_data'
 *  with a key of "user_data". It is merely provided as a convenience.
 */
gpointer gtk_object_get_user_data (GtkObject *object);

GtkObject* gtk_object_check_cast (GtkObject *obj,
				  GtkType    cast_type);

GtkObjectClass* gtk_object_check_class_cast (GtkObjectClass *klass,
					     GtkType	     cast_type);

void	gtk_trace_referencing	(GtkObject   *object,
				 const gchar *func,
				 guint	      dummy,
				 guint	      line,
				 gboolean     do_ref);

#if G_ENABLE_DEBUG
#  define gtk_object_ref(o)   G_STMT_START{gtk_trace_referencing((o),G_GNUC_PRETTY_FUNCTION,0,__LINE__,1);}G_STMT_END
#  define gtk_object_unref(o) G_STMT_START{gtk_trace_referencing((o),G_GNUC_PRETTY_FUNCTION,0,__LINE__,0);}G_STMT_END
#endif	/* G_ENABLE_DEBUG && __GNUC__ */



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_OBJECT_H__ */
