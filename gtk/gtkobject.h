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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_OBJECT_H__
#define __GTK_OBJECT_H__


#include <gtk/gtkenums.h>
#include <gtk/gtktypeutils.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* GtkObject only uses the first 3 bits of the "flags" field.
 *  They refer to the following flags.
 * GtkWidget uses the remaining bits. Though this is a kinda nasty
 *  break up, it does make the size of GtkWidget smaller.
 */
enum
{
  GTK_NEED_DESTROY      = 1 << 0,
  GTK_BEING_DESTROYED   = 1 << 1,
  GTK_IN_CALL           = 1 << 2
};


/* The debugging versions of the casting macros make sure the cast is "ok"
 *  before proceeding, but they are definately slower than their less
 *  careful counterparts as they involve no less than 3 function calls.
 */
#ifdef NDEBUG

#define GTK_CHECK_CAST(obj,cast_type,cast)         ((cast*) obj)
#define GTK_CHECK_CLASS_CAST(klass,cast_type,cast) ((cast*) klass)

#else /* NDEBUG */

#define GTK_CHECK_CAST(obj,cast_type,cast) \
  ((cast*) gtk_object_check_cast ((GtkObject*) obj, cast_type))

#define GTK_CHECK_CLASS_CAST(klass,cast_type,cast) \
  ((cast*) gtk_object_check_class_cast ((GtkObjectClass*) klass, cast_type))

#endif /* NDEBUG */


/* Determines whether 'obj' is a type of 'otype'.
 */
#define GTK_CHECK_TYPE(obj,otype)  (gtk_type_is_a (((GtkObject*) obj)->klass->type, otype))


/* Macro for casting a pointer to a GtkObject pointer.
 */
#define GTK_OBJECT(obj)                   GTK_CHECK_CAST (obj, gtk_object_get_type (), GtkObject)

/* Macros for extracting various fields from GtkObject and
 *  GtkObjectClass.
 */
#define GTK_OBJECT_CLASS(klass)           GTK_CHECK_CLASS_CAST (klass, gtk_object_get_type (), GtkObjectClass)
#define GTK_OBJECT_FLAGS(obj)             (GTK_OBJECT (obj)->flags)
#define GTK_OBJECT_NEED_DESTROY(obj)      (GTK_OBJECT_FLAGS (obj) & GTK_NEED_DESTROY)
#define GTK_OBJECT_BEING_DESTROYED(obj)   (GTK_OBJECT_FLAGS (obj) & GTK_BEING_DESTROYED)
#define GTK_OBJECT_IN_CALL(obj)           (GTK_OBJECT_FLAGS (obj) & GTK_IN_CALL)
#define GTK_OBJECT_DESTROY(obj)           (GTK_OBJECT (obj)->klass->destroy)
#define GTK_OBJECT_TYPE(obj)              (GTK_OBJECT (obj)->klass->type)
#define GTK_OBJECT_SIGNALS(obj)           (GTK_OBJECT (obj)->klass->signals)
#define GTK_OBJECT_NSIGNALS(obj)          (GTK_OBJECT (obj)->klass->nsignals)

/* Macro for testing whether "obj" is of type GtkObject.
 */
#define GTK_IS_OBJECT(obj)                GTK_CHECK_TYPE (obj, gtk_object_get_type ())

/* Macros for setting and clearing bits in the "flags" field of GtkObject.
 */
#define GTK_OBJECT_SET_FLAGS(obj,flag)    (GTK_OBJECT_FLAGS (obj) |= (flag))
#define GTK_OBJECT_UNSET_FLAGS(obj,flag)  (GTK_OBJECT_FLAGS (obj) &= ~(flag))


typedef struct _GtkObjectClass  GtkObjectClass;


/* GtkObject is the base of the object hierarchy. It defines
 *  the few basic items that all derived classes contain.
 */
struct _GtkObject
{
  /* 32 bits of flags. GtkObject only uses 3 of these bits and
   *  GtkWidget uses the rest. This is done because structs are
   *  aligned on 4 or 8 byte boundaries. If bitfields were used
   *  both here and in GtkWidget much space would be wasted.
   */
  guint32 flags;

  /* 16 bit reference count. "gtk_object_destroy" actually only
   *  destroys an object when its ref count is 0. (Decrementing
   *  a reference count of 0 is defined as a no-op).
   */
  guint16 ref_count;

  /* A pointer to the objects class. This will actually point to
   *  the derived objects class struct (which will be derived from
   *  GtkObjectClass).
   */
  GtkObjectClass *klass;

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
  gint *signals;

  /* The number of signals listed in "signals".
   */
  gint nsignals;

  /* The destroy function for objects. In one way ore another
   *  this is defined for all objects. If an object class overrides
   *  this method in order to perform class specific destruction
   *  then it should still call it after it is finished with its
   *  own cleanup. (See the destroy function for GtkWidget for
   *  an example of how to do this).
   */
  void (* destroy) (GtkObject *object);

  /* The number of arguments per class.
   */
  guint n_args;
};


/* Get the type identifier for GtkObject's.
 */
guint	gtk_object_get_type		(void);

/* Append "signals" to those already defined in "class".
 */
void	gtk_object_class_add_signals	(GtkObjectClass	*klass,
					 gint           *signals,
					 gint            nsignals);

GtkObject*	gtk_object_new		(guint		type,
					 ...);

GtkObject*	gtk_object_newv		(guint		type,
					 guint		nargs,
					 GtkArg		*args);

void	gtk_object_ref		(GtkObject	*object);

void	gtk_object_unref	(GtkObject	*object);

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
 * gtk_object_set/gtk_object_get.
 * It is the callers response to do a
 * g_free (returned_args).
 */
GtkArg* gtk_object_query_args   (GtkType	class_type,
				 guint          *nargs);

void	gtk_object_add_arg_type	(const gchar	*arg_name,
				 GtkType	arg_type,
				 guint		arg_id);

GtkType	gtk_object_get_arg_type	(const gchar	*arg_name);

/* Emit the "destroy" signal for "object". Normally it is
 *  permissible to emit a signal for an object instead of
 *  calling the corresponding convenience routine, however
 *  "gtk_object_destroy" should be called instead of emitting
 *  the signal manually as it checks to see if the object is
 *  currently handling another signal emittion (very likely)
 *  and sets the GTK_NEED_DESTROY flag which tells the object
 *  to be destroyed when it is done handling the signal emittion.
 */
void gtk_object_destroy	(GtkObject *object);

/* Set 'data' to the "object_data" field of the object. The
 *  data is indexed by the "key". If there is already data
 *  associated with "key" then the new data will replace it.
 *  If 'data' is NULL then this call is equivalent to
 *  'gtk_object_remove_data'.
 */
void gtk_object_set_data (GtkObject   *object,
			  const gchar *key,
			  gpointer     data);

/* Get the data associated with "key".
 */
gpointer gtk_object_get_data (GtkObject   *object,
			      const gchar *key);

/* Remove the data associated with "key". This call is
 *  equivalent to 'gtk_object_set_data' where 'data' is NULL.
 */
void gtk_object_remove_data (GtkObject   *object,
			     const gchar *key);

/* Set the "user_data" object data field of "object". It should
 *  be noted that this is no different than calling 'gtk_object_data_add'
 *  with a key of "user_data". It is merely provided as a convenience.
 */
void gtk_object_set_user_data (GtkObject *object,
			       gpointer   data);

/* Get the "user_data" object data field of "object". It should
 *  be noted that this is no different than calling 'gtk_object_data_find'
 *  with a key of "user_data". It is merely provided as a convenience.
 */
gpointer gtk_object_get_user_data (GtkObject *object);

GtkObject* gtk_object_check_cast (GtkObject *obj,
				  GtkType    cast_type);

GtkObjectClass* gtk_object_check_class_cast (GtkObjectClass *klass,
					     GtkType         cast_type);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_OBJECT_H__ */
